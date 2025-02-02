/*
 * arch/arm/mm/cache-l2x0.c - L210/L220 cache controller support
 *
 * Copyright (C) 2007 ARM Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/io.h>

#include <asm/cacheflush.h>

#include <mach/cache-l3x0.h>
#include <mach/hardware.h>
#define writel_relaxed __raw_writel

#define CACHE_LINE_SIZE		32
#ifdef CONFIG_CACHE_L2X0_310
#define CONFIG_CACHE_PL310
#define CONFIG_ARM_ERRATA_753970
#define CONFIG_PL310_ERRATA_588369
#define CONFIG_PL310_ERRATA_727915
#endif

#ifdef CONFIG_NKERNEL
#define hw_spin_lock_irqsave(LOCK, FLAGS) do {hw_local_irq_save(FLAGS); spin_lock(LOCK);} while(0) 
#define hw_spin_unlock_irqrestore(LOCK, FLAGS) do {spin_unlock(LOCK); hw_local_irq_restore(FLAGS);} while(0)
#endif

static void __iomem *l2x0_base;
static DEFINE_SPINLOCK(l2x0_lock);
static uint32_t l2x0_way_mask;	/* Bitmask of active ways */
static uint32_t l2x0_size;

static inline void cache_wait_way(void __iomem *reg, unsigned long mask)
{
	/* wait for cache operation by line or way to complete */
	while (readl_relaxed(reg) & mask)
		;
}

#ifdef CONFIG_CACHE_PL310
static inline void cache_wait(void __iomem *reg, unsigned long mask)
{
	/* cache operations by line are atomic on PL310 */
}
#else
#define cache_wait	cache_wait_way
#endif

static inline void cache_sync(void)
{
	void __iomem *base = l2x0_base;

#ifdef CONFIG_ARM_ERRATA_753970
	/* write to an unmmapped register */
	writel_relaxed(0, base + L2X0_DUMMY_REG);
#else
	writel_relaxed(0, base + L2X0_CACHE_SYNC);
#endif
	cache_wait(base + L2X0_CACHE_SYNC, 1);
}

static inline void l2x0_clean_line(unsigned long addr)
{
	void __iomem *base = l2x0_base;
	cache_wait(base + L2X0_CLEAN_LINE_PA, 1);
	writel_relaxed(addr, base + L2X0_CLEAN_LINE_PA);
}

static inline void l2x0_inv_line(unsigned long addr)
{
	void __iomem *base = l2x0_base;
	cache_wait(base + L2X0_INV_LINE_PA, 1);
	writel_relaxed(addr, base + L2X0_INV_LINE_PA);
}

#if defined(CONFIG_PL310_ERRATA_588369) || defined(CONFIG_PL310_ERRATA_727915)

#define debug_writel(val)	l2x0_set_debug(val)

static void l2x0_set_debug(unsigned long val)
{
	writel_relaxed(val, l2x0_base + L2X0_DEBUG_CTRL);
}
#else
/* Optimised out for non-errata case */
static inline void debug_writel(unsigned long val)
{
}

#define l2x0_set_debug	NULL
#endif

#ifdef CONFIG_PL310_ERRATA_588369
static inline void l2x0_flush_line(unsigned long addr)
{
	void __iomem *base = l2x0_base;

	/* Clean by PA followed by Invalidate by PA */
	cache_wait(base + L2X0_CLEAN_LINE_PA, 1);
	writel_relaxed(addr, base + L2X0_CLEAN_LINE_PA);
	cache_wait(base + L2X0_INV_LINE_PA, 1);
	writel_relaxed(addr, base + L2X0_INV_LINE_PA);
}
#else

static inline void l2x0_flush_line(unsigned long addr)
{
	void __iomem *base = l2x0_base;
	cache_wait(base + L2X0_CLEAN_INV_LINE_PA, 1);
	writel_relaxed(addr, base + L2X0_CLEAN_INV_LINE_PA);
}
#endif


