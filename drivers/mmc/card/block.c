/*
 * Block driver for media (i.e., flash cards)
 *
 * Copyright 2002 Hewlett-Packard Company
 * Copyright 2005-2008 Pierre Ossman
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * HEWLETT-PACKARD COMPANY MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
 * AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
 * FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 * Many thanks to Alessandro Rubini and Jonathan Corbet!
 *
 * Author:  Andrew Christian
 *          28 May 2002
 */
#include <linux/moduleparam.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/hdreg.h>
#include <linux/kdev_t.h>
#include <linux/blkdev.h>
#include <linux/mutex.h>
#include <linux/scatterlist.h>
#include <linux/string_helpers.h>
#include <linux/genhd.h>
#include <linux/delay.h>

#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>

#include <asm/system.h>
#include <asm/uaccess.h>

#include "queue.h"

MODULE_ALIAS("mmc:block");

/*
 * max 8 partitions per card
 */
#define MMC_SHIFT	3
#define MMC_NUM_MINORS	(256 >> MMC_SHIFT)

static DECLARE_BITMAP(dev_use, MMC_NUM_MINORS);

/*
 * There is one mmc_blk_data per slot.
 */
struct mmc_blk_data {
	spinlock_t	lock;
	struct gendisk	*disk;
	struct mmc_queue queue;

	unsigned int	usage;
	unsigned int	read_only;
};

static DEFINE_MUTEX(open_lock);

static struct mmc_blk_data *mmc_blk_get(struct gendisk *disk)
{
	struct mmc_blk_data *md;

	mutex_lock(&open_lock);
	md = disk->private_data;
	if (md && md->usage == 0)
		md = NULL;
	if (md)
		md->usage++;
	mutex_unlock(&open_lock);

	return md;
}

static void mmc_blk_put(struct mmc_blk_data *md)
{
	mutex_lock(&open_lock);
	md->usage--;
	if (md->usage == 0) {
		int devidx = md->disk->first_minor >> MMC_SHIFT;

		blk_cleanup_queue(md->queue.queue);

		__clear_bit(devidx, dev_use);

		put_disk(md->disk);
		kfree(md);
	}
	mutex_unlock(&open_lock);
}

static int mmc_blk_open(struct block_device *bdev, fmode_t mode)
{
	struct mmc_blk_data *md = mmc_blk_get(bdev->bd_disk);
	int ret = -ENXIO;

	if (md) {
		if (md->usage == 2)
			check_disk_change(bdev);
		ret = 0;

		if ((mode & FMODE_WRITE) && md->read_only) {
			mmc_blk_put(md);
			ret = -EROFS;
		}
	}

	return ret;
}

static int mmc_blk_release(struct gendisk *disk, fmode_t mode)
{
	struct mmc_blk_data *md = disk->private_data;

	mmc_blk_put(md);
	return 0;
}

static int
mmc_blk_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	geo->cylinders = get_capacity(bdev->bd_disk) / (4 * 16);
	geo->heads = 4;
	geo->sectors = 16;
	return 0;
}

static const struct block_device_operations mmc_bdops = {
	.open			= mmc_blk_open,
	.release		= mmc_blk_release,
	.getgeo			= mmc_blk_getgeo,
	.owner			= THIS_MODULE,
};

struct mmc_blk_request {
	struct mmc_request	mrq;
	struct mmc_command	cmd;
	struct mmc_command	stop;
	struct mmc_data		data;
};

