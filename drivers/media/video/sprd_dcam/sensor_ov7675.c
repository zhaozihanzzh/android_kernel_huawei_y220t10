/******************************************************************************
 ** Copyright (c) 
 ** File Name:		sensor_OV7675.c 										  *
 ** Author: 													  *
 ** DATE:															  *
 ** Description:   This file contains driver for sensor OV7675. 
 ** 														 
 ******************************************************************************

 ******************************************************************************
 ** 					   Edit History 									  *
 ** ------------------------------------------------------------------------- *
 ** DATE		   NAME 			DESCRIPTION 							  *
 ** 	  
 ******************************************************************************/

/**---------------------------------------------------------------------------*
 ** 						Dependencies									  *
 **---------------------------------------------------------------------------*/
#include "sensor.h"
#include <linux/delay.h>
/*lint -save -e661 */	
/**---------------------------------------------------------------------------*
 ** 						Compiler Flag									  *
 **---------------------------------------------------------------------------*/
#ifdef	 __cplusplus
	extern	 "C" 
	{
#endif
/**---------------------------------------------------------------------------*
 ** 					Extern Function Declaration 						  *
 **---------------------------------------------------------------------------*/

/**---------------------------------------------------------------------------*
 ** 						Const variables 								  *
 **---------------------------------------------------------------------------*/

/**---------------------------------------------------------------------------*
 ** 						   Macro Define
 **---------------------------------------------------------------------------*/
 #define SENSOR_TRACE SENSOR_PRINT
#define OV7675_I2C_ADDR_W		0x21//0x42
#define OV7675_I2C_ADDR_R		0x21//0x43

#define SENSOR_GAIN_SCALE		16

#define OV7675_COM11 0x3b
#define OV7675_REGCF 0xcf
#define PLL_ADDR    0x11
#define AE_ENABLE 0x13


 
/**---------------------------------------------------------------------------*
 ** 					Local Function Prototypes							  *
 **---------------------------------------------------------------------------*/
//LOCAL uint32_t set_ov7675_ae_awb_enable(uint32_t ae_enable, uint32_t awb_enable);
LOCAL uint32_t set_ov7675_ae_enable(uint32_t enable);
LOCAL uint32_t set_hmirror_enable(uint32_t enable);
LOCAL uint32_t set_vmirror_enable(uint32_t enable);
LOCAL uint32_t set_preview_mode(uint32_t preview_mode);
LOCAL uint32_t OV7675_Identify(uint32_t param);
//LOCAL uint32_t OV7675_BeforeSnapshot(uint32_t param);
//LOCAL uint32_t OV7675_After_Snapshot(uint32_t param);

LOCAL uint32_t set_brightness(uint32_t level);
LOCAL uint32_t set_contrast(uint32_t level);
LOCAL uint32_t set_sharpness(uint32_t level);
LOCAL uint32_t set_saturation(uint32_t level);
LOCAL uint32_t set_image_effect(uint32_t effect_type);

LOCAL uint32_t read_ev_value(uint32_t value);
LOCAL uint32_t write_ev_value(uint32_t exposure_value);
LOCAL uint32_t read_gain_value(uint32_t value);
LOCAL uint32_t write_gain_value(uint32_t gain_value);
LOCAL uint32_t read_gain_scale(uint32_t value);
LOCAL uint32_t set_frame_rate(uint32_t param);
LOCAL uint32_t OV7675_set_work_mode(uint32_t mode);

LOCAL uint32_t set_ov7675_ev(uint32_t level);
LOCAL uint32_t set_ov7675_awb(uint32_t mode);
LOCAL uint32_t set_ov7675_anti_flicker(uint32_t mode);
LOCAL uint32_t set_ov7675_video_mode(uint32_t mode);
//LOCAL uint32_t _ov7675_Power_On(uint32_t power_on);
LOCAL uint32_t _ov7675_GetExifInfo(uint32_t param);
LOCAL uint32_t _ov7675_InitExifInfo(void);

LOCAL uint32_t s_preview_mode;

//LOCAL EXIF_SPEC_PIC_TAKING_COND_T s_ov7675_exif = {0};


/**---------------------------------------------------------------------------*
 ** 						Local Variables 								 *
 **---------------------------------------------------------------------------*/
 typedef enum
{
	FLICKER_50HZ = 0,
	FLICKER_60HZ,
	FLICKER_MAX
}FLICKER_E;
/*lint -save -e533 */
 LOCAL const SENSOR_REG_T ov7675_YUV_640X480[]=
{
    //Input clock 24Mhz, 25fps                     
    //SCCB_salve_Address = 0x42;                   
    {0x12, 0x80},                                  
    {SENSOR_WRITE_DELAY, 0x20},//delay 100ms     
    {0x11, 0x80}, 
       
    {0x3a, 0x04},                                  
    {0x12, 0x00},                                  
    {0x17, 0x13},                                  
    {0x18, 0x01},                                  
    {0x32, 0xb6},                                  
    {0x19, 0x03},                                  
    {0x1a, 0x7b},                                  
    {0x03, 0x0a},                                  
    {0x0c, 0x00},                                  
    {0x3e, 0x00},                                  
    {0x70, 0x3a},                                  
    {0x71, 0x35},                                  
    {0x72, 0x11},                                  
    {0x73, 0xf0},                                  
    {0xa2, 0x02},     
    
    {0x7a, 0x18},
    {0x7b, 0x04},
    {0x7c, 0x09},
    {0x7d, 0x18},
    {0x7e, 0x38},
    {0x7f, 0x47},
    {0x80, 0x56},
    {0x81, 0x66},
    {0x82, 0x74},
    {0x83, 0x7f},
    {0x84, 0x89},
    {0x85, 0x9a},
    {0x86, 0xA9},
    {0x87, 0xC4},
    {0x88, 0xDb},
    {0x89, 0xEe},  
    
    {0x13, 0xe0},                                
    {0x01, 0x50},                                  
    {0x02, 0x68},                                  
    {0x00, 0x00},                                  
    {0x10, 0x00},                                  
    {0x0d, 0x40},                                  
    {0x14, 0x38},                                  
    {0xa5, 0x07},                                  
    {0xab, 0x08},                                  
    {0x24, 0x60},    
    {0x25, 0x50},                           
    {0x26, 0xe3},                                
    {0x9f, 0x78},                                  
    {0xa0, 0x68},  
    
                                    
    {0xa1, 0x03},                                
    {0xa6, 0xd8},                                  
    {0xa7, 0xd8},                                  
    {0xa8, 0xf0},                                  
    {0xa9, 0x90},                                  
    {0xaa, 0x14},                                  
    {0x13, 0xe5},                                  
    {0x0e, 0x61},                                
    {0x0f, 0x4b},                                  
    {0x16, 0x02},                                  
    {0x1e, 0x17},
    {0x21, 0x02},                                  
    {0x22, 0x91},                                  
    {0x29, 0x07},                                  
    {0x33, 0x0b},                                  
    {0x35, 0x0b},                                  
    {0x37, 0x1d},                                  
    {0x38, 0x71},                                  
    {0x39, 0x2a},                                  
    {0x3c, 0x78},                                  
    {0x4d, 0x40},                                  
    {0x4e, 0x20},                                  
    {0x69, 0x00},                                  
    {0x6b, 0x0a},                                  
    {0x74, 0x10},                                  
    {0x8d, 0x4f},                                  
    {0x8e, 0x00},                                  
    {0x8f, 0x00},                                  
    {0x90, 0x00},                                  
    {0x91, 0x00},                                  
    {0x92, 0x66},                                  
    {0x96, 0x00},                                  
    {0x9a, 0x80},                                  
    {0xb0, 0x84},                                  
    {0xb1, 0x0c},                                  
    {0xb2, 0x0e},                                  
    {0xb3, 0x82},                                  
    {0xb8, 0x0a}, 
    
    {0x43, 0x14},                                
    {0x44, 0xf0},                                  
    {0x45, 0x41},                                  
    {0x46, 0x66},                                  
    {0x47, 0x2a},                                  
    {0x48, 0x3e},                                  
    {0x59, 0x8d},                                  
    {0x5a, 0x8e},                                  
    {0x5b, 0x53},                                  
    {0x5c, 0x83},                                  
    {0x5d, 0x4f},                                  
    {0x5e, 0x0e},                                  
    {0x6c, 0x0a},                                  
    {0x6d, 0x55},                                  
    {0x6e, 0x11},                                  
    {0x6f, 0x9e}, 
    {0x62, 0x90},                                
    {0x63, 0x30},
    {0x64, 0x11},
    {0x65, 0x00},                                  
    {0x66, 0x05},  
    {0x94, 0x11},    
    {0x95, 0x18},  //18 
    {0x6a, 0x40},                                
    {0x01, 0x40},                                  
    {0x02, 0x40}, 
    
    {0x13, 0xe7},                                  
    {0x4f, 0x80},                                
    {0x50, 0x80},                                  
    {0x51, 0x00},                                  
    {0x52, 0x22},                                  
    {0x53, 0x5e},                                  
    {0x54, 0x80},                                  
    {0x58, 0x9e},                                  
    {0x41, 0x08},                                
    {0x3f, 0x00},                                  
    {0x75, 0x03},                                  
    {0x76, 0xe1},                                  
    {0x4c, 0x00},                                  
    {0x77, 0x00},                                  
    {0x3d, 0xc2},                                  
    {0x4b, 0x09},                                  
    {0xc9, 0x60},                                  
    {0x41, 0x38},                                  
    {0x56, 0x40},                                  
    {0x34, 0x11},                                
    {0x3b, 0x0a},                                  
    {0xa4, 0x88},                                  
    {0x96, 0x00},                                  
    {0x97, 0x30},                                  
    {0x98, 0x20},                                  
    {0x99, 0x30},                                  
    {0x9a, 0x84},                                  
    {0x9b, 0x29},                                  
    {0x9c, 0x03},                                  
    {0x9d, 0x98},                                
    {0x9e, 0x3f},                                  
    {0x78, 0x04},                                  
    {0x79, 0x01},                                
    {0xc8, 0xf0},                                  
    {0x79, 0x0f},                                  
    {0xc8, 0x00},                                  
    {0x79, 0x10},                                  
    {0xc8, 0x7e},                                  
    {0x79, 0x0a},                                  
    {0xc8, 0x80},                                  
    {0x79, 0x0b},                                  
    {0xc8, 0x01},                                  
    {0x79, 0x0c},                                  
    {0xc8, 0x0f},                                  
    {0x79, 0x0d},                                  
    {0xc8, 0x20},                                  
    {0x79, 0x09},                                  
    {0xc8, 0x80},                                  
    {0x79, 0x02},                                  
    {0xc8, 0xc0},                                  
    {0x79, 0x03},                                  
    {0xc8, 0x40},                                  
    {0x79, 0x05},                                  
    {0xc8, 0x30},                                  
    {0x79, 0x26},                                  
    {0x2d, 0x00},                                  
    {0x2e, 0x00}, 
    //wxz20110527: config the driver capabitlity to 1x.
    {0x09, 0x00},
    {SENSOR_WRITE_DELAY, 100},//delay 100ms
};

 LOCAL const SENSOR_REG_T ov7675_YUV_MOTION_320X240[]=
{
    {0x12, 0x80},
    {0x09, 0x10},
    {SENSOR_WRITE_DELAY, 0x20},//delay 100ms
    {0x11, 0x00},	//0x80},
    {0x3a, 0x0c},
    {0x3d, 0xc0},
    {0x12, 0x00},
    {0x15, 0x40},
    {0x17, 0x13},
    {0x18, 0x01},
    {0x32, 0xbf},
    {0x19, 0x03},   //0x02
    {0x1a, 0x7b},   //0x7a
    {0x03, 0x0a},
    {0x0c, 0x00},
    {0x3e, 0x00},
    {0x70, 0x3a},
    {0x71, 0x35},
    {0x72, 0x11},
    {0x73, 0xf0},
    {0xa2, 0x02},
    {0x7a, 0x20},
    {0x7b, 0x03},
    {0x7c, 0x0a},
    {0x7d, 0x1a},
    {0x7e, 0x3f},
    {0x7f, 0x4e},
    {0x80, 0x5b},
    {0x81, 0x68},
    {0x82, 0x75},
    {0x83, 0x7f},
    {0x84, 0x89},
    {0x85, 0x9a},
    {0x86, 0xa6},
    {0x87, 0xbd},
    {0x88, 0xd3},
    {0x89, 0xe8},
    {0x13, 0xe0},
    {0x00, 0x00},
    {0x10, 0x00},
    {0x0d, 0x40},
    {0x14, 0x38},
    {0xa5, 0x02},
    {0xab, 0x02},
    {0x24, 0x68},
    {0x25, 0x58},
    {0x26, 0xc2},
    {0x9f, 0x78},
    {0xa0, 0x68},
    {0xa1, 0x03},
    {0xa6, 0xd8},
    {0xa7, 0xd8},
    {0xa8, 0xf0},
    {0xa9, 0x90},
    {0xaa, 0x14},
    {0x13, 0xe5},
    {0x0e, 0x61},
    {0x0f, 0x4b},
    {0x16, 0x02},
    {0x1e, 0x17},
    {0x21, 0x02},
    {0x22, 0x91},
    {0x29, 0x07},
    {0x33, 0x0b},
    {0x35, 0x0b},
    {0x37, 0x1d},
    {0x38, 0x71},
    {0x39, 0x2a},
    {0x3c, 0x78},
    {0x4d, 0x40},
    {0x4e, 0x20},
    {0x69, 0x00},
    {0x6b, 0x0a},
    {0x74, 0x10},
    {0x8d, 0x4f},
    {0x8e, 0x00},
    {0x8f, 0x00},
    {0x90, 0x00},
    {0x91, 0x00},
    {0x96, 0x00},
    {0x9a, 0x80},
    {0xb0, 0x84},
    {0xb1, 0x0c},
    {0xb2, 0x0e},
    {0xb3, 0x82},
    {0xb8, 0x0a},
    {0x43, 0x0a},
    {0x44, 0xf2},
    {0x45, 0x39},
    {0x46, 0x62},
    {0x47, 0x3d},
    {0x48, 0x55},
    {0x59, 0x83},
    {0x5a, 0x0d},
    {0x5b, 0xcd},
    {0x5c, 0x8c},
    {0x5d, 0x77},
    {0x5e, 0x16},
    {0x6c, 0x0a},
    {0x6d, 0x65},
    {0x6e, 0x11},
    {0x6f, 0x9e},
    {0x6a, 0x40},
    {0x01, 0x56},
    {0x02, 0x44},
    {0x13, 0xe7},
    {0x4f, 0x88},
    {0x50, 0x8b},
    {0x51, 0x04},
    {0x52, 0x11},
    {0x53, 0x8c},
    {0x54, 0x9d},
    {0x55, 0x00},
    {0x56, 0x40},
    {0x57, 0x80},
    {0x58, 0x9a},
    {0x41, 0x08},
    {0x3f, 0x00},
    {0x75, 0x04},
    {0x76, 0x60},
    {0x4c, 0x00},
    {0x77, 0x01},
    {0x3d, 0xc2},
    {0x4b, 0x09},
    {0xc9, 0x30},
    {0x41, 0x38},
    {0x56, 0x40},
    {0x34, 0x11},
    {0x3b, 0x12},
    {0xa4, 0x88},
    {0x96, 0x00},
    {0x97, 0x30},
    {0x98, 0x20},
    {0x99, 0x30},
    {0x9a, 0x84},
    {0x9b, 0x29},
    {0x9c, 0x03},
    {0x9d, 0x99},
    {0x9e, 0x7f},
    {0x78, 0x04},
    {0x79, 0x01},
    {0xc8, 0xf0},
    {0x79, 0x0f},
    {0xc8, 0x00},
    {0x79, 0x10},
    {0xc8, 0x7e},
    {0x79, 0x0a},
    {0xc8, 0x80},
    {0x79, 0x0b},
    {0xc8, 0x01},
    {0x79, 0x0c},
    {0xc8, 0x0f},
    {0x79, 0x0d},
    {0xc8, 0x20},
    {0x79, 0x09},
    {0xc8, 0x80},
    {0x79, 0x02},
    {0xc8, 0xc0},
    {0x79, 0x03},
    {0xc8, 0x40},
    {0x79, 0x05},
    {0xc8, 0x30},
    {0x79, 0x26},
    {0x62, 0x00},
    {0x63, 0x00},
    {0x64, 0x06},
    {0x65, 0x00},
    {0x66, 0x05},
    {0x94, 0x05},
    {0x95, 0x09},
    {0x2a, 0x10},
    {0x2b, 0xc2},
    {0x15, 0x00},
    {0x3a, 0x04},
    //;LENS
    {0x3d, 0xc3},
    {0x19, 0x03},
    {0x1a, 0x7b},
    {0x2a, 0x00},
    {0x2b, 0x00},
    {0x18, 0x01},
    {0x66, 0x05},
    {0x62, 0x00},
    {0x63, 0x00},
    {0x65, 0x07},
    {0x64, 0x0f},
    {0x94, 0x0e},
    {0x95, 0x10},
    //; CMX
    {0x4f, 0x87},
    {0x50, 0x68},
    {0x51, 0x1e},
    {0x52, 0x15},
    {0x53, 0x7c},
    {0x54, 0x91},
    {0x58, 0x1e},
    {0x41, 0x38},
    {0x76, 0xe0},
     {0x24, 0x68},
    {0x25, 0x58},
    {0x26, 0xc2},
    {0x7a, 0x09},
    {0x7b, 0x0c},
    {0x7c, 0x16},
    {0x7d, 0x28},
    {0x7e, 0x48},
    {0x7f, 0x57},
    {0x80, 0x64},
    {0x81, 0x71},
    {0x82, 0x7e},
    {0x83, 0x89},
    {0x84, 0x94},
    {0x85, 0xa8},
    {0x86, 0xba},
    {0x87, 0xd7},
    {0x88, 0xec},
    {0x89, 0xf9},
    {0x09, 0x00}
};



LOCAL SENSOR_REG_TAB_INFO_T s_OV7675_resolution_Tab_YUV[]=
{		
	// COMMON INIT
	{ADDR_AND_LEN_OF_ARRAY(ov7675_YUV_640X480), 640, 480, 24, SENSOR_IMAGE_FORMAT_YUV422},
	
	// YUV422 PREVIEW 1	
	{PNULL, 0, 640, 480,24, SENSOR_IMAGE_FORMAT_YUV422},
	{PNULL, 0, 0, 0, 0, 0},
	{PNULL, 0, 0, 0, 0, 0},
	{PNULL, 0, 0, 0, 0, 0},
	
	// YUV422 PREVIEW 2 
	{PNULL, 0, 0, 0, 0, 0},
	{PNULL, 0, 0, 0, 0, 0},
	{PNULL, 0, 0, 0, 0, 0},
	{PNULL, 0, 0, 0, 0, 0}
};


LOCAL SENSOR_IOCTL_FUNC_TAB_T s_OV7675_ioctl_func_tab = 
{
    // Internal 
    PNULL,
#if defined(PLATFORM_SC8800G)
    PNULL,
#else
    //_ov7675_Power_On,
#endif
    PNULL,
    OV7675_Identify,

    PNULL,			// write register
    PNULL,			// read  register	
    PNULL,
    PNULL,

    // External
    set_ov7675_ae_enable,
    set_hmirror_enable,
    set_vmirror_enable,

    set_brightness,
    set_contrast,
    set_sharpness,
    set_saturation,

    set_preview_mode,	
    set_image_effect,

    PNULL,//OV7675_BeforeSnapshot, 
    PNULL, //OV7675_After_Snapshot,

    PNULL,

    read_ev_value,
    write_ev_value,
    read_gain_value,
    write_gain_value,
    read_gain_scale,
    set_frame_rate,	
    PNULL,
    PNULL,
    set_ov7675_awb,
    PNULL,
    PNULL,
    set_ov7675_ev,
    PNULL,
    PNULL,
    PNULL,
    _ov7675_GetExifInfo,
    PNULL,
    set_ov7675_anti_flicker,
    set_ov7675_video_mode,
//    PNULL,
    PNULL    
};

/**---------------------------------------------------------------------------*
 ** 						Global Variables								  *
 **---------------------------------------------------------------------------*/
 PUBLIC SENSOR_INFO_T g_OV7675_yuv_info =/*lint !e765*/
{
	OV7675_I2C_ADDR_W,				// salve i2c write address
	OV7675_I2C_ADDR_R, 				// salve i2c read address
	
	0,								// bit0: 0: i2c register value is 8 bit, 1: i2c register value is 16 bit
									// bit2: 0: i2c register addr  is 8 bit, 1: i2c register addr  is 16 bit
									// other bit: reseved
	SENSOR_HW_SIGNAL_PCLK_N|\
	SENSOR_HW_SIGNAL_VSYNC_P|\
	SENSOR_HW_SIGNAL_HSYNC_P,		// bit0: 0:negative; 1:positive -> polarily of pixel clock
									// bit2: 0:negative; 1:positive -> polarily of horizontal synchronization signal
									// bit4: 0:negative; 1:positive -> polarily of vertical synchronization signal
									// other bit: reseved											
											
	// preview mode
	SENSOR_ENVIROMENT_NORMAL|\
	SENSOR_ENVIROMENT_NIGHT|\
	SENSOR_ENVIROMENT_SUNNY,		
	
	// image effect
	SENSOR_IMAGE_EFFECT_NORMAL|\
	SENSOR_IMAGE_EFFECT_BLACKWHITE|\
	SENSOR_IMAGE_EFFECT_RED|\
	SENSOR_IMAGE_EFFECT_GREEN|\
	SENSOR_IMAGE_EFFECT_BLUE|\
	SENSOR_IMAGE_EFFECT_YELLOW|\
	SENSOR_IMAGE_EFFECT_NEGATIVE|\
	SENSOR_IMAGE_EFFECT_CANVAS,
	
	// while balance mode
	0,
		
	0x77777,						// brightness/contrast/sharpness/saturation/EV

	
	SENSOR_LOW_PULSE_RESET,			// reset pulse level
	20,								// reset pulse width(ms)
	
	SENSOR_HIGH_LEVEL_PWDN,			// 1: high level valid; 0: low level valid
	
	1,								// count of identify code
	{{0x0A, 0x76},						// supply two code to identify sensor.
	{0x0B, 0x73}},						// for Example: index = 0-> Device id, index = 1 -> version id	
									
	SENSOR_AVDD_2800MV,				// voltage of avdd	

	640,							// max width of source image
	480,							// max height of source image
	"OV7675",						// name of sensor												

	SENSOR_IMAGE_FORMAT_YUV422,		// define in SENSOR_IMAGE_FORMAT_E enum,
									// if set to SENSOR_IMAGE_FORMAT_MAX here, image format depent on SENSOR_REG_TAB_INFO_T
	SENSOR_IMAGE_PATTERN_YUV422_YUYV,	// pattern of input image form sensor;			

	s_OV7675_resolution_Tab_YUV,	// point to resolution table information structure
	&s_OV7675_ioctl_func_tab,		// point to ioctl function table
			
	PNULL,							// information and table about Rawrgb sensor
	PNULL,							// extend information about sensor	
	SENSOR_AVDD_2800MV,                     // iovdd
	SENSOR_AVDD_2800MV,                      // dvdd
	4,                     // skip frame num before preview 
	3,                      // skip frame num before capture
	0,                      // deci frame num during preview	
	2,                      // deci frame num during video preview
	0,                     // threshold enable(only analog TV)	
	0,                     // atv output mode 0 fix mode 1 auto mode	
	0,                    // atv output start postion	
	0,                     // atv output end postion
	0
};
/**---------------------------------------------------------------------------*
 ** 							Function  Definitions
 **---------------------------------------------------------------------------*/
 LOCAL void OV7675_WriteReg( uint8_t  subaddr, uint8_t data )
{
	Sensor_WriteReg_8bits( subaddr, data);

	SENSOR_TRACE("SENSOR: OV7675_WriteReg reg/value(%x,%x) !!\n", subaddr, data);

}
LOCAL uint8_t OV7675_ReadReg( uint8_t  subaddr)
{
	uint8_t value = 0;	

	value = Sensor_ReadReg(subaddr);
	
    //SENSOR_TRACE("SENSOR: OV7675_ReadReg reg/value(%x,%x) !!\n", subaddr, value);
	return value;
}

LOCAL uint32_t OV7675_Identify(uint32_t param)
{
#define OV7675_PID_VALUE	0x76	
#define OV7675_PID_ADDR	0x0A
#define OV7675_VER_VALUE	0x73	
#define OV7675_VER_ADDR		0x0B	

	uint32_t i;
	uint32_t nLoop;
	uint8_t ret;
	uint32_t err_cnt = 0;

	uint8_t reg[2] 	= {0x0A, 0x0B};
	uint8_t value[2] 	= {0x76, 0x73};

	for(i = 0; i<2; )
	{
		nLoop = 1000;
		ret = OV7675_ReadReg(reg[i]);
		if( ret != value[i])
		{
			err_cnt++;
			if(err_cnt>3)
			{
				SENSOR_TRACE("Fail to OV7690_Identify: ret: %d, value[%d]: %d.\n", ret, i, value[i]);
				return SENSOR_FAIL;
			}
			else
			{
				//Masked by frank.yang,msleep() will cause a  Assert when called in boot precedure
				//msleep(10);
				//while(nLoop--)
				msleep(10);
     				continue;
			}
		}
        err_cnt = 0;
		i++;
	}

    _ov7675_InitExifInfo();
    
	SENSOR_TRACE("SENSOR: OV7675_Identify: it is OV7675.\n");
	
	return 0;
}

/******************************************************************************/
// Description: Intialize Exif information
// Global resource dependence: 
// Author:
// Note:
/******************************************************************************/
LOCAL uint32_t _ov7675_InitExifInfo(void)
{
#if 0
    EXIF_SPEC_PIC_TAKING_COND_T* exif_ptr=&s_ov7675_exif;

    SENSOR_TRACE("SENSOR: _ov7675_InitExifInfo");

    exif_ptr->valid.FNumber = 1;
    exif_ptr->FNumber.numerator = 14;
    exif_ptr->FNumber.denominator = 5;
    
    exif_ptr->valid.ExposureProgram = 1;
    exif_ptr->ExposureProgram = 0x04;

    //exif_ptr->SpectralSensitivity[MAX_ASCII_STR_SIZE];	
    //exif_ptr->ISOSpeedRatings;
    //exif_ptr->OECF;
    
    //exif_ptr->ShutterSpeedValue;
    
    exif_ptr->valid.ApertureValue=1;
    exif_ptr->ApertureValue.numerator=14;
    exif_ptr->ApertureValue.denominator=5;
    
    //exif_ptr->BrightnessValue;
    //exif_ptr->ExposureBiasValue;

    exif_ptr->valid.MaxApertureValue=1;
    exif_ptr->MaxApertureValue.numerator=14;
    exif_ptr->MaxApertureValue.denominator=5;
    
    //exif_ptr->SubjectDistance;
    //exif_ptr->MeteringMode;
    //exif_ptr->LightSource;
    //exif_ptr->Flash;

    exif_ptr->valid.FocalLength=1;
    exif_ptr->FocalLength.numerator=289;
    exif_ptr->FocalLength.denominator=100;
    
    //exif_ptr->SubjectArea;
    //exif_ptr->FlashEnergy;
    //exif_ptr->SpatialFrequencyResponse;
    //exif_ptr->FocalPlaneXResolution;
    //exif_ptr->FocalPlaneYResolution;
    //exif_ptr->FocalPlaneResolutionUnit;
    //exif_ptr->SubjectLocation[2];
    //exif_ptr->ExposureIndex;
    //exif_ptr->SensingMethod;

    exif_ptr->valid.FileSource=1;
    exif_ptr->FileSource=0x03;

    //exif_ptr->SceneType;
    //exif_ptr->CFAPattern;
    //exif_ptr->CustomRendered;

    exif_ptr->valid.ExposureMode=1;
    exif_ptr->ExposureMode=0x00;

    exif_ptr->valid.WhiteBalance=1;
    exif_ptr->WhiteBalance=0x00;
    
    //exif_ptr->DigitalZoomRatio;
    //exif_ptr->FocalLengthIn35mmFilm;
    //exif_ptr->SceneCaptureType;	
    //exif_ptr->GainControl;
    //exif_ptr->Contrast;
    //exif_ptr->Saturation;
    //exif_ptr->Sharpness;
    //exif_ptr->DeviceSettingDescription;
    //exif_ptr->SubjectDistanceRange;
#endif
    return 0;
}
#if 0 
LOCAL uint32_t _ov7675_Power_On(uint32_t power_on)
{
    SENSOR_AVDD_VAL_E		dvdd_val=g_OV7675_yuv_info.dvdd_val;
    SENSOR_AVDD_VAL_E		avdd_val=g_OV7675_yuv_info.avdd_val;
    SENSOR_AVDD_VAL_E		iovdd_val=g_OV7675_yuv_info.iovdd_val;  
//    BOOLEAN 				power_down=g_OV7675_yuv_info.power_down_level;	    
    BOOLEAN 				reset_level=g_OV7675_yuv_info.reset_pulse_level;
    uint32_t 				reset_width=g_OV7675_yuv_info.reset_pulse_width;	    
    
    if(1==power_on)
    {
        Sensor_SetVoltage(dvdd_val, avdd_val, iovdd_val);
 //       GPIO_SetSensorPower(TRUE);
  //      GPIO_SetFrontSensorPwdn((BOOLEAN)!power_down);

        // Open Mclk in default frequency
        Sensor_SetMCLK(SENSOR_DEFALUT_MCLK);   
        
        msleep(20);
        Sensor_SetResetLevel(reset_level);
        msleep(reset_width);
        Sensor_SetResetLevel((BOOLEAN)!reset_level);
        msleep(100);
    }
    else
    {
//        GPIO_SetFrontSensorPwdn(power_down);

        Sensor_SetMCLK(SENSOR_DISABLE_MCLK);           
//        GPIO_SetSensorPower(FALSE);
        Sensor_SetVoltage(SENSOR_AVDD_CLOSED, SENSOR_AVDD_CLOSED, SENSOR_AVDD_CLOSED);        
    }
    
    SENSOR_TRACE("SENSOR: _ov7675_Power_On(1:on, 0:off): %d", power_on);    
    
    return 0;
}
#endif

LOCAL uint32_t set_ov7675_ae_enable(uint32_t enable)
{

    unsigned char ae_value;

        ae_value=OV7675_ReadReg(AE_ENABLE);

        if(0x00==enable)
        {
            ae_value&=0xfa;
            OV7675_WriteReg(AE_ENABLE,ae_value);
        }
        else if(0x01==enable)
        {
            ae_value|=0x05;
            OV7675_WriteReg(AE_ENABLE,ae_value);
        }

	SENSOR_TRACE("SENSOR: set_ae_enable: enable = %d", enable);

	return 0;
}

#if 0

LOCAL uint32_t set_ov7675_ae_awb_enable(uint32_t ae_enable, uint32_t awb_enable)
{
        unsigned char ae_value;

        ae_value=OV7675_ReadReg(AE_ENABLE);

        if(0x00==ae_enable)
        {
            ae_value&=0xfa;
        }
        else if(0x01==ae_enable)
        {
            ae_value|=0x05;
        }

        if(0x00==awb_enable)
        {
            ae_value&=0xfd;
        }
        else if(0x01==awb_enable)
        {
            ae_value|=0x02;
        }        

        OV7675_WriteReg(AE_ENABLE,ae_value);

        SENSOR_TRACE("SENSOR: set_ae_awb_enable: ae=%d awb=%d", ae_enable, awb_enable);

	return 0;
}

#endif

LOCAL uint32_t set_hmirror_enable(uint32_t enable)
{
 	uint8_t value = 0;	
	value = OV7675_ReadReg(0x1e);
	value = (value & 0xDF) | ((enable & 0x1) << 5); //landscape
	SENSOR_TRACE("set_hmirror_enable: enable = %d, 0x1e: 0x%x.\n", enable, value);
	OV7675_WriteReg(0x1e, value);

	return 0;
}


LOCAL uint32_t set_vmirror_enable(uint32_t enable)
{
 	uint8_t value = 0;	
	value = OV7675_ReadReg(0x1e);
	value = (value & 0xEF) | (((enable == 1 ? 0 : 1)) << 4); //portrait
	SENSOR_TRACE("set_vmirror_enable: enable = %d, 0x1e: 0x%x.\n", enable, value);
	OV7675_WriteReg(0x1e, value);

	return 0;
}


 LOCAL const SENSOR_REG_T ov7675_ev_tab[][4]=
{
 
    {{0x24, 0x40}, {0x25, 0x30}, {0x26, 0xb2}, {0xff, 0xff}},            
    {{0x24, 0x50}, {0x25, 0x40}, {0x26, 0xc2}, {0xff, 0xff}},            
    {{0x24, 0x60}, {0x25, 0x50}, {0x26, 0xc2}, {0xff, 0xff}},            
    {{0x24, 0x70}, {0x25, 0x60}, {0x26, 0xc2}, {0xff, 0xff}},            
    {{0x24, 0x80}, {0x25, 0x70}, {0x26, 0xd4}, {0xff, 0xff}},            
    {{0x24, 0x90}, {0x25, 0x80}, {0x26, 0xd5}, {0xff, 0xff}},
    {{0x24, 0xa0}, {0x25, 0x90}, {0x26, 0xe6}, {0xff, 0xff}}   
};

LOCAL uint32_t set_ov7675_ev(uint32_t level)
{
    uint16_t i;    
    SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)ov7675_ev_tab[level];

    SENSOR_ASSERT(PNULL != sensor_reg_ptr);
    SENSOR_ASSERT(level < 7);

    for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) && (0xFF != sensor_reg_ptr[i].reg_value) ; i++)
    {
        OV7675_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
    }
    
    SENSOR_TRACE("SENSOR: set_ev: level = %d", level);

    return 0;
}
/******************************************************************************/
// Description: anti 50/60 hz banding flicker
// Global resource dependence: 
// Author:
// Note:
//		level  must smaller than 8
/******************************************************************************/
LOCAL uint32_t set_ov7675_anti_flicker(uint32_t mode)
{//24m->65.6 us 12m->131us
    uint8_t data=0x00;

    data=OV7675_ReadReg(OV7675_COM11);
    switch(mode)
    {
        case FLICKER_50HZ:
                data|=0x08;
                OV7675_WriteReg(OV7675_COM11,data);                    
            break;
            
        case FLICKER_60HZ:
                data&=0xf7;
                OV7675_WriteReg(OV7675_COM11,data);                      
            break;
            
        default:
            break;
    }
    
    msleep(20);
    SENSOR_TRACE("SENSOR: set_banding_mode: mode = %d", mode);

    return 0;
}
/******************************************************************************/
// Description: set video mode
// Global resource dependence: 
// Author:
// Note:
//		 
/******************************************************************************/
 LOCAL const SENSOR_REG_T ov7675_video_mode_nand_tab[][4]=
{
    // normal mode
    {
        {0x2d, 0x00},{0x2e, 0x00},{0x14, 0x38},{0xff, 0xff}
    },    
    //vodeo mode
    {
        {0x2d, 0x00},{0x2e, 0x00},{0x14, 0x38},{0xff, 0xff}      
    }
};
/******************************************************************************/
// Description: set video mode
// Global resource dependence: 
// Author:
// Note:
//		 
/******************************************************************************/
 LOCAL const SENSOR_REG_T ov7675_video_mode_nor_tab[][7]=
{
    // normal mode
    {
        {0x2d, 0x00},{0x2e, 0x00},{0x14, 0x38},{0x92, 0xfb}, {0x93, 0x01}, {0xa5, 0x05},{0xff, 0xff}
    }, 
    {
        {0x2d, 0x00},{0x2e, 0x00},{0x14, 0x38}, {0x92, 0x68}, {0x93, 0x01}, {0xff, 0xff}, {0xff, 0xff}       
    }
 
};    

