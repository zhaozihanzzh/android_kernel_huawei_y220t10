/* arch/arm/mach-sc8800g/io.c
 *
 * sc8800s io support
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

#include <linux/kernel.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/page.h>
#include <asm/mach/map.h>
#include <asm/tlb.h>

#include <mach/hardware.h>
#include <mach/board.h>

#define SPRD_DEVICE(name) { \
		.virtual = SPRD_##name##_BASE, \
		.pfn = __phys_to_pfn(SPRD_##name##_PHYS), \
		.length = SPRD_##name##_SIZE, \
		.type = MT_DEVICE_NONSHARED, \
	 }

static struct map_desc sprd_io_desc[] __initdata = {
	SPRD_DEVICE(INTCV),
	SPRD_DEVICE(DMA),
	SPRD_DEVICE(ISP),
	SPRD_DEVICE(USB),
	SPRD_DEVICE(BUSM0),
	SPRD_DEVICE(BUSM1),
	SPRD_DEVICE(SDIO0),
	SPRD_DEVICE(SDIO1),
	SPRD_DEVICE(LCDC),
	SPRD_DEVICE(ROTO),
	SPRD_DEVICE(AHB),
	SPRD_DEVICE(AXIM),
	SPRD_DEVICE(EMC),
	SPRD_DEVICE(DRM),
	SPRD_DEVICE(MEA),
	SPRD_DEVICE(NAND),
	SPRD_DEVICE(PCMCIA),
	SPRD_DEVICE(ASHB),
	SPRD_DEVICE(TIMER),
	SPRD_DEVICE(VB),
	SPRD_DEVICE(SERIAL0),
	SPRD_DEVICE(SERIAL1),
	SPRD_DEVICE(SIM0),
	SPRD_DEVICE(SIM1),
	SPRD_DEVICE(I2C0),
	SPRD_DEVICE(I2C1),
	SPRD_DEVICE(I2C2),
	SPRD_DEVICE(I2C3),
	SPRD_DEVICE(SPI0),
	SPRD_DEVICE(SPI1),
	SPRD_DEVICE(KPD),
	SPRD_DEVICE(SYSCNT),
	SPRD_DEVICE(PWM),
	SPRD_DEVICE(RTC),
	SPRD_DEVICE(WDG),
	SPRD_DEVICE(GPIO),
	SPRD_DEVICE(GREG),
	SPRD_DEVICE(CPC),
	SPRD_DEVICE(EPT),
	SPRD_DEVICE(SERIAL2),
	SPRD_DEVICE(MISC),
	//SPRD_DEVICE(EICA),
	SPRD_DEVICE(EIC),
	SPRD_DEVICE(IIS0),
	SPRD_DEVICE(IIS1),
	SPRD_DEVICE(PMU),
	SPRD_DEVICE(AXIM_GPU),
#ifdef CONFIG_CACHE_L2X0_310
	SPRD_DEVICE(CACHE310),
#endif
	SPRD_DEVICE(A5_DEBUG),
};

void __init sprd_map_common_io(void)
{
	/* Normally devicemaps_init() would flush caches and tlb after
	 * mdesc->map_io(), but we must also do it here because of the CPU
	 * revision check below.
	 */
	local_flush_tlb_all();
	flush_cache_all();

	iotable_init(sprd_io_desc, ARRAY_SIZE(sprd_io_desc));
}

