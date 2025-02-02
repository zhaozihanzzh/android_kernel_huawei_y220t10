/* drivers/video/sc8800g/sc8800g_fb.c
 *
 * SC8800S LCM0 framebuffer driver.
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
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/fb.h>
#include <linux/delay.h>

#include <linux/freezer.h>
#include <linux/wait.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/interrupt.h>

#include <mach/hardware.h>
#include <mach/irqs.h>
#include <mach/adi_hal_internal.h>
#include <mach/regs_ana.h>
#include <mach/mfp.h>
#include "sc8800g_lcd.h"
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
    
//#define TEST_RRM /* enable rrm test */
//#define  FB_DEBUG 
#ifdef FB_DEBUG
#define FB_PRINT printk
#else
#define FB_PRINT(...)
#endif

/* default lcdc clock */
#define DEF_CLOCK 96000000

#define AHB_CTL0                (SPRD_AHB_BASE + 0x200)
#define AHB_SOFT_RST            (SPRD_AHB_BASE + 0x210)
#define AHB_ARM_CLK         (SPRD_AHB_BASE + 0x224)

#define GR_PLL_SRC              (SPRD_GREG_BASE + 0x70)
#define GR_GEN4                 (SPRD_GREG_BASE + 0x60)


#define LCDC_CTRL                   (SPRD_LCDC_BASE + 0x0000)
#define LCDC_DISP_SIZE              (SPRD_LCDC_BASE + 0x0004)
#define LCDC_LCM_START              (SPRD_LCDC_BASE + 0x0008)
#define LCDC_LCM_SIZE               (SPRD_LCDC_BASE + 0x000c)
#define LCDC_BG_COLOR               (SPRD_LCDC_BASE + 0x0010)
#define LCDC_FIFO_STATUS            (SPRD_LCDC_BASE + 0x0014)

#define LCDC_IMG_CTRL                    (SPRD_LCDC_BASE + 0x0020)
#define LCDC_IMG_Y_BASE_ADDR         (SPRD_LCDC_BASE + 0x0024)
#define LCDC_IMG_UV_BASE_ADDR            (SPRD_LCDC_BASE + 0x0028)
#define LCDC_IMG_SIZE_XY             (SPRD_LCDC_BASE + 0x002c)
#define LCDC_IMG_PITCH                   (SPRD_LCDC_BASE + 0x0030)
#define LCDC_IMG_DISP_XY             (SPRD_LCDC_BASE + 0x0034)

#define LCDC_OSD1_CTRL                    (SPRD_LCDC_BASE + 0x0040)
#define LCDC_OSD2_CTRL                    (SPRD_LCDC_BASE + 0x0070)
#define LCDC_OSD3_CTRL                    (SPRD_LCDC_BASE + 0x0090)
#define LCDC_OSD4_CTRL                    (SPRD_LCDC_BASE + 0x00b0)
#define LCDC_OSD5_CTRL                    (SPRD_LCDC_BASE + 0x00d0)

#define LCDC_OSD1_BASE_ADDR                 (SPRD_LCDC_BASE + 0x0044)
#define LCDC_OSD1_ALPHA_BASE_ADDR           (SPRD_LCDC_BASE + 0x0048)
#define LCDC_OSD1_SIZE_XY                   (SPRD_LCDC_BASE + 0x004c)
#define LCDC_OSD1_PITCH                     (SPRD_LCDC_BASE + 0x0050)
#define LCDC_OSD1_DISP_XY                   (SPRD_LCDC_BASE + 0x0054)
#define LCDC_OSD1_ALPHA                     (SPRD_LCDC_BASE + 0x0058)
#define LCDC_OSD1_GREY_RGB                  (SPRD_LCDC_BASE + 0x005c)
#define LCDC_OSD1_CK                        (SPRD_LCDC_BASE + 0x0060)

#define LCDC_IRQ_EN             (SPRD_LCDC_BASE + 0x0120)
#define LCDC_IRQ_CLR                (SPRD_LCDC_BASE + 0x0124)
#define LCDC_IRQ_STATUS         (SPRD_LCDC_BASE + 0x0128)
#define LCDC_IRQ_RAW                (SPRD_LCDC_BASE + 0x012c)

#define LCM_CTRL                (SPRD_LCDC_BASE + 0x140)
#define LCM_PARAMETER0                (SPRD_LCDC_BASE + 0x144)
#define LCM_PARAMETER1                (SPRD_LCDC_BASE + 0x148)
#define LCM_IFMODE                (SPRD_LCDC_BASE + 0x14c)
#define LCM_RGBMODE              (SPRD_LCDC_BASE + 0x150)
#define LCM_RDDATA               (SPRD_LCDC_BASE + 0x154)
#define LCM_STATUS               (SPRD_LCDC_BASE + 0x158)
#define LCM_RSTN                 (SPRD_LCDC_BASE + 0x15c)
#define LCM_CD0                  (SPRD_LCDC_BASE + 0x180)
#define LCM_DATA0                (SPRD_LCDC_BASE + 0x184)
#define LCM_CD1                  (SPRD_LCDC_BASE + 0x190)
#define LCM_DATA1                (SPRD_LCDC_BASE + 0x194)

#define BITS_PER_PIXEL 16

#include "sc8800g_rrm.h"
#include "sc8800g_lcdc_manager.h" /* TEMP */
#include "sc8800g_copybit_lcdc.h" /* TEMP */
/* TEMP, software make-up for lcdc's 4-byte-align only limitation */
unsigned int fb_len;
unsigned int fb_pa;
unsigned int fb_va;
unsigned int fb_va_cached;
/* TEMP, end */

struct sc8800fb_info {
	struct fb_info *fb;
	uint32_t cap;
	uint32_t need_reinit;
	struct ops_mcu *ops;
	struct lcd_spec *panel;
	struct rrmanager *rrm;
	struct clk *clk_lcdc;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif

};


struct lcd_cfg{ 
	uint32_t lcd_id;
	struct lcd_spec* panel;
};


//overlord for lcd adapt
static struct lcd_cfg lcd_panel[] = {
	[0]={
		.lcd_id = 0x1,
		.panel = &lcd_panel_r61581,
		},

	[1]={
		.lcd_id = 0x9481,
		.panel = &lcd_panel_rm68040,
		},
	[2]={
		.lcd_id = 0x3,
		.panel = &lcd_panel_hx8357,
		},
	[3]={
		.lcd_id = 0x4,
		.panel = &lcd_panel_ili9328,
		},
};

