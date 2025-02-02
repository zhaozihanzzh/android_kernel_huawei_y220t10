/*
 * Vendor specific serial Gadget Driver use for put modem log.
 * From the Gadget side, it is a misc device(No TTY),
 * from PC side, sprd's usb2serial driver will enumerate the
 * device as a serial device.
 * This driver basicly from android adb driver, many thanks to
 * Android guys.
 *
 * Copyright (C) 2011 Spreadtrum, Inc.
 * Author: Yingchun Li<yingchun.li@spreadtrum.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* #define DEBUG */
/* #define VERBOSE_DEBUG */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/err.h>
#include <linux/interrupt.h>

#include <linux/types.h>
#include <linux/device.h>
#include <linux/miscdevice.h>

#include <linux/usb/android_composite.h>

#include <linux/sched.h>
#include <mach/board.h>

#define BULK_BUFFER_SIZE           4096

/* number of tx requests to allocate */
#define TX_REQ_MAX 4

static const char shortname[] = "vser";

struct vser_dev {
	struct usb_function function;
	struct usb_composite_dev *cdev;
	spinlock_t lock;

	struct usb_ep *ep_in;
	struct usb_ep *ep_out;

	int online;
	int rd_error;
	int wr_error;

	atomic_t read_excl;
	atomic_t write_excl;
	atomic_t open_excl;
	int open_count;

	struct list_head tx_idle;

	wait_queue_head_t read_wq;
	wait_queue_head_t write_wq;
	struct usb_request *rx_req;
	int rx_done;
};

static struct usb_interface_descriptor vser_interface_desc = {
	.bLength                = USB_DT_INTERFACE_SIZE,
	.bDescriptorType        = USB_DT_INTERFACE,
	.bInterfaceNumber       = 0,
	.bNumEndpoints          = 2,
	.bInterfaceClass        = USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass     = 0,
	.bInterfaceProtocol     = 0,
};

static struct usb_endpoint_descriptor vser_highspeed_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor vser_highspeed_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor vser_fullspeed_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor vser_fullspeed_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *fs_vser_descs[] = {
	(struct usb_descriptor_header *) &vser_interface_desc,
	(struct usb_descriptor_header *) &vser_fullspeed_in_desc,
	(struct usb_descriptor_header *) &vser_fullspeed_out_desc,
	NULL,
};

static struct usb_descriptor_header *hs_vser_descs[] = {
	(struct usb_descriptor_header *) &vser_interface_desc,
	(struct usb_descriptor_header *) &vser_highspeed_in_desc,
	(struct usb_descriptor_header *) &vser_highspeed_out_desc,
	NULL,
};


/* temporary variable used between vser_open() and vser_gadget_bind() */
static struct vser_dev *_vser_dev;

static inline struct vser_dev *func_to_dev(struct usb_function *f)
{
	return container_of(f, struct vser_dev, function);
}


static struct usb_request *vser_request_new(struct usb_ep *ep, int buffer_size)
{
	struct usb_request *req = usb_ep_alloc_request(ep, GFP_KERNEL);
	if (!req)
		return NULL;

	/* now allocate buffers for the requests */
	req->buf = kmalloc(buffer_size, GFP_KERNEL);
	if (!req->buf) {
		usb_ep_free_request(ep, req);
		return NULL;
	}

	return req;
}

static void vser_request_free(struct usb_request *req, struct usb_ep *ep)
{
	if (req) {
		kfree(req->buf);
		usb_ep_free_request(ep, req);
	}
}

static inline int _lock(atomic_t *excl)
{
	if (atomic_inc_return(excl) == 1) {
		return 0;
	} else {
		atomic_dec(excl);
		return -1;
	}
}

static inline void _unlock(atomic_t *excl)
{
	atomic_dec(excl);
}

/* add a request to the tail of a list */
static void req_put(struct vser_dev *dev, struct list_head *head,
		struct usb_request *req)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	list_add_tail(&req->list, head);
	spin_unlock_irqrestore(&dev->lock, flags);
}

/* remove a request from the head of a list */
static struct usb_request *req_get(struct vser_dev *dev, struct list_head *head)
{
	unsigned long flags;
	struct usb_request *req;

	spin_lock_irqsave(&dev->lock, flags);
	if (list_empty(head)) {
		req = 0;
	} else {
		req = list_first_entry(head, struct usb_request, list);
		list_del(&req->list);
	}
	spin_unlock_irqrestore(&dev->lock, flags);
	return req;
}

static void vser_complete_in(struct usb_ep *ep, struct usb_request *req)
{
	struct vser_dev *dev = _vser_dev;

	if (req->status != 0)
		dev->wr_error = 1;

	req_put(dev, &dev->tx_idle, req);

	wake_up(&dev->write_wq);
}

