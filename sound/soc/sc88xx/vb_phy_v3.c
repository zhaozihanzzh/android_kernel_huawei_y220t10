/******************************************************************************
 ** File Name:      vb_phy_v3.c                                               *
 ** Author:         Jeff.Li                                                   *
 ** DATE:           16/09/2010                                                *
 ** Copyright:      2003 Spreadtrum, Incoporated. All Rights Reserved.        *
 ** Description:    Physical layer of voiceband, which exports interfaces to  *
 **                 audio dev HAL layer.
 ******************************************************************************

 ******************************************************************************
 **                        Edit History                                       *
 ** ------------------------------------------------------------------------- *
 ** DATE           NAME             DESCRIPTION                               *
 ** 16/09/2010     Jeff.Li          Create.                                   *
 *****************************************************************************/

/**---------------------------------------------------------------------------*
 **                         Dependencies                                      *
 **---------------------------------------------------------------------------*/
#include "vb_reg_v3.h"
#include "audio_dev_phy.h"
#include "vb_drvapi.h"
#include "sci_types.h"
#include "vbc-codec.h"
#include "filter_calc.h"
#include "aud_enha.h"
#include <linux/delay.h>

/**---------------------------------------------------------------------------*
 **                         Compiler Flag                                     *
 **---------------------------------------------------------------------------*/
