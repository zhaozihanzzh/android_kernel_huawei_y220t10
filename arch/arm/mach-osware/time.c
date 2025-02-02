/*
 *  linux/arch/arm/mach-osware/time.c
 *
 *  Copyright (C) 2006 Jaluna SA.
 *
 * This program is free software;  you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>

#include <asm/mach/time.h>

#include <asm/nkern.h>
#include <nk/nkern.h>

/*
 * The following driver implements the tick of the generic ARM OSware machine.
 * It uses the services provided by the OSware vtick front-end driver.
 * The current driver is similar to the OSware vtick back-end driver but
 * does neither handle tick loss nor provide microseconds precision
 * (these features rely on board dependent services).
 */

/*
 * Functions provided by the OSware vtick back-end driver;
 */
extern NkDevTick* nk_tick_lookup(void);
extern void nk_tick_connect (NkDevTick*    nktick,
			     irqreturn_t (*hdl)(int, void *));

/*
 * Dummy gettimeoffset function.
 */
static unsigned long osware_timer_gettimeoffset(void)
{
	return 0;
}

/*
 * Generic OSware timer interrupt service routine.
 */
static irqreturn_t osware_timer_interrupt(int irq, void *dev_id)
{
	timer_tick();
	return IRQ_HANDLED;
}

/*
 * Generic OSware timer initialization function.
 */
static __init void osware_timer_init(void)
{
	static NkDevTick* nktick;

	nktick = nk_tick_lookup();
	nk_tick_connect(nktick, osware_timer_interrupt);
}

/*
 * Generic OSware timer descriptor.
 */
struct sys_timer osware_timer = {
	.init		= osware_timer_init,
	.offset		= osware_timer_gettimeoffset,
};

/*
 * Dummy stubs for the OSware vtick back-end driver.
 */
unsigned long nk_vtick_get_ticks_per_hz(void)
{
	return 0;
}

unsigned long nk_vtick_get_modulo(void)
{
	return 0;
}

unsigned long nk_vtick_read_stamp(void)
{
	return 0;
}

unsigned long nk_vtick_ticks_to_usecs(unsigned long ticks)
{
	return 0;
}