static u32 mmc_sd_num_wr_blocks(struct mmc_card *card)
{
	int err;
	u32 result;
	__be32 *blocks;

	struct mmc_request mrq;
	struct mmc_command cmd;
	struct mmc_data data;
	unsigned int timeout_us;

	struct scatterlist sg;

	memset(&cmd, 0, sizeof(struct mmc_command));

	cmd.opcode = MMC_APP_CMD;
	cmd.arg = card->rca << 16;
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_AC;

	err = mmc_wait_for_cmd(card->host, &cmd, 0);
	if (err)
		return (u32)-1;
	if (!mmc_host_is_spi(card->host) && !(cmd.resp[0] & R1_APP_CMD))
		return (u32)-1;

	memset(&cmd, 0, sizeof(struct mmc_command));

	cmd.opcode = SD_APP_SEND_NUM_WR_BLKS;
	cmd.arg = 0;
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;

	memset(&data, 0, sizeof(struct mmc_data));

	data.timeout_ns = card->csd.tacc_ns * 100;
	data.timeout_clks = card->csd.tacc_clks * 100;

	timeout_us = data.timeout_ns / 1000;
	timeout_us += data.timeout_clks * 1000 /
		(card->host->ios.clock / 1000);

	if (timeout_us > 100000) {
		data.timeout_ns = 100000000;
		data.timeout_clks = 0;
	}

	data.blksz = 4;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;
	data.sg = &sg;
	data.sg_len = 1;

	memset(&mrq, 0, sizeof(struct mmc_request));

	mrq.cmd = &cmd;
	mrq.data = &data;

	blocks = kmalloc(4, GFP_KERNEL);
	if (!blocks)
		return (u32)-1;

	sg_init_one(&sg, blocks, 4);

	mmc_wait_for_req(card->host, &mrq);

	result = ntohl(*blocks);
	kfree(blocks);

	if (cmd.error || data.error)
		result = (u32)-1;

	return result;
}

static u32 get_card_status(struct mmc_card *card, struct request *req)
{
	struct mmc_command cmd;
	int err;

	memset(&cmd, 0, sizeof(struct mmc_command));
	cmd.opcode = MMC_SEND_STATUS;
	if (!mmc_host_is_spi(card->host))
		cmd.arg = card->rca << 16;
	cmd.flags = MMC_RSP_SPI_R2 | MMC_RSP_R1 | MMC_CMD_AC;
	err = mmc_wait_for_cmd(card->host, &cmd, 0);
	if (err)
		printk(KERN_ERR "%s: error %d sending status comand",
		       req->rq_disk->disk_name, err);
	return cmd.resp[0];
}

