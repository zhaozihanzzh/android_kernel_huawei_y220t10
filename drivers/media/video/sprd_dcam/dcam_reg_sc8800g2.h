/*
* drivers/media/video/sprd_dcam/dcam_reg_sc8800g2.h
 * Dcam driver based on sc8800g2
 *
 * Copyright (C) 2011 Spreadtrum 
 * 
 * Author: Xiaozhe wang <xiaozhe.wang@spreadtrum.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
 #ifndef _DCAM_REG_SC8800G2_H_
#define _DCAM_REG_SC8800G2_H_

//0xE0002000UL <-->0x20200000
#define DCAM_REG_BASE SPRD_ISP_BASE 
#define DCAM_CFG DCAM_REG_BASE + 0x00UL
#define DCAM_PATH_CFG DCAM_REG_BASE + 0x04UL
#define DCAM_SRC_SIZE DCAM_REG_BASE + 0x08UL
#define DCAM_DES_SIZE DCAM_REG_BASE + 0x0CUL
#define DCAM_TRIM_START DCAM_REG_BASE + 0x10UL
#define DCAM_TRIM_SIZE DCAM_REG_BASE + 0x14UL
#define REV_PATH_CFG DCAM_REG_BASE + 0x18UL
#define DCAM_INT_STS DCAM_REG_BASE + 0x30UL
#define DCAM_INT_MASK DCAM_REG_BASE + 0x34UL
#define DCAM_INT_CLR DCAM_REG_BASE + 0x38UL
#define DCAM_INT_RAW DCAM_REG_BASE + 0x3CUL
#define GMC_TAB_TEST_MODE DCAM_REG_BASE + 0x40UL
#define GMC_TAB_TEST_ADDR DCAM_REG_BASE + 0x44UL
#define GMC_TAB_TEST_DOUT DCAM_REG_BASE + 0x48UL
#define SC1_TAB0_TEST_MODE DCAM_REG_BASE + 0x50UL
#define SC1_TAB0_TEST_ADDR DCAM_REG_BASE + 0x54UL
#define SC1_TAB0_TEST_DOUT DCAM_REG_BASE + 0x58UL
#define DCAM_ADDR_H DCAM_REG_BASE + 0x5CUL
#define SC1_TAB1_TEST_MODE DCAM_REG_BASE + 0x60UL
#define ENDIAN_SEL DCAM_REG_BASE + 0x64UL
#define SC1_TAB1_TEST_DOUT DCAM_REG_BASE + 0x68UL
#define DCAM_ADDR_7 DCAM_REG_BASE + 0x6CUL
#define DCAM_ADDR_8 DCAM_REG_BASE + 0x70UL
#define CAP_CNTRL DCAM_REG_BASE + 0x100UL
#define CAP_FRM_CNT DCAM_REG_BASE + 0x104UL
#define CAP_START DCAM_REG_BASE + 0x108UL
#define CAP_END DCAM_REG_BASE + 0x10CUL
#define CAP_IMAGE_DECI DCAM_REG_BASE + 0x110UL
#define CAP_TRANS DCAM_REG_BASE + 0x114UL
#define CAP_JPG_FRM_CTL DCAM_REG_BASE + 0x11CUL
#define CAP_JPG_FRM_SIZE DCAM_REG_BASE + 0x120UL
#define PREF_CONF DCAM_REG_BASE + 0x200UL
#define BLC_CONF DCAM_REG_BASE + 0x210UL
#define AWB_START DCAM_REG_BASE + 0x220UL
#define AWB_BLK_CONF DCAM_REG_BASE + 0x224UL
#define AWBC_CONF DCAM_REG_BASE + 0x230UL
#define NSR_WEIGHT DCAM_REG_BASE + 0x240UL
#define NSR_LIMIT DCAM_REG_BASE + 0x244UL
#define AF_SHIFT DCAM_REG_BASE + 0x250UL
#define AF_START DCAM_REG_BASE + 0x254UL
#define AF_END DCAM_REG_BASE + 0x258UL
#define AF_RESULT DCAM_REG_BASE + 0x25CUL
#define LNC_CENTER DCAM_REG_BASE + 0x260UL
#define LNC_PARA DCAM_REG_BASE + 0x264UL
#define LNC_CENTER_SQR DCAM_REG_BASE + 0x268UL
#define CFA_CONF DCAM_REG_BASE + 0x270UL
#define CCE_MATRIX_YR DCAM_REG_BASE + 0x280UL
#define CCE_MATRIX_YG DCAM_REG_BASE + 0x284UL
#define CCE_MATRIX_YB DCAM_REG_BASE + 0x288UL
#define CCE_MATRIX_UR DCAM_REG_BASE + 0x28CUL
#define CCE_MATRIX_UG DCAM_REG_BASE + 0x290UL
#define CCE_MATRIX_UB DCAM_REG_BASE + 0x294UL
#define CCE_MATRIX_VR DCAM_REG_BASE + 0x298UL
#define CCE_MATRIX_VG DCAM_REG_BASE + 0x29CUL
#define CCE_MATRIX_VB DCAM_REG_BASE + 0x2A0UL
#define CCE_Y_SHIFT_VALUE DCAM_REG_BASE + 0x2A4UL
#define CCE_U_SHIFT_VALUE DCAM_REG_BASE + 0x2A4UL
#define CCE_V_SHIFT_VALUE DCAM_REG_BASE + 0x2A4UL

#define DCAM_SC_MODE DCAM_REG_BASE + 0x304UL
#define DCAM_SC2_SIZE DCAM_REG_BASE + 0x308UL
#define DCAM_SC2_START DCAM_REG_BASE + 0x30CUL
#define DCAM_DISP_SIZE DCAM_REG_BASE + 0x310UL
#define DCAM_DISP_START DCAM_REG_BASE + 0x314UL
#define DCAM_ENC_SIZE DCAM_REG_BASE + 0x318UL
#define DCAM_ENC_START DCAM_REG_BASE + 0x31CUL
#define SC1_CONF DCAM_REG_BASE + 0x320UL
#define SC2_CONF DCAM_REG_BASE + 0x324UL
#define DCAM_DBLK_THR DCAM_REG_BASE + 0x334UL
#define DCAM_SC_LINE_BLANK DCAM_REG_BASE + 0x338UL
#define DCAM_PORTA_CONF DCAM_REG_BASE + 0x340UL
#define DCAM_PORTA_STATE DCAM_REG_BASE + 0x344UL
#define DCAM_PORTA_FRM DCAM_REG_BASE + 0x348UL
#define DCAM_PORTA_CURR_YBA DCAM_REG_BASE + 0x34CUL
#define DCAM_PORTA_CURR_UBA DCAM_REG_BASE + 0x350UL
#define DCAM_PORTA_CURR_VBA DCAM_REG_BASE + 0x344UL
#define DCAM_PORTA_NEXT_YBA DCAM_REG_BASE + 0x358UL
#define DCAM_PORTA_NEXT_UBA DCAM_REG_BASE + 0x35CUL
#define DCAM_PORTA_NEXT_VBA DCAM_REG_BASE + 0x360UL
#define DCAM_PORTA_LAST_END DCAM_REG_BASE + 0x364UL
#define DCAM_PORTA_CURR_YADDR DCAM_REG_BASE + 0x368UL
#define DCAM_PORTA_SKIP DCAM_REG_BASE + 0x36CUL
#define DCAM_PORTA_TH DCAM_REG_BASE + 0x370UL
#define DCAM_PORTB_CONF DCAM_REG_BASE + 0x380UL
#define DCAM_PORTB_STATE DCAM_REG_BASE + 0x384UL
#define DCAM_PORTB_FRM DCAM_REG_BASE + 0x388UL
#define DCAM_PORTB_CURR_BA DCAM_REG_BASE + 0x38CUL
#define DCAM_PORTB_NEXT_BA DCAM_REG_BASE + 0x390UL
#define DCAM_PORTB_LAST_END DCAM_REG_BASE + 0x394UL
#define DCAM_PORTB_CURR_YADDR DCAM_REG_BASE + 0x398UL
#define DCAM_PORTB_SKIP DCAM_REG_BASE + 0x39CUL
#define DCAM_PORTB_TH DCAM_REG_BASE + 0x3A0UL
#define DCAM_PORTC_START DCAM_REG_BASE + 0x3C0UL
#define DCAM_PORTC_CONF DCAM_REG_BASE + 0x3C4UL
#define DCAM_PORTC_STATE DCAM_REG_BASE + 0x3C8UL
#define DCAM_PORTC_CURR_YBA DCAM_REG_BASE + 0x3CCUL
#define DCAM_PORTC_CURR_UBA DCAM_REG_BASE + 0x3D0UL
#define DCAM_PORTC_CURR_VBA DCAM_REG_BASE + 0x3D4UL
#define DCAM_PORTC_SKIP DCAM_REG_BASE + 0x3D8UL
#define DCAM_PORTC_CURR_YADDR DCAM_REG_BASE + 0x3DCUL
#define VP_REVIEW_SIZE DCAM_REG_BASE + 0x3E0UL
#define VP_REVIEW_SELF_RATE DCAM_REG_BASE + 0x3E4UL

#define DCAM_AWB_RESULT DCAM_REG_BASE + 0x400UL
#define DCAM_OBSERVED_ENABLE DCAM_REG_BASE + 0x3F8UL
#define DCAM_GAMMA_TABLE DCAM_REG_BASE + 0xB00UL
#define DCAM_OBSERVED_STATE DCAM_REG_BASE + 0x3F8UL

#define DCAM_SC1_CEOFX_S DCAM_REG_BASE + 0xC00
#define DCAM_SC1_CEOFX_E DCAM_REG_BASE + 0xD20
#define DCAM_SC1_CEOFY_S DCAM_REG_BASE + 0xE00
#define DCAM_SC1_CEOFY_E DCAM_REG_BASE + 0xF20
#define DCAM_SC2_CEOFX_S DCAM_REG_BASE + 0x1000 
#define DCAM_SC2_CEOFX_E DCAM_REG_BASE + 0x111C
#define DCAM_SC2_CEOFY_S DCAM_REG_BASE + 0x1200
#define DCAM_SC2_CEOFY_E DCAM_REG_BASE + 0x131C

#define GLOBAL_BASE SPRD_GREG_BASE //0xE0002E00UL <--> 0x8b000000
#define ARM_GLOBAL_REG_GEN0 GLOBAL_BASE + 0x008UL
#define ARM_GLOBAL_REG_GEN3 GLOBAL_BASE + 0x01CUL
#define ARM_GLOBAL_PLL_MN GLOBAL_BASE + 0x024UL
#define ARM_GLOBAL_VPLL_MN GLOBAL_BASE + 0x068UL
#define ARM_GLOBAL_PLL_SCR GLOBAL_BASE + 0x070UL
#define GR_CLK_DLY GLOBAL_BASE + 0x05CUL
#define GR_CLK_GEN5 GLOBAL_BASE + 0x07CUL

#define AHB_BASE SPRD_AHB_BASE //0xE000A000 <--> 0x20900000UL
#define AHB_GLOBAL_REG_CTL0 AHB_BASE + 0x200UL
#define AHB_GLOBAL_REG_CTL1 AHB_BASE + 0x204UL
#define AHB_GLOBAL_REG_SOFTRST AHB_BASE + 0x210UL

#define PIN_CTL_BASE SPRD_CPC_BASE//0xE002F000<-->0x8C000000UL
#define PIN_CTL_CCIRRST PIN_CTL_BASE + 0x340UL
#define PIN_CTL_CCIRPD1 PIN_CTL_BASE + 0x344UL
#define PIN_CTL_CCIRPD0 PIN_CTL_BASE + 0x348UL

#define IRQ_BASE SPRD_ASHB_BASE //0xE0021000<-->0x80003000
#define INT_IRQ_EN IRQ_BASE + 0x008UL
#define INT_IRQ_DISABLE IRQ_BASE + 0x00CUL

#define MISC_BASE SPRD_MISC_BASE //0xE0033000<-->0x82000000
#define ANA_REG_BASE MISC_BASE + 0x480
#define ANA_LDO_PD_CTL ANA_REG_BASE + 0x10
#define ANA_LDO_VCTL0 ANA_REG_BASE + 0x14
#define ANA_LDO_VCTL1 ANA_REG_BASE + 0x18
#define ANA_LDO_VCTL2 ANA_REG_BASE + 0x1C

#endif //_DCAM_REG_SC8800G2_H_