static void l2x0_cache_sync(void)
{
	unsigned long flags;
#ifdef CONFIG_NKERNEL
	hw_spin_lock_irqsave(&l2x0_lock, flags);
#else
	spin_lock_irqsave(&l2x0_lock, flags);
#endif
	cache_sync();
#ifdef CONFIG_NKERNEL
	hw_spin_unlock_irqrestore(&l2x0_lock, flags);
#else
	spin_unlock_irqrestore(&l2x0_lock, flags);
#endif
}

void l2x0_flush_all(void)
{
	unsigned long flags;

#ifdef CONFIG_NKERNEL
	hw_spin_lock_irqsave(&l2x0_lock, flags);
#else
	spin_lock_irqsave(&l2x0_lock, flags);
#endif	/* clean all ways */
	debug_writel(0x03);
	writel_relaxed(l2x0_way_mask, l2x0_base + L2X0_CLEAN_INV_WAY);
	cache_wait_way(l2x0_base + L2X0_CLEAN_INV_WAY, l2x0_way_mask);
	cache_sync();
	debug_writel(0x00);
#ifdef CONFIG_NKERNEL
	hw_spin_unlock_irqrestore(&l2x0_lock, flags);
#else
	spin_unlock_irqrestore(&l2x0_lock, flags);
#endif
}

static void l2x0_clean_all(void)
{
	unsigned long flags;
#ifdef CONFIG_NKERNEL
	hw_spin_lock_irqsave(&l2x0_lock, flags);
#else
	spin_lock_irqsave(&l2x0_lock, flags);
#endif
	/* clean all ways */
	writel_relaxed(l2x0_way_mask, l2x0_base + L2X0_CLEAN_WAY);
	cache_wait_way(l2x0_base + L2X0_CLEAN_WAY, l2x0_way_mask);
	cache_sync();
#ifdef CONFIG_NKERNEL
	hw_spin_unlock_irqrestore(&l2x0_lock, flags);
#else
	spin_unlock_irqrestore(&l2x0_lock, flags);
#endif
}

static void l2x0_inv_all(void)
{
	unsigned long flags;
	/* invalidate all ways */
#ifdef CONFIG_NKERNEL
	hw_spin_lock_irqsave(&l2x0_lock, flags);
#else
	spin_lock_irqsave(&l2x0_lock, flags);
#endif
	/* Invalidating when L2 is enabled is a nono */
	BUG_ON(readl(l2x0_base + L2X0_CTRL) & 1);
	writel_relaxed(l2x0_way_mask, l2x0_base + L2X0_INV_WAY);
	cache_wait_way(l2x0_base + L2X0_INV_WAY, l2x0_way_mask);
	cache_sync();
#ifdef CONFIG_NKERNEL
	hw_spin_unlock_irqrestore(&l2x0_lock, flags);
#else
	spin_unlock_irqrestore(&l2x0_lock, flags);
#endif
}

static void l2x0_inv_range(unsigned long start, unsigned long end)
{
	void __iomem *base = l2x0_base;
	unsigned long flags;
#ifdef CONFIG_NKERNEL
	hw_spin_lock_irqsave(&l2x0_lock, flags);
#else
	spin_lock_irqsave(&l2x0_lock, flags);
#endif
	if (start & (CACHE_LINE_SIZE - 1)) {
		start &= ~(CACHE_LINE_SIZE - 1);
		debug_writel(0x03);
		l2x0_flush_line(start);
		debug_writel(0x00);
		start += CACHE_LINE_SIZE;
	}

	if (end & (CACHE_LINE_SIZE - 1)) {
		end &= ~(CACHE_LINE_SIZE - 1);
		debug_writel(0x03);
		l2x0_flush_line(end);
		debug_writel(0x00);
	}

	while (start < end) {
		unsigned long blk_end = start + min(end - start, 4096UL);

		while (start < blk_end) {
			l2x0_inv_line(start);
			start += CACHE_LINE_SIZE;
		}

		if (blk_end < end) {
#ifdef CONFIG_NKERNEL
			hw_spin_unlock_irqrestore(&l2x0_lock, flags);
			hw_spin_lock_irqsave(&l2x0_lock, flags);
#else
			spin_unlock_irqrestore(&l2x0_lock, flags);
			spin_lock_irqsave(&l2x0_lock, flags);
#endif
		}
	}
	cache_wait(base + L2X0_INV_LINE_PA, 1);
	cache_sync();
#ifdef CONFIG_NKERNEL
	hw_spin_unlock_irqrestore(&l2x0_lock, flags);
#else
	spin_unlock_irqrestore(&l2x0_lock, flags);
#endif
}

