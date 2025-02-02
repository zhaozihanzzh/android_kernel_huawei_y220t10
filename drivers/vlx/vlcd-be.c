/*
 ****************************************************************
 *
 *  Component:	VirtualLogix VLCD backend
 *
 *  Copyright (C) 2008-2010, VirtualLogix. All Rights Reserved.
 *
 *  Contributor(s):
 *    Adam Mirowski (adam.mirowski@vlx.com)
 *
 ****************************************************************
 */

/*
 * This is the back end of the vlcd driver.
 * Its intialization starts after real lcd initialization.
 * When an event is received from a vlcd frontend,
 * a hw driver callback can be called to handle it.
 */

#define _VLCD_BACKEND_C_

#include <linux/fb.h>
#include <linux/slab.h>

#include <vlx/vlcd_backend.h>
#include "vlcd-vogl.h"

/*----- Tracing -----*/

#define TRACE(format, args...)	 printk ("VLCD-BE: " format, ## args)
#define ETRACE(format, args...)	 TRACE ("[E] " format, ## args)
#define EFTRACE(format, args...) ETRACE ("%s: " format, __func__, ## args)

#ifdef VLCD_DEBUG
#define DTRACE(format, args...)	TRACE ("%s: " format, __func__, ## args)
#define VLCD_ASSERT(c)		BUG_ON (!(c))
#else
#define DTRACE(format, args...)	do {} while(0)
#define VLCD_ASSERT(c)
#endif

/*----- Driver -----*/

void vlcd_handle_command(vlcd_b_driver_t* vlcd_drv,
                         vlcd_frontend_device_t* frontend_device);
void vlcd_b_handshakes(vlcd_b_driver_t* vlcd_drv, nku8_f hw_device_id);
void vlcd_b_handshake(vlcd_b_driver_t* vlcd_drv,
                      vlcd_frontend_device_t* fdev);

#define VLCD_FOR_ALL_FRONTEND_DEVICES(dev, drv) \
    for ((dev) = (drv)->frontend_device_list; (dev); \
	 (dev) = (dev)->next_frontend_device)

static vlcd_b_driver_t vlcd_b_driver;

/*
 * This mutex is used to synchronize the registration of the different frame
 * buffer info structures. Three drivers (native, vlcd backend, vlcd frontend)
 * register these structures from two threads (the init thread and the thread
 * created by the vlcd frontend driver).
 */
DEFINE_MUTEX(vlcdMutex);
EXPORT_SYMBOL(vlcdMutex);

static int xirq_thread(void* cookie)
{
    vlcd_b_driver_t* vlcd_drv = (vlcd_b_driver_t*)cookie;
    vlcd_frontend_device_t* frontend_device;

    daemonize("vlcd_be");
    for (;;) {
        down(&vlcd_drv->xirq_semaphore);
        if (vlcd_drv->xirq_active == 0) {
            break;
        }
	VLCD_FOR_ALL_FRONTEND_DEVICES (frontend_device, vlcd_drv) {
            vlcd_handle_command(vlcd_drv, frontend_device);
        }
    }
    complete_and_exit(&vlcd_drv->xirq_completion, 0);
}

static void vlcd_cx_intr_handler(void* cookie, NkXIrq xirq)
{
    vlcd_b_driver_t* vlcd_drv = (vlcd_b_driver_t*)cookie;

    up(&vlcd_drv->xirq_semaphore);
}

static void vlcd_sysconf_intr_handler(void* cookie, NkXIrq xirq)
{
    vlcd_b_driver_t* vlcd_drv = (vlcd_b_driver_t*)cookie;
    vlcd_frontend_device_t* frontend_device;

    VLCD_FOR_ALL_FRONTEND_DEVICES (frontend_device, vlcd_drv) {
	vlcd_b_handshake(vlcd_drv, frontend_device);
    }
}

void vlcd_sysconf_trigger(vlcd_frontend_device_t* fdev)
{
    nkops.nk_xirq_trigger(NK_XIRQ_SYSCONF, fdev->vlink->c_id);
}