LOCAL uint32_t set_ov7675_video_mode(uint32_t mode)
{
    uint8_t data=0x00;
    uint8_t data1=0x00;
    uint16_t i;
    SENSOR_REG_T* sensor_reg_ptr = PNULL;

  //  SENSOR_ASSERT(mode <= 1);
  //  SENSOR_ASSERT(mode<DCAMERA_MODE_MAX);

//    if(CHIP_DetectMemType())
        sensor_reg_ptr = (SENSOR_REG_T*)ov7675_video_mode_nand_tab[mode];
//    else
 //       sensor_reg_ptr = (SENSOR_REG_T*)ov7675_video_mode_nor_tab[mode];

    SENSOR_ASSERT(PNULL != sensor_reg_ptr);
    
    data=OV7675_ReadReg(OV7675_COM11);
    data&=0x0f; 
    OV7675_WriteReg(OV7675_COM11,data);      

    data1=OV7675_ReadReg(OV7675_REGCF);
    data1&= 0xf7 ;
    OV7675_WriteReg(OV7675_COM11,data1); 
    
    switch(mode)
    {
        case 0: //DCAMERA_NORMAL_MODE:
            OV7675_WriteReg(PLL_ADDR,0x81);                  
            break;

        case 1: //DCAMERA_VIDEO_MODE:     
            OV7675_WriteReg(PLL_ADDR,0x80);      
            break;

        default :
            break;
    }    

    for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) || (0xFF != sensor_reg_ptr[i].reg_value); i++)
    {
    	OV7675_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
    }

    //msleep(500);

    SENSOR_TRACE("SENSOR: set_video_mode: mode = %d", mode);
    return 0;
}

