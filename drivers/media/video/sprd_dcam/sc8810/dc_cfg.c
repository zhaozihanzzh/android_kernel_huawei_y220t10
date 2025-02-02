/******************************************************************************
 ** File Name:      dc_cfg.c                                           *
 ** Author:         Tim.zhu                                             *
 ** DATE:           11/16/2009                                                *
 ** Copyright:      2009 Spreadtrum, Incoporated. All Rights Reserved.        *
 ** Description:    This file defines the product configure dc parameter    *
 **                                                                           *
 ******************************************************************************

 ******************************************************************************
 **                        Edit History                                       *
 ** ------------------------------------------------------------------------- *
 ** DATE           NAME             DESCRIPTION                               *
 ** 11/16/2009     Tim.zhu	  Create.                         		  *
 ******************************************************************************/

/**---------------------------------------------------------------------------*
 **                         Dependencies                                      *
 **---------------------------------------------------------------------------*/ 


/**---------------------------------------------------------------------------*
 **                         Debugging Flag                                    *
 **---------------------------------------------------------------------------*/
//#include <linux/time.h>

//#include "os_api.h"
//#include "dal_time.h"
#include <mach/dc_cfg.h>
#include <mach/dc_product_cfg.h>
#include <mach/sensor_drv.h>

//#include "dcam_common.h"



/**---------------------------------------------------------------------------*
 **                         Compiler Flag                                     *
 **---------------------------------------------------------------------------*/