static int
mmc_blk_set_blksize(struct mmc_blk_data *md, struct mmc_card *card)
{
	struct mmc_command cmd;
	int err;

	/* Block-addressed cards ignore MMC_SET_BLOCKLEN. */
	if (mmc_card_blockaddr(card))
		return 0;

	mmc_claim_host(card->host);
	cmd.opcode = MMC_SET_BLOCKLEN;
	cmd.arg = 512;
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_AC;
	err = mmc_wait_for_cmd(card->host, &cmd, 5);
	mmc_release_host(card->host);

	if (err) {
	    dev_t devt;
		struct gendisk *disk = md->disk;
		int retval = blk_alloc_devt(&disk->part0, &devt);
		printk(KERN_ERR "%s: unable to set block size to %d: %d\n",
			md->disk->disk_name, cmd.arg, err);
		if (retval) {
			WARN_ON(1);
			return retval;
		}
		disk_to_dev(disk)->devt = devt;
		return -EINVAL;
	}

	return 0;
}
static void remove_card(struct mmc_host *host)
{
	printk(KERN_INFO "%s: remove card\n",
		mmc_hostname(host));
	if (!host->card || host->card->removed) {
		printk(KERN_INFO "%s: card already removed\n",
			mmc_hostname(host));
		return;
	}
	if (!mmc_card_present(host->card)) {
		printk(KERN_INFO "%s: card is not present\n",
			mmc_hostname(host));
		return;
	}
	host->card->removed = 1;
	mmc_schedule_card_removal_work(&host->remove, 0);
}
static int mmc_blk_issue_rq(struct mmc_queue *mq, struct request *req)
{
	struct mmc_blk_data *md = mq->data;
	struct mmc_card *card = md->queue.card;
	struct mmc_blk_request brq;
	int ret = 1, disable_multi = 0, card_no_ready = 0;
	int err = 0;
	int try_recovery = 1, do_reinit = 0, do_remove = 0;

#ifdef CONFIG_MMC_BLOCK_DEFERRED_RESUME
        int retries = 3;
	if (mmc_bus_needs_resume(card->host)) {
		printk("mmc resume deferred in mmc_blk_issue_rq\n");
		err = mmc_resume_bus(card->host);
		if (err) {
			printk("wow, mmc deferred resume failed\n");
			if (mmc_card_sd(card))
				remove_card(card->host);
			spin_lock_irq(&md->lock);
			__blk_end_request_all(req, -EIO);
			spin_unlock_irq(&md->lock);
			return 0;
		}
		mmc_blk_set_blksize(md, card);
	        if (mmc_card_mmc(card)) {
		        pr_debug("mmc card\n");
			struct mmc_command cmd;
			unsigned long delay = jiffies + HZ;
			do {
				int err;
				cmd.opcode = MMC_SEND_STATUS;
				cmd.arg = mq->card->rca << 16;
				cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;
				mmc_claim_host(mq->card->host);
				err = mmc_wait_for_cmd(mq->card->host, &cmd, 5);
				mmc_release_host(mq->card->host);
				if (err) {
				    printk(KERN_ERR "failed to get status(%d)!!\n"
						, err);
					msleep(5);
					retries--;
					if(retries)
					   continue;
				}
                                printk("check mmc-card status\n");//dbg
				if (time_after(jiffies, delay)) {
					card_no_ready++;
					printk(KERN_ERR
						"failed to get card ready %d\n",
						card_no_ready);
					break;
				}
			} while (retries &&
				(!(cmd.resp[0] & R1_READY_FOR_DATA) ||
				(R1_CURRENT_STATE(cmd.resp[0]) == 7)));
		}
	}
	if (mmc_bus_fails_resume(card->host) || card_no_ready ||
		!retries) {
		spin_lock_irq(&md->lock);
		__blk_end_request_all(req, -EIO);
		spin_unlock_irq(&md->lock);
		return 0;
	}
#endif

	mmc_claim_host(card->host);

	do {
		struct mmc_command cmd;
		u32 readcmd, writecmd, status = 0;

		memset(&brq, 0, sizeof(struct mmc_blk_request));
		brq.mrq.cmd = &brq.cmd;
		brq.mrq.data = &brq.data;

		brq.cmd.arg = blk_rq_pos(req);
		if (!mmc_card_blockaddr(card))
			brq.cmd.arg <<= 9;
		brq.cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;
		brq.data.blksz = 512;
		brq.stop.opcode = MMC_STOP_TRANSMISSION;
		brq.stop.arg = 0;
		brq.stop.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;
		brq.data.blocks = blk_rq_sectors(req);

		/*
		 * The block layer doesn't support all sector count
		 * restrictions, so we need to be prepared for too big
		 * requests.
		 */
		if (brq.data.blocks > card->host->max_blk_count)
			brq.data.blocks = card->host->max_blk_count;

		/*
		 * After a read error, we redo the request one sector at a time
		 * in order to accurately determine which sectors can be read
		 * successfully.
		 */
		if (disable_multi && brq.data.blocks > 1)
			brq.data.blocks = 1;

		if (brq.data.blocks > 1) {
			/* SPI multiblock writes terminate using a special
			 * token, not a STOP_TRANSMISSION request.
			 */
			if (!mmc_host_is_spi(card->host)
					|| rq_data_dir(req) == READ)
				brq.mrq.stop = &brq.stop;
			readcmd = MMC_READ_MULTIPLE_BLOCK;
			writecmd = MMC_WRITE_MULTIPLE_BLOCK;
		} else {
			brq.mrq.stop = NULL;
			readcmd = MMC_READ_SINGLE_BLOCK;
			writecmd = MMC_WRITE_BLOCK;
		}

		if (rq_data_dir(req) == READ) {
			brq.cmd.opcode = readcmd;
			brq.data.flags |= MMC_DATA_READ;
		} else {
			brq.cmd.opcode = writecmd;
			brq.data.flags |= MMC_DATA_WRITE;
		}

		mmc_set_data_timeout(&brq.data, card);

		brq.data.sg = mq->sg;
		brq.data.sg_len = mmc_queue_map_sg(mq);

		/*
		 * Adjust the sg list so it is the same size as the
		 * request.
		 */
		if (brq.data.blocks != blk_rq_sectors(req)) {
			int i, data_size = brq.data.blocks << 9;
			struct scatterlist *sg;

			for_each_sg(brq.data.sg, sg, brq.data.sg_len, i) {
				data_size -= sg->length;
				if (data_size <= 0) {
					sg->length += data_size;
					i++;
					break;
				}
			}
			brq.data.sg_len = i;
		}

		mmc_queue_bounce_pre(mq);

		mmc_wait_for_req(card->host, &brq.mrq);

		mmc_queue_bounce_post(mq);

		/*
		 * Check for errors here, but don't jump to cmd_err
		 * until later as we need to wait for the card to leave
		 * programming mode even when things go wrong.
		 */
		if (brq.cmd.error || brq.data.error || brq.stop.error) {
			if (brq.data.blocks > 1 && rq_data_dir(req) == READ) {
				/* Redo read one sector at a time */
				printk(KERN_WARNING "%s: retrying using single "
				       "block read\n", req->rq_disk->disk_name);
				disable_multi = 1;
				continue;
			}
			status = get_card_status(card, req);
		} else if (disable_multi == 1) {
			disable_multi = 0;
		}

		if (brq.cmd.error) {
			printk(KERN_ERR "%s: error %d sending command"
			       ":%d, response %#x, card status %#x\n",
			       req->rq_disk->disk_name, brq.cmd.error,
			       brq.cmd.opcode, brq.cmd.resp[0], status);
		}

		if (brq.data.error) {
			if (brq.data.error == -ETIMEDOUT && brq.mrq.stop)
				/* 'Stop' response contains card status */
				status = brq.mrq.stop->resp[0];
			printk(KERN_ERR "%s: error %d transferring data,"
			       " sector %u, nr %u, card status %#x\n",
			       req->rq_disk->disk_name, brq.data.error,
			       (unsigned)blk_rq_pos(req),
			       (unsigned)blk_rq_sectors(req), status);
		}

		if (brq.stop.error) {
			printk(KERN_ERR "%s: error %d sending stop command, "
			       "response %#x, card status %#x\n",
			       req->rq_disk->disk_name, brq.stop.error,
			       brq.stop.resp[0], status);
		}

		if (!mmc_host_is_spi(card->host) && rq_data_dir(req) != READ) {
                        int i = 0;
			int sleepy = mmc_card_sd(card) ? 1 : 0;
			unsigned int msec = 0;
			unsigned long delay = jiffies + HZ;
			err = 0;
			do {
                          if (sleepy && (fls(++i) > 11)) {
			    msec = (unsigned int)fls(i >> 11);
			    msleep(msec);

			    if (msec > 3 && ((i - 1) & i) == 0) {
				printk(KERN_INFO "%s: start "
					"sleep %u msecs\n",
					req->rq_disk->disk_name,
					msec);
				}
			   }

				cmd.opcode = MMC_SEND_STATUS;
				cmd.arg = card->rca << 16;
				cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;
				err = mmc_wait_for_cmd(card->host, &cmd, 5);
				if (err) {
					printk(KERN_ERR "%s: error %d requesting status\n",
					       req->rq_disk->disk_name, err);
			     if(card->removed){
				printk("error request card status, goto cmd_err\n");
					goto cmd_err;
				}
			   }
			   if (time_after(jiffies, delay)) {
				if (try_recovery == 1)
					do_reinit = 1;
				else if (mmc_card_sd(card) && (try_recovery == 2))
					do_remove = 1;
				try_recovery++;
				err = 1;
				card_no_ready++;
				printk(KERN_ERR "%s: failed to get card ready\n",
						mmc_hostname(card->host));
				break;
		           }  
				/*
				 * Some cards mishandle the status bits,
				 * so make sure to check both the busy
				 * indication and the card state.
				 */
		            //dbg 		
			    if(cmd.resp[0] != 0x900)
                               printk("check cards status: 0x%x\n", cmd.resp[0]);//dbg
			} while (!(cmd.resp[0] & R1_READY_FOR_DATA) ||
				(R1_CURRENT_STATE(cmd.resp[0]) == 7));

#if 0
			if (cmd.resp[0] & ~0x00000900)
				printk(KERN_ERR "%s: status = %08x\n",
				       req->rq_disk->disk_name, cmd.resp[0]);
			if (mmc_decode_status(cmd.resp))
				goto cmd_err;
#endif
                        if (!err)
				card_no_ready = 0;
		}
recovery:
		if (do_reinit) {
			do_reinit = 0;
			if (card->removed){
			        printk("card was removed, goto cmd_err\n");
				goto cmd_err;
			}
			printk(KERN_INFO "%s: reinit card\n",
				          mmc_hostname(card->host));
			card->host->bus_resume_flags |= MMC_BUSRESUME_NEEDS_RESUME;
			mmc_power_off(card->host);//power off sdio before re-init
			err = mmc_resume_bus(card->host);
			if (!err) {
				mmc_blk_set_blksize(md, card);
				printk("mmc: recovery card ok\n");
				continue;
			} else {
				if (mmc_card_sd(card)) {
					printk("mmc: reinit failed, remove card\n");
					remove_card(card->host);
				}
				printk("mmc: reinit failed, goto cmd_err\n");
				goto cmd_err;
			}
		} else if (do_remove) {
			do_remove = 0;
			remove_card(card->host);
			goto cmd_err;
		}

    	        if (brq.cmd.error || brq.stop.error ||
			brq.data.error || card_no_ready) {
			if (try_recovery == 1)
				do_reinit = 1;
			else if (mmc_card_sd(card) && (try_recovery == 2))
				do_remove = 1;
			try_recovery++;
			if (do_reinit || do_remove)
				goto recovery;
			if (rq_data_dir(req) == READ) {
				/*
				 * After an error, we redo I/O one sector at a
				 * time, so we only reach here after trying to
				 * read a single sector.
				 */
				spin_lock_irq(&md->lock);
				ret = __blk_end_request(req, -EIO, brq.data.blksz);
				spin_unlock_irq(&md->lock);
				continue;
			}
			goto cmd_err;
		}

		/*
		 * A block was successfully transferred.
		 */
		spin_lock_irq(&md->lock);
		ret = __blk_end_request(req, 0, brq.data.bytes_xfered);
		spin_unlock_irq(&md->lock);
	} while (ret);

	mmc_release_host(card->host);

	return 1;

 cmd_err:
 	/*
 	 * If this is an SD card and we're writing, we can first
 	 * mark the known good sectors as ok.
 	 *
	 * If the card is not SD, we can still ok written sectors
	 * as reported by the controller (which might be less than
	 * the real number of written sectors, but never more).
	 */
	if (mmc_card_sd(card)) {
		u32 blocks;

		blocks = mmc_sd_num_wr_blocks(card);
		if (blocks != (u32)-1) {
			spin_lock_irq(&md->lock);
			ret = __blk_end_request(req, 0, blocks << 9);
			spin_unlock_irq(&md->lock);
		}
	} else {
		spin_lock_irq(&md->lock);
		ret = __blk_end_request(req, 0, brq.data.bytes_xfered);
		spin_unlock_irq(&md->lock);
	}

	mmc_release_host(card->host);

	spin_lock_irq(&md->lock);
	while (ret)
		ret = __blk_end_request(req, -EIO, blk_rq_cur_bytes(req));
	spin_unlock_irq(&md->lock);

	return 0;
}