/******************************************************************************/
// Description: set wb mode 
// Global resource dependence: 
// Author:
// Note:
//		
/******************************************************************************/
 LOCAL const SENSOR_REG_T ov7675_awb_tab[][7]=

{
    //AUTO
    {
        {0x13, 0xe7},
        {0x2d, 0x00},
        {0x2e, 0x00},
        {0xff, 0xff},
        {0xff, 0xff},
        {0xff, 0xff},
        {0xff, 0xff}            
    },    
    //INCANDESCENCE:
    {
        {0x13, 0xe5},
        {0x01, 0x78},
        {0x02, 0x58},
        {0x6a, 0x40},
        {0x2d, 0x00},
        {0x2e, 0x00},
        {0xff, 0xff}         
    },
    //U30
    {
        {0x13, 0xe7},
        {0x2d, 0x00},
        {0x2e, 0x00},
        {0xff, 0xff},
        {0xff, 0xff},
        {0xff, 0xff},
        {0xff, 0xff}            
    },  
    //CWF
    {
        {0x13, 0xe7},
        {0x2d, 0x00},
        {0x2e, 0x00},
        {0xff, 0xff},
        {0xff, 0xff},
        {0xff, 0xff},
        {0xff, 0xff}            
    },    
    //FLUORESCENT:
    {
        {0x13, 0xe5},
        {0x01, 0x96},
        {0x02, 0x40},
        {0x6a, 0x4a},
        {0x2d, 0x00},
        {0x2e, 0x00},
        {0xff, 0xff}           
    },
    //SUN:
    {
        {0x13, 0xe5},
        {0x01, 0x5a},
        {0x02, 0x5c},
        {0x6a, 0x42},
        {0x2d, 0x00},
        {0x2e, 0x00},
        {0xff, 0xff}            
    },
    //CLOUD:
    {
        {0x13, 0xe5},
        {0x01, 0x58},
        {0x02, 0x60},
        {0x6a, 0x40},
        {0x2d, 0x00},
        {0x2e, 0x00},
        {0xff, 0xff}            
    },
};