static void l2x0_clean_range(unsigned long start, unsigned long end)
{
	void __iomem *base = l2x0_base;
	unsigned long flags;

	if ((end - start) >= l2x0_size) {
		l2x0_clean_all();
		return;
	}
#ifdef CONFIG_NKERNEL
	hw_spin_lock_irqsave(&l2x0_lock, flags);
#else
	spin_lock_irqsave(&l2x0_lock, flags);
#endif

	start &= ~(CACHE_LINE_SIZE - 1);
	while (start < end) {
		unsigned long blk_end = start + min(end - start, 4096UL);

		while (start < blk_end) {
			l2x0_clean_line(start);
			start += CACHE_LINE_SIZE;
		}

		if (blk_end < end) {
#ifdef CONFIG_NKERNEL
			hw_spin_unlock_irqrestore(&l2x0_lock, flags);
			hw_spin_lock_irqsave(&l2x0_lock, flags);
#else
			spin_unlock_irqrestore(&l2x0_lock, flags);
			spin_lock_irqsave(&l2x0_lock, flags);
#endif

		}
	}
	cache_wait(base + L2X0_CLEAN_LINE_PA, 1);
	cache_sync();
#ifdef CONFIG_NKERNEL
	hw_spin_unlock_irqrestore(&l2x0_lock, flags);
#else
	spin_unlock_irqrestore(&l2x0_lock, flags);
#endif
}

static void l2x0_flush_range(unsigned long start, unsigned long end)
{
	void __iomem *base = l2x0_base;
	unsigned long flags;

	if ((end - start) >= l2x0_size) {
		l2x0_flush_all();
		return;
	}
#ifdef CONFIG_NKERNEL
	hw_spin_lock_irqsave(&l2x0_lock, flags);
#else
	spin_lock_irqsave(&l2x0_lock, flags);
#endif

	start &= ~(CACHE_LINE_SIZE - 1);
	while (start < end) {
		unsigned long blk_end = start + min(end - start, 4096UL);

		debug_writel(0x03);
		while (start < blk_end) {
			l2x0_flush_line(start);
			start += CACHE_LINE_SIZE;
		}
		debug_writel(0x00);

		if (blk_end < end) {
#ifdef CONFIG_NKERNEL
			hw_spin_unlock_irqrestore(&l2x0_lock, flags);
			hw_spin_lock_irqsave(&l2x0_lock, flags);
#else
			spin_unlock_irqrestore(&l2x0_lock, flags);
			spin_lock_irqsave(&l2x0_lock, flags);
#endif
		}
	}
	cache_wait(base + L2X0_CLEAN_INV_LINE_PA, 1);
	cache_sync();
#ifdef CONFIG_NKERNEL
	hw_spin_unlock_irqrestore(&l2x0_lock, flags);
#else
	spin_unlock_irqrestore(&l2x0_lock, flags);
#endif
}

static void l2x0_disable(void)
{
	unsigned long flags;
#ifdef CONFIG_NKERNEL
	hw_spin_lock_irqsave(&l2x0_lock, flags);
#else
	spin_lock_irqsave(&l2x0_lock, flags);
#endif

	writel(0, l2x0_base + L2X0_CTRL);
#ifdef CONFIG_NKERNEL
	hw_spin_unlock_irqrestore(&l2x0_lock, flags);
#else
	spin_unlock_irqrestore(&l2x0_lock, flags);
#endif
}

static void l2x0_tag_latency(unsigned long value)
{
	writel(value, l2x0_base + L2X0_TAG_LATENCY_CTRL);
}
void l2x0_data_latency(unsigned long value)
{
	writel(value, l2x0_base + L2X0_DATA_LATENCY_CTRL);
}
void l2x0_intr_clr(unsigned long value)
{
	writel(value, l2x0_base + L2X0_INTR_CLEAR);
}
void l2x0_intr_mask(unsigned long value)
{
	writel(value, l2x0_base + L2X0_INTR_MASK);
}

