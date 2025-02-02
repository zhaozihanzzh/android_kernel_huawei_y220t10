#ifndef __ENG_AUDIO_H__
#define __ENG_AUDIO_H__


#define NAME_LEN_MAX                            16
#define EQ_BAND_MAX				8
#define EQ_MODE_MAX                             6

#define AUDMOD_NAME_MAX_LEN                     16

#define AUDIO_DEVICE_MODE_PARAM_MAX       	8 
#define AUDIO_AGC_INPUG_GAIN_MAX 		8
#define AUDIO_ARM_VOLUME_LEVEL                  31
#define AUDIO_NV_ARM_APP_PARA_RESERVE           9
#define AUDIO_ARM_APP_TYPE_MAX 			16
#define AUDIO_NV_ARM_PARA_RESERVE		64
#define AUDIO_NV_ARM_MODE_NAME_MAX_LEN          16     


#define	MAX_AUDIO_MODE_COUNT			14			/*  */
#define	MAX_AUDIO_VOLUME_MODE_COUNT		4			/*  */

#define	AUDIO_NV_ARM_DATA_FLASH			0x01			/*  */
#define	AUDIO_NV_ARM_DATA_MEMORY                0x02			/*  */
#define	AUDIO_ENHA_DATA_MEMORY	                0x04			/*  */
#define	AUDIO_ENHA_DATA_FLASH	                0x08			/*  */


#define AUDIO_NV_FM_GAINL_INDEX			41
#define AUDIO_NV_FM_GAINR_INDEX			42
#define AUDIO_NV_CAPTURE_GAIN_INDEX		43
#define AUDIO_NV_INTPA_SWITCH_INDEX		44
#define AUDIO_NV_INTPA_GAIN_INDEX			45

typedef enum{
	GET_AUDIO_MODE_COUNT,
	GET_AUDIO_MODE_NAME,
	GET_AUDIO_MODE_DATA_FROM_RAM,
	SET_AUDIO_MODE_DATA_TO_RAM,
	SET_AUDIO_MODE_NAME,
	SET_AUDIO_MODE_DATA_TO_FLASH,
	GET_AUDIO_MODE_DATA_FROM_FLASH,
	GET_ARM_VOLUME_MODE_COUNT,
	GET_ARM_VOLUME_MODE_NAME,
	GET_ARM_VOLUME_DATA_FROM_RAM,
	SET_ARM_VOLUME_DATA_TO_RAM,
	SET_ARM_VOLUME_DATA_TO_FLASH,
	GET_ARM_VOLUME_DATA_FROM_FLASH,
};

typedef enum{
	GET_AUDIO_ENHA_MODE_COUNT,
	GET_AUDIO_ENHA_DATA_FROM_MEMORY,
	SET_AUDIO_ENHA_DATA_TO_MEMORY,
	GET_AUDIO_ENHA_DATA_FROM_FLASH,
	SET_AUDIO_ENHA_DATA_TO_FLASH,
};
	


typedef unsigned char  uint8;
typedef          char  int8;
typedef unsigned short uint16;
typedef          short int16;

typedef struct
{
	int16  fm_headset_stat;
	int16  fm_handsfree_stat;
}__attribute__((packed)) AUDIO_FM_DEVSTAT_T;

typedef struct 
{
    int16   fo ;  /*f0*/
    int16   q;    /*q*/   
    int16   boostdB;   /*boost */
    int16   gaindB ;      /*gain*/
}__attribute__((packed)) EQ_BAND_INPUT_PARA_T;

typedef struct 
{
    int16   agc_in_gain;  /*agc in gain set*/
    int16   band_control; /*bit15-bit8 :filter_sw_1~8 ; bit 1: high shelve;bit0:low shelve */
    EQ_BAND_INPUT_PARA_T  eq_band[EQ_BAND_MAX];   
}__attribute__((packed)) EQ_MODE_PARA_T;  


typedef struct //PACKED  272 words
{
    uint8   para_name[NAME_LEN_MAX];/*struct name*/ 
    uint16  eq_control;//bit15:8-bands-sw
    EQ_MODE_PARA_T eq_modes[EQ_MODE_MAX];     /*eq mode para*/
    int16 externdArray[59]; /*reserved for future*/
}__attribute__((packed)) AUDIO_ENHA_EQ_STRUCT_T;


typedef struct Audio_Nv_Arm_Device_Path_Struct
{
	uint16 valid_dev_set_count;
	//AUDMOD_DEVICECTL_T bit0-bit7:
	//ucADChannel&0x1, 
	//ucDAChannel&0x1, 
	//ucDA0Reverse&0x1, 
	//ucDA1Reverse&0x1, 
	//ucStereoEn&0x1, 
	//ucSpkAmpEn&0x1, 
	//ucSpkAmpEn&0x1,
	//ucMixerInput&0x1 
	uint16 reserve;//shujing add for align
	uint16 dev_set[AUDIO_DEVICE_MODE_PARAM_MAX];
}__attribute__((packed)) AUDIO_NV_ARM_DEVICE_PATH_T;

typedef struct Audio_Nv_Arm_App_Config_Info_Struct
{
	uint16 eq_switch;		
	uint16 agc_input_gain[AUDIO_AGC_INPUG_GAIN_MAX];		
	uint16 valid_volume_level_count;
	uint16 arm_volume[AUDIO_ARM_VOLUME_LEVEL];
	uint16 reserve[AUDIO_NV_ARM_APP_PARA_RESERVE];
}__attribute__((packed)) AUDIO_NV_ARM_APP_CONFIG_INFO_T;//include eq_switch/agc_input_gain/arm_volume_table

typedef struct Audio_Nv_Arm_App_Set_Struct
{
	uint16 valid_app_set_count;
	uint16 valid_agc_input_gain_count;
	uint16 aud_proc_exp_control[2];//switch of agc/lcf/eq_select--original dsp audio nv:extend2[110]	
	AUDIO_NV_ARM_APP_CONFIG_INFO_T app_config_info[AUDIO_ARM_APP_TYPE_MAX];	
}__attribute__((packed)) AUDIO_NV_ARM_APP_SET_T;

typedef struct Audio_Nv_Arm_Mode_Struct
{	 
	AUDIO_NV_ARM_DEVICE_PATH_T dev_path_set;
	AUDIO_NV_ARM_APP_SET_T app_config_info_set; 	
	uint16 midi_opt;
    uint16 aud_dev; 
    uint16 reserve[AUDIO_NV_ARM_PARA_RESERVE];    
}__attribute__((packed)) AUDIO_NV_ARM_MODE_STRUCT_T;

typedef struct Audio_Nv_Arm_Mode_Info_struct
{
    uint8 ucModeName[AUDIO_NV_ARM_MODE_NAME_MAX_LEN];	//node name.
    AUDIO_NV_ARM_MODE_STRUCT_T tAudioNvArmModeStruct;	// Audio structure    
}__attribute__((packed)) AUDIO_NV_ARM_MODE_INFO_T;


//Sorry,it's so large,and so complicated,but the audio standard defines it like this :(
typedef struct audio_total_t{
	AUDIO_ENHA_EQ_STRUCT_T audio_enha_eq;
	AUDIO_NV_ARM_MODE_INFO_T audio_nv_arm_mode_info;
}__attribute__((packed)) AUDIO_TOTAL_T;


#endif