static inline int mmc_blk_readonly(struct mmc_card *card)
{
	return mmc_card_readonly(card) ||
	       !(card->csd.cmdclass & CCC_BLOCK_WRITE);
}

static struct mmc_blk_data *mmc_blk_alloc(struct mmc_card *card)
{
	struct mmc_blk_data *md;
	int devidx, ret;

	devidx = find_first_zero_bit(dev_use, MMC_NUM_MINORS);
	if (devidx >= MMC_NUM_MINORS)
		return ERR_PTR(-ENOSPC);
	__set_bit(devidx, dev_use);

	md = kzalloc(sizeof(struct mmc_blk_data), GFP_KERNEL);
	if (!md) {
		ret = -ENOMEM;
		goto out;
	}


	/*
	 * Set the read-only status based on the supported commands
	 * and the write protect switch.
	 */
	md->read_only = mmc_blk_readonly(card);

	md->disk = alloc_disk(1 << MMC_SHIFT);
	if (md->disk == NULL) {
		ret = -ENOMEM;
		goto err_kfree;
	}

	spin_lock_init(&md->lock);
	md->usage = 1;

	ret = mmc_init_queue(&md->queue, card, &md->lock);
	if (ret)
		goto err_putdisk;

	md->queue.issue_fn = mmc_blk_issue_rq;
	md->queue.data = md;