void vlcd_evt_reset_device(vlcd_b_driver_t* vlcd_drv,
                           vlcd_frontend_device_t* fdev)
{
    NkDevVlcd* shared = fdev->common;

    DTRACE ("fOsId:%i\n", fdev->vlink->c_id);

    shared->modified = 0;
    /* Reset current configuration. */
    memset(&(shared->current_conf), 0, sizeof(vlcd_conf_t));
    shared->flags = 0;
    shared->caps = 0;

    if (fdev->get_focus && vlcd_drv->hw_ops[fdev->vlink->link]) {
        vlcd_drv->hw_ops[fdev->vlink->link]->switch_off_screen(
                    vlcd_drv, fdev->vlink->link);
    }
    fdev->state = VLCD_FSTATE_UNINITIALIZED;
    fdev->vlink->s_state = NK_DEV_VLINK_RESET;
    vlcd_sysconf_trigger(fdev);
}

void vlcd_evt_init_device(vlcd_b_driver_t* vlcd_drv,
                          vlcd_frontend_device_t* fdev)
{
    DTRACE ("hw:%i, fosId:%i\n", fdev->vlink->link, fdev->vlink->c_id);

    if (vlcd_drv->hw_ops[fdev->vlink->link]) {
        vlcd_drv->hw_ops[fdev->vlink->link]->get_possible_config(
                vlcd_drv, fdev->vlink->link,
                fdev->common->pconf);
        fdev->vlink->s_state = NK_DEV_VLINK_ON;
        vlcd_sysconf_trigger(fdev);
    }
}

void vlcd_evt_frontend_init(vlcd_b_driver_t* vlcd_drv,
                            vlcd_frontend_device_t* fdev)
{
    NkDevVlcd* shared = fdev->common;
    if (fdev->state == VLCD_FSTATE_UNINITIALIZED) {
	if (voglEnabled()) {
	    voglUpdateFB(fdev);
	}

        if (fdev->get_focus) {
            vlcd_drv->hw_ops[fdev->vlink->link]->set_dma_zone(vlcd_drv, fdev->vlink->link,
                            shared->current_conf.dma_zone_size,
                            shared->current_conf.dma_zone_paddr);
        }
        fdev->state = VLCD_FSTATE_INITIALIZED;
	DTRACE ("received for hw:%i, fosId:%i\n",
		fdev->vlink->link, fdev->vlink->c_id);
    } else {
        // Already initialized. */
        EFTRACE ("received in wrong state:%lu\n",
                     (unsigned long)(fdev->state));
    }
}

void vlcd_evt_size_modified(vlcd_b_driver_t* vlcd_drv,
                            vlcd_frontend_device_t* fdev)
{
    NkDevVlcd* shared = fdev->common;

    DTRACE ("os:%u\n", fdev->vlink->c_id);
    if (fdev->state == VLCD_FSTATE_INITIALIZED) {
        if (fdev->get_focus) {
            vlcd_drv->hw_ops[fdev->vlink->link]->set_size(vlcd_drv, fdev->vlink->link,
                                       &(shared->current_conf));
        }
    } else {
        EFTRACE ("received in wrong state:%lu\n",
                     (unsigned long)(fdev->state));
    }
}

void vlcd_evt_color_modified(vlcd_b_driver_t* vlcd_drv,
                             vlcd_frontend_device_t* fdev)
{
    NkDevVlcd* shared = fdev->common;

    DTRACE ("os:%u\n", fdev->vlink->c_id);
    if (fdev->state == VLCD_FSTATE_INITIALIZED) {
        if (fdev->get_focus) {
            vlcd_drv->hw_ops[fdev->vlink->link]->set_color(vlcd_drv, fdev->vlink->link,
                                        &(shared->current_conf));
        }
    } else {
        EFTRACE ("received in wrong state:%lu\n",
                     (unsigned long)(fdev->state));
    }
}

    static void
vlcd_switch_off_screen (vlcd_b_driver_t* vlcd_drv,
                        vlcd_frontend_device_t* fdev)
{
    if (fdev->get_focus && fdev->state == VLCD_FSTATE_INITIALIZED) {
	vlcd_drv->hw_ops[fdev->vlink->link]->switch_off_screen(vlcd_drv, fdev->vlink->link);
	DTRACE ("switch_off_screen for link:%i, "
	        "c_id:%i\n", fdev->vlink->link, fdev->vlink->c_id);
    }
}

    static void