static int32_t lcm_send_cmd (uint32_t cmd)
{
	/* busy wait for ahb fifo full sign's disappearance */
	while(__raw_readl(LCM_STATUS) & 0x2);

	__raw_writel(cmd, LCM_CD0);

	return 0;
}

static int32_t lcm_send_cmd_data (uint32_t cmd, uint32_t data)
{
	/* busy wait for ahb fifo full sign's disappearance */
	while(__raw_readl(LCM_STATUS) & 0x2);

	__raw_writel(cmd, LCM_CD0);

	/* busy wait for ahb fifo full sign's disappearance */
	while(__raw_readl(LCM_STATUS) & 0x2);

	__raw_writel(data, LCM_DATA0);

	return 0;
}

static int32_t lcm_send_data (uint32_t data)
{
	/* busy wait for ahb fifo full sign's disappearance */
	while(__raw_readl(LCM_STATUS) & 0x2);

	__raw_writel(data, LCM_DATA0);

	return 0;
}

static uint32_t lcm_read_data ()
{
	volatile uint32_t i =32;
	/* busy wait for ahb fifo full sign's disappearance */
	while(__raw_readl(LCM_STATUS) & 0x2);
	/* bit18 means 'read' */
	__raw_writel(1<<18, LCM_DATA0);

	/* busy wait for ahb fifo full sign's disappearance */
	while(__raw_readl(LCM_STATUS) & 0x2);

	/*wait*/
	while(i--);
	return __raw_readl(LCM_RDDATA);
}

static struct ops_mcu lcm_mcu_ops = {
	.send_cmd = lcm_send_cmd,
	.send_cmd_data = lcm_send_cmd_data,
	.send_data = lcm_send_data,
	.read_data = lcm_read_data,
};

extern struct lcdc_manager lm; /* TEMP */
extern struct semaphore copybit_wait; /* TEMP */
static irqreturn_t lcdc_isr(int irq, void *data)
{
	uint32_t val ;
	struct sc8800fb_info *fb = (struct sc8800fb_info *)data;

	val = __raw_readl(LCDC_IRQ_STATUS);
	if (val & (1<<0)){      /* lcdc done */
		FB_PRINT("--> lcdc_isr lm.mode=%d\n", lm.mode);
			__raw_bits_or((1<<0), LCDC_IRQ_CLR);
			if(lm.mode == LMODE_DISPLAY) /* TEMP */
			rrm_interrupt(fb->rrm);
			else
				up(&copybit_wait);
	}

	return IRQ_HANDLED;
}

static void set_pins(void)
{	

}

static int32_t panel_reset(struct lcd_spec *self)
{
	//panel reset
	__raw_writel(0x1, LCM_RSTN);	
	mdelay(0x10);
	__raw_writel(0x0, LCM_RSTN);
	mdelay(0x10);
	__raw_writel(0x1, LCM_RSTN);
	mdelay(0x10);

	return 0;
}

static void lcdc_mcu_init(struct sc8800fb_info *sc8800fb)
{
	//LCDC module enable
	__raw_bits_or(1<<0, LCDC_CTRL);

	/*FMARK mode*/
	__raw_bits_or(1<<1, LCDC_CTRL);

	/*FMARK pol*/
	__raw_bits_and(~(1<<2), LCDC_CTRL);
	
	FB_PRINT("@fool2[%s] LCDC_CTRL: 0x%x\n", __FUNCTION__, __raw_readl(LCDC_CTRL));
	
	/* set background*/
	__raw_writel(0x0, LCDC_BG_COLOR);   //red

	FB_PRINT("@fool2[%s] LCDC_BG_COLOR: 0x%x\n", __FUNCTION__, __raw_readl(LCDC_BG_COLOR));

	/* dithering enable*/
	//__raw_bits_or(1<<4, LCDC_CTRL);   
	
}
static int mount_panel(struct sc8800fb_info *sc8800fb, struct lcd_spec *panel)
{
	/* TODO: check whether the mode/res are supported */

	sc8800fb->panel = panel;

	panel->info.mcu->ops = sc8800fb->ops;

	panel->ops->lcd_reset = panel_reset;
	
	return 0;
}
static int setup_fbmem(struct sc8800fb_info *sc8800fb, struct platform_device *pdev)
{
	uint32_t len, addr;
	
	len = sc8800fb->panel->width * sc8800fb->panel->height * (BITS_PER_PIXEL/8) * 2;

	/* the addr should be 8 byte align */
	addr = __get_free_pages(GFP_ATOMIC | __GFP_ZERO, get_order(len));
	if (!addr)
		return -ENOMEM;

	printk("sc8800g_fb got %d bytes mem at 0x%x\n", len, addr);
	sc8800fb->fb->fix.smem_start = __pa(addr);
	sc8800fb->fb->fix.smem_len = len;
	sc8800fb->fb->screen_base = (char*)addr;

	/* TEMP, software make-up for lcdc's 4-byte-align only limitation */
	fb_len = len;
	fb_pa = sc8800fb->fb->fix.smem_start;
	fb_va_cached = (unsigned int)sc8800fb->fb->screen_base;
	fb_va = (unsigned int)ioremap(sc8800fb->fb->fix.smem_start, 
		sc8800fb->fb->fix.smem_len);
	printk("sc8800g_fb fb_va=0x%x, size=%d\n", fb_va, sc8800fb->fb->fix.smem_len);
	/* TEMP, end */
	
	return 0;
}
static int sc8800fb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	if ((var->xres != info->var.xres) ||
	    (var->yres != info->var.yres) ||
	    (var->xres_virtual != info->var.xres_virtual) ||
	    (var->yres_virtual != info->var.yres_virtual) ||
	    (var->xoffset != info->var.xoffset) ||
	    (var->bits_per_pixel != info->var.bits_per_pixel) ||
	    (var->grayscale != info->var.grayscale))
		 return -EINVAL;
	return 0;
}

static void real_set_layer(void *data)
{
	struct fb_info *info = (struct fb_info *)data;
	uint32_t reg_val;
	uint32_t x,y;

	/* image layer base */
	reg_val = (info->var.yoffset == 0)?info->fix.smem_start:
		(info->fix.smem_start+ info->fix.smem_len/2);
	reg_val = (reg_val>>2) & 0x3fffffff;

	//this is for further Optimization
	if(info->var.reserved[0] == 0x6f766572)
	{	
		x = info->var.reserved[1] & 0xfffe;//make align
		y = info->var.reserved[1] >>16;

		reg_val += (x+y*info->var.xres)>>1;
	}
	//this is for further Optimization
	
	__raw_writel(reg_val, LCDC_OSD1_BASE_ADDR);
}