static void vser_complete_out(struct usb_ep *ep, struct usb_request *req)
{
	struct vser_dev *dev = _vser_dev;

	dev->rx_done = 1;
	if (req->status != 0)
		dev->rd_error = 1;

	wake_up(&dev->read_wq);
}

static int __init create_bulk_endpoints(struct vser_dev *dev,
				struct usb_endpoint_descriptor *in_desc,
				struct usb_endpoint_descriptor *out_desc)
{
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req;
	struct usb_ep *ep;
	int i;

	DBG(cdev, "create_bulk_endpoints dev: %p\n", dev);

	ep = usb_ep_autoconfig(cdev->gadget, in_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for ep_in failed\n");
		return -ENODEV;
	}
	DBG(cdev, "usb_ep_autoconfig for ep_in got %s\n", ep->name);
	ep->driver_data = dev;		/* claim the endpoint */
	dev->ep_in = ep;

	ep = usb_ep_autoconfig(cdev->gadget, out_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for ep_out failed\n");
		return -ENODEV;
	}
	DBG(cdev, "usb_ep_autoconfig for vser ep_out got %s\n", ep->name);
	ep->driver_data = dev;		/* claim the endpoint */
	dev->ep_out = ep;

	/* now allocate requests for our endpoints */
	req = vser_request_new(dev->ep_out, BULK_BUFFER_SIZE);
	if (!req)
		goto fail;
	req->complete = vser_complete_out;
	dev->rx_req = req;

	for (i = 0; i < TX_REQ_MAX; i++) {
		req = vser_request_new(dev->ep_in, BULK_BUFFER_SIZE);
		if (!req)
			goto fail;
		req->complete = vser_complete_in;
		req_put(dev, &dev->tx_idle, req);
	}

	return 0;

fail:
	printk(KERN_ERR "vser_bind() could not allocate requests\n");
	return -1;
}

static ssize_t vser_read(struct file *fp, char __user *buf,
				size_t count, loff_t *pos)
{
	struct vser_dev *dev = fp->private_data;
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req;
	int r = count, xfer;
	int ret;

	DBG(cdev, "vser_read(%d)\n", count);

	if (count > BULK_BUFFER_SIZE)
		return -EINVAL;

	if (_lock(&dev->read_excl))
		return -EBUSY;

	/* we will block until we're online */
	while (!(dev->online || dev->rd_error)) {
		DBG(cdev, "vser_read: waiting for online state\n");
		ret = wait_event_interruptible(dev->read_wq,
				(dev->online || dev->rd_error));
		if (ret < 0) {
			_unlock(&dev->read_excl);
			return ret;
		}
	}
	if (dev->rd_error) {
		r = -EIO;
		goto done;
	}

requeue_req:
	/* queue a request */
	req = dev->rx_req;
	req->length = count;
	dev->rx_done = 0;
	ret = usb_ep_queue(dev->ep_out, req, GFP_ATOMIC);
	if (ret < 0) {
		DBG(cdev, "vser_read: failed to queue req %p (%d)\n", req, ret);
		r = -EIO;
		dev->rd_error = 1;
		goto done;
	} else {
		DBG(cdev, "rx %p queue\n", req);
	}

	/* wait for a request to complete */
	ret = wait_event_interruptible(dev->read_wq, dev->rx_done);
	if (ret < 0) {
		dev->rd_error = 1;
		r = ret;
		goto done;
	}
	if (!dev->rd_error) {
		/* If we got a 0-len packet, throw it back and try again. */
		if (req->actual == 0)
			goto requeue_req;

		DBG(cdev, "rx %p %d\n", req, req->actual);
		xfer = (req->actual < count) ? req->actual : count;
		r = xfer;
		if (copy_to_user(buf, req->buf, xfer))
			r = -EFAULT;
	} else
		r = -EIO;

done:
	_unlock(&dev->read_excl);
	DBG(cdev, "vser_read returning %d\n", r);
	return r;
}