vlcd_switch_on_screen (vlcd_b_driver_t* vlcd_drv,
                       vlcd_frontend_device_t* fdev)
{
    if (fdev->get_focus && fdev->state == VLCD_FSTATE_INITIALIZED) {
	NkDevVlcd* shared = fdev->common;
	vlcd_drv->hw_ops[fdev->vlink->link]->set_color(vlcd_drv,
			 fdev->vlink->link, &(shared->current_conf));
	vlcd_drv->hw_ops[fdev->vlink->link]->set_dma_zone(vlcd_drv,
			 fdev->vlink->link,
			 shared->current_conf.dma_zone_size,
		         shared->current_conf.dma_zone_paddr);
	vlcd_drv->hw_ops[fdev->vlink->link]->set_size(vlcd_drv,
			 fdev->vlink->link, &(shared->current_conf));
	DTRACE ("switch_on_screen for link:%i, "
	        "c_id:%i\n", fdev->vlink->link, fdev->vlink->c_id);
    }
}

void vlcd_handle_command(vlcd_b_driver_t* vlcd_drv,
                         vlcd_frontend_device_t* fdev)
{
    if (fdev->event & VLCD_BEVENT_FRESET) {
	nkops.nk_atomic_clear(&fdev->event, VLCD_BEVENT_FRESET);
        vlcd_evt_reset_device(vlcd_drv, fdev);
    }
    if (fdev->event & VLCD_BEVENT_FSTARTED) {
	nkops.nk_atomic_clear(&fdev->event, VLCD_BEVENT_FSTARTED);
        vlcd_evt_init_device(vlcd_drv, fdev);
    }
    if (fdev->common->modified & VLCD_EVT_SIZE_MODIFIED) {
	nkops.nk_atomic_clear(&fdev->common->modified, VLCD_EVT_SIZE_MODIFIED);
        vlcd_evt_size_modified(vlcd_drv, fdev);
    }
    if (fdev->common->modified & VLCD_EVT_INIT) {
	nkops.nk_atomic_clear(&fdev->common->modified, VLCD_EVT_INIT);
        vlcd_evt_frontend_init(vlcd_drv, fdev);
    }
    if (fdev->common->modified & VLCD_EVT_COLOR_MODIFIED) {
	nkops.nk_atomic_clear(&fdev->common->modified, VLCD_EVT_COLOR_MODIFIED);
        vlcd_evt_color_modified(vlcd_drv, fdev);
    }
    if (fdev->common->modified & VLCD_EVT_FLAGS_MODIFIED) {
	nkops.nk_atomic_clear(&fdev->common->modified, VLCD_EVT_FLAGS_MODIFIED);
	if (fdev->common->flags & VLCD_FLAGS_SWITCH_OFF) {
	    vlcd_switch_off_screen(vlcd_drv, fdev);
	} else {
	    vlcd_switch_on_screen(vlcd_drv, fdev);
	}
    }
}

extern int focus_register_client(struct notifier_block *nb);
extern int focus_unregister_client(struct notifier_block *nb);

static struct notifier_block focus_nb;

int vlcd_notify_focus(struct notifier_block *self, 
                       unsigned long event, void *data) 
{
    vlcd_b_driver_t* vlcd_drv = &vlcd_b_driver;
    vlcd_frontend_device_t* fdev;
    NkDevVlcd* shared;
    NkOsId frontend_os_id;

    frontend_os_id = (NkOsId)event;
    DTRACE ("to OS#%d\n", frontend_os_id);

    /* Remove focus when needed. */
    VLCD_FOR_ALL_FRONTEND_DEVICES (fdev, vlcd_drv) {
        if (fdev->get_focus && (fdev->vlink->c_id != frontend_os_id)) {
            fdev->get_focus = 0;
            if (fdev->state == VLCD_FSTATE_INITIALIZED) {
                vlcd_drv->hw_ops[fdev->vlink->link]->switch_off_screen(
                        vlcd_drv, fdev->vlink->link);
            }
            DTRACE ("remove focus for link:%i, "
                    "c_id:%i\n", fdev->vlink->link, fdev->vlink->c_id);
        }
    }
    /* Set focus when needed. */
    VLCD_FOR_ALL_FRONTEND_DEVICES (fdev, vlcd_drv) {
        if ((fdev->get_focus == 0) && (fdev->vlink->c_id == frontend_os_id)) {
            fdev->get_focus = 1;
            if (fdev->state == VLCD_FSTATE_INITIALIZED &&
		!(fdev->common->flags & VLCD_FLAGS_SWITCH_OFF) ) {
                shared = fdev->common;
                vlcd_drv->hw_ops[fdev->vlink->link]->set_color(vlcd_drv,
				 fdev->vlink->link, &(shared->current_conf));
                vlcd_drv->hw_ops[fdev->vlink->link]->set_dma_zone(vlcd_drv,
				 fdev->vlink->link,
				 shared->current_conf.dma_zone_size,
			         shared->current_conf.dma_zone_paddr);
                vlcd_drv->hw_ops[fdev->vlink->link]->set_size(vlcd_drv,
				 fdev->vlink->link, &(shared->current_conf));
            }
            DTRACE ("set focus for link:%i, "
                    "c_id:%i\n", fdev->vlink->link, fdev->vlink->c_id);
        }
    }
    return 0;
}