static void real_refresh(void *para)
{
	struct sc8800fb_info *sc8800fb = (struct sc8800fb_info *)para;
	uint32_t reg_val;
	struct fb_info *info = sc8800fb->fb;
	uint16_t left,top,width,height;

	if(info->var.reserved[0] == 0x6f766572)
	{	
		info->var.reserved[2] = (info->var.reserved[2] + ( info->var.reserved[1] & 1) +1) & 0xfffffffe;//make align
		info->var.reserved[1] &= 0xfffffffe;//make align

		//never use for more further Optimization
		//__raw_writel(info->var.reserved[1], LCDC_LCM_START);	
		__raw_writel(info->var.reserved[2], LCDC_LCM_SIZE);

		//this is for further Optimization
		//never use for more further Optimization
		//__raw_writel(info->var.reserved[1], LCDC_OSD1_DISP_XY);	
		__raw_writel(info->var.reserved[2], LCDC_OSD1_SIZE_XY);
		//this is for further Optimization

		//this is for more further Optimization
		__raw_writel(info->var.reserved[2], LCDC_DISP_SIZE);
		//this is for more further Optimization
		
		left = info->var.reserved[1] & 0xffff;
		top = info->var.reserved[1] >>16;
		width = info->var.reserved[2] & 0xffff;
		height = info->var.reserved[2] >>16;

		sc8800fb->panel->ops->lcd_invalidate_rect(sc8800fb->panel,left,top,left+width-1,top+height-1);
	}
	else
	{
		left = 0;
		top = 0;
		width = info->var.xres;
		height = info->var.yres;
		sc8800fb->panel->ops->lcd_invalidate(sc8800fb->panel);
	}

	reg_val = width * height;
	reg_val |= (1<<20); /* for device 0 */
	reg_val &=~ ((1<<26)|(1<<27) | (1<<28)); /* FIXME: hardcoded cs 1 */
	__raw_writel(reg_val, LCM_CTRL);
	__raw_bits_or((1<<3), LCDC_CTRL); /* start refresh */
}

static void real_display_callback(void * data)
{
	uint32_t reg_val;
	struct fb_info* info  = (struct fb_info*)data;
	//this is for more further Optimization
	reg_val = ( 640 & 0x3ff) | (( 640 & 0x3ff )<<16);
	__raw_writel(reg_val, LCDC_DISP_SIZE);
	//this is for more further Optimization
}

static int real_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	rrm_refresh(LID_OSD1, real_display_callback, info);

	FB_PRINT("@fool2[%s] LCDC_CTRL: 0x%x\n", __FUNCTION__, __raw_readl(LCDC_CTRL));
	FB_PRINT("@fool2[%s] LCDC_DISP_SIZE: 0x%x\n", __FUNCTION__, __raw_readl(LCDC_DISP_SIZE));
	FB_PRINT("@fool2[%s] LCDC_LCM_START: 0x%x\n", __FUNCTION__, __raw_readl(LCDC_LCM_START));
	FB_PRINT("@fool2[%s] LCDC_LCM_SIZE: 0x%x\n", __FUNCTION__, __raw_readl(LCDC_LCM_SIZE));
	FB_PRINT("@fool2[%s] LCDC_BG_COLOR: 0x%x\n", __FUNCTION__, __raw_readl(LCDC_BG_COLOR));

	FB_PRINT("@fool2[%s] LCDC_OSD1_CTRL: 0x%x\n", __FUNCTION__, __raw_readl(LCDC_OSD1_CTRL));
	FB_PRINT("@fool2[%s] LCDC_OSD1_BASE_ADDR: 0x%x\n", __FUNCTION__, __raw_readl(LCDC_OSD1_BASE_ADDR));
	FB_PRINT("@fool2[%s] LCDC_OSD1_ALPHA_BASE_ADDR: 0x%x\n", __FUNCTION__, __raw_readl(LCDC_OSD1_ALPHA_BASE_ADDR));
	FB_PRINT("@fool2[%s] LCDC_OSD1_SIZE_XY: 0x%x\n", __FUNCTION__, __raw_readl(LCDC_OSD1_SIZE_XY));
	FB_PRINT("@fool2[%s] LCDC_OSD1_PITCH: 0x%x\n", __FUNCTION__, __raw_readl(LCDC_OSD1_PITCH));
	FB_PRINT("@fool2[%s] LCDC_OSD1_DISP_XY: 0x%x\n", __FUNCTION__, __raw_readl(LCDC_OSD1_DISP_XY));
	FB_PRINT("@fool2[%s] LCDC_OSD1_ALPHA: 0x%x\n", __FUNCTION__, __raw_readl(LCDC_OSD1_ALPHA));
	FB_PRINT("@fool2[%s] LCDC_OSD1_GREY_RGB: 0x%x\n", __FUNCTION__, __raw_readl(LCDC_OSD1_GREY_RGB));
	FB_PRINT("@fool2[%s] LCDC_OSD1_CK: 0x%x\n", __FUNCTION__, __raw_readl(LCDC_OSD1_CK));

	FB_PRINT("@fool2[%s] LCM_CTRL: 0x%x\n", __FUNCTION__, __raw_readl(LCM_CTRL));
	FB_PRINT("@fool2[%s] LCM_PARAMETER0: 0x%x\n", __FUNCTION__, __raw_readl(LCM_PARAMETER0));
	FB_PRINT("@fool2[%s] LCM_PARAMETER1: 0x%x\n", __FUNCTION__, __raw_readl(LCM_PARAMETER1));
	FB_PRINT("@fool2[%s] LCM_IFMODE: 0x%x\n", __FUNCTION__, __raw_readl(LCM_IFMODE));
	FB_PRINT("@fool2[%s] LCM_RGBMODE: 0x%x\n", __FUNCTION__, __raw_readl(LCM_RGBMODE));
	return 0;
}

static struct fb_ops sc8800fb_ops = {
	.owner = THIS_MODULE,
	.fb_check_var = sc8800fb_check_var,
	.fb_pan_display = real_pan_display,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit,
};