LOCAL uint32_t set_ov7675_awb(uint32_t mode)
{
	uint16_t i;
	SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)ov7675_awb_tab[mode];

	SENSOR_ASSERT(mode < 7);
	SENSOR_ASSERT(PNULL != sensor_reg_ptr);
	
	for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) && (0xFF != sensor_reg_ptr[i].reg_value); i++)
	{
		OV7675_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
	}

//	Sensor_SetSensorExifInfo(SENSOR_EXIF_CTRL_LIGHTSOURCE, (uint32_t)mode);
    msleep(20);
  
	SENSOR_TRACE("SENSOR: set_awb_mode: mode = %d", mode);
	
	return 0;
}

/******************************************************************************/
// Description: set brightness 
// Global resource dependence: 
// Author:
// Note:
//		level  must smaller than 8
/******************************************************************************/
 LOCAL const SENSOR_REG_T ov7675_brightness_tab[][2]=
{
	{{0x55, 0xb0},{0xff,0xff}},
	{{0x55, 0xa0},{0xff,0xff}},
	{{0x55, 0x90},{0xff,0xff}},
	{{0x55, 0x00},{0xff,0xff}},
	{{0x55, 0x10},{0xff,0xff}},
	{{0x55, 0x20},{0xff,0xff}},
	{{0x55, 0x30},{0xff,0xff}},
};

