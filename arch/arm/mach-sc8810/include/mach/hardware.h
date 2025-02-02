/* arch/arm/mach-sc8800g/include/mach/hardware.h
 *
 * Copyright (C) 2011 Spreadtrum
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

#ifndef __ASM_ARCH_SPRD_HARDWARE_H
#define __ASM_ARCH_SPRD_HARDWARE_H

/*
 * SC8810 internal I/O mappings
 *
 * We have the following mapping:
 *      phys            virt
 *      20000000        E0000000
 *      60000000        E0010000
 *      80000000        E0020000/E0030000
 */

/* INTCV control registers */
#define SPRD_INTCV_BASE          0xE0000000
#define SPRD_INTCV_PHYS          0x80003000
#define SPRD_INTCV_SIZE          SZ_4K

/* DMA control registers */
#define SPRD_DMA_BASE            0xE0001000
#define SPRD_DMA_PHYS            0x20100000
#define SPRD_DMA_SIZE            SZ_4K

/* ISP control registers */
#define SPRD_ISP_BASE            0xE0002000
#define SPRD_ISP_PHYS            0x20200000
#define SPRD_ISP_SIZE            SZ_4K

/* USB device space */
//#define SPRD_USB_BASE            0xE0003000
//#define SPRD_USB_PHYS            0x20300000
//#define SPRD_USB_SIZE            SZ_4K

/* busmonitor control 0 registers */
#define SPRD_BUSM0_BASE            0xE0004000
#define SPRD_BUSM0_PHYS            0x20400000
#define SPRD_BUSM0_SIZE            SZ_4K

/* busmonitor control 1 registers */
#define SPRD_BUSM1_BASE            0xE0005000
#define SPRD_BUSM1_PHYS            0x20401000
#define SPRD_BUSM1_SIZE            SZ_4K

/* SDIO 0 control registers */
#define SPRD_SDIO0_BASE           0xE0006000
#define SPRD_SDIO0_PHYS           0x20500000
#define SPRD_SDIO0_SIZE           SZ_4K

/* SDIO 1 control registers */
#define SPRD_SDIO1_BASE           0xE0007000
#define SPRD_SDIO1_PHYS           0x20600000
#define SPRD_SDIO1_SIZE           SZ_4K

/* LCD control registers */
#define SPRD_LCDC_BASE        0xE0008000
#define SPRD_LCDC_PHYS        0x20700000
#define SPRD_LCDC_SIZE        SZ_4K

/* rotation  control registers*/
#define SPRD_ROTO_BASE           0xE0009000
#define SPRD_ROTO_PHYS           0x20800000
#define SPRD_ROTO_SIZE           SZ_4K

/* AHB control registers 
   NOTE: the real AHB phisical address base is 0x20900100, but here
	 we can	only map the address aligned by page size, so the
	 defination of AHB registers should be SPRD_AHB_BASE + 0x100
	 + register offset.
*/

#define SPRD_AHB_BASE            0xE000A000
#define SPRD_AHB_PHYS            0x20900000
#define SPRD_AHB_SIZE            SZ_4K

#define SPRD_AXIM_BASE            0xE000B000
#define SPRD_AXIM_PHYS            0x20A00000
#define SPRD_AXIM_SIZE            SZ_4K

/* EMC control registers */
#define SPRD_EMC_BASE            0xE000C000
#define SPRD_EMC_PHYS            0x20000000
#define SPRD_EMC_SIZE            SZ_4K

/* DRM control registers */
#define SPRD_DRM_BASE            0xE000D000
#define SPRD_DRM_PHYS            0x20B00000
#define SPRD_DRM_SIZE            SZ_4K

/* MEA control registers */
#define SPRD_MEA_BASE            0xE000E000
#define SPRD_MEA_PHYS            0x20C00000
#define SPRD_MEA_SIZE            SZ_4K

/* NAND & LCM0 */
#define SPRD_NAND_BASE           0xE0010000
#define SPRD_NAND_PHYS           0x60000000
#define SPRD_NAND_SIZE           SZ_32K

/* PCMCIA */
#define SPRD_PCMCIA_BASE         0xE0018000
#define SPRD_PCMCIA_PHYS         0x80000000
#define SPRD_PCMCIA_SIZE         SZ_4K

/* Ceva access ARM */
#define SPRD_ASHB_BASE           0xE0019000
#define SPRD_ASHB_PHYS           0x80000000
#define SPRD_ASHB_SIZE           SZ_4K

/* general RTC timers */
#define SPRD_TIMER_BASE          0xE001a000
#define SPRD_TIMER_PHYS          0x81000000
#define SPRD_TIMER_SIZE          SZ_4K

/* voice band */
#define SPRD_VB_BASE             0xE0023000
#define SPRD_VB_PHYS             0x82003000
#define SPRD_VB_SIZE             SZ_4K

/* serial 0 */
#define SPRD_SERIAL0_BASE        0xE0024000
#define SPRD_SERIAL0_PHYS        0x83000000
#define SPRD_SERIAL0_SIZE        SZ_4K

/* serial 1 */
#define SPRD_SERIAL1_BASE        0xE0025000
#define SPRD_SERIAL1_PHYS        0x84000000
#define SPRD_SERIAL1_SIZE        SZ_4K