static unsigned PP[16];
static void setup_fb_info(struct sc8800fb_info *sc8800fb)
{
	struct fb_info *fb= sc8800fb->fb;
	int r;

	fb->fbops = &sc8800fb_ops;
	fb->flags = FBINFO_DEFAULT;
	
	/* finish setting up the fb_info struct */
	strncpy(fb->fix.id, "sc8800fb", 16);
	fb->fix.ypanstep = 1;
	fb->fix.type = FB_TYPE_PACKED_PIXELS;
	fb->fix.visual = FB_VISUAL_TRUECOLOR;
	fb->fix.line_length = sc8800fb->panel->width * 16/8;
	if(sc8800fb->panel->ops->lcd_is_invalidaterect != 0)
	{
		if(sc8800fb->panel->ops->lcd_is_invalidaterect())
		{
			fb->fix.reserved[0] = 0x6f76;
			fb->fix.reserved[1] = 0x6572;
		}
	}

	fb->var.xres = sc8800fb->panel->width;
	fb->var.yres = sc8800fb->panel->height;
	fb->var.width = sc8800fb->panel->width;
	fb->var.height = sc8800fb->panel->height;
	fb->var.xres_virtual = sc8800fb->panel->width;
	fb->var.yres_virtual = sc8800fb->panel->height * 2;
	fb->var.bits_per_pixel = BITS_PER_PIXEL;
	fb->var.accel_flags = 0;

	fb->var.yoffset = 0;

	fb->var.red.offset = 11;
	fb->var.red.length = 5;
	fb->var.red.msb_right = 0;
	fb->var.green.offset = 5;
	fb->var.green.length = 6;
	fb->var.green.msb_right = 0;
	fb->var.blue.offset = 0;
	fb->var.blue.length = 5;
	fb->var.blue.msb_right = 0;

	r = fb_alloc_cmap(&fb->cmap, 16, 0);
	fb->pseudo_palette = PP;

	PP[0] = 0;
	for (r = 1; r < 16; r++)
		PP[r] = 0xffffffff;
}
static void lcdc_lcm_configure(struct sc8800fb_info *sc8800fb)
{
	uint32_t bits;
	
	//wait  until AHB FIFO is empty
	while(!(__raw_readl(LCM_STATUS) & (1<<2)));
	
	//__raw_writel(0, LCDC_LCM0PARAMETER0); /* LCM_PARAMETER0 */

	/* CS0 bus mode [BIT0]: 8080/6800 */
	switch (sc8800fb->panel->info.mcu->bus_mode) {
	case LCD_BUS_8080:
		bits = 0;
		break;
	case LCD_BUS_6800:
		bits = 1;
		break;
	default:
		bits = 0;
		break;
	}
	__raw_writel((bits&0x1), LCM_IFMODE);
	FB_PRINT("@fool2[%s] LCM_IFMODE: 0x%x\n", __FUNCTION__, __raw_readl(LCM_IFMODE));

	/* CS0 bus width [BIT1:0] */
	switch (sc8800fb->panel->info.mcu->bus_width) {
	case 16:
		bits = 0;
		break;
	case 18:
		bits = 1;
		break;
	case 8:
		bits = 2;
		break;
	case 9:
		bits = 3;
		break;
	default:
		bits = 0;
		break;
	}
	__raw_writel((bits&0x3), LCM_RGBMODE);
	FB_PRINT("@fool2[%s] LCM_RGBMODE: 0x%x\n", __FUNCTION__, __raw_readl(LCM_RGBMODE));

}
uint32_t CHIP_GetMcuClk (void)
{
	uint32_t clk_mcu_sel;
    	uint32_t clk_mcu = 0;
    	
         clk_mcu_sel = __raw_readl(AHB_ARM_CLK);
	clk_mcu_sel = (clk_mcu_sel >>  23)  &  0x3;
         switch (clk_mcu_sel)
         {
            case 0:
            	clk_mcu = 400000000;
            	break;
            case 1:
            	clk_mcu = 153600000;
            	break;
            case 2:
            	clk_mcu = 64000000;
            	break;
            case 3:
            	clk_mcu = 26000000;
            	break;
            default:
             	// can't go there
            	 break;
            }       
         return clk_mcu;
	
}
static void lcdc_update_lcm_timing(struct sc8800fb_info *sc8800fb)
{
	uint32_t  reg_value;
	uint32_t  ahb_div,ahb_clk;   
	uint32_t  ahb_clk_cycle;
	uint32_t rcss, rlpw, rhpw, wcss, wlpw, whpw;

	reg_value = __raw_readl(AHB_ARM_CLK);
	
	ahb_div = ( reg_value>>4 ) & 0x7;
	
	ahb_div = ahb_div + 1;

	if(__raw_readl (AHB_ARM_CLK) & (1<<30))
    	{
        		ahb_div = ahb_div << 1;
    	}
    	if(__raw_readl (AHB_ARM_CLK) & (1<<31))
    	{
        		ahb_div=ahb_div<<1;
    	}	
	ahb_clk = CHIP_GetMcuClk()/ahb_div;

	FB_PRINT("@fool2[%s] ahb_clk: 0x%x\n", __FUNCTION__, ahb_clk);

	/* LCD_UpdateTiming() */
	ahb_clk_cycle = (100000000 >> 17)/(ahb_clk >> 20 );

	rcss = ((sc8800fb->panel->info.mcu->timing->rcss/ahb_clk_cycle+1)<3)?
	       (sc8800fb->panel->info.mcu->timing->rcss/ahb_clk_cycle+1):3;
	rlpw = ((sc8800fb->panel->info.mcu->timing->rlpw/ahb_clk_cycle+1)<14)?
	       (sc8800fb->panel->info.mcu->timing->rlpw/ahb_clk_cycle+1):14;
	rhpw = ((sc8800fb->panel->info.mcu->timing->rhpw/ahb_clk_cycle+1)<14)?
	       (sc8800fb->panel->info.mcu->timing->rhpw/ahb_clk_cycle+1):14;
	wcss = ((sc8800fb->panel->info.mcu->timing->wcss/ahb_clk_cycle+1)<3)?
	       (sc8800fb->panel->info.mcu->timing->wcss/ahb_clk_cycle+1):3;
	wlpw = ((sc8800fb->panel->info.mcu->timing->wlpw/ahb_clk_cycle+1)<14)?
	       (sc8800fb->panel->info.mcu->timing->wlpw/ahb_clk_cycle+1):14;
	whpw = ((sc8800fb->panel->info.mcu->timing->whpw/ahb_clk_cycle+1)<14)?
	       (sc8800fb->panel->info.mcu->timing->whpw/ahb_clk_cycle+1):14;
	
	//wait  until AHB FIFO if empty
	while(!(__raw_readl(LCM_STATUS) & (1<<2)));
	
	/*   LCDC_ChangePulseWidth() */
	__raw_writel(whpw |(wlpw<<4) |(wcss<<8) | (rhpw<<10) |	(rlpw<<14) |(rcss<<18),LCM_PARAMETER0); /* FIXME: hardcoded for !CS0 */

	//__raw_writel( 0x77555, LCDC_LCMPARAMETER0); /* @fool2, tmp */
	FB_PRINT("@fool2[%s] LCM_PARAMETER0: 0x%x\n", __FUNCTION__, __raw_readl(LCM_PARAMETER0));
}
static inline int set_lcdsize( struct fb_info *info)
{
	uint32_t reg_val;
	
	//reg_val = ( info->var.xres & 0x3ff) | (( info->var.yres & 0x3ff )<<16);
	reg_val = ( 640 & 0x3ff) | (( 640 & 0x3ff )<<16);
	__raw_writel(reg_val, LCDC_DISP_SIZE);
	
	FB_PRINT("@fool2[%s] LCDC_DISP_SIZE: 0x%x\n", __FUNCTION__, __raw_readl(LCDC_DISP_SIZE));

	return 0;
}
static inline int set_lcmrect( struct fb_info *info)
{
	uint32_t reg_val;
	
	__raw_writel(0, LCDC_LCM_START);
	
	reg_val = ( info->var.xres & 0x3ff) | (( info->var.yres & 0x3ff )<<16);
	__raw_writel(reg_val, LCDC_LCM_SIZE);
	
	FB_PRINT("@fool2[%s] LCDC_LCM_START: 0x%x\n", __FUNCTION__, __raw_readl(LCDC_LCM_START));
	FB_PRINT("@fool2[%s] LCDC_LCM_SIZE: 0x%x\n", __FUNCTION__, __raw_readl(LCDC_LCM_SIZE));

	return 0;
}
int set_lcdc_layers(struct fb_var_screeninfo *var, struct fb_info *info)
{
	uint32_t reg_val;
    /******************* OSD1 layer setting **********************/
#if 1
    {
	/* we assume that
	 * 1. there's only one fbdev, and only block0 is used
	 * 2. the pan operation is a sync one
	 */

	__raw_bits_and(~(1<<0),LCDC_IMG_CTRL);  
	__raw_bits_and(~(1<<0),LCDC_OSD2_CTRL);  
	__raw_bits_and(~(1<<0),LCDC_OSD3_CTRL);  
	__raw_bits_and(~(1<<0),LCDC_OSD4_CTRL);  
	__raw_bits_and(~(1<<0),LCDC_OSD5_CTRL);  
	/*enable OSD1 layer*/
	__raw_bits_or((1<<0),LCDC_OSD1_CTRL);  

    /*color key */
	__raw_bits_and(~(1<<1),LCDC_OSD1_CTRL);  //disable
    
	/*alpha mode select*/
	__raw_bits_or((1<<2),LCDC_OSD1_CTRL);  //block alpha

    /*data format*/
	__raw_bits_and(~(1<<6),LCDC_OSD1_CTRL);  //RGB565
	__raw_bits_or(1<<5,LCDC_OSD1_CTRL);  
	__raw_bits_and(~(1<<4),LCDC_OSD1_CTRL);  
	__raw_bits_or(1<<3,LCDC_OSD1_CTRL);  	

    /*data endian*/
	__raw_bits_or(1<<8,LCDC_OSD1_CTRL);  //little endian
	__raw_bits_and(~(1<<7),LCDC_OSD1_CTRL);  
	
    /*alpha endian*/
    /*
    __raw_bits_or(1<<10,LCDC_IMG_CTRL);  
	__raw_bits_and(~(1<<9),LCDC_IMG_CTRL);  
	*/
	
	FB_PRINT("@fool2[%s] LCDC_OSD1_CTRL: 0x%x\n", __FUNCTION__, __raw_readl(LCDC_OSD1_CTRL));

	/* OSD1 layer base */
	reg_val = (info->var.yoffset)?info->fix.smem_start:	(info->fix.smem_start+ info->fix.smem_len/2);
	reg_val = (reg_val >>2)& 0x3fffffff;
	__raw_writel(reg_val, LCDC_OSD1_BASE_ADDR);
	
    FB_PRINT("@fool2[%s] LCDC_OSD1_BASE_ADDR: 0x%x\n", __FUNCTION__, __raw_readl(LCDC_OSD1_BASE_ADDR));

    /*OSD1 layer alpha value*/
	__raw_writel(0xff, LCDC_OSD1_ALPHA);

	FB_PRINT("@fool2[%s] LCDC_OSD1_ALPHA: 0x%x\n", __FUNCTION__, __raw_readl(LCDC_OSD1_ALPHA));
    
    /*alpha base addr*/
	//__raw_writel(reg_val, LCDC_OSD1_ALPHA_BASE_ADDR); 
	
	/*OSD1 layer size*/
	reg_val = ( info->var.xres & 0x3ff) | (( info->var.yres & 0x3ff )<<16);
	__raw_writel(reg_val, LCDC_OSD1_SIZE_XY);

	FB_PRINT("@fool2[%s] LCDC_OSD1_SIZE_XY: 0x%x\n", __FUNCTION__, __raw_readl(LCDC_OSD1_SIZE_XY));
	
	/*OSD1 layer start position*/
	__raw_writel(0, LCDC_OSD1_DISP_XY);
	
	FB_PRINT("@fool2[%s] LCDC_OSD1_DISP_XY: 0x%x\n", __FUNCTION__, __raw_readl(LCDC_OSD1_DISP_XY));
	
    /*OSD1 layer pitch*/
	reg_val = ( info->var.xres & 0x3ff) ;
	__raw_writel(reg_val, LCDC_OSD1_PITCH);
	
	FB_PRINT("@fool2[%s] LCDC_OSD1_PITCH: 0x%x\n", __FUNCTION__, __raw_readl(LCDC_OSD1_PITCH));

    /*OSD1 color_key value*/
    //Fix me
    /*OSD1 grey RGB*/
    //Fix me
    }
#endif
    /******************** Image layer settting **********************/
#if 0
	{
    /* we assume that
	 * 1. there's only one fbdev, and only block0 is used
	 * 2. the pan operation is a sync one
	 */

	__raw_bits_and(~(1<<0),LCDC_OSD1_CTRL);  
	__raw_bits_and(~(1<<0),LCDC_OSD2_CTRL);  
	__raw_bits_and(~(1<<0),LCDC_OSD3_CTRL);  
	__raw_bits_and(~(1<<0),LCDC_OSD4_CTRL);  
	__raw_bits_and(~(1<<0),LCDC_OSD5_CTRL);  
	/*enable imge layer*/
	__raw_bits_or((1<<0),LCDC_IMG_CTRL);  

	/*little endian*/
	__raw_bits_or(1<<6,LCDC_IMG_CTRL);  
	__raw_bits_and(~(1<<5),LCDC_IMG_CTRL);  
	//__raw_bits_or(1<<7,LCDC_IMG_CTRL);  
	//__raw_bits_and(~(1<<8),LCDC_IMG_CTRL);  
	
	/*data format*/
	__raw_bits_or(1<<1,LCDC_IMG_CTRL);  //RGB565
	__raw_bits_and(~(1<<2),LCDC_IMG_CTRL);  
	__raw_bits_or(1<<3,LCDC_IMG_CTRL);  
	__raw_bits_and(~(1<<4),LCDC_IMG_CTRL);  	

	FB_PRINT("@fool2[%s] LCDC_IMG_CTRL: 0x%x\n", __FUNCTION__, __raw_readl(LCDC_IMG_CTRL));
	
	/* image layer base */
	reg_val = (info->var.yoffset)?info->fix.smem_start:	(info->fix.smem_start+ info->fix.smem_len/2);
	reg_val = (reg_val >>2)& 0x3fffffff;
	__raw_writel(reg_val, LCDC_IMG_Y_BASE_ADDR);

	FB_PRINT("@fool2[%s] LCDC_IMG_Y_BASE_ADDR: 0x%x\n", __FUNCTION__, __raw_readl(LCDC_IMG_Y_BASE_ADDR));
	
	/*image layer size*/
	reg_val = ( info->var.xres & 0x3ff) | (( info->var.yres & 0x3ff )<<16);
	__raw_writel(reg_val, LCDC_IMG_SIZE_XY);

	FB_PRINT("@fool2[%s] LCDC_IMG_SIZE_XY: 0x%x\n", __FUNCTION__, __raw_readl(LCDC_IMG_SIZE_XY));
	
	/*image layer start position*/
	__raw_writel(0, LCDC_IMG_DISP_XY);
	
	/*image layer pitch*/
	reg_val = ( info->var.xres & 0x3ff) ;
	__raw_writel(reg_val, LCDC_IMG_PITCH);
	
	FB_PRINT("@fool2[%s] LCDC_IMG_DISP_XY: 0x%x\n", __FUNCTION__, __raw_readl(LCDC_IMG_DISP_XY));
    }
#endif

	/*LCDC workplane size*/
	set_lcdsize(info);
	
	/*LCDC LCM rect size*/
	set_lcmrect(info);

	return 0;
}