int vlcd_create(vlcd_b_driver_t* vlcd_drv)
{
    NkDevVlcd*              shared;
    vlcd_frontend_device_t* fdev;
    vlcd_frontend_device_t* last;
    NkXIrqId                xid = 0;
    NkOsId                  my_os_id = nkops.nk_id_get();
    NkPhAddr                plink;
    NkDevVlink*             vlink;
    int                     ret = 0;
    char                    *vinfo;
    unsigned long           focused_os;

    /* Init globals. */
    vlcd_drv->frontend_device_list = NULL;
    vlcd_drv->xirq_active = 0;
    vlcd_drv->sysconf_xid = 0;

	/* Accessed by vlcd_cx_intr_handler() attached below */
    sema_init(&vlcd_drv->xirq_semaphore, 0);

    /* Create devices for secondaries. */
    plink = 0;
    last  = NULL;
    while ((plink = nkops.nk_vlink_lookup("vlcd", plink))) {
        vlink = nkops.nk_ptov(plink);
	if (vlink->link < 0 || vlink->link > VLCD_BMAX_HW_DEV_SUP) {
		EFTRACE ("vlink id %d is out of range (should from %d to %d)\n",
			     vlink->link, 0, VLCD_BMAX_HW_DEV_SUP - 1);
		goto error0;
	}
        if (vlink->s_id == my_os_id) {
	    NkPhAddr data;

            /* Allocate shared data if not already allocated. */
            data = nkops.nk_pdev_alloc(plink, 0, sizeof(NkDevVlcd));
	    if (data == 0) {
                EFTRACE ("can't allocate shared data\n");
                ret = -ENOMEM;
                goto error0;
            }
            shared = (NkDevVlcd*)nkops.nk_ptov(data);
            shared->cxirq = nkops.nk_pxirq_alloc(plink, 0, my_os_id, 1);
            if (!shared->cxirq) {
                EFTRACE ("can't allocate pxirq\n");
                ret = -ENXIO;
                goto error1;
            }
            /* Attach cross irq handler. */
            xid = nkops.nk_xirq_attach(shared->cxirq, vlcd_cx_intr_handler,
                                       vlcd_drv);
            if (xid == 0) {
                EFTRACE ("nk_xirq_attach failed\n");
                ret = -ENXIO;
                goto error2;
            }
            shared->modified = 0;
            /* Reset current configuration. */
            memset(&(shared->current_conf), 0,
                     sizeof(vlcd_conf_t));
	    shared->flags = 0;
	    shared->caps = 0;
            /* Reset possible configurations. */
            memset(&(shared->pconf), 0,
                     sizeof(vlcd_pconf_t) * VLCD_MAX_CONF_NUMBER);
            /* Create backend structure. */
            fdev = (vlcd_frontend_device_t*)kmalloc(
                               sizeof(vlcd_frontend_device_t), GFP_KERNEL);
            if (fdev == NULL) {
                EFTRACE ("can't allocate secondary device\n");
                ret = -ENOMEM;
                goto error3;
            }
            /* Set sec device fields. */
            fdev->next_frontend_device = NULL;
            fdev->common = shared;
            fdev->state = VLCD_FSTATE_UNINITIALIZED;
            fdev->xid = xid;
            fdev->vlink = vlink;
            if (vlink->s_info != 0) {
                vinfo = nkops.nk_ptov(vlink->s_info);
                focused_os = simple_strtoul(vinfo, NULL, 10);
            } else {
                focused_os = 0;
            }
            if (focused_os == 0) {
                /*
                 * Nothing precised in command line or bad command line.
                 * Let's say that this os has screen focus at the beginning.
                 */
                focused_os = my_os_id;
            }
            if (vlink->c_id == focused_os) {
                fdev->get_focus = 1;
            } else {
                fdev->get_focus = 0;
            }

	    fdev->fbInfo       = NULL;
	    fdev->fbRegistered = 0;
	    if (voglEnabled() && vlcd_drv->hw_ops[vlink->link]) {
		VLCD_ASSERT(vlcd_drv->hw_data[vlink->link]);
		ret = voglInitFB(fdev);
		if (ret) {
		    voglCleanupFB(fdev);
		}
	    }

            /* Add in list. */
            if (last == NULL) {
                vlcd_drv->frontend_device_list = fdev;
            } else {
                last->next_frontend_device = fdev;
            }
            last = fdev;
	    TRACE ("created backend for OS %i. hw_device %i\n",
		   fdev->vlink->c_id, fdev->vlink->link);
        }/* end I must create a device */
    }/* end while */

    init_completion(&vlcd_drv->xirq_completion);
    vlcd_drv->xirq_active = 1;

    vlcd_drv->sysconf_xid = nkops.nk_xirq_attach(NK_XIRQ_SYSCONF,
            vlcd_sysconf_intr_handler, vlcd_drv);
    if (vlcd_drv->sysconf_xid == 0) {
        EFTRACE ("attach to sysconf irq failed\n");
        ret = -ENXIO;
        goto error0;
    }

    if (kernel_thread(xirq_thread, vlcd_drv, CLONE_KERNEL) < 0) {
        EFTRACE ("xirq_thread create failed\n");
        ret = -ENOMEM;
        goto error4;
    }
    DTRACE ("ok\n");
    return 0;

error4:
    nkops.nk_xirq_detach(vlcd_drv->sysconf_xid);
    vlcd_drv->sysconf_xid = 0;
    goto error0;
error3:
    nkops.nk_xirq_detach(xid);
error2:
	/* No nk_free_irq ops */
error1:
	/* No nk_free_mem ops */
error0:
	/* Free list of completely allocated frontend device */
    vlcd_drv->xirq_active = 0;
    fdev = vlcd_drv->frontend_device_list;
    while (fdev != NULL) {
        nkops.nk_xirq_detach(fdev->xid);
	if (voglEnabled()) {
	    voglCleanupFB(fdev);
	}
        last = fdev->next_frontend_device;
        kfree(fdev);
        fdev = last;
    }
    return ret;
}