#ifdef   __cplusplus
extern   "C" 
{
#endif
/**---------------------------------------------------------------------------*
 **                         Macro Definition                                  *
 **---------------------------------------------------------------------------*/

/**---------------------------------------------------------------------------*
 **                         Local Variables                                   *
 **---------------------------------------------------------------------------*/
 #if 0
LOCAL const  DCAMERA_SIZE_T s_dc_sensor_size[]={
    {176, 220},
    {220, 176},
    {240, 320},
    {320, 240},
    {352, 288},
    {240, 400},
    {400, 240},
    {320, 480},
    {480, 320},
    {480, 640},
    {640, 480},
    {720, 480},
    {720, 576},
    {800, 600},
    {1024, 768},
    {1280, 960},
    {1600, 1200},
    {2048, 1536},
    {2592, 1944},
    {3264, 2448}
};
#endif

 
LOCAL JINF_EXIF_INFO_T 				*s_dc_exif_info_ptr;

LOCAL EXIF_GPS_INFO_T 				*s_dc_gps_info_ptr;
LOCAL EXIF_SPEC_DATE_TIME_T 			*image_date_time_ptr;
LOCAL EXIF_SPECIFIC_INFO_T 			*s_dc_specific_info_ptr;
LOCAL EXIF_SPEC_OTHER_T 				*s_dc_spec_other_ptr;
LOCAL EXIF_PRI_DATA_STRUCT_T 			*exif_prim_data_ptr;

/* get from dc_product_cfg */
LOCAL EXIF_SPEC_USER_T                		*s_dc_spec_user_ptr; 
LOCAL EXIF_PRI_DESC_T                 		*s_dc_primary_img_desc_ptr;
/* get from sensor_drv */
LOCAL EXIF_SPEC_PIC_TAKING_COND_T     	*s_dc_spec_pic_taking_cond_ptr;

void *g_exif_info_end_ptr;
void *g_exif_info_start_ptr;

/**---------------------------------------------------------------------------*
 **                         Global Variables                                  *
 **---------------------------------------------------------------------------*/

/**---------------------------------------------------------------------------*
 **                     Public Function Prototypes                            *
 **---------------------------------------------------------------------------*/

/*****************************************************************************/
//  Description:  This function is used to get exif spec basic Param     
//  Author:         Tim.zhu
//  Note:           
/*****************************************************************************/
LOCAL void _DC_SetExifSpecificBasicParameter(void)
{
    EXIF_SPECIFIC_INFO_T* dc_specific_info_ptr=s_dc_specific_info_ptr;

    if(NULL == dc_specific_info_ptr)
		return;

    dc_specific_info_ptr->basic.ColorSpace=1;
    dc_specific_info_ptr->basic.ComponentsConfiguration[0]=1;
    dc_specific_info_ptr->basic.ComponentsConfiguration[1]=2;
    dc_specific_info_ptr->basic.ComponentsConfiguration[2]=3;
    dc_specific_info_ptr->basic.ComponentsConfiguration[3]=0;
}

/*****************************************************************************/
//  Description:  This function is used to get exif spec user Param     
//  Author:         Tim.zhu
//  Note:           
/*****************************************************************************/
LOCAL void _DC_SetExifSpecificUserParameter(void)
{
    DC_PRODUCT_CFG_FUNC_TAB_T_PTR exif_ptr=DC_GetDcProductCfgFun();
    EXIF_SPECIFIC_INFO_T* dc_specific_info_ptr=s_dc_specific_info_ptr;
    EXIF_SPEC_USER_T* product_user_ptr = NULL;

    if(NULL == dc_specific_info_ptr)
		return;

    if(PNULL!=exif_ptr->get_exifspecuser)
    {
	//dc_specific_info_ptr->user_ptr=(EXIF_SPEC_USER_T*)exif_ptr->get_exifspecuser(0x00);

	product_user_ptr = (EXIF_SPEC_USER_T*)exif_ptr->get_exifspecuser(0x00);
	dc_specific_info_ptr->user_ptr = s_dc_spec_user_ptr;
        memcpy(dc_specific_info_ptr->user_ptr, product_user_ptr, sizeof(EXIF_SPEC_USER_T));
    }
    else
    {
        dc_specific_info_ptr->user_ptr=NULL;
    }
}

/*****************************************************************************/
//  Description:  This function is used to get spec picture taking Param     
//  Author:         Tim.zhu
//  Note:           
/*****************************************************************************/
LOCAL void _DC_SetExifSpecificPicTakingParameter(void)
{
    EXIF_SPEC_PIC_TAKING_COND_T* img_sensor_exif_ptr= Sensor_GetSensorExifInfo();
    EXIF_SPECIFIC_INFO_T* dc_specific_info_ptr=s_dc_specific_info_ptr;

    if(NULL == dc_specific_info_ptr)
		return;

    if(NULL != img_sensor_exif_ptr)
    {
	    //dc_specific_info_ptr->pic_taking_cond_ptr = img_sensor_exif_ptr;
	    dc_specific_info_ptr->pic_taking_cond_ptr = s_dc_spec_pic_taking_cond_ptr;
	    memcpy(dc_specific_info_ptr->pic_taking_cond_ptr, img_sensor_exif_ptr, sizeof(EXIF_SPEC_PIC_TAKING_COND_T));
    }
    else
    {
		dc_specific_info_ptr->pic_taking_cond_ptr = NULL;
    }
}

/*****************************************************************************/
//  Description:  This function is used to get exif primary Param     
//  Author:         Tim.zhu
//  Note:           
/*****************************************************************************/
LOCAL void _DC_SetExifPrimaryParameter(void)
{
    JINF_EXIF_INFO_T* dc_exif_info_ptr=(JINF_EXIF_INFO_T* )s_dc_exif_info_ptr;
    DC_PRODUCT_CFG_FUNC_TAB_T_PTR exif_ptr=DC_GetDcProductCfgFun();
    EXIF_PRI_DESC_T                 *product_img_desc_ptr;

    dc_exif_info_ptr->primary.basic.YCbCrPositioning=1;
    dc_exif_info_ptr->primary.basic.XResolution.numerator=72;
    dc_exif_info_ptr->primary.basic.XResolution.denominator=1;
    dc_exif_info_ptr->primary.basic.YResolution.numerator=72;
    dc_exif_info_ptr->primary.basic.YResolution.denominator=1;
    dc_exif_info_ptr->primary.basic.ResolutionUnit=2;
    exif_prim_data_ptr->valid.Orientation = 1;
    exif_prim_data_ptr->Orientation = 1;
    dc_exif_info_ptr->primary.data_struct_ptr = exif_prim_data_ptr;

    if(PNULL!=exif_ptr->get_exifprimarypridesc)
    {
	//dc_exif_info_ptr->primary.img_desc_ptr=(EXIF_PRI_DESC_T*)exif_ptr->get_exifprimarypridesc(0x00);
	product_img_desc_ptr = (EXIF_PRI_DESC_T*)exif_ptr->get_exifprimarypridesc(0x00);
	dc_exif_info_ptr->primary.img_desc_ptr = s_dc_primary_img_desc_ptr;
        
        memcpy(dc_exif_info_ptr->primary.img_desc_ptr, product_img_desc_ptr, sizeof(EXIF_PRI_DESC_T));
    }
    else
    {
        dc_exif_info_ptr->primary.img_desc_ptr=NULL;
    }
}

/*****************************************************************************/
//  Description:  This function is used to get exif spec Param     
//  Author:         Tim.zhu
//  Note:           
/*****************************************************************************/
LOCAL void _DC_SetExifSpecificParameter(void)
{
    JINF_EXIF_INFO_T* dc_exif_info_ptr=(JINF_EXIF_INFO_T* )s_dc_exif_info_ptr;

#ifdef KERNEL_TIME
    SCI_DATE_T       cur_date = {0};
    SCI_TIME_T       cur_time = {0};

    TM_GetSysDate(&cur_date);
    TM_GetSysTime(&cur_time);
#endif

    dc_exif_info_ptr->spec_ptr=s_dc_specific_info_ptr;

    DCAM_CFG_PRINT("DC_CFG:  spec_ptr = %x, s_dc_specific_info_ptr=%x \n", (uint32_t)dc_exif_info_ptr->spec_ptr,  (uint32_t)s_dc_specific_info_ptr);

    if(NULL == dc_exif_info_ptr->spec_ptr)
		return;

    _DC_SetExifSpecificBasicParameter();
    _DC_SetExifSpecificUserParameter();
    _DC_SetExifSpecificPicTakingParameter();
	
    s_dc_spec_other_ptr->valid.ImageUniqueID = 1;
#ifdef KERNEL_TIME
    sprintf(s_dc_spec_other_ptr->ImageUniqueID,
                "Spreadtrum IMAGE %04d:%02d:%02d %02d:%02d:%02d",            
                cur_date.year, 
                cur_date.mon, 
                cur_date.mday,
                cur_time.hour, 
                cur_time.min,
                cur_time.sec);
#endif

    DCAM_CFG_PRINT("DC_CFG: _DC_SetExifSpecificParameter %s \n",s_dc_spec_other_ptr->ImageUniqueID);    
	
    dc_exif_info_ptr->spec_ptr->other_ptr = s_dc_spec_other_ptr;
}

/*****************************************************************************/
//  Description:  This function is used to get gps Param     
//  Author:         Tim.zhu
//  Note:           
/*****************************************************************************/
LOCAL void _DC_SetExifGpsParameter(void)
{
    JINF_EXIF_INFO_T* dc_exif_info_ptr=(JINF_EXIF_INFO_T* )s_dc_exif_info_ptr;

    EXIF_GPS_INFO_T* dc_gps_info_ptr=s_dc_gps_info_ptr;
    if(NULL == dc_gps_info_ptr)
		return;

    //memset(&s_dc_gps_info_ptr, 0, sizeof(EXIF_GPS_INFO_T)); // already set to 0 when DC_InitExifParameter

#if 0
    *(uint32*)&dc_gps_info_ptr->valid = (uint32)0x7F;

    dc_gps_info_ptr->GPSVersionID[0] = 2;
    dc_gps_info_ptr->GPSVersionID[1] = 2;
    dc_gps_info_ptr->GPSVersionID[2] = 0;
    dc_gps_info_ptr->GPSVersionID[3] = 0;    
    //dc_gps_info_ptr->valid = (uint32)0x7F;
    dc_gps_info_ptr->GPSLatitudeRef[0] = 'N';
    dc_gps_info_ptr->GPSLatitude[0].numerator = 31;
    dc_gps_info_ptr->GPSLatitude[0].denominator = 1;    
    dc_gps_info_ptr->GPSLatitude[1].numerator = 2;
    dc_gps_info_ptr->GPSLatitude[1].denominator = 1;    
    dc_gps_info_ptr->GPSLatitude[2].numerator = 0;
    dc_gps_info_ptr->GPSLatitude[2].denominator = 1;    

    dc_gps_info_ptr->GPSLongitudeRef[0] = 'E';
    dc_gps_info_ptr->GPSLongitude[0].numerator = 121;
    dc_gps_info_ptr->GPSLongitude[0].denominator = 1;    
    dc_gps_info_ptr->GPSLongitude[1].numerator = 4;
    dc_gps_info_ptr->GPSLongitude[1].denominator = 1;    
    dc_gps_info_ptr->GPSLongitude[2].numerator = 0;
    dc_gps_info_ptr->GPSLongitude[2].denominator = 1;    

    dc_gps_info_ptr->GPSAltitudeRef = 0;
    dc_gps_info_ptr->GPSAltitude.numerator = 4;
    dc_gps_info_ptr->GPSAltitude.denominator = 1;
  #endif
    dc_exif_info_ptr->gps_ptr=s_dc_gps_info_ptr;

    DCAM_CFG_PRINT("DC_CFG:  gps_ptr = %x, s_dc_gps_info_ptr=%x \n", (uint32_t)dc_exif_info_ptr->gps_ptr,  (uint32_t)s_dc_gps_info_ptr);
    
}

PUBLIC void DC_SetExifImagePixel(uint32 width, uint32 height)
{
    EXIF_SPECIFIC_INFO_T* dc_specific_info_ptr=s_dc_specific_info_ptr;

    if(NULL == dc_specific_info_ptr)
		return;

    dc_specific_info_ptr->basic.PixelXDimension = width;
    dc_specific_info_ptr->basic.PixelYDimension = height;    
}

PUBLIC void DC_SetExifImageDataTime(void)
{
    EXIF_SPECIFIC_INFO_T *dc_specific_info_ptr=s_dc_specific_info_ptr;
    EXIF_SPEC_DATE_TIME_T *dc_image_data_time_ptr=image_date_time_ptr;


#ifdef KERNEL_TIME
    SCI_DATE_T                   cur_date = {0};
    SCI_TIME_T                    cur_time = {0};
#endif

    if((NULL == dc_specific_info_ptr) || (NULL == dc_image_data_time_ptr))
		return;
	
#ifdef KERNEL_TIME
    TM_GetSysDate(&cur_date);
    TM_GetSysTime(&cur_time);
    
    sprintf((int8 *)dc_image_data_time_ptr->DateTimeOriginal, 
                "%04d:%02d:%02d %02d:%02d:%02d", 
                cur_date.year, 
                cur_date.mon, 
                cur_date.mday,
                cur_time.hour, 
                cur_time.min,
                cur_time.sec);

    strcpy(dc_image_data_time_ptr->DateTimeDigitized, dc_image_data_time_ptr->DateTimeOriginal);
#endif

    dc_image_data_time_ptr->valid.DateTimeOriginal = 1;
    dc_image_data_time_ptr->valid.DateTimeDigitized = 1;

    DCAM_CFG_PRINT("DC_CFG: DC_SetExifImageDataTime %s \n",dc_image_data_time_ptr->DateTimeOriginal);    

    dc_specific_info_ptr->date_time_ptr = dc_image_data_time_ptr;
}

/*****************************************************************************/
//  Description:  This function is used to init exif Param, it will put the extra data 
//                       behind the exif_info_ptr.
//  Author:         
//  Note:           
/*****************************************************************************/
PUBLIC JINF_EXIF_INFO_T* DC_InitExifParameter(JINF_EXIF_INFO_T *exif_info_ptr, uint32_t size)
{
	uint32_t used_size = 0;

	s_dc_exif_info_ptr 				= NULL;

	s_dc_exif_info_ptr 				= NULL;
	s_dc_gps_info_ptr 				= NULL;
	image_date_time_ptr 				= NULL;
	s_dc_specific_info_ptr 			= NULL;
	exif_prim_data_ptr 				= NULL;

	s_dc_spec_user_ptr				= NULL;
	s_dc_primary_img_desc_ptr		= NULL;
	s_dc_spec_pic_taking_cond_ptr 	= NULL;

	if(size < sizeof(JINF_EXIF_INFO_T))
	{
		s_dc_exif_info_ptr = NULL;
		DCAM_CFG_PRINT("DC_CFG: DC_InitExifParameter Error: size=%x, sizeof=%x \n", size, sizeof(JINF_EXIF_INFO_T));    
	}
	else
	{
		DC_PRODUCT_CFG_FUNC_TAB_T_PTR exif_ptr=DC_GetDcProductCfgFun();
		EXIF_SPEC_PIC_TAKING_COND_T* img_sensor_exif_ptr=Sensor_GetSensorExifInfo();
		
		g_exif_info_end_ptr = exif_info_ptr;
		g_exif_info_start_ptr = exif_info_ptr;

		s_dc_exif_info_ptr = exif_info_ptr;		
		used_size = sizeof(JINF_EXIF_INFO_T);
		g_exif_info_end_ptr += used_size;
		DCAM_CFG_PRINT("DC_CFG:  s_dc_exif_info_ptr = %x, size=%x,  g_exif_info_end_ptr=%x \n", (uint32_t)s_dc_exif_info_ptr, used_size, (uint32_t)g_exif_info_end_ptr);

		s_dc_gps_info_ptr = g_exif_info_end_ptr;
		used_size = sizeof(EXIF_GPS_INFO_T);
		g_exif_info_end_ptr += used_size;
		DCAM_CFG_PRINT("DC_CFG:  s_dc_gps_info_ptr = %x, size=%x, g_exif_info_end_ptr=%x \n", (uint32_t)s_dc_gps_info_ptr, used_size, (uint32_t)g_exif_info_end_ptr);

		image_date_time_ptr = g_exif_info_end_ptr;
		used_size = sizeof(EXIF_SPEC_DATE_TIME_T);
		g_exif_info_end_ptr += used_size;
		DCAM_CFG_PRINT("DC_CFG:  image_date_time_ptr = %x, size=%x, g_exif_info_end_ptr=%x \n", (uint32_t)image_date_time_ptr, used_size, (uint32_t)g_exif_info_end_ptr);

		s_dc_specific_info_ptr = g_exif_info_end_ptr;
		used_size = sizeof(EXIF_SPECIFIC_INFO_T);
		g_exif_info_end_ptr += used_size;
		DCAM_CFG_PRINT("DC_CFG:  s_dc_specific_info_ptr = %x, size=%x, g_exif_info_end_ptr=%x \n", (uint32_t)s_dc_specific_info_ptr, used_size, (uint32_t)g_exif_info_end_ptr);

		s_dc_spec_other_ptr = g_exif_info_end_ptr;
		used_size = sizeof(EXIF_SPEC_OTHER_T);
		g_exif_info_end_ptr += used_size;
		DCAM_CFG_PRINT("DC_CFG:  s_dc_spec_other_ptr = %x, size=%x, g_exif_info_end_ptr=%x \n", (uint32_t)s_dc_spec_other_ptr, used_size, (uint32_t)g_exif_info_end_ptr);

		exif_prim_data_ptr = g_exif_info_end_ptr;
		used_size = sizeof(EXIF_PRI_DATA_STRUCT_T);
		g_exif_info_end_ptr += used_size;
		DCAM_CFG_PRINT("DC_CFG:  exif_prim_data_ptr = %x, size=%x, g_exif_info_end_ptr=%x \n", (uint32_t)exif_prim_data_ptr, used_size, (uint32_t)g_exif_info_end_ptr);



		/* get from dc_product_cfg */
		if(PNULL!=exif_ptr->get_exifspecuser)
		{
			s_dc_spec_user_ptr = g_exif_info_end_ptr;
			used_size = sizeof(EXIF_SPEC_USER_T);
			g_exif_info_end_ptr += used_size;

			DCAM_CFG_PRINT("DC_CFG:  s_dc_spec_user_ptr = %x, size=%x, g_exif_info_end_ptr=%x \n", (uint32_t)s_dc_spec_user_ptr, used_size, (uint32_t)g_exif_info_end_ptr);
		}

		if(PNULL!=exif_ptr->get_exifprimarypridesc)
		{
			s_dc_primary_img_desc_ptr = g_exif_info_end_ptr;
			used_size = sizeof(EXIF_PRI_DESC_T);
			g_exif_info_end_ptr += used_size;
			
			DCAM_CFG_PRINT("DC_CFG:  s_dc_primary_img_desc_ptr = %x, size=%x, g_exif_info_end_ptr=%x \n", (uint32_t)s_dc_primary_img_desc_ptr, used_size, (uint32_t)g_exif_info_end_ptr);
		}

		/* get from sensor_drv */
		if(PNULL!=img_sensor_exif_ptr)
		{
			s_dc_spec_pic_taking_cond_ptr = g_exif_info_end_ptr;
			used_size = sizeof(EXIF_SPEC_PIC_TAKING_COND_T);
			g_exif_info_end_ptr += used_size;
			DCAM_CFG_PRINT("DC_CFG:  s_dc_spec_pic_taking_cond_ptr = %x, size=%x, g_exif_info_end_ptr=%x \n", (uint32_t)s_dc_spec_pic_taking_cond_ptr, used_size, (uint32_t)g_exif_info_end_ptr);
		}

		memset((uint32_t *)g_exif_info_start_ptr, 0, g_exif_info_end_ptr-g_exif_info_start_ptr);	

		if(size  < g_exif_info_end_ptr - g_exif_info_start_ptr)
		{
			s_dc_exif_info_ptr 				= NULL;
			
			s_dc_exif_info_ptr 				= NULL;
			s_dc_gps_info_ptr 				= NULL;
			image_date_time_ptr 				= NULL;
			s_dc_specific_info_ptr 			= NULL;
			exif_prim_data_ptr 				= NULL;

			s_dc_spec_user_ptr				= NULL;
			s_dc_primary_img_desc_ptr		= NULL;
			s_dc_spec_pic_taking_cond_ptr 	= NULL;
			
			DCAM_CFG_PRINT("DC_CFG: DC_InitExifParameter Error: size=%x < used size=%x \n", size, used_size);    
		}
		DCAM_CFG_PRINT("DC_CFG: DC_InitExifParameter: size=%x, used size=%x \n", size, g_exif_info_end_ptr-g_exif_info_start_ptr);    
		
	}

	return s_dc_exif_info_ptr;
}

/*****************************************************************************/
//  Description:  This function is change the ptr to relative address.
//  Author:         
//  Note:           
/*****************************************************************************/
PUBLIC void DC_GetExifParameter_Post(void)
{
	JINF_EXIF_INFO_T* dc_exif_info_ptr=(JINF_EXIF_INFO_T* )s_dc_exif_info_ptr;

	if(NULL == s_dc_exif_info_ptr)
	{
		DCAM_CFG_PRINT("DC_CFG: DC_GetExifParameter_Post error,  s_dc_exif_info_ptr = NULL \n");    
		return NULL;
	}


	DCAM_CFG_PRINT("DC_CFG: before: (%x), %x, %x, %x %x \n", 
					(uint32_t)g_exif_info_start_ptr, (uint32_t)dc_exif_info_ptr->gps_ptr, (uint32_t)dc_exif_info_ptr->primary.data_struct_ptr,
					(uint32_t)dc_exif_info_ptr->spec_ptr, (uint32_t)dc_exif_info_ptr->spec_ptr->other_ptr);
	DCAM_CFG_PRINT("DC_CFG: before: (%x), %x, %x, %x %x \n", 
					(uint32_t)g_exif_info_start_ptr, (uint32_t)dc_exif_info_ptr->primary.img_desc_ptr, (uint32_t)dc_exif_info_ptr->spec_ptr->date_time_ptr,
					(uint32_t)dc_exif_info_ptr->spec_ptr->user_ptr, (uint32_t)dc_exif_info_ptr->spec_ptr->pic_taking_cond_ptr);

	if(NULL != dc_exif_info_ptr->gps_ptr)
	{
		dc_exif_info_ptr->gps_ptr 		= (void *)((uint32_t)dc_exif_info_ptr->gps_ptr  - (uint32_t)g_exif_info_start_ptr);
	}

	if(NULL != dc_exif_info_ptr->primary.data_struct_ptr)
	{
		dc_exif_info_ptr->primary.data_struct_ptr		= (void *)((uint32_t)dc_exif_info_ptr->primary.data_struct_ptr - (uint32_t)g_exif_info_start_ptr);
	}

	if(NULL != dc_exif_info_ptr->spec_ptr->other_ptr)
	{
		dc_exif_info_ptr->spec_ptr->other_ptr 		= (void *)((uint32_t)dc_exif_info_ptr->spec_ptr->other_ptr - (uint32_t)g_exif_info_start_ptr);
	}

	if(NULL != dc_exif_info_ptr->primary.img_desc_ptr)
	{
		dc_exif_info_ptr->primary.img_desc_ptr		= (void *)((uint32_t)dc_exif_info_ptr->primary.img_desc_ptr - (uint32_t)g_exif_info_start_ptr);
	}

	if(NULL != dc_exif_info_ptr->spec_ptr->date_time_ptr)
	{
		dc_exif_info_ptr->spec_ptr->date_time_ptr 		= (void *)((uint32_t)dc_exif_info_ptr->spec_ptr->date_time_ptr - (uint32_t)g_exif_info_start_ptr);
	}

	if(NULL != dc_exif_info_ptr->spec_ptr->user_ptr)
	{
		dc_exif_info_ptr->spec_ptr->user_ptr 		= (void *)((uint32_t)dc_exif_info_ptr->spec_ptr->user_ptr - (uint32_t)g_exif_info_start_ptr);
	}

	if(NULL != dc_exif_info_ptr->spec_ptr->pic_taking_cond_ptr)
	{
		dc_exif_info_ptr->spec_ptr->pic_taking_cond_ptr 		= (void *)((uint32_t)dc_exif_info_ptr->spec_ptr->pic_taking_cond_ptr - (uint32_t)g_exif_info_start_ptr);
	}

	DCAM_CFG_PRINT("DC_CFG: after: (%x), %x, %x, %x %x \n", 
					(uint32_t)g_exif_info_start_ptr, (uint32_t)dc_exif_info_ptr->gps_ptr, (uint32_t)dc_exif_info_ptr->primary.data_struct_ptr,
					(uint32_t)dc_exif_info_ptr->spec_ptr, (uint32_t)dc_exif_info_ptr->spec_ptr->other_ptr);
	DCAM_CFG_PRINT("DC_CFG: before: (%x), %x, %x, %x %x \n", 
					(uint32_t)g_exif_info_start_ptr, (uint32_t)dc_exif_info_ptr->primary.img_desc_ptr, (uint32_t)dc_exif_info_ptr->spec_ptr->date_time_ptr,
					(uint32_t)dc_exif_info_ptr->spec_ptr->user_ptr, (uint32_t)dc_exif_info_ptr->spec_ptr->pic_taking_cond_ptr);

	if(NULL != dc_exif_info_ptr->spec_ptr)
	{
		dc_exif_info_ptr->spec_ptr 		= (void *)((uint32_t)dc_exif_info_ptr->spec_ptr - (uint32_t)g_exif_info_start_ptr);
	}


}

/*****************************************************************************/
//  Description:  This function is used to get exif Param    
//  Author:         Tim.zhu
//  Note:           
/*****************************************************************************/
PUBLIC JINF_EXIF_INFO_T* DC_GetExifParameter(void)
{

    if(NULL == s_dc_exif_info_ptr)
    {
		DCAM_CFG_PRINT("DC_CFG: DC_GetExifParameter error,  s_dc_exif_info_ptr = NULL \n");    
		return NULL;
    }
	
    _DC_SetExifPrimaryParameter();
    _DC_SetExifSpecificParameter();
    _DC_SetExifGpsParameter();
	
    DC_SetExifImageDataTime();
    
    DCAM_CFG_PRINT("DC_CFG: DC_GetExifParameter \n");    
    return (JINF_EXIF_INFO_T* )s_dc_exif_info_ptr;
}

#if 0
/******************************************************************************
// Description: get the size struct  pointer of specific type
// Author:      Tim.zhu
// Input:       uint32 type: img size
// Output:      none
// Return:      sensor size struct pointer
******************************************************************************/ 
PUBLIC DCAMERA_SIZE_T* DC_GetSensorSizeStPtr(uint32 type)
{
    return (DCAMERA_SIZE_T *)(&s_dc_sensor_size[type]);
}

/*****************************************************************************/
//  Description:  This function is used to get dc Param    
//  Author:         Tim.zhu
//  Note:           
/*****************************************************************************/
PUBLIC DC_PRODUCT_CFG_T_PTR DC_GeProductCfgPtr(void)
{
    DC_PRODUCT_CFG_FUNC_TAB_T_PTR dc_productcfgfunptr=DC_GetDcProductCfgFun();
    DC_PRODUCT_CFG_T_PTR dc_productcfgptr=NULL;

    if(PNULL!=dc_productcfgfunptr->get_productcfg)
    {
        dc_productcfgptr=(DC_PRODUCT_CFG_T_PTR)dc_productcfgfunptr->get_productcfg(0x00);
    }
    
    return dc_productcfgptr;
}
#endif

/**---------------------------------------------------------------------------*
 **                         Compiler Flag                                     *
 **---------------------------------------------------------------------------*/
#ifdef   __cplusplus
}
#endif