static void hw_early_init(struct sc8800fb_info *sc8800fb)
{
	int ret;

	set_pins();

	/* prepare clock stuff */
	sc8800fb->clk_lcdc = clk_get(NULL, "clk_lcdc");
	if (IS_ERR(sc8800fb->clk_lcdc))
		printk(KERN_ERR "lcdc: failed to get clk!\n");

	if (clk_set_rate(sc8800fb->clk_lcdc, DEF_CLOCK))
		printk(KERN_ERR "lcdc: failed to set clk!\n");

	if (clk_enable(sc8800fb->clk_lcdc))
		printk(KERN_ERR "lcdc: failed to enable clk!\n");

	//LCD soft reset
	//__raw_bits_or(1<<3, AHB_SOFT_RST);
	//mdelay(10);	
	//__raw_bits_and(~(1<<3), AHB_SOFT_RST); 

	__raw_bits_and(~(1<<0), LCDC_IRQ_EN);
	__raw_bits_or((1<<0), LCDC_IRQ_CLR);

	/* register isr */
	ret = request_irq(IRQ_LCDC_INT, lcdc_isr, IRQF_DISABLED, "LCDC", sc8800fb);
	if (ret) {
		printk(KERN_ERR "lcdc: failed to request irq!\n");
		return;
	}

	/* enable LCDC_DONE IRQ */
	//__raw_bits_or((1<<0), LCDC_IRQ_EN);

	/* init lcdc mcu mode using default configuration */
	lcdc_mcu_init(sc8800fb);
}

