/* arch/arm/mach-sc8800g/include/mach/irqs.h
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

#ifndef __ASM_ARCH_SPRD_IRQS_H
#define __ASM_ARCH_SPRD_IRQS_H

/* 
 * naming rule: name in chip spec plus  an "IRQ_" prefix 
 * see spec 4.5.2 
 */
#define IRQ_SLEEP_INT		0
#define IRQ_SOFT_TRIG_INT	1
#define IRQ_SER0_INT		2

#define IRQ_SER1_INT		3
#define IRQ_CA5_L2CC_INT	3
#define IRQ_CA5_PMU_INT	3
#define IRQ_CA5_NCT_INT	3

#define IRQ_SER2_INT		4

#define IRQ_TIMER0_INT		5
#define IRQ_TIMER1_INT		6

#define IRQ_TIMER2_INT		7
#define IRQ_GPIO_EIC_INT	8	

#define IRQ_SPI0_INT		9
#define IRQ_SPI1_INT		9

#define IRQ_KPD_INT		10

#define IRQ_I2C0_INT		11
#define IRQ_I2C1_INT		11

#define IRQ_SIM_INT		12
#define IRQ_PIU_SER_INT	13

#define IRQ_PIU_CR_INT		14
#define IRQ_I2C2		14

#define IRQ_I2C3		14
#define IRQ_DSP0_INT		15
#define IRQ_DSP1_INT		16
#define IRQ_SYST_INT		17
#define IRQ_IIS0_INT		19
#define IRQ_IIS1_INT		19
#define IRQ_DSP_ICU_INT	20
#define IRQ_DMA_INT		21
#define IRQ_VBC_INT		22
#define IRQ_VSP_INT		23
#define IRQ_ANA_INT		24

#define IRQ_ADI_INT		25
#define IRQ_GPU_INT		25

#define IRQ_USBD_INT		26
#define IRQ_ISP_INT		27
#define IRQ_NLC_INT		28
#define IRQ_LCDC_INT		29
#define IRQ_SDIO0_INT		30

#define IRQ_BM0_INT		31
#define IRQ_BM1_INT		31
#define IRQ_SDIO1_INT		31
#define IRQ_AXI_BUS_MON	31

#define IRQ_ANA_ADC_INT	32
#define IRQ_ANA_GPIO_INT	33
#define IRQ_ANA_RTC_INT	34
#define IRQ_ANA_WDG_INT	35
#define IRQ_ANA_TPC_INT	36
#define IRQ_ANA_EIC_INT	37
#define IRQ_ANA_CHGRWDG_INT	38


#define IRQ_ANA_INT_START	IRQ_ANA_ADC_INT


#define NR_SPRD_IRQS  32
#define NR_ANA_IRQS		7

#define GPIO_IRQ_START  (NR_SPRD_IRQS + NR_ANA_IRQS)
#define NR_GPIO_IRQS  10

#define NR_EIC_ALL_IRQS	(16+8)
#define EIC_IRQ_START  (GPIO_IRQ_START + NR_GPIO_IRQS)

#define NR_BOARD_IRQS 0 /* to be added later */

#define NR_IRQS (NR_SPRD_IRQS +NR_ANA_IRQS+ NR_GPIO_IRQS + NR_BOARD_IRQS + NR_EIC_ALL_IRQS)


#ifdef CONFIG_NKERNEL
void sprd_enable_ana_irq(void);
#endif

#endif
