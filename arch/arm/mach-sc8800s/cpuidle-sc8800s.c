/*
 * arch/arm/mach-sc8800s/cpuidle-sc8800s.c
 *
 * CPU idle Spreadtrum SoCs
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 * The cpu idle uses wait-for-interrupt and DDR self refresh in order
 * to implement two idle states -
 * #1 wait-for-interrupt
 * #2 wait-for-interrupt and DDR self refresh
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/cpuidle.h>
#include <linux/io.h>
#include <asm/proc-fns.h>

#define SC8800S_MAX_STATES	2

static struct cpuidle_driver sc8800s_idle_driver = {
	.name =         "sc8800s_idle",
	.owner =        THIS_MODULE,
};

static DEFINE_PER_CPU(struct cpuidle_device, sc8800s_cpuidle_device);

/* Actual code that puts the SoC in different idle states */
static int sc8800s_enter_idle(struct cpuidle_device *dev,
			       struct cpuidle_state *state)
{
	struct timeval before, after;
	int idle_time;
	//void (*point)(void) = cpu_do_idle;
	/* cpu_do_idle = cpu_arm926_do_idle() */
	//printk("state = 0x%08x  states0 = 0x%08x  states1 = 0x%08x\n", state, &dev->states[0], &dev->states[1]);
	local_irq_disable();
	do_gettimeofday(&before);
	if (state == &dev->states[0]) {
		/* Wait for interrupt state */
		cpu_do_idle();
	} else if (state == &dev->states[1]) {
		/*
		 * Following write will put DDR in self refresh.
		 * Note that we have 256 cycles before DDR puts it
		 * self in self-refresh, so the wait-for-interrupt
		 * call afterwards won't get the DDR from self refresh
		 * mode.
		 */
		//writel(0x7, DDR_OPERATION_BASE);
		cpu_do_idle();
	}
	do_gettimeofday(&after);
	local_irq_enable();
	idle_time = (after.tv_sec - before.tv_sec) * USEC_PER_SEC +
			(after.tv_usec - before.tv_usec);
	//printk("idle time = %d\n", idle_time);
	return idle_time;
}

/* Initialize CPU idle by registering the idle states */
static int sc8800s_init_cpuidle(void)
{
	struct cpuidle_device *device;
	cpuidle_register_driver(&sc8800s_idle_driver);

	device = &per_cpu(sc8800s_cpuidle_device, smp_processor_id());
	device->state_count = SC8800S_MAX_STATES;

	/* Wait for interrupt state */
	device->states[0].enter = sc8800s_enter_idle;
	device->states[0].exit_latency = 1;
	device->states[0].target_residency = 10000;
	device->states[0].flags = CPUIDLE_FLAG_TIME_VALID;
	strcpy(device->states[0].name, "WFI");
	strcpy(device->states[0].desc, "Wait for interrupt");

	/* Wait for interrupt and DDR self refresh state */
	device->states[1].enter = sc8800s_enter_idle;
	device->states[1].exit_latency = 10;
	device->states[1].target_residency = 10000;
	device->states[1].flags = CPUIDLE_FLAG_TIME_VALID;
	strcpy(device->states[1].name, "Sdram SR");
	strcpy(device->states[1].desc, "WFI and Sdram Self Refresh");
	if (cpuidle_register_device(device)) {
		printk(KERN_ERR "sc8800s_init_cpuidle: Failed registering\n");
		return -EIO;
	}
	return 0;
}

device_initcall(sc8800s_init_cpuidle);