static void hw_init(struct sc8800fb_info *sc8800fb)
{
	/* only MCU mode is supported currently */
	if (LCD_MODE_RGB == sc8800fb->panel->mode)
		return;

	//panel reset
	if(sc8800fb->need_reinit)
	{
	    panel_reset(sc8800fb->panel);
	}

	/* set lcdc-lcd interface parameters */
	lcdc_lcm_configure(sc8800fb);

	/* set timing parameters for LCD */
	lcdc_update_lcm_timing(sc8800fb);
}

static void hw_later_init(struct sc8800fb_info *sc8800fb)
{
	/* init mounted lcd panel */
	if(sc8800fb->need_reinit)
	{
	    sc8800fb->panel->ops->lcd_init(sc8800fb->panel);
	}

	set_lcdc_layers(&sc8800fb->fb->var, sc8800fb->fb); 
	__raw_bits_or((1<<0), LCDC_IRQ_EN);
}


static void set_backlight(uint32_t value)
{
	
	ANA_REG_AND(WHTLED_CTL, ~(WHTLED_PD_SET | WHTLED_PD_RST));
	ANA_REG_OR(WHTLED_CTL,  WHTLED_PD_RST);
	ANA_REG_MSK_OR (WHTLED_CTL, ( (value << WHTLED_V_SHIFT) &WHTLED_V_MSK), WHTLED_V_MSK);
}

#ifdef TEST_RRM
void test_set_layer(void *data)
{
	uint32_t reg_val = (uint32_t)data;

	/* image layer base */
	reg_val = (reg_val >>2)& 0x3fffffff;
	__raw_writel(reg_val, LCDC_IMG_Y_BASE_ADDR);
}