LOCAL uint32_t set_brightness(uint32_t level)
{
	uint16_t i;
	SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)ov7675_brightness_tab[level];

	SENSOR_ASSERT(level < 7);
	SENSOR_ASSERT(PNULL != sensor_reg_ptr);
	
	for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) && (0xFF != sensor_reg_ptr[i].reg_value); i++)
	{
		OV7675_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
	}
    msleep(20);
    SENSOR_TRACE("SENSOR: set_brightness: level = %d", level);
	
	return 0;
}

 LOCAL const SENSOR_REG_T ov7675_contrast_tab[][2]=
{
	{{0x56, 0x20},{0xff,0xff}},
	{{0x56, 0x30},{0xff,0xff}},
	{{0x56, 0x40},{0xff,0xff}},	
	{{0x56, 0x50},{0xff,0xff}},
	{{0x56, 0x60},{0xff,0xff}},
	{{0x56, 0x70},{0xff,0xff}},
	{{0x56, 0x80},{0xff,0xff}},
};

LOCAL uint32_t set_contrast(uint32_t level)
{
    uint16_t i;
    SENSOR_REG_T* sensor_reg_ptr;

    sensor_reg_ptr = (SENSOR_REG_T*)ov7675_contrast_tab[level];

    SENSOR_ASSERT(level < 7 );
    SENSOR_ASSERT(PNULL != sensor_reg_ptr);

    for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) && (0xFF != sensor_reg_ptr[i].reg_value); i++)
    {
        OV7675_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
    }

 //   Sensor_SetSensorExifInfo(SENSOR_EXIF_CTRL_CONTRAST, (uint32_t)level);
    msleep(20);
    SENSOR_TRACE("SENSOR: set_contrast: level = %d", level);

    return 0;
}


