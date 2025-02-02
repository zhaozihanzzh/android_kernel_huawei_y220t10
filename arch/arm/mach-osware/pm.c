/*
 *  linux/arch/arm/mach-osware/pm.c
 *
 *  Copyright (C) 2010 VirtualLogix
 *
 * This program is free software;  you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pm.h>
#include <linux/suspend.h>
#include <asm/system.h>

extern void printnk (char* f, ...);

static DECLARE_COMPLETION(cpu_killed);

    int
mach_cpu_disable (unsigned int cpu)
{
    return (cpu ? 0 : -EPERM);
}

    int
platform_cpu_kill (unsigned int cpu)
{
    return wait_for_completion_timeout(&cpu_killed, 5000);
}

    /*
     * platform-specific code to shutdown a CPU
     * Called with IRQs disabled
     */
    void
platform_cpu_die (unsigned int cpu)
{
#ifdef CONFIG_SMP
	if (cpu != hard_smp_processor_id()) {
	    BUG();
	}
#endif
	complete(&cpu_killed);
	os_ctx->smp_cpu_stop(cpu);
	BUG();	/* never returns */
}

    static int
osware_pm_enter(suspend_state_t state)
{
	hw_local_irq_disable();

#ifdef CONFIG_SMP
	if (hard_smp_processor_id()) {	/* must be the boot CPU */
	    BUG();
	}
#endif

	os_ctx->suspend_to_memory(os_ctx);

	hw_local_irq_enable();

	return 0;
}

static struct platform_suspend_ops osware_pm_ops = {
	.enter	= osware_pm_enter,
	.valid	= suspend_valid_only_mem,
};

static int __init osware_pm_init(void)
{
	suspend_set_ops(&osware_pm_ops);
	return 0;
}

__initcall(osware_pm_init);