static int rrm_test_thread(void * data)
{
	struct fb_info *fb = (struct fb_info *)data;
	struct sc8800fb_info *sc8800fb = fb->par;
	uint32_t len, addr;
	int i = 0;
	uint32_t base[3];

	len = sc8800fb->panel->width * sc8800fb->panel->height *
		 (BITS_PER_PIXEL/8) * 3;
	printk("rrm_test_thread started!\n");

	/* the addr should be 8 byte align */
	addr = __get_free_pages(GFP_ATOMIC | __GFP_ZERO, get_order(len));
	if (!addr)
		return -ENOMEM;
	
	if (1) {
		unsigned short *ptr = (unsigned short*)addr;
		int l = len/3 /2  /3 ; /* 1/3 frame pixels */
		int offset;

		base[0] = __pa(ptr);
		printk("rrm_test_thread ptr:%x base[0]:%x\n", ptr, base[0]);
		for(offset=0;offset< l;offset++)
			(* (volatile unsigned short *)(ptr++))= 0xf800; //red 
		for(offset=0;offset< l;offset++)
			(* (volatile unsigned short *)(ptr++))= 0x07e0; //green
		for(offset=0;offset< l;offset++)
			(* (volatile unsigned short *)(ptr++))= 0x001f; //blue
		/* now, the second frame */
		base[1] = __pa(ptr);
		printk("rrm_test_thread ptr:%x base[1]:%x\n", ptr, base[1]);
		for(offset=0;offset< l;offset++)
			(* (volatile unsigned short *)(ptr++))= 0x001f; 
		for(offset=0;offset< l;offset++)
			(* (volatile unsigned short *)(ptr++))= 0xf800; 
		for(offset=0;offset< l;offset++)
			(* (volatile unsigned short *)(ptr++))= 0x07e0; 
		/* now, the third */
		base[2] = __pa(ptr);
		printk("rrm_test_thread ptr:%x base[2]:%x\n", ptr, base[2]);
		for(offset=0;offset< l;offset++)
			(* (volatile unsigned short *)(ptr++))= 0x07e0; 
		for(offset=0;offset< l;offset++)
			(* (volatile unsigned short *)(ptr++))= 0x001f; 
		for(offset=0;offset< l;offset++)
			(* (volatile unsigned short *)(ptr++))= 0xf800; 

	}

	printk("rrm_test_thread call rrm_layer_init()\n");
	rrm_layer_init(LID_VIDEO, 3, test_set_layer);

	printk("rrm_test_thread start looping\n");
	while (1)
	{
		rrm_refresh(LID_VIDEO, NULL, (void *)base[i++]);
		msleep(100);
		if(i == 3) i=0;
	}
}