LOCAL uint32_t set_sharpness(uint32_t level)
{
	
	return 0;
}


LOCAL uint32_t set_saturation(uint32_t level)
{

	
	return 0;
}

/******************************************************************************/
// Description: set brightness 
// Global resource dependence: 
// Author:
// Note:
//		level  must smaller than 8
/******************************************************************************/

LOCAL uint32_t set_preview_mode(uint32_t preview_mode)
{
	SENSOR_TRACE("SENSOR: set_preview_mode: preview_mode = %d", preview_mode);
	
	s_preview_mode = preview_mode;
	
	switch (preview_mode)
	{
		case DCAMERA_ENVIRONMENT_NORMAL: 
		{
			OV7675_set_work_mode(0);
			break;
		}
		case DCAMERA_ENVIRONMENT_NIGHT:
		{
			OV7675_set_work_mode(1);
			break;
		}
		case DCAMERA_ENVIRONMENT_SUNNY:
		{
			OV7675_set_work_mode(0);
			break;
		}
		default:
		{
			break;
		}
			
	}
	
	msleep(100);
	
	return 0;
}


/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:
// Note:
//		
/******************************************************************************/
 LOCAL const SENSOR_REG_T ov7675_image_effect_tab[][4]=
{
    // effect normal
    {
        {0x3a, 0x04},{0x67, 0x80},{0x68, 0x80},{0xff, 0xff}
    },
    //effect BLACKWHITE
    {
        {0x3a, 0x14},{0x67, 0x80},{0x68, 0x80},{0xff, 0xff}
    },
    // effect RED
    {
        {0x3a , 0x14},{0x67, 0xc0},{0x68, 0x80},{0xff, 0xff}
    },
    // effect GREEN
    {
        {0x3a , 0x14}, {0x67, 0x40},{0x68 , 0x40},{0xff, 0xff}
    },
    // effect  BLUE
    {
        {0x3a, 0x14},{0x67, 0x80},{0x68, 0xc0},{0xff, 0xff}
    },
    // effect  YELLOW
    {
        {0x3a, 0x14},{0x67, 0x90},{0x68, 0x20},{0xff, 0xff}
    },  
    // effect NEGATIVE
    {	     
        {0x3a, 0x24}, {0x67, 0x80},{0x68, 0x80},{0xff, 0xff}
    },    
    //effect ANTIQUE
    {
        {0x3a, 0x14}, {0x67, 0xa0},{0x68 , 0x40},{0xff, 0xff}
    },

};

	
LOCAL uint32_t set_image_effect(uint32_t effect_type)
{
	uint16_t i;
	SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)ov7675_image_effect_tab[effect_type];

	
	SENSOR_ASSERT(PNULL != sensor_reg_ptr);
	
	for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) && (0xFF != sensor_reg_ptr[i].reg_value) ; i++)
	{
		OV7675_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
	}
	SENSOR_TRACE("SENSOR: set_image_effect: effect_type = %d", effect_type);
    
	return 0;
}
#if 0
LOCAL uint32_t OV7675_After_Snapshot(uint32_t param)
{// Tim.zhu@20080520 for cr116612 modify the switch preview to capture

    SENSOR_TRACE("SENSOR: OV7675_After_Snapshot ");  

    set_ov7675_ae_awb_enable(0x01, 0x01);

    OV7675_WriteReg(PLL_ADDR, 0x81); 

    if(DCAMERA_ENVIRONMENT_NIGHT == s_preview_mode)
    {
          OV7675_WriteReg(OV7675_COM11,(OV7675_ReadReg(OV7675_COM11)&0x0f)|0xc0);  
    } 
    else
    {
        OV7675_WriteReg(OV7675_COM11,(OV7675_ReadReg(OV7675_COM11)&0x0f)|0xa0);  
    }
    
    OV7675_WriteReg(0x92, 0);//0xfb);  
    OV7675_WriteReg(0x93, 0);//0x01);    

    msleep(200); // wait 2 frame the sensor working normal if no delay the lum is incorrect
 
    return 0;
    
}