	md->disk->major	= MMC_BLOCK_MAJOR;
	md->disk->first_minor = devidx << MMC_SHIFT;
	md->disk->fops = &mmc_bdops;
	md->disk->private_data = md;
	md->disk->queue = md->queue.queue;
	md->disk->driverfs_dev = &card->dev;
	md->disk->flags = GENHD_FL_EXT_DEVT;

	/*
	 * As discussed on lkml, GENHD_FL_REMOVABLE should:
	 *
	 * - be set for removable media with permanent block devices
	 * - be unset for removable block devices with permanent media
	 *
	 * Since MMC block devices clearly fall under the second
	 * case, we do not set GENHD_FL_REMOVABLE.  Userspace
	 * should use the block device creation/destruction hotplug
	 * messages to tell when the card is present.
	 */

	sprintf(md->disk->disk_name, "mmcblk%d", devidx);

	blk_queue_logical_block_size(md->queue.queue, 512);

	if (!mmc_card_sd(card) && mmc_card_blockaddr(card)) {
		/*
		 * The EXT_CSD sector count is in number or 512 byte
		 * sectors.
		 */
		set_capacity(md->disk, card->ext_csd.sectors);
	} else {
		/*
		 * The CSD capacity field is in units of read_blkbits.
		 * set_capacity takes units of 512 bytes.
		 */
		set_capacity(md->disk,
			card->csd.capacity << (card->csd.read_blkbits - 9));
	}
	return md;