static void l3x0_init(void __iomem *base, __u32 aux_val, __u32 aux_mask)
{
	__u32 aux;
	__u32 cache_id;
	__u32 way_size = 0;
	int ways;
	const char *type;

	l2x0_base = base;

	cache_id = readl_relaxed(l2x0_base + L2X0_CACHE_ID);
	aux = readl_relaxed(l2x0_base + L2X0_AUX_CTRL);

	aux &= aux_mask;
	aux |= aux_val;

	/* Determine the number of ways */
	switch (cache_id & L2X0_CACHE_ID_PART_MASK) {
	case L2X0_CACHE_ID_PART_L310:
		if (aux & (1 << 16))
			ways = 16;
		else
			ways = 8;
		type = "L310";
		break;
	case L2X0_CACHE_ID_PART_L210:
		ways = (aux >> 13) & 0xf;
		type = "L210";
		break;
	default:
		/* Assume unknown chips have 8 ways */
		ways = 8;
		type = "L2x0 series";
		break;
	}

	l2x0_way_mask = (1 << ways) - 1;
	/*
	 * L2 cache Size =  Way size * Number of ways
	 */
	way_size = (aux & L2X0_AUX_CTRL_WAY_SIZE_MASK) >> 17;
	way_size = 1 << (way_size + 3);
	l2x0_size = ways * way_size * SZ_1K;

	writel_relaxed(0x1ff,l2x0_base + L2X0_INTR_CLEAR);
	l2x0_intr_clr(0x1ff);
	l2x0_intr_mask(0x1ff);

	/*
	 * Check if l2x0 controller is already enabled.
	 * If you are booting from non-secure mode
	 * accessing the below registers will fault.
	 */
	if (!(readl_relaxed(l2x0_base + L2X0_CTRL) & 1)) {

		/* l2x0 controller is disabled */
		writel_relaxed(aux, l2x0_base + L2X0_AUX_CTRL);

		l2x0_inv_all();

		l2x0_tag_latency(PL310_TAG_RAM_LATENCY);
		l2x0_data_latency(PL310_DATA_RAM_LATENCY);
		/* enable L2X0 */
		writel_relaxed(1, l2x0_base + L2X0_CTRL);
	}

	outer_cache.inv_range = l2x0_inv_range;
	outer_cache.clean_range = l2x0_clean_range;
	outer_cache.flush_range = l2x0_flush_range;
	outer_cache.sync = l2x0_cache_sync;

	outer_cache.flush_all = l2x0_flush_all;
	outer_cache.inv_all = l2x0_inv_all;
	outer_cache.disable = l2x0_disable;
	//outer_cache.set_debug = l2x0_set_debug;



//	printk(KERN_INFO "%s cache controller enabled\n", type);
//	printk(KERN_INFO "lxx0: %d ways, CACHE_ID 0x%08x, AUX_CTRL 0x%08x, Cache size: %d B\n",
//			ways, cache_id, aux, l2x0_size);
}



 int sp_init_l3x0(void)
{
	l3x0_init((void __iomem *)SPRD_CACHE310_BASE, PL310_CACHE_AUX_VALUE, PL310_CACHE_AUX_MASK);
	return 0;
}

static void __l2x0_flush_all(void)
{
	debug_writel(0x03);
	writel_relaxed(l2x0_way_mask, l2x0_base + L2X0_CLEAN_INV_WAY);
	cache_wait_way(l2x0_base + L2X0_CLEAN_INV_WAY, l2x0_way_mask);
	cache_sync();
	debug_writel(0x00);
}
int outer_cache_poweroff(void)
{
	__l2x0_flush_all();
	writel_relaxed(0, l2x0_base + L2X0_CTRL);
	dsb();
	return 0;
}
int outer_cache_poweron(void)
{
	sp_init_l3x0();
	return 0;
}