#include <linux/kthread.h>
static void setup_rrm_test(struct fb_info *fb)
{
	uint32_t reg_val;

	/*enable imge layer*/
	__raw_bits_or((1<<0),LCDC_IMG_CTRL);  

	/*little endian*/
	__raw_bits_or(1<<6,LCDC_IMG_CTRL);  
	__raw_bits_and(~(1<<5),LCDC_IMG_CTRL);  

	/*data format*/
	__raw_bits_or(1<<1,LCDC_IMG_CTRL);  //RGB565
	__raw_bits_and(~(1<<2),LCDC_IMG_CTRL);  
	__raw_bits_or(1<<3,LCDC_IMG_CTRL);  
	__raw_bits_and(~(1<<4),LCDC_IMG_CTRL);  	

	/*image layer size*/
	reg_val = ( fb->var.xres & 0x3ff) | (( fb->var.yres & 0x3ff )<<16);
	__raw_writel(reg_val, LCDC_IMG_SIZE_XY);

	/*image layer start position*/
	__raw_writel(0, LCDC_IMG_DISP_XY);

	/*image layer pitch*/
	reg_val = ( fb->var.xres & 0x3ff) ;
	__raw_writel(reg_val, LCDC_IMG_PITCH);

	/* OSD1 block alpha */
	__raw_bits_or((1<<2),LCDC_OSD1_CTRL);  

	/* OSD1 alpha 50% */
	__raw_writel((0xff/2), LCDC_OSD1_ALPHA);

	/* initiate the test thread */
	kernel_thread(rrm_test_thread, fb, 0);
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void sc8800fb_early_suspend (struct early_suspend* es)
{
	struct sc8800fb_info *sc8800fb = container_of(es, struct sc8800fb_info, early_suspend);
	if(sc8800fb->panel->ops->lcd_enter_sleep != NULL){
		sc8800fb->panel->ops->lcd_enter_sleep(sc8800fb->panel,1);
	}

	clk_disable(sc8800fb->clk_lcdc);
}

static void sc8800fb_early_resume (struct early_suspend* es)
{
	struct sc8800fb_info *sc8800fb = container_of(es, struct sc8800fb_info, early_suspend);

	if (clk_enable(sc8800fb->clk_lcdc))
		printk(KERN_ERR "lcdc: failed to enable clk!\n");

	if(sc8800fb->panel->ops->lcd_enter_sleep != NULL){
		sc8800fb->panel->ops->lcd_enter_sleep(sc8800fb->panel,0);
	}
}
#endif


static uint32_t lcd_id_from_uboot = 0;


static int __init calibration_start(char *str)
{
	if((str[0]=='I')&&(str[1]=='D'))
	{
		lcd_id_from_uboot = (((uint32_t)(str[2]))&0xff)<<8;
		lcd_id_from_uboot |= ((uint32_t)(str[3]))&0xff;
	}
	else
	{
		lcd_id_from_uboot = 0;
	}
        return 1;
}
__setup("lcd_id=", calibration_start);


static int find_adapt_from_uboot()
{
	int i;
	if(lcd_id_from_uboot != 0)
	{
		for(i = 0;i<(sizeof(lcd_panel))/(sizeof(lcd_panel[0]));i++)
		{
			if(lcd_id_from_uboot == lcd_panel[i].lcd_id)
			{
				return i;
			}
		}
	}
	return -1;		
}

static int lcd_readid_default(struct lcd_spec *self)
{
	uint32_t dummy;
	//default id reg is 0
	self->info.mcu->ops->send_cmd(0x0);

	if(self->info.mcu->bus_width == 8)
	{
		dummy = (self->info.mcu->ops->read_data())&0xff;
		dummy <<= 8;
		dummy |= (self->info.mcu->ops->read_data())&0xff;
	}
	else
	{
		dummy = (self->info.mcu->ops->read_data())&0xffff;
	}
	return dummy;
}


static int find_adapt_from_readid(struct sc8800fb_info *sc8800fb)
{
	int i;
	uint32_t id;
	for(i = 0;i<(sizeof(lcd_panel))/(sizeof(lcd_panel[0]));i++)
	{
		//first ,try mount
		mount_panel(sc8800fb,lcd_panel[i].panel);
		//hw init to every panel
		hw_init(sc8800fb);
		//readid
		if(sc8800fb->panel->ops->lcd_readid)
		{
			id = sc8800fb->panel->ops->lcd_readid(sc8800fb->panel);
		}
		else
		{
			id = lcd_readid_default(sc8800fb->panel);
		}
		//if the id is right?
		if(id == lcd_panel[i].lcd_id)
		{
			return i;
		}
	}
	return -1;		
}


#define ANA_INT_EN (SPRD_MISC_BASE+0x380+0x08)

static int sc8800fb_probe(struct platform_device *pdev)
{
	struct fb_info *fb;
	struct sc8800fb_info *sc8800fb;
	int32_t ret;
	int lcd_adapt;

	FB_PRINT("@fool2[%s]\n", __FUNCTION__);
	printk("sc8800g_fb initialize!\n");

	lm_init(4); /* TEMP */

	fb = framebuffer_alloc(sizeof(struct sc8800fb_info), &pdev->dev);
	if (!fb)
		return -ENOMEM;
	sc8800fb = fb->par;
	sc8800fb->fb = fb;
	sc8800fb->ops = &lcm_mcu_ops;
	sc8800fb->rrm = rrm_init(real_refresh, (void*)sc8800fb);
	rrm_layer_init(LID_OSD1, 2, real_set_layer);


	//we maybe readid ,so hardware should be init
	sc8800fb->need_reinit = 1;
	hw_early_init(sc8800fb);

//make sure we have no wrong on 8805 or openphone .cause have no chance to test on every plateform
#ifdef CONFIG_MACH_SP6810A
	lcd_adapt = find_adapt_from_uboot();
	if(lcd_adapt == -1)
	{
		lcd_adapt = find_adapt_from_readid(sc8800fb);
	}
	else
	{
		sc8800fb->need_reinit = 0;
	}
	if(lcd_adapt == -1)
	{
		lcd_adapt = 0;
	}
#endif
#ifdef CONFIG_MACH_SP8805GA
	lcd_adapt = 0;
	sc8800fb->need_reinit = 0;
#endif
#ifdef CONFIG_MACH_OPENPHONE
	lcd_adapt = 2;
	sc8800fb->need_reinit = 0;
#endif
#ifdef CONFIG_MACH_G2PHONE
	lcd_adapt = 3;
	sc8800fb->need_reinit = 0;
#endif

	ret = mount_panel(sc8800fb, lcd_panel[lcd_adapt].panel);
	if (ret) {
		printk(KERN_ERR "unsupported panel!!");
		return -EFAULT;
	}

	ret = setup_fbmem(sc8800fb, pdev);
	if (ret)
		return ret;

	setup_fb_info(sc8800fb);

	ret = register_framebuffer(fb);
	if (ret) {
		framebuffer_release(fb);
		return ret;
	}

	hw_init(sc8800fb);
	hw_later_init(sc8800fb);


	copybit_lcdc_init(); /* TEMP */

	/* FIXME: put the BL stuff to where it belongs. */
	/* richard feng delete the following line */
    /* set_backlight(50);*/
	platform_set_drvdata(pdev, sc8800fb);
#ifdef CONFIG_HAS_EARLYSUSPEND
	sc8800fb->early_suspend.suspend = sc8800fb_early_suspend;
	sc8800fb->early_suspend.resume  = sc8800fb_early_resume;
	sc8800fb->early_suspend.level   = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	register_early_suspend(&sc8800fb->early_suspend);
#endif
	if(0){ /* in-kernel test code */
		struct fb_info test_info;
		int size = sc8800fb->fb->var.xres * sc8800fb->fb->var.yres *2;
		short adie_chip_id = ANA_REG_GET(ANA_ADIE_CHIP_ID);

		/* set color */
		if (adie_chip_id == 0) {
			unsigned short *ptr=(unsigned short*)sc8800fb->fb->screen_base;
			int len = size/2 /3 ; /* 1/3 frame pixels */
			int offset;

			for(offset=0;offset< len;offset++)
				(* (volatile unsigned short *)(ptr++))= 0xf800; //red 
			for(offset=0;offset< len;offset++)
				(* (volatile unsigned short *)(ptr++))= 0x07e0; //green
			for(offset=0;offset< len;offset++)
				(* (volatile unsigned short *)(ptr++))= 0x001f; //blue
			/* now, the second frame */
			for(offset=0;offset< len;offset++)
				(* (volatile unsigned short *)(ptr++))= 0xf800; 
			for(offset=0;offset< len;offset++)
				(* (volatile unsigned short *)(ptr++))= 0x07e0; 
			for(offset=0;offset< len;offset++)
				(* (volatile unsigned short *)(ptr++))= 0x001f; 
		} else {
			unsigned short *ptr=(unsigned short*)sc8800fb->fb->screen_base;
			int len = size ; /* 1/3 frame pixels */
			int offset;

			for(offset=0;offset< len;offset++)
				(* (volatile unsigned short *)(ptr++))= 0xf800; //red 

			ANA_REG_OR(ANA_INT_EN, 0x1f);
		}

		/* pan display */
		test_info = *sc8800fb->fb;

#ifndef TEST_RRM
		real_pan_display(&sc8800fb->fb->var, &test_info);
#endif
	}
	short adie_chip_id = ANA_REG_GET(ANA_ADIE_CHIP_ID);
	if (adie_chip_id != 0) 
		ANA_REG_OR(ANA_INT_EN, 0x1f);

#ifdef TEST_RRM
	setup_rrm_test(fb);
#endif

	return 0;
}
static int sc8800fb_suspend(struct platform_device *pdev,pm_message_t state)
{
	struct sc8800fb_info *sc8800fb = platform_get_drvdata(pdev);
	if(sc8800fb->panel->ops->lcd_enter_sleep != NULL)
		sc8800fb->panel->ops->lcd_enter_sleep(sc8800fb->panel,1);

	clk_disable(sc8800fb->clk_lcdc);

	return 0;
}
static int sc8800fb_resume(struct platform_device *pdev)
{
	struct sc8800fb_info *sc8800fb = platform_get_drvdata(pdev);

	if (clk_enable(sc8800fb->clk_lcdc))
		printk(KERN_ERR "lcdc: failed to enable clk!\n");

	if(sc8800fb->panel->ops->lcd_enter_sleep != NULL)
		sc8800fb->panel->ops->lcd_enter_sleep(sc8800fb->panel,0);
	return 0;
}
static struct platform_driver sc8800fb_driver = {
	.probe = sc8800fb_probe,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend = sc8800fb_suspend,
	.resume = sc8800fb_resume,
#endif
	.driver = {
		.name = "sc8800fb",
		.owner = THIS_MODULE,
	},
};
static int __init sc8800fb_init(void)
{
	FB_PRINT("@fool2[%s]\n", __FUNCTION__);
	return platform_driver_register(&sc8800fb_driver);
}

module_init(sc8800fb_init);