#ifdef __cplusplus
extern   "C"
{
#endif

/**---------------------------------------------------------------------------*
 **                         Macro defination                                  *
 **---------------------------------------------------------------------------*/


/*****************************************************************************/
// Description :
// Author :         Jeff.Li
// Note :
/*****************************************************************************/
PUBLIC void VB_PHY_HPFSwitch (BOOLEAN is_enable)
{
    if (is_enable)
    {
        REG32 (DAHPCTL) |= (1 << VBDAHP_EN_SHIFT);
    }
    else
    {
        REG32 (DAHPCTL) &= ~ (1 << VBDAHP_EN_SHIFT);
    }
}

/*****************************************************************************/
// Description :
// Author :         Jeff.Li
// Note :
/*****************************************************************************/
PUBLIC void VB_PHY_SetHPFMode (BOOLEAN is_reset_mode)
{
    if (is_reset_mode)
    {
        REG32 (DAHPCTL) |= (1 << VBDAHP_REG_CLR_SHIFT);
    }
    else
    {
        REG32 (DAHPCTL) &= ~ (1 << VBDAHP_REG_CLR_SHIFT);
    }
}

/*****************************************************************************/
// Description :
// Author :         Jeff.Li
// Note :
/*****************************************************************************/
PUBLIC void VB_PHY_SetHPFParas (
    int16_t filter_index,
    int16_t B0,
    int16_t B1,
    int16_t B2,
    int16_t A0,
    int16_t minusA1,
    int16_t minusA2
)
{
    switch (filter_index)
    {
        case 1:
            {
                REG32 (HPCOEF1)  = B0;
                REG32 (HPCOEF3)  = B1;
                REG32 (HPCOEF5)  = B2;
                REG32 (HPCOEF2)  = A0;
                REG32 (HPCOEF4)  = minusA1;
                REG32 (HPCOEF6)  = minusA2;
            }
            break;
        case 2:
            {
                REG32 (HPCOEF8)  = B0;
                REG32 (HPCOEF10) = B1;
                REG32 (HPCOEF12) = B2;
                REG32 (HPCOEF9)  = A0;
                REG32 (HPCOEF11) = minusA1;
                REG32 (HPCOEF13) = minusA2;
            }
            break;
        case 3:
            {
                REG32 (HPCOEF15) = B0;
                REG32 (HPCOEF17) = B1;
                REG32 (HPCOEF19) = B2;
                REG32 (HPCOEF16) = A0;
                REG32 (HPCOEF18) = minusA1;
                REG32 (HPCOEF20) = minusA2;
            }
            break;
        case 4:
            {
                REG32 (HPCOEF22) = B0;
                REG32 (HPCOEF24) = B1;
                REG32 (HPCOEF26) = B2;
                REG32 (HPCOEF23) = A0;
                REG32 (HPCOEF25) = minusA1;
                REG32 (HPCOEF27) = minusA2;
            }
            break;
        case 5:
            {
                REG32 (HPCOEF29) = B0;
                REG32 (HPCOEF31) = B1;
                REG32 (HPCOEF33) = B2;
                REG32 (HPCOEF30) = A0;
                REG32 (HPCOEF32) = minusA1;
                REG32 (HPCOEF34) = minusA2;
            }
            break;
        case 6:
            {
                REG32 (HPCOEF36) = B0;
                REG32 (HPCOEF38) = B1;
                REG32 (HPCOEF40) = B2;
                REG32 (HPCOEF37) = A0;
                REG32 (HPCOEF39) = minusA1;
                REG32 (HPCOEF41) = minusA2;
            }
            break;
        default:
            break;
    }
}


/*****************************************************************************/
// Description :
// Author :         Jeff.Li
// Note :
/*****************************************************************************/
PUBLIC void VB_PHY_SetHPFGain (
    int16_t gain_index,//gain index:0,1,2,3,4,5,6
    int16_t gain_value
)
{
    switch (gain_index)
    {
        case 0://S0
            {
                REG32 (HPCOEF0) = gain_value;
            }
            break;
        case 1://S1
            {
                REG32 (HPCOEF7) = gain_value;
            }
            break;
        case 2://S2
            {
                REG32 (HPCOEF14) = gain_value;
            }
            break;
        case 3://S3
            {
                REG32 (HPCOEF21) = gain_value;
            }
            break;
        case 4://S4
            {
                REG32 (HPCOEF28) = gain_value;
            }
            break;
        case 5://S5
            {
                REG32 (HPCOEF35) = gain_value;
            }
            break;
        case 6://S6
            {
                REG32 (HPCOEF42) = gain_value;
            }
            break;
        default:
            break;
    }
}

/*****************************************************************************/
// Description :
// Author :         Jeff.Li
// Note :
/*****************************************************************************/
PUBLIC uint32_t VB_PHY_GetHPFGain (
    int16_t gain_index
)
{
    uint32_t gain = 0;

    switch (gain_index)
    {
        case 0://S0
            {
                gain = REG32 (HPCOEF0);
            }
            break;
        case 1://S1
            {
                gain = REG32 (HPCOEF7);
            }
            break;
        case 2://S2
            {
                gain = REG32 (HPCOEF14);
            }
            break;
        case 3://S3
            {
                gain = REG32 (HPCOEF21);
            }
            break;
        case 4://S4
            {
                gain = REG32 (HPCOEF28);
            }
            break;
        case 5://S5
            {
                gain = REG32 (HPCOEF35);
            }
            break;
        case 6://S6
            {
                gain = REG32 (HPCOEF42);
            }
            break;
        default:
            {
                gain = 0;
            }
            break;
    }

    return gain;
}

/*****************************************************************************/
// Description :
// Author :         Jeff.Li
// Note :
/*****************************************************************************/
PUBLIC void VB_PHY_SetHPFLimit (
    int8_t rLimit //0~127
)
{
    int16_t limit_value = rLimit;

    if (limit_value < 0)
    {
        limit_value = 0;
    }

    REG32 (DAHPCTL) &= ~ (0xff<<VBDAHP_LIMIT_SHIFT);
    REG32 (DAHPCTL) |= (limit_value<< VBDAHP_LIMIT_SHIFT);
}

/*****************************************************************************/
// Description :
// Author :         Jeff.Li
// Note :
/*****************************************************************************/
PUBLIC void VB_PHY_SetHPFWidth (
    uint16_t  width //width = 16 or 24 (bits)
)
{
    uint16_t data_width_value = 0;

    switch (width)
    {
        case 16:
            data_width_value = (0<<VBDAHP_WID_SEL_SHIFT);
            break;
        case 24:
            data_width_value = (1<<VBDAHP_WID_SEL_SHIFT);
            break;
        default:
            data_width_value = (1<<VBDAHP_WID_SEL_SHIFT);
            break;
    }

    if ( (REG32 (DAHPCTL) & (1<<VBDAHP_WID_SEL_SHIFT)) != data_width_value)
    {
        REG32 (DAHPCTL) &= ~ (1<<VBDAHP_WID_SEL_SHIFT);
        REG32 (DAHPCTL) |= data_width_value;
    }
}

/*****************************************************************************/
// Description :
// Author :         cherry.liu
// Note :
/*****************************************************************************/
PUBLIC void VB_PHY_ALCSwitch (
    BOOLEAN is_enable
)
{
    if (is_enable)
    {
        REG32 (DAHPCTL) |= (1 << VBDAALC_EN_SHIFT);
    }
    else
    {
        REG32 (DAHPCTL) &= ~ (1 << VBDAALC_EN_SHIFT);
    }
}

/*****************************************************************************/
// Description :
// Author :         cherry.liu
// Note :
/*****************************************************************************/
PUBLIC void VB_PHY_SetALCParas (
    VBC_ALC_PARAS_T *ptAlcPara
)
{
    REG32 (DAALCCTL0)  = ptAlcPara->hold;
    REG32 (DAALCCTL1)  = ptAlcPara->rise;
    REG32 (DAALCCTL2)  = ptAlcPara->fall;
    REG32 (DAALCCTL3)  = ptAlcPara->limit;
    REG32 (DAALCCTL4)  = ptAlcPara->threshold;
    REG32 (DAALCCTL5)  = ptAlcPara->ratio;
    REG32 (DAALCCTL6)  = ptAlcPara->cg_var;
    REG32 (DAALCCTL7)  = ptAlcPara->release_rate;
    REG32 (DAALCCTL8)  = ptAlcPara->attack_rate;
    REG32 (DAALCCTL9)  = ptAlcPara->release_rate_ex;
    REG32 (DAALCCTL10) = ptAlcPara->attack_rate_ex;
}


/*****************************************************************************/
// Description :
// Author :         cherry.liu
// Note :
/*****************************************************************************/
PUBLIC void VB_PHY_SetFMMixMode (
    int16_t da_index,//0,1
    int16_t mix_mode  //0,1,2
)
{
    uint16_t mix_mode_value = 0;

    if (0==da_index) //DAC0
    {
        switch (mix_mode)
        {
            case 0://no side tone
                mix_mode_value = (0<<VBDAPATH_DA0_ADDFM_SHIFT);
                break;
            case 1:// + side tone
                mix_mode_value = (1<<VBDAPATH_DA0_ADDFM_SHIFT);
                break;
            case 2://- side tone
                mix_mode_value = (2<<VBDAPATH_DA0_ADDFM_SHIFT);
                break;
            default:
                mix_mode_value = (0<<VBDAPATH_DA0_ADDFM_SHIFT);
                break;
        }

        REG32 (DAPATCHCTL) &= ~ (3<<VBDAPATH_DA0_ADDFM_SHIFT);
        REG32 (DAPATCHCTL) |= mix_mode_value;
    }
    else if (1==da_index) //DAC1
    {
        switch (mix_mode)
        {
            case 0://no side tone
                mix_mode_value = (0<<VBDAPATH_DA1_ADDFM_SHIFT);
                break;
            case 1:// + side tone
                mix_mode_value = (1<<VBDAPATH_DA1_ADDFM_SHIFT);
                break;
            case 2://- side tone
                mix_mode_value = (2<<VBDAPATH_DA1_ADDFM_SHIFT);
                break;
            default:
                mix_mode_value = (0<<VBDAPATH_DA1_ADDFM_SHIFT);
                break;
        }

        REG32 (DAPATCHCTL) &= ~ (3<<VBDAPATH_DA1_ADDFM_SHIFT);
        REG32 (DAPATCHCTL) |= mix_mode_value;
    }
    else
    {
        // printk ("vb_phy_v2.c,[VB_PHY_SetFMMixMode] invalid da index :%d \n",da_index);
    }
}

/*****************************************************************************/
// Description :
// Author :         cherry.liu
// Note :
/*****************************************************************************/
PUBLIC void VB_PHY_DGSwitch (
    int16_t da_index,//0,1
    BOOLEAN is_enable
)
{
    if (0==da_index) //DAC0
    {
        if (is_enable)
        {
            REG32 (DADGCTL) |= (1 << VBDADG_EN0_SHIFT);
        }
        else
        {
            REG32 (DADGCTL) &= ~ (1 << VBDADG_EN0_SHIFT);
        }
    }
    else if (1==da_index) //DAC1
    {
        if (is_enable)
        {
            REG32 (DADGCTL) |= (1 << VBDADG_EN1_SHIFT);
        }
        else
        {
            REG32 (DADGCTL) &= ~ (1 << VBDADG_EN1_SHIFT);
        }
    }
    else
    {
        // printk ("vb_phy_v2.c,[VB_PHY_DGSwitch] invalid da index :%d \n",da_index);
    }
}

/*****************************************************************************/
// Description :
// Author :         cherry.liu
// Note :
/*****************************************************************************/
PUBLIC void VB_PHY_SetDG (
    int16_t da_index,//0,1
    int16_t dg_value
)
{
    //check the validity of input paras
    if (dg_value>63)
    {
        dg_value = 63;
    }

    if (dg_value<-64)
    {
        dg_value = -64;
    }

    if (0==da_index) //DAC0
    {
        REG32 (DADGCTL) &= ~ (0x7f << VBDADG_DG0_SHIFT);
        REG32 (DADGCTL) |= (dg_value<< VBDADG_DG0_SHIFT);
    }
    else if (1==da_index) //DAC1
    {
        REG32 (DADGCTL) &= ~ (0x7f << VBDADG_DG1_SHIFT);
        REG32 (DADGCTL) |= (dg_value<< VBDADG_DG1_SHIFT);
    }
    else
    {
        // printk ("vb_phy_v2.c,[VB_PHY_SetDG] invalid da index :%d \n",da_index);
    }
}


/**---------------------------------------------------------------------------*
 **                         Compiler Flag                                     *
 **---------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif


