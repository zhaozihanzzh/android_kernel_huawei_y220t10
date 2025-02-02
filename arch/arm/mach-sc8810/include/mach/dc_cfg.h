/******************************************************************************
 ** File Name:      dc_cfg.h                                          *
 ** Author:                                                            *
 ** DATE:                                                           *
 ** Copyright:      2012 Spreadtrum, Incoporated. All Rights Reserved.        *
 ** Description:                                                              *
 ** Note:                                                      				  *
 *****************************************************************************/
/******************************************************************************
 **                   Edit    History                                         *
 **---------------------------------------------------------------------------* 
 ** DATE              NAME            DESCRIPTION                             * 
 *****************************************************************************/
#ifndef _DC_CFG_H_
#define _DC_CFG_H_

/**---------------------------------------------------------------------------*
 **                         Dependencies                                      *
 **---------------------------------------------------------------------------*/
/**---------------------------------------------------------------------------*
 **                         Macros                                            *
 **---------------------------------------------------------------------------*/
#define DCAM_CFG_DEBUG 1

#ifdef DCAM_CFG_DEBUG
#define DCAM_CFG_PRINT printk 
#else
#define DCAM_CFG_PRINT(...)
#endif
#define DCAM_CFG_ERR printk 

/**---------------------------------------------------------------------------*
 **                         Compiler Flag                                     *
 **---------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C"
{
#endif

#include <linux/types.h>

#include <mach/jpeg_exif_header_k.h>

//typedef uint8_t		BOOLEAN;

/**---------------------------------------------------------------------------*
 **                         Public Functions                                  *
 **---------------------------------------------------------------------------*/
JINF_EXIF_INFO_T* DC_InitExifParameter(JINF_EXIF_INFO_T *exif_info_ptr, uint32_t size);

JINF_EXIF_INFO_T* DC_GetExifParameter(void);

void DC_GetExifParameter_Post(void);

/**---------------------------------------------------------------------------*
 **                         Compiler Flag                                     *
 **---------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

#endif /* _DC_CFG_H_ */