/* SIM Card 0 */
#define SPRD_SIM0_BASE           0xE0026000
#define SPRD_SIM0_PHYS           0x85000000
#define SPRD_SIM0_SIZE           SZ_4K

/* SIM Card 1 */
#define SPRD_SIM1_BASE           0xE0027000
#define SPRD_SIM1_PHYS           0x85003000
#define SPRD_SIM1_SIZE           SZ_4K

/* I2C */ 
#define SPRD_I2C0_BASE            0xE0028000
#define SPRD_I2C0_PHYS            0x86000000
#define SPRD_I2C0_SIZE            SZ_4K

#define SPRD_I2C1_BASE            0xE0029000
#define SPRD_I2C1_PHYS            0x86001000
#define SPRD_I2C1_SIZE            SZ_4K

#define SPRD_I2C2_BASE            0xE002a000
#define SPRD_I2C2_PHYS            0x86002000
#define SPRD_I2C2_SIZE            SZ_4K

#define SPRD_I2C3_BASE            0xE002b000
#define SPRD_I2C3_PHYS            0x86003000
#define SPRD_I2C3_SIZE            SZ_4K


/* keypad and system counter share the same base */
#define SPRD_KPD_BASE            0xE002c000
#define SPRD_KPD_PHYS            0x87000000
#define SPRD_KPD_SIZE            SZ_4K
//#define SPRD_SYSCNT_BASE         SPRD_KPD_BASE

#define SPRD_SYSCNT_BASE            0xE002d000
#define SPRD_SYSCNT_PHYS            0x87003000
#define SPRD_SYSCNT_SIZE            SZ_4K

/* PWM */
#define SPRD_PWM_BASE            0xE002e000
#define SPRD_PWM_PHYS            0x88000000
#define SPRD_PWM_SIZE            SZ_4K

/* real time clock */
#define SPRD_RTC_BASE            0xE002f000
#define SPRD_RTC_PHYS            0x89000000
#define SPRD_RTC_SIZE            SZ_4K

/* watchdog */
#define SPRD_WDG_BASE            0xE0030000
#define SPRD_WDG_PHYS            0x82000040
#define SPRD_WDG_SIZE            SZ_4K

/* GPIO */
#define SPRD_GPIO_BASE           0xE0031000
#define SPRD_GPIO_PHYS           0x8a000000
#define SPRD_GPIO_SIZE           SZ_4K

/* eic "d" space */
#define SPRD_EIC_BASE             0xE0032000
#define SPRD_EIC_PHYS             0x8a001000
#define SPRD_EIC_SIZE              SZ_4K

/* global regsiters */
#define SPRD_GREG_BASE           0xE0033000
#define SPRD_GREG_PHYS           0x8B000000
#define SPRD_GREG_SIZE           SZ_4K

/* chip pin control registers */
#define SPRD_CPC_BASE            0xE0034000
#define SPRD_CPC_PHYS            0x8C000000
#define SPRD_CPC_SIZE            SZ_4K

/* EPT */
#define SPRD_EPT_BASE            0xE0035000
#define SPRD_EPT_PHYS            0x8D000000
#define SPRD_EPT_SIZE            SZ_4K

/* serial 2 */
#define SPRD_SERIAL2_BASE        0xE0036000
#define SPRD_SERIAL2_PHYS        0x8E000000
#define SPRD_SERIAL2_SIZE        SZ_4K

/* registers for watchdog ,RTC, touch panel, aux adc, analog die... */
#define SPRD_MISC_BASE          0xE0037000
#define SPRD_MISC_PHYS          0x82000000
#define SPRD_MISC_SIZE          SZ_4K

/* iis0*/
#define SPRD_IIS0_BASE            0xE0038000
#define SPRD_IIS0_PHYS            0x8E001000
#define SPRD_IIS0_SIZE            SZ_4K

/* SPI0 */
#define SPRD_SPI0_BASE            0xE0039000
#define SPRD_SPI0_PHYS            0x8E002000
#define SPRD_SPI0_SIZE            SZ_4K

/* SPI1 */
#define SPRD_SPI1_BASE            0xE003a000
#define SPRD_SPI1_PHYS            0x8E003000
#define SPRD_SPI1_SIZE            SZ_4K

/* iis1*/
#define SPRD_IIS1_BASE            0xE003b000
#define SPRD_IIS1_PHYS            0x8E004000
#define SPRD_IIS1_SIZE            SZ_4K

/* USB device space */
#define SPRD_USB_BASE            0xE003c000
#define SPRD_USB_PHYS            0x20300000
#define SPRD_USB_SIZE            SZ_1M

/*.CACHE310 space.*/
#define SPRD_CACHE310_BASE            0xE004c000
#define SPRD_CACHE310_PHYS            0xa2002000
#define SPRD_CACHE310_SIZE             SZ_4K


#define SPRD_PMU_BASE            0xE004D000
#define SPRD_PMU_PHYS            0xa0009000
#define SPRD_PMU_SIZE             SZ_4K


#define SPRD_AXIM_GPU_BASE            0xE0051000
#define SPRD_AXIM_GPU_PHYS            0x20A01000
#define SPRD_AXIM_GPU_SIZE            SZ_4K

#define SPRD_A5_DEBUG_BASE            0xE0052000
#define SPRD_A5_DEBUG_PHYS            0xa0008000
#define SPRD_A5_DEBUG_SIZE            SZ_4K


#endif