 err_putdisk:
	put_disk(md->disk);
 err_kfree:
	kfree(md);
 out:
	return ERR_PTR(ret);
}

static int mmc_blk_probe(struct mmc_card *card)
{
	struct mmc_blk_data *md;
	int err;

	char cap_str[10];

	/*
	 * Check that the card supports the command class(es) we need.
	 */
	if (!(card->csd.cmdclass & CCC_BLOCK_READ))
		return -ENODEV;

	md = mmc_blk_alloc(card);
	if (IS_ERR(md))
		return PTR_ERR(md);

	err = mmc_blk_set_blksize(md, card);
	if (err)
		goto out;

	string_get_size((u64)get_capacity(md->disk) << 9, STRING_UNITS_2,
			cap_str, sizeof(cap_str));
	printk(KERN_INFO "%s: %s %s %s %s\n",
		md->disk->disk_name, mmc_card_id(card), mmc_card_name(card),
		cap_str, md->read_only ? "(ro)" : "");

	mmc_set_drvdata(card, md);
        mmc_init_bus_resume_flags(card->host);
#ifdef CONFIG_MMC_BLOCK_DEFERRED_RESUME
        if (mmc_card_sd(card) || mmc_card_mmc(card))
	mmc_set_bus_resume_policy(card->host, 1);
#endif
	add_disk(md->disk);
	return 0;

 out:
	mmc_cleanup_queue(&md->queue);
	mmc_blk_put(md);

	return err;
}

/*
 * Duplicate from fs/partitions/check.c del_gendisk(), but disable
 * fsync_bdev().
 */