static ssize_t vser_write(struct file *fp, const char __user *buf,
				 size_t count, loff_t *pos)
{
	struct vser_dev *dev = fp->private_data;
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req = 0;
	int r = count, xfer;
	int ret;

	DBG(cdev, "vser_write(%d)\n", count);

	if (_lock(&dev->write_excl))
		return -EBUSY;

	/* we will block until we're online */
	while (!(dev->online || dev->wr_error)) {
		DBG(cdev, "vser_write: waiting for online state\n");
		ret = wait_event_interruptible(dev->write_wq,
				(dev->online || dev->wr_error));
		if (ret < 0) {
			_unlock(&dev->write_excl);
			return ret;
		}
	}
	while (count > 0) {
		if (dev->wr_error) {
			DBG(cdev, "vser_write dev->wr_error\n");
			r = -EIO;
			break;
		}

		/* get an idle tx request to use */
		req = 0;
		ret = wait_event_interruptible(dev->write_wq,
			((req = req_get(dev, &dev->tx_idle)) || dev->wr_error));

		if (ret < 0) {
			r = ret;
			break;
		}

		if (req != 0) {
			if (count > BULK_BUFFER_SIZE)
				xfer = BULK_BUFFER_SIZE;
			else
				xfer = count;
			if (copy_from_user(req->buf, buf, xfer)) {
				r = -EFAULT;
				break;
			}

			req->length = xfer;
			ret = usb_ep_queue(dev->ep_in, req, GFP_ATOMIC);
			if (ret < 0) {
				DBG(cdev, "vser_write: xfer error %d\n", ret);
				dev->wr_error = 1;
				r = -EIO;
				break;
			}

			buf += xfer;
			count -= xfer;

			/* zero this so we don't try to free it on error exit */
			req = 0;
		}
	}

	if (req)
		req_put(dev, &dev->tx_idle, req);

	_unlock(&dev->write_excl);
	DBG(cdev, "vser_write returning %d\n", r);
	return r;
}

static int vser_open(struct inode *ip, struct file *fp)
{
	struct vser_dev *dev = _vser_dev;

	/*
	if (_lock(&_vser_dev->open_excl))
		return -EBUSY;
	*/
	spin_lock_irq(&dev->lock);
	fp->private_data = dev;

	dev->open_count++;
	if (dev->open_count > 2){
		dev->open_count--;
		pr_warning("too many user open vser\n");
		spin_unlock_irq(&dev->lock);
		return -EBUSY;
	}
	/* clear the error latch */
	dev->wr_error = 0;
	dev->rd_error = 0;

	spin_unlock_irq(&dev->lock);
	printk(KERN_INFO "vser_open %d times\n", dev->open_count);
	return 0;
}

static int vser_release(struct inode *ip, struct file *fp)
{
	struct vser_dev *dev = _vser_dev;

	printk(KERN_INFO "vser_release %d\n", dev->open_count);

	spin_lock_irq(&dev->lock);
	dev->open_count--;
	spin_unlock_irq(&dev->lock);
	//_unlock(&_vser_dev->open_excl);
	return 0;
}

/* file operations for ADB device /dev/android_vser */
static struct file_operations vser_fops = {
	.owner = THIS_MODULE,
	.read = vser_read,
	.write = vser_write,
	.open = vser_open,
	.release = vser_release,
};

static struct miscdevice vser_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = shortname,
	.fops = &vser_fops,
};


static int
vser_function_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct vser_dev	*dev = func_to_dev(f);
	int			id;
	int			ret;

	dev->cdev = cdev;
	DBG(cdev, "vser_function_bind dev: %p\n", dev);

	/* allocate interface ID(s) */
	id = usb_interface_id(c, f);
	if (id < 0)
		return id;
	vser_interface_desc.bInterfaceNumber = id;

	/* allocate endpoints */
	ret = create_bulk_endpoints(dev, &vser_fullspeed_in_desc,
			&vser_fullspeed_out_desc);
	if (ret)
		return ret;

	/* support high speed hardware */
	if (gadget_is_dualspeed(c->cdev->gadget)) {
		vser_highspeed_in_desc.bEndpointAddress =
			vser_fullspeed_in_desc.bEndpointAddress;
		vser_highspeed_out_desc.bEndpointAddress =
			vser_fullspeed_out_desc.bEndpointAddress;
	}

	DBG(cdev, "%s speed %s: IN/%s, OUT/%s\n",
			gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
			f->name, dev->ep_in->name, dev->ep_out->name);
	return 0;
}

static void
vser_function_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct vser_dev	*dev = func_to_dev(f);
	struct usb_request *req;

	spin_lock_irq(&dev->lock);

	vser_request_free(dev->rx_req, dev->ep_out);
	while ((req = req_get(dev, &dev->tx_idle)))
		vser_request_free(req, dev->ep_in);

	dev->online = 0;
	dev->wr_error = 1;
	dev->rd_error = 1;
	spin_unlock_irq(&dev->lock);

	misc_deregister(&vser_device);
	kfree(_vser_dev);
	_vser_dev = NULL;
}

static int vser_function_set_alt(struct usb_function *f,
		unsigned intf, unsigned alt)
{
	struct vser_dev	*dev = func_to_dev(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	int ret;

	DBG(cdev, "vser_function_set_alt intf: %d alt: %d\n", intf, alt);
	ret = usb_ep_enable(dev->ep_in,
			ep_choose(cdev->gadget,
				&vser_highspeed_in_desc,
				&vser_fullspeed_in_desc));
	if (ret)
		return ret;
	ret = usb_ep_enable(dev->ep_out,
			ep_choose(cdev->gadget,
				&vser_highspeed_out_desc,
				&vser_fullspeed_out_desc));
	if (ret) {
		usb_ep_disable(dev->ep_in);
		return ret;
	}

	/* readers may be blocked waiting for us to go online */
/*
	wake_up(&dev->read_wq);
	wake_up(&dev->write_wq);
*/
	return 0;
}

static void vser_function_disable(struct usb_function *f)
{
	struct vser_dev	*dev = func_to_dev(f);
	struct usb_composite_dev	*cdev = dev->cdev;

	DBG(cdev, "vser_function_disable\n");
	dev->online = 0;
	dev->wr_error = 1;
	dev->rd_error = 1;
	usb_ep_disable(dev->ep_in);
	usb_ep_disable(dev->ep_out);

	/* readers may be blocked waiting for us to go online */
	wake_up(&dev->read_wq);
	wake_up(&dev->write_wq);

	VDBG(cdev, "%s disabled\n", dev->function.name);
}

static int vser_setup(struct usb_function *f, const struct usb_ctrlrequest *ctrl)
{
	struct usb_composite_dev *cdev = f->config->cdev;
	struct vser_dev	*dev = func_to_dev(f);
	u16 w_length = le16_to_cpu(ctrl->wLength);
	u16 w_value = le16_to_cpu(ctrl->wValue);
	int value = -EOPNOTSUPP;
	int port_open = false;

	VDBG(cdev, "request :%x request type: %x, wValue %x, wIndex %x, wLength %x\n",
	 ctrl->bRequest, ctrl->bRequestType,
	 w_value,
	 le16_to_cpu(ctrl->wIndex),
	 w_length);
	/* Handle Bulk-only class-specific requests */
	if ((ctrl->bRequestType & USB_TYPE_MASK) == USB_TYPE_CLASS) {
		switch (ctrl->bRequest) {
		case 0x22:
			value = 0;
			if ((w_value & 0xff) == 1)
				port_open = true;
			break;
		}
	}

	/* respond with data transfer or status phase? */
	if (value >= 0) {
		int rc;
		cdev->req->zero = value < w_length;
		cdev->req->length = value;
		rc = usb_ep_queue(cdev->gadget->ep0, cdev->req, GFP_ATOMIC);
		if (rc < 0)
			printk("%s setup response queue error\n", __func__);
	}

	if (value == -EOPNOTSUPP)
		VDBG(cdev,
			"unknown class-specific control req "
			"%02x.%02x v%04x i%04x l%u\n",
			ctrl->bRequestType, ctrl->bRequest,
			le16_to_cpu(ctrl->wValue), le16_to_cpu(ctrl->wIndex),
			le16_to_cpu(ctrl->wLength));
	if (port_open){
		DBG(cdev, "cmd for port open\n");
		dev->online = 1;
		wake_up(&dev->read_wq);
		wake_up(&dev->write_wq);
	}
	return value;
}

static int vser_bind_config(struct usb_configuration *c)
{
	struct vser_dev *dev;
	int ret;

	printk(KERN_INFO "vser_bind_config\n");

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	spin_lock_init(&dev->lock);

	init_waitqueue_head(&dev->read_wq);
	init_waitqueue_head(&dev->write_wq);

	atomic_set(&dev->open_excl, 0);
	atomic_set(&dev->read_excl, 0);
	atomic_set(&dev ->write_excl, 0);
	dev->open_count = 0;

	INIT_LIST_HEAD(&dev->tx_idle);

	dev->cdev = c->cdev;
	dev->function.name = "vser";
	dev->function.descriptors = fs_vser_descs;
	dev->function.hs_descriptors = hs_vser_descs;
	dev->function.bind = vser_function_bind;
	dev->function.unbind = vser_function_unbind;
	dev->function.set_alt = vser_function_set_alt;
	dev->function.disable = vser_function_disable;
	dev->function.setup = vser_setup;
	if (!in_factory_mode())
		dev->function.disabled = 1;

	/* _vser_dev must be set before calling usb_gadget_register_driver */
	_vser_dev = dev;

	ret = misc_register(&vser_device);
	if (ret)
		goto err1;

	ret = usb_add_function(c, &dev->function);
	if (ret)
		goto err2;

	return 0;

err2:
	misc_deregister(&vser_device);
err1:
	kfree(dev);
	printk(KERN_ERR "vser gadget driver failed to initialize\n");
	return ret;
}

static struct android_usb_function vser_function = {
	.name = "vser",
	.bind_config = vser_bind_config,
};

static int __init init(void)
{
	printk(KERN_INFO "f_vser init\n");
	android_register_function(&vser_function);
	return 0;
}
module_init(init);