#ifdef MODULE
void vlcd_release(vlcd_b_driver_t* vlcd_drv)
{
    vlcd_frontend_device_t* fdev;
    vlcd_frontend_device_t* last;

    vlcd_drv->xirq_active = 0;
    up(&vlcd_drv->xirq_semaphore);
    wait_for_completion(&vlcd_drv->xirq_completion);
    nkops.nk_xirq_detach(vlcd_drv->sysconf_xid);
    vlcd_drv->sysconf_xid = 0;
    fdev = vlcd_drv->frontend_device_list;
    while(fdev != NULL)
    {
        nkops.nk_xirq_detach(fdev->xid);
	if (voglEnabled()) {
	    voglCleanupFB(fdev);
	}
        last = fdev->next_frontend_device;
        kfree(fdev);
        fdev = last;
    }
}
#endif

    /*
     * Called by the hw driver to retrieve driver specific information
     */
    void*
vlcd_b_get_hw_data (nku8_f hw_device_id)
{
    return vlcd_b_driver.hw_data[hw_device_id];
}

/* Callback called by hw driver when init done */
int vlcd_b_hw_initialized(void* hw_data, vlcd_hw_ops_t* hw_ops, nku8_f hw_device_id)
{
    NkOsId                  my_os_id = nkops.nk_id_get();
    NkPhAddr                plink;
    NkDevVlink*             vlink;
    int                     found_link = 0;
    int                     ret;
    vlcd_frontend_device_t* fdev;

    plink = 0;
    while ((plink = nkops.nk_vlink_lookup("vlcd", plink))) {
        vlink = nkops.nk_ptov(plink);
        if (vlink->s_id == my_os_id) {
	    if (vlink->link == hw_device_id) {
		found_link = 1;
		break;
	    }
	}
    }

    if (found_link == 0) {
	EFTRACE ("no vlink for device %d\n", hw_device_id);
	return -ENODEV;
    }
    
    vlcd_b_driver.hw_data[hw_device_id] = hw_data;

    if (voglEnabled()) {
	hw_ops->get_possible_config(&vlcd_b_driver, hw_device_id,
				    voglPConf[hw_device_id]);
    }

    /* Continue all handshakes (if they are started) */
    if (vlcd_b_driver.state[hw_device_id] == VLCD_BSTATE_INITIALIZING) {
	if (voglEnabled()) {
	    VLCD_FOR_ALL_FRONTEND_DEVICES (fdev, &vlcd_b_driver) {
		if (fdev->vlink->link == hw_device_id) {
		    ret = voglInitFB(fdev);
		    if (ret) {
			voglCleanupFB(fdev);
		    }
		}
	    }
	}

	vlcd_b_driver.hw_ops[hw_device_id] = hw_ops;

        vlcd_b_handshakes(&vlcd_b_driver, hw_device_id);
    } else {
	vlcd_b_driver.hw_ops[hw_device_id] = hw_ops;
    }
    DTRACE ("done for device %d\n", hw_device_id);
    return 0;
}