LOCAL uint32_t OV7675_BeforeSnapshot(uint32_t param)
{// Tim.zhu@20080514 for crcr115898 modify the switch preview to capture

#define GAIN_ADDR   0x00
#define EXPOSAL_ADDR0   0x04
#define EXPOSAL_ADDR2   0x07
#define EXPOSAL_ADDR1    0x10
#define DUMMY_LINE0     0x2d
#define DUMMY_LINE1     0x2e

#define OV7675_LINE_LENGTH (784*2)
#define OV7675_C2P_RATIO 2 //preview 12M PCLK, capture 6M PCLK
#define OV7675_NIGHT_MAX_EXPLINE 1530 //Including dummline

#define  OV7675_NORMAL_MAX_EXPLINE 508 

    uint8_t temp=0x00;
    uint8_t gain=0x00;
    uint16_t exposal=0x00;
    uint16_t dummy_line=0x00;
    uint16_t max_line = OV7675_NORMAL_MAX_EXPLINE;
    
    SENSOR_TRACE("SENSOR: OV7675_BeforeSnapshot: OV7675_BeforeSnapshot ");  

    if(DCAMERA_ENVIRONMENT_NORMAL == s_preview_mode)
    {
    	    max_line = OV7675_NORMAL_MAX_EXPLINE;
    }
    else if(DCAMERA_ENVIRONMENT_NIGHT == s_preview_mode)
    {
    	   max_line = OV7675_NIGHT_MAX_EXPLINE;
    }
    else
    {
         SENSOR_TRACE("unknown Camera mode!");
    }
	
    SENSOR_TRACE("OV7675_BeforeSnapshot max_line= %d", max_line );
    set_ov7675_ae_awb_enable(0x00, 0x00);

    gain=OV7675_ReadReg(GAIN_ADDR);

    temp=OV7675_ReadReg(EXPOSAL_ADDR0);
    exposal|=(temp&0X03);
    temp=OV7675_ReadReg(EXPOSAL_ADDR1);
    exposal|=(temp<<0x02);
    temp=OV7675_ReadReg(EXPOSAL_ADDR2); 
    exposal|=((temp&0x3f)<<0x0a);

    temp=OV7675_ReadReg(DUMMY_LINE0);
    dummy_line|=temp;
    temp=OV7675_ReadReg(DUMMY_LINE1); 
    dummy_line|=(temp<<0x08); 

    OV7675_WriteReg(PLL_ADDR, 0x83);

    OV7675_WriteReg(OV7675_COM11,OV7675_ReadReg(OV7675_COM11)&0x0f);  //0x3b->0x0a
   
    set_ov7675_ae_awb_enable(0x00, 0x00);

    exposal=exposal + dummy_line;

    exposal = exposal/OV7675_C2P_RATIO;
    
    if(0x80==(gain&0x80) &&( (exposal *2) < max_line ))
    {
        gain&=0x7f;
        exposal*=0x02;
    }

    if(0x40==(gain&0x40)&&( (exposal *2) < max_line ))
    {
        gain&=0xbf;
        exposal*=0x02;
    }
        
     if(0x20==(gain&0x20)&&( (exposal *2) < max_line ))
    {
        gain&=0xdf;
        exposal*=0x02;
    }

    if(0x10==(gain&0x10)&&( (exposal *2) < max_line ))
    {
        gain&=0xef;
        exposal*=0x02;
    } 

    SENSOR_TRACE("OV7675_BeforeSnapshot exposal= %d", exposal );
    if(exposal>498)
    {
        dummy_line=(exposal-498);
    }
 
    OV7675_WriteReg(0x2d, 0);      
    OV7675_WriteReg(0x2e, 0); 
  	
    OV7675_WriteReg(GAIN_ADDR, gain);

    temp=OV7675_ReadReg(EXPOSAL_ADDR0);
    temp&=0xfc;
    temp|=(exposal&0X03);
    OV7675_WriteReg(EXPOSAL_ADDR0, temp);

     temp=((exposal>>0x02)&0xff);
    OV7675_WriteReg(EXPOSAL_ADDR1, temp);

    temp=OV7675_ReadReg(EXPOSAL_ADDR2);
    temp&=0xc0;
    temp|=(exposal>>0x0a)&0x3f;
    OV7675_WriteReg(EXPOSAL_ADDR2, temp);  
    
    temp=(uint8_t)(dummy_line&0xff);

    OV7675_WriteReg(0x92, temp);  

    
    temp=(uint8_t)(dummy_line>>0x08)&0xff;
    OV7675_WriteReg(0x93, temp);      


   // Sensor_SetSensorExifInfo(SENSOR_EXIF_CTRL_EXPOSURETIME, (uint32_t)exposal);

    if(DCAMERA_ENVIRONMENT_NORMAL == s_preview_mode)
    {
    	     msleep(200); // wait 2 frame the sensor working normal if no delay the lum is incorrect
    }
    else if(DCAMERA_ENVIRONMENT_NIGHT == s_preview_mode)
    {
    	    msleep(800); // wait 2 frame the sensor working normal if no delay the lum is incorrect
    }
    else
    {
          msleep(800); // wait 2 frame the sensor working normal if no delay the lum is incorrect

    }
 
    return 0;
    
}

