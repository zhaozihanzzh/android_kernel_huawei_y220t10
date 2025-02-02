/* linux/arch/arm/mach-sc8800s/dma.c
 *
 * Copyright (C) 2010 Spreadtrum
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
#include <linux/module.h>
#include <asm/io.h>
#include <linux/interrupt.h>
#include <mach/dma.h>

struct sc88xx_irq_handler {
    void (*handler)(int dma, void *dev_id);
    void *dev_id;
};

static struct sc88xx_irq_handler sc88xx_irq_handlers[DMA_CH_NUM];

static irqreturn_t sc88xx_dma_irq(int irq, void *dev_id)
{
    u32 irq_status = __raw_readl(DMA_INT_STS);
    while (irq_status) {
        int i = 31 - __builtin_clz(irq_status);
        irq_status &= ~(1<<i);
        if (sc88xx_irq_handlers[i].handler)
            sc88xx_irq_handlers[i].handler(i, sc88xx_irq_handlers[i].dev_id);
        else printk(KERN_ERR "DMA channel %d needs handler!\n", i);
#if 1 || SC88XX_PCM_DMA_SG_CIRCLE
        dma_reg_write(DMA_TRANSF_INT_CLR, i, 1, 1);
        dma_reg_write(DMA_BURST_INT_CLR, i, 1, 1);
        dma_reg_write(DMA_LISTDONE_INT_CLR, i, 1, 1);
#endif
    }
    return IRQ_HANDLED;
}

int sc88xx_request_dma(int ch_id, void (*irq_handler)(int, void *), void *data)
{
    if (sc88xx_irq_handlers[ch_id].handler) {
        printk(KERN_WARNING "%s: dma channel %d is busy\n", __func__, ch_id);
        return -EBUSY;
    }
    sc88xx_irq_handlers[ch_id].handler = irq_handler;
    sc88xx_irq_handlers[ch_id].dev_id = data;
    return 0;
}
EXPORT_SYMBOL_GPL(sc88xx_request_dma);

void sc88xx_free_dma(int ch_id)
{
    sc88xx_irq_handlers[ch_id].handler = NULL;
}
EXPORT_SYMBOL_GPL(sc88xx_free_dma);

int sc88xx_irq_handler_ready(int ch_id)
{
    return sc88xx_irq_handlers[ch_id].handler != NULL;
}
EXPORT_SYMBOL_GPL(sc88xx_irq_handler_ready);

void sc88xx_dma_setup(sc88xx_dma_ctrl *ctrl)
{
    int ch_id = ctrl->ch_id;
    u32 ch_base = DMA_CHx_CTL_BASE + (ch_id * 0x20);
    int interrupt_type = ctrl->interrupt_type; // TRANS_DONE_EN;
    sc88xx_dma_desc *dma_desc = ctrl->dma_desc;
    dma_addr_t dma_desc_phy = ctrl->dma_desc_phy;
    u32 modes = ctrl->modes; // DMA_LINKLIST;
    u32 wrap_addr_start = 0, wrap_addr_end = 0;
    u32 softlist_size = 0, softlist_baseaddr = 0, softlist_cmd = 0;

    // DMA Channel Control
    dma_reg_write(DMA_CHx_EN, ch_id, 0, 1); // stop hard transfer for hard channel
    dma_reg_write(DMA_SOFT_REQ, ch_id, 0, 1); // stop soft transfer for soft channel

    // Clear dma mode
    dma_reg_write(DMA_LINKLIST_EN, ch_id, 0, 1); // disable linklist
    dma_reg_write(DMA_SOFTLINK_EN, ch_id, 0, 1); // disable softlink

    // Clear all interrupt type
    dma_reg_write(DMA_LISTDONE_INT_EN, ch_id, 0, 1);
    dma_reg_write(DMA_BURST_INT_EN, ch_id, 0, 1);
    dma_reg_write(DMA_TRANSF_INT_EN, ch_id, 0, 1);

    // Clear all interrupt status
    dma_reg_write(DMA_TRANSF_INT_CLR, ch_id, 1, 1);
    dma_reg_write(DMA_BURST_INT_CLR, ch_id, 1, 1);
    dma_reg_write(DMA_LISTDONE_INT_CLR, ch_id, 1, 1);

    if (interrupt_type & LLIST_DONE_EN)
        __raw_bits_or(1<<ch_id, DMA_LISTDONE_INT_EN);
    if (interrupt_type & BURST_DONE_EN)
        __raw_bits_or(1<<ch_id, DMA_BURST_INT_EN);
    if (interrupt_type & TRANS_DONE_EN)
        __raw_bits_or(1<<ch_id, DMA_TRANSF_INT_EN);

    if (modes & DMA_WRAP) {
        __raw_writel(wrap_addr_start, DMA_WRAP_START);
        __raw_writel(wrap_addr_end, DMA_WRAP_END);
    }

    // set user id
    dma_reg_write(DMA_CHN_UID_BASE + (ch_id & ~0x03), 
                  (ch_id & 0x03) << 3,
                  (ch_id > DMA_USB_EP4) ? DMA_SOFT0:ch_id,
                  0x1f);

    if (modes & DMA_LINKLIST) {
#if 0
        __raw_writel(dma_desc->cfg  , ch_base + 0x00);
        __raw_writel(dma_desc->tlen , ch_base + 0x04);
        __raw_writel(dma_desc->dsrc , ch_base + 0x08);
        __raw_writel(dma_desc->ddst , ch_base + 0x0c);
        __raw_writel(dma_desc->llptr, ch_base + 0x10);
        __raw_writel(dma_desc->pmod , ch_base + 0x14);
        __raw_writel(dma_desc->sbm  , ch_base + 0x18);
        __raw_writel(dma_desc->dbm  , ch_base + 0x1c);
#else
        __raw_writel(0, ch_base + 0x00);
        __raw_writel(0, ch_base + 0x04);
        __raw_writel(dma_desc->dsrc, ch_base + 0x08);
        __raw_writel(0, ch_base + 0x0c);
        __raw_writel(dma_desc_phy, ch_base + 0x10);
        __raw_writel(0, ch_base + 0x15);
        __raw_writel(0, ch_base + 0x18);
        __raw_writel(0, ch_base + 0x1c);
#endif
        dma_reg_write(DMA_LINKLIST_EN, ch_id, 1, 1); // enable channel ch_id linklist mode dma
        return;
    } else if (modes & DMA_SOFTLIST) {
        dma_reg_write(DMA_SOFTLINK_EN, ch_id, 1, 1); // enable softlink
        // Size of the request list. When the DMA reaches the end of the list, 
        // it starts from the beginning again if SOFT_LIST_CNT != 0. 
        // For example, if SOFTLIST_SIZE is 5, then SW can append maximum (N) 5 requests to DMA. 
        // And if SOFTLIST_EN is active, this value cannot be zero. 
        __raw_writel(softlist_size, DMA_SOFTLIST_SIZE);
        __raw_writel(softlist_baseaddr, DMA_SOFTLIST_BASEADDR);
        __raw_writel(softlist_cmd, DMA_SOFTLIST_CMD);
        return;
    }
    // DMA_NORMAL process
    __raw_writel(dma_desc->cfg  , ch_base + 0x00);
    __raw_writel(dma_desc->tlen , ch_base + 0x04);
    __raw_writel(dma_desc->dsrc , ch_base + 0x08);
    __raw_writel(dma_desc->ddst , ch_base + 0x0c);
    __raw_writel(dma_desc->llptr, ch_base + 0x10);
    __raw_writel(dma_desc->pmod , ch_base + 0x14);
    __raw_writel(dma_desc->sbm  , ch_base + 0x18);
    __raw_writel(dma_desc->dbm  , ch_base + 0x1c);
}
EXPORT_SYMBOL_GPL(sc88xx_dma_setup);

static int sprd_dma_init(void)
{
    int ret;

    dma_reg_write(AHB_CTL0, 6, 1, 1); // DMA Enable
    __raw_writel(0, DMA_CHx_EN);
    __raw_writel(0, DMA_LINKLIST_EN);
    __raw_writel(0, DMA_SOFT_REQ);
    __raw_writel(0, DMA_PRI_REG0);
    __raw_writel(0, DMA_PRI_REG1);
    __raw_writel(-1,DMA_LISTDONE_INT_CLR);
    __raw_writel(-1,DMA_TRANSF_INT_CLR);
    __raw_writel(-1,DMA_BURST_INT_CLR);
    __raw_writel(0, DMA_LISTDONE_INT_EN);
    __raw_writel(0, DMA_TRANSF_INT_EN);
    __raw_writel(0, DMA_BURST_INT_EN);

    /*set hard/soft burst wait time*/
    dma_reg_write(DMA_CFG, 0, DMA_HARD_WAITTIME, 0xff);
    dma_reg_write(DMA_CFG,16, DMA_SOFT_WAITTIME, 0xffff);

    /*register dma irq handle to host*/
    ret = request_irq(IRQ_DMA_INT, sc88xx_dma_irq, 0, "sc88xx-dma", NULL);

    /*enable dma int*/
    if (ret == 0) {
        dma_reg_write(INT_IRQ_EN, 20, 1, 1);
        printk(KERN_INFO "request dma irq ok\n");
    } else printk(KERN_ERR "request dma irq failed %d\n", ret);

    return ret;
}

arch_initcall(sprd_dma_init);

MODULE_DESCRIPTION("SC88XX DMA Module");
MODULE_AUTHOR("Luther Ge <luther.ge@spreadtrum.com>");
MODULE_LICENSE("GPL");