void del_gendisk_async(struct gendisk *disk)
{
	struct disk_part_iter piter;
	struct hd_struct *part;
    struct super_block *sb;
	struct block_device *bdev;
	disk_part_iter_init(&piter, disk,
    			     DISK_PITER_INCL_EMPTY | DISK_PITER_REVERSE);
	while ((part = disk_part_iter_next(&piter))) {
		bdev = bdget_disk(disk, part->partno);
		if (bdev) {
			__invalidate_device(bdev);
			bdput(bdev);
		}
		delete_partition(disk, part->partno);
	}
	disk_part_iter_exit(&piter);
        printk("piter.idx:%d\n", piter.idx);
	if(piter.idx){
	   invalidate_partition(disk, 0);
        }else{
	   printk("sd card with no partition\n");
	   bdev = bdget_disk(disk, 0);
	   if(bdev){
	     __invalidate_device(bdev);
             bdput(bdev); 
	   }
	}
	blk_free_devt(disk_to_dev(disk)->devt);
	set_capacity(disk, 0);
	disk->flags &= ~GENHD_FL_UP;
	unlink_gendisk(disk);
	part_stat_set_all(&disk->part0, 0);
	disk->part0.stamp = 0;
	kobject_put(disk->part0.holder_dir);
	kobject_put(disk->slave_dir);
	disk->driverfs_dev = NULL;
#ifndef CONFIG_SYSFS_DEPRECATED
	sysfs_remove_link(block_depr, dev_name(disk_to_dev(disk)));
#endif
	device_del(disk_to_dev(disk));
}
static void mmc_blk_remove(struct mmc_card *card)
{
	struct mmc_blk_data *md = mmc_get_drvdata(card);

	if (md) {
		/* Stop new requests from getting into the queue */
		if (mmc_card_sd(card))
			del_gendisk_async(md->disk);
		else
		del_gendisk(md->disk);

		/* Then flush out any already in there */
		mmc_cleanup_queue(&md->queue);

		mmc_blk_put(md);
	}
	mmc_set_drvdata(card, NULL);
#ifdef CONFIG_MMC_BLOCK_DEFERRED_RESUME
	mmc_set_bus_resume_policy(card->host, 0);
#endif
}

#ifdef CONFIG_PM
static int mmc_blk_suspend(struct mmc_card *card, pm_message_t state)
{
    printk("%s, entry\n", __func__);
	int error = 0;
	struct mmc_blk_data *md = mmc_get_drvdata(card);

	if (md) {
		//mmc_queue_suspend(&md->queue);//ok case
		error = mmc_queue_suspend(&md->queue);
	}
        printk("mmc_blk_suspend, done, return:%d\n", error);
	return error;
}

static int mmc_blk_resume(struct mmc_card *card)
{
	printk("%s, start\n", __func__);
	struct mmc_blk_data *md = mmc_get_drvdata(card);

	if (md) {
	    if (!mmc_bus_manual_resume(card->host)) {
		printk("%s, set_blksize\n", __func__);
		mmc_blk_set_blksize(md, card);
#ifdef CONFIG_MMC_BLOCK_PARANOID_RESUME
			md->queue.check_status = 1;
#endif
            }
		mmc_queue_resume(&md->queue);
	}
	printk("%s, done\n", __func__);
	return 0;
}
#else
#define	mmc_blk_suspend	NULL
#define mmc_blk_resume	NULL
#endif

static struct mmc_driver mmc_driver = {
	.drv		= {
		.name	= "mmcblk",
	},
	.probe		= mmc_blk_probe,
	.remove		= mmc_blk_remove,
	.suspend	= mmc_blk_suspend,
	.resume		= mmc_blk_resume,
};

static int __init mmc_blk_init(void)
{
	int res;

	res = register_blkdev(MMC_BLOCK_MAJOR, "mmc");
	if (res)
		goto out;

	res = mmc_register_driver(&mmc_driver);
	if (res)
		goto out2;

	return 0;
 out2:
	unregister_blkdev(MMC_BLOCK_MAJOR, "mmc");
 out:
	return res;
}

static void __exit mmc_blk_exit(void)
{
	mmc_unregister_driver(&mmc_driver);
	unregister_blkdev(MMC_BLOCK_MAJOR, "mmc");
}

module_init(mmc_blk_init);
module_exit(mmc_blk_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Multimedia Card (MMC) block device driver");