#endif

LOCAL uint32_t read_ev_value(uint32_t value)
{
	return 0;
}

LOCAL uint32_t write_ev_value(uint32_t exposure_value)
{
	
	return 0;	
}

LOCAL uint32_t read_gain_value(uint32_t value)
{

	
	return 0;
}

LOCAL uint32_t write_gain_value(uint32_t gain_value)
{

	
	return 0;
}

LOCAL uint32_t read_gain_scale(uint32_t value)
{
	return SENSOR_GAIN_SCALE;
	
}


LOCAL uint32_t set_frame_rate(uint32_t param)
    
{
	
	
	return 0;
}

/******************************************************************************/
// Description:
// Global resource dependence: 
// Author:
// Note:
//		mode 0:normal;	 1:night 
/******************************************************************************/
// LOCAL const SENSOR_REG_T ov7675_mode_tab[][5]=
LOCAL const SENSOR_REG_T ov7675_mode_tab[][5]=
{
    // 30 fps
    {{0x2d, 0x00},{0x2e, 0x00},{0x14, 0x38},{0xa5, 02}, {0xff,0xff}}, // normal
    {{0x2d, 0x00},{0x2e, 0x00},{0x14, 0x38},{0xa5, 06}, {0xff,0xff}} // night  
};

LOCAL uint32_t OV7675_set_work_mode(uint32_t mode)
{
    uint8_t data=0x00;
    uint8_t data1=0x00;
    uint16_t i;

    SENSOR_REG_T* sensor_reg_ptr = (SENSOR_REG_T*)ov7675_mode_tab[mode];

    SENSOR_ASSERT(mode <= 1);
    SENSOR_ASSERT(PNULL != sensor_reg_ptr);

    data=OV7675_ReadReg(OV7675_COM11);    
    data&=0x0f;

    data1=OV7675_ReadReg(OV7675_REGCF);
    data1&= 0xf7 ;
    
    switch(mode)
    {
        case DCAMERA_ENVIRONMENT_NORMAL:
            OV7675_WriteReg(OV7675_COM11,data);
            OV7675_WriteReg(OV7675_REGCF,data1);
            break;

        case DCAMERA_ENVIRONMENT_NIGHT:
            data|=0xa0;
            OV7675_WriteReg(OV7675_COM11,data);
            OV7675_WriteReg(OV7675_REGCF,data1);
            break;

        default :
            break;
    }    

    for(i = 0; (0xFF != sensor_reg_ptr[i].reg_addr) || (0xFF != sensor_reg_ptr[i].reg_value); i++)
    {
    OV7675_WriteReg(sensor_reg_ptr[i].reg_addr, sensor_reg_ptr[i].reg_value);
    }

    //Sensor_SetSensorExifInfo(SENSOR_EXIF_CTRL_SCENECAPTURETYPE, (uint32_t)mode);

    SENSOR_TRACE("SENSOR: set_work_mode: mode = %d", mode);
    return 0;
}

LOCAL uint32_t _ov7675_GetExifInfo(uint32_t param)
{
    //return (uint32_t)&s_ov7675_exif;
    return 0;
}
/*lint -restore*/