void vlcd_b_handshake(vlcd_b_driver_t* vlcd_drv,
                      vlcd_frontend_device_t* fdev)
{
    volatile int* my_state;
    int           peer_state;

    my_state   = &(fdev->vlink->s_state);
    peer_state =  fdev->vlink->c_state;

    DTRACE ("(hwId:%i, fId:%i) %i %i\n",
                 fdev->vlink->link,
                 fdev->vlink->c_id, *my_state,
                 peer_state);
    switch (*my_state) {
    case NK_DEV_VLINK_OFF:
        if (peer_state != NK_DEV_VLINK_ON) {
            /* Treat event in xirq thread. */
            fdev->event = VLCD_BEVENT_FRESET;
            up(&vlcd_drv->xirq_semaphore);
        }
        break;
    case NK_DEV_VLINK_RESET:
        if (peer_state != NK_DEV_VLINK_OFF) {
	     if (vlcd_drv->hw_ops[fdev->vlink->link]) {
                /* Hw driver is initialized. */
                /* Treat event in xirq thread. */
                fdev->event = VLCD_BEVENT_FSTARTED;
                up(&vlcd_drv->xirq_semaphore);
            }
        }
        break;
    case NK_DEV_VLINK_ON:
        if (peer_state == NK_DEV_VLINK_OFF) {
            *my_state = NK_DEV_VLINK_OFF;
            vlcd_sysconf_trigger(fdev);
        }
        break;
    }
}

void vlcd_b_handshakes(vlcd_b_driver_t* vlcd_drv, nku8_f hw_device_id)
{
    vlcd_frontend_device_t* frontend_device;

    VLCD_FOR_ALL_FRONTEND_DEVICES (frontend_device, vlcd_drv) {
	if (frontend_device->vlink->link == hw_device_id)
	    vlcd_b_handshake(vlcd_drv, frontend_device);
    }
}

static int __init vlcd_b_init(void)
{
    int ret = 0;
    int i;

    DTRACE ("enter\n");

    /* Creating device for each vlink and initialize vlcd_b_driver. */
    ret = vlcd_create(&vlcd_b_driver);
    if (ret != 0) {
        EFTRACE ("error in vlcd_create\n");
        return ret;
    }
    /* Start all handshakes. */
    for(i=0; i<VLCD_BMAX_HW_DEV_SUP;i++) {
	    vlcd_b_driver.state[i] = VLCD_BSTATE_INITIALIZING;
	    vlcd_b_handshakes(&vlcd_b_driver, i);
    }
    focus_nb.notifier_call = vlcd_notify_focus;
    focus_register_client(&focus_nb);
    return ret;
}

module_init(vlcd_b_init);

#ifdef MODULE
static void __exit vlcd_b_exit(void)
{
    vlcd_release(&vlcd_b_driver);
    focus_unregister_client(&focus_nb);
}

module_exit(vlcd_b_exit);

MODULE_LICENSE("PROPRIETARY");
#endif /* MODULE */
