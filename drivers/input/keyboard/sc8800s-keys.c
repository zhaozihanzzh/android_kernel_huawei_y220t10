/*
 * File:         drivers/input/keyboard/sc8800s-keys.c
 * Based on:
 * Author:       Richard Feng <Richard Feng@spreadtrum.com>
 *
 * Created:
 * Description:  keypad driver for sc8800s Processors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <linux/module.h>

#include <linux/init.h>


#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <mach/mfp.h>
#include "sprd_key.h"
#include "regs_kpd_sc8800s.h"

#define DRV_NAME        	"sprd-keys"

#define INT_STS                 (SPRD_INTCV_BASE + 0x0004)
#define INT_EN                  (SPRD_INTCV_BASE + 0x0010)
#define INT_EN_CLR              (SPRD_INTCV_BASE + 0x0014)

#define REG_INT_STS             (*((volatile unsigned int *)INT_STS))
#define REG_INT_EN              (*((volatile unsigned int *)INT_EN))
#define REG_INT_EN_CLR          (*((volatile unsigned int *)INT_EN_CLR))

#define GR_GEN0                 (SPRD_GREG_BASE + 0x0008)
#define REG_GR_GEN0             (*((volatile unsigned int *)GR_GEN0))
#define REG_CPC_KEYOUT0         (*((volatile unsigned int *)CPC_KEYOUT0_REG))
#define REG_CPC_KEYOUT1         (*((volatile unsigned int *)CPC_KEYOUT1_REG))
#define REG_CPC_KEYOUT2         (*((volatile unsigned int *)CPC_KEYOUT2_REG))
#define REG_CPC_KEYOUT3         (*((volatile unsigned int *)CPC_KEYOUT3_REG))
#define REG_CPC_KEYOUT4         (*((volatile unsigned int *)CPC_KEYOUT4_REG))
#define REG_CPC_KEYOUT5         (*((volatile unsigned int *)CPC_KEYOUT5_REG))
#define REG_CPC_KEYOUT6         (*((volatile unsigned int *)CPC_KEYOUT6_REG))
#define REG_CPC_KEYOUT7         (*((volatile unsigned int *)CPC_KEYOUT7_REG))

#define REG_CPC_KEYIN0          (*((volatile unsigned int *)CPC_KEYIN0_REG))
#define REG_CPC_KEYIN1          (*((volatile unsigned int *)CPC_KEYIN1_REG))
#define REG_CPC_KEYIN2          (*((volatile unsigned int *)CPC_KEYIN2_REG))
#define REG_CPC_KEYIN3          (*((volatile unsigned int *)CPC_KEYIN3_REG))
#define REG_CPC_KEYIN4          (*((volatile unsigned int *)CPC_KEYIN4_REG))

#define KPDPOLARITY_ROW         (0x00ff)
#define KPDPOLARITY_COL         (0xff00)
#define CFG_ROW_POLARITY        (0x00ff & KPDPOLARITY_ROW)
#define CFG_COL_POLARITY        (0x1f00 & KPDPOLARITY_COL)

/* the corresponding bit of KPD_STS register */
#define KPDSTS_KPD          	(1 << 0)    //Keypad interrupt
#define KPDSTS_TOUT         	(1 << 1)    //time out interrupt
#define KPDSTS_ROW_COUNT    	(7 << 2)    //row counter
#define KPDSTS_COL_COUNT    	(7 << 5)    //column counter

#define KPDICLR_KPD_INT     	(1 << 0)
#define KPDICLR_PB_INT	     	(3 << 0)
#define KPDICLR_TOUT_INT    	(1 << 1)

//The corresponding bit of KPD_CTL register.
#define KPDCTL_KPD_INT      	(0x1 << 0)  //Keypad interrupt enable
#define KPDCTL_TOUT_INT     	(0x1 << 1)  //Time out interrupt enable
#define KPDCTL_KPD          	(0x1 << 2)  //Keypad enable
#define KPDCTL_READ_ICLR    	(0x1 << 3)  //Write 1 to it, INT will be cleared

#define KPD_ROW_NUM             6 /* from row0 to row5*/
#define KPD_COL_NUM             5 /* from col0 to col4 */

#define KPD_ROW_MIN_NUM         4  /* when config keypad type, the value of */
#define KPD_ROW_MAX_NUM         8  /* row should be one of 4/5/6/7/8 */
#define KPD_COL_MIN_NUM         3  /* when config keypad type, the value of */
#define KPD_COL_MAX_NUM         5  /* col should be one of 3/4/5 */

#define KPDCTL_ROW              (0xf << 4)  /* enable bit for rows(row4 --- row7) */
#define KPDCTL_COL              (0x3 << 8)  /* enable bit for cols(col3 --- col4) */

#define CFG_KEY_TYPE            ((((~(0xf << (KPD_COL_NUM - KPD_COL_MIN_NUM))) << 8) \
                                | ((~(0xf << (KPD_ROW_NUM - KPD_ROW_MIN_NUM))) << 4)) \
                                & (KPDCTL_ROW | KPDCTL_COL))

#define MAX_MUL_KEY_NUM		3
/* keypad constant */
#define TB_KPD_CONST_BASE       (0x80)
#define TB_KPD_RELEASED         (TB_KPD_CONST_BASE)
#define TB_KPD_PRESSED          (TB_KPD_CONST_BASE + 1)
#define TB_KPD_INVALID_KEY      (0x0FFFF)

#define CHECK_TIMER_EXPIRE      20 // ms
#define PB_TIMER_EXPIRE		20 // ms
#define PB_TIMER_COUNTER	10  /// ms

/*this time shoud bigger than 5 ms */
#define QUIVER_TIME_OUT         50
/* 1.2ms * (11 + 1) = 15ms  delete shrinking time is 15ms */
#define AVOID_QUIVER_MIN_COUNT  5

/* check if this key is the same as the previous one */
#define IS_KEY_VALID(_key)      ((_key == key_ptr->key_code) ? 1 : 0)
/* check if this key is shaking too long */
#define IS_TIME_OUT(_key_ptr)   (((jiffies - _key_ptr->time_stamp) >= QUIVER_TIME_OUT) ? 1 : 0)

#define SCAN2KEYVAl(codeval) ((((codeval) & 0x38) << 5) | ((codeval) & 0x7))

/* use power button */
#define PB_INT_ENABLE		1

static const unsigned int sprd_keymap[] = {
        // 0 col
	KEYVAL(0, 0, KEY_BACK),
        KEYVAL(0, 1, KEY_UP),
        KEYVAL(0, 2, KEY_CAMERA),
        KEYVAL(0, 3, KEY_7),
        KEYVAL(0, 4, KEY_VOLUMEDOWN),
        KEYVAL(0, 5, KEY_PAUSECD),
        // 1 col
        KEYVAL(1, 0, KEY_RIGHT),
        //KEYVAL(1, 1, KEY_OK),
	KEYVAL(1, 1, KEY_ENTER),
        KEYVAL(1, 2, KEY_LEFT),
        //KEYVAL(1, 3, KEY_HELP), //KEY_HELP is not known
	KEYVAL(1, 3, KEY_ENTER),
        KEYVAL(1, 4, KEY_VOLUMEUP),
        KEYVAL(1, 5, KEY_BACK),
        // 2 col
        KEYVAL(2, 0, KEY_HELP), //KEY_HELP is not known
        KEYVAL(2, 1, KEY_3),
        KEYVAL(2, 2, KEY_6),
        KEYVAL(2, 3, KEY_9),
        KEYVAL(2, 4, KEY_KPDOT), // is #
        KEYVAL(2, 5, KEY_FORWARD),
        // 3 col
        KEYVAL(3, 0, KEY_SEND),
        KEYVAL(3, 1, KEY_1),
        KEYVAL(3, 2, KEY_4),
        //KEYVAL(3, 3, KEY_1),
        KEYVAL(3, 4, KEY_KPASTERISK), //is *
        KEYVAL(3, 5, KEY_MENU),
        // 4 col
        KEYVAL(4, 0, KEY_DOWN),
        KEYVAL(4, 1, KEY_2),
        KEYVAL(4, 2, KEY_5),
        KEYVAL(4, 3, KEY_8),
        KEYVAL(4, 4, KEY_0),
};

static struct sprd_kpad_platform_data sprd_kpad_data = {
        .rows                   = 6,
        .cols                   = 5,
        .keymap                 = sprd_keymap,
        .keymapsize             = ARRAY_SIZE(sprd_keymap),
        .repeat                 = 0,
        .debounce_time          = 5000, /* ns (5ms) */
        .coldrive_time          = 1000, /* ns (1ms) */
        .keyup_test_interval    = 50, /* 50 ms (50ms) */
};

static unsigned long keypad_func_cfg[] = {
	MFP_CFG_X(KEYOUT0, AF0, DS1, PULL_NONE, IO_OE),
	MFP_CFG_X(KEYOUT1, AF0, DS1, PULL_NONE, IO_OE),
	MFP_CFG_X(KEYOUT2, AF0, DS1, PULL_NONE, IO_OE),
	MFP_CFG_X(KEYOUT3, AF0, DS1, PULL_NONE, IO_OE),
	MFP_CFG_X(KEYOUT4, AF0, DS1, PULL_NONE, IO_OE),
	MFP_CFG_X(KEYOUT5, AF0, DS1, PULL_NONE, IO_OE),
	MFP_CFG_X(KEYOUT6, AF0, DS1, PULL_NONE, IO_OE),
	MFP_CFG_X(KEYOUT7, AF0, DS1, PULL_NONE, IO_OE),
	MFP_CFG_X(KEYIN0,  AF0, DS3, PULL_UP, IO_IE),
	MFP_CFG_X(KEYIN1,  AF0, DS3, PULL_UP, IO_IE),
	MFP_CFG_X(KEYIN2,  AF0, DS3, PULL_UP, IO_IE),
	MFP_CFG_X(KEYIN3,  AF0, DS3, PULL_UP, IO_IE),
	MFP_CFG_X(KEYIN4,  AF0, DS3, PULL_UP, IO_IE),
};

static void sprd_config_keypad_pins(void)
{
	sprd_mfp_config(keypad_func_cfg, ARRAY_SIZE(keypad_func_cfg));
}

struct sprd_kpad_t {
        struct input_dev *input;
        int irq;
        unsigned short *keycode;
        unsigned int keyup_test_jiffies;
};

typedef struct kpd_key_tag
{
    unsigned short key_code;
    unsigned char state;
    unsigned char count;
    unsigned long time_stamp;
    unsigned long timer_id;
} kpd_key_t;

struct timer_list s_kpd_timer[MAX_MUL_KEY_NUM];
kpd_key_t s_key[MAX_MUL_KEY_NUM];
struct sprd_kpad_t *sprd_kpad;

#ifdef PB_INT_ENABLE
kpd_key_t s_pb;
struct timer_list s_pb_timer;
#endif

void clear_key(kpd_key_t *key_ptr)
{
	key_ptr->key_code   = TB_KPD_INVALID_KEY;
	key_ptr->state      = TB_KPD_RELEASED;
    	key_ptr->count      = 0;
    	key_ptr->time_stamp = 0;
}

static inline int sprd_kpad_find_key(struct sprd_kpad_t *sprd_kpad,
                        struct input_dev *input, u16 keyident)
{
        u16 i;

        for (i = 0; i < input->keycodemax; i++)
                if (sprd_kpad->keycode[i + input->keycodemax] == keyident)
                        return sprd_kpad->keycode[i];
        return -1;
}

static inline void sprd_keycodecpy(unsigned short *keycode,
                        const unsigned int *pdata_kc,
                        unsigned short keymapsize)
{
        unsigned int i;

        for (i = 0; i < keymapsize; i++) {
                keycode[i] = pdata_kc[i] & 0xffff;
                keycode[i + keymapsize] = pdata_kc[i] >> 16;
        }
}

static void print_kpad(void)
{
	printk("\n\nREG_KPD_STS = 0x%08x\n", REG_KPD_STS);
	printk("REG_KPD_CTL = 0x%08x\n", REG_KPD_CTL);
	printk("REG_KPD_POLARITY = 0x%08x\n", REG_KPD_POLARITY);
	printk("REG_KPD_CLKDIV = 0x%08x\n", REG_KPD_CLKDIV);
	printk("REG_KPD_TOUTCNT = 0x%08x\n", REG_KPD_TOUTCNT);
	printk("REG_KPD_INT_MSK = 0x%08x\n", REG_KPD_INT_MSK);
	printk("REG_KPD_PBINT_CTL = 0x%08x\n", REG_KPD_PBINT_CTL);
	printk("REG_KPD_PBINT_CNT = 0x%08x\n", REG_KPD_PBINT_CNT);
	printk("REG_KPD_PBINT_LATCNT = 0x%08x\n", REG_KPD_PBINT_LATCNT);
	printk("REG_INT_EN = 0x%08x\n", REG_INT_EN);
}

void change_state(kpd_key_t *key_ptr)
{   
	unsigned short colrow, key;
    	/* Change state of the key according to it's current state */
    	if (TB_KPD_RELEASED == key_ptr->state) {
		/* Change state from TB_KPD_RELEASED to TB_KPD_PRESSED */
        	key_ptr->state  = TB_KPD_PRESSED;
        	key_ptr->count  = 1;
		mod_timer(&s_kpd_timer[key_ptr->timer_id], jiffies + CHECK_TIMER_EXPIRE);
#if 1
		colrow = SCAN2KEYVAl(key_ptr->key_code);
		key = sprd_kpad_find_key(sprd_kpad, sprd_kpad->input, colrow);
		printk("colrow = 0x%0x  key = %d  DOWN\n", colrow, key);
        	input_report_key(sprd_kpad->input, key, 1);
        	input_sync(sprd_kpad->input);
#endif
        	/* added by yuehz 20060418 begin */
        	/*if(GetVirtualKey(key_ptr->key_code) == SCI_VK_CALL) {
             		KPD_SetGreenKeyPressed(TRUE);
         	} else {
             		KPD_SetGreenKeyPressed(FALSE);
         	}*/
	} else {
        	/* Change state from TB_KPD_PRESSED to TB_KPD_RELEASED */
#if 1
		colrow = SCAN2KEYVAl(key_ptr->key_code);
		key = sprd_kpad_find_key(sprd_kpad, sprd_kpad->input, colrow);
		printk("colrow = 0x%0x  key = %d  UP\n", colrow, key);
        	input_report_key(sprd_kpad->input, key, 0);
        	input_sync(sprd_kpad->input);
#endif		
        	clear_key(key_ptr); 
    	}
}

unsigned long handle_key(unsigned short key_code, kpd_key_t *key_ptr)
{
    unsigned long status = 0;
    
    switch(key_ptr->state) {
	case TB_KPD_RELEASED:
        	/* check if it is the first INT of a key */
        	if (key_ptr->count == 0) {
            		/* Always discard the first key val */
           		key_ptr->key_code   = TB_KPD_INVALID_KEY;
            		/* Add count of INT to avoid dead loop */
            		key_ptr->count ++;
        	} else { //if (key_ptr->count == 0)
            		/* Check if this key is the same as the previous key, if same, handle it, else ignore the previous key */
            		if (IS_KEY_VALID(key_code)) {
                		/* Can't be TB_KPD_INVALID_KEY */
                		if (key_code == TB_KPD_INVALID_KEY) {
					printk("assert error : %s  %s  %d\n", __FILE__, __FUNCTION__, __LINE__);
					status = TB_KPD_INVALID_KEY;
					return status;
				}          
                		/* They are same */
                		/* Check if the time is out, if time is out, the reset this key, else hadle it */
                		if (IS_TIME_OUT(key_ptr)) {
                    			/* Time out, then reset time stamp and count */
                    			key_ptr->count = 1;
                    			key_ptr->time_stamp = jiffies;
					printk("timeout\n");
                		} else {
                    			/* Add count of INT */
                    			key_ptr->count++;
                    			/* if it is pressed long enough, then changes it's state */
                    			if (key_ptr->count >= AVOID_QUIVER_MIN_COUNT)
                        			change_state(key_ptr);
                		} //if (IS_TIME_OUT(key_ptr))
            		} else if ((key_ptr->key_code == TB_KPD_INVALID_KEY) || IS_TIME_OUT(key_ptr)) { //if (IS_KEY_VALID(key_code))
                		/* Not same, Get key code */
                		key_ptr->key_code = key_code;
                		/* Set time stamp */
                		key_ptr->time_stamp = jiffies;
                		/* Add count of INT */
                		key_ptr->count = 1;             
            		} else {
				status = TB_KPD_INVALID_KEY;
	    		}
        	}// if (key_ptr->count == 0)
        
        break;
        
    	case TB_KPD_PRESSED:
        	/* Check if this key is the same as the old key, if same, handle it, else then only skip this key */
        	if (IS_KEY_VALID(key_code)) {
            		/* Add count of INT */
            		key_ptr->count ++;
        	} else {
           		/* Not same */
            		status = TB_KPD_INVALID_KEY;
        	}
        break;
    
	default: 
        break;

    } //switch(key_ptr->state)

    return status;
}

static void sprd_kpad_timer(unsigned long data)
{
	kpd_key_t *key_ptr = (kpd_key_t *)data;

	/* Check if the key is released, if the state is TB_KPD_PRESSED and count is 0, it means the key is released */
    	if (key_ptr->state == TB_KPD_PRESSED) {
        	if (key_ptr->count == 0) {
            		change_state(key_ptr);
        	} else {
            		key_ptr->count = 0;
			mod_timer(&s_kpd_timer[key_ptr->timer_id], jiffies + CHECK_TIMER_EXPIRE);
		}
    	}
}

static int kpd_getpbstate (void)
{
	return ((REG_KPD_INT_MSK & BIT_3) >> 3);
}

static void sprd_pb_timer_modify(unsigned long data)
{
	kpd_key_t *key_ptr = &s_pb;

    	if (kpd_getpbstate() && (TB_KPD_RELEASED == key_ptr->state)) {
	    	key_ptr->count ++ ;
	    	if ( key_ptr->count > PB_TIMER_COUNTER) {
			input_report_key(sprd_kpad->input, KEY_POWER, 1);
        		input_sync(sprd_kpad->input);
	        	key_ptr->count = 0 ;
	        	key_ptr->state = TB_KPD_PRESSED;
			//printk("key = %d  DOWN\n", KEY_POWER);
		}
		/* continue check power button state */
		REG_INT_EN |= 1 << IRQ_MIX_INT;
    	} else if (!kpd_getpbstate() && (TB_KPD_PRESSED == key_ptr->state)) {
        		/* Timer found power button state has been changed */
        		input_report_key(sprd_kpad->input, KEY_POWER, 0);
        		input_sync(sprd_kpad->input);
			//printk("key = %d  UP\n", KEY_POWER);
			key_ptr->state = TB_KPD_RELEASED;
			clear_key(&s_pb);
        		// Clear the keypad INT status.
	    		REG_KPD_ICLR |= 1 << KPDICLR_PB_INT;
	    		//Enable PB INT;
	    		REG_INT_EN |= 1 << IRQ_MIX_INT;
	} else if (!kpd_getpbstate() && (TB_KPD_RELEASED == key_ptr->state)) {
			//Timer judge whether this is a shake ;
			if (key_ptr->count){
				key_ptr->count--;
				mod_timer(&s_pb_timer, jiffies + PB_TIMER_EXPIRE);
			} else {
				/* If counter is zero ,this interrupt must be a shake ;
				So close timer and reenable pb interrupt for waitting next int ;
				key_ptr->count == NULL && TB_KPD_RELEASED == key_ptr->state
            			Clear the keypad INT status */
				REG_KPD_ICLR |= 1 << KPDICLR_PB_INT;
				//Enable PB INT;
				REG_INT_EN |= 1 << IRQ_MIX_INT;
			}
	} else { //kpd_getpbstate() && (TB_KPD_PRESSED == key_ptr->state)
		/* continue check power button state */
		REG_INT_EN |= 1 << IRQ_MIX_INT;
	}
}

static void  sprd_pb_timer(unsigned long data)
{
	kpd_key_t *key_ptr = &s_pb;

    	if (kpd_getpbstate() && (TB_KPD_RELEASED == key_ptr->state)) {
	    	key_ptr->count ++ ;
	    	if ( key_ptr->count > PB_TIMER_COUNTER) {      
	        	//DRV_Callback(TB_GPIO_INT,&pb_msg);
			input_report_key(sprd_kpad->input, KEY_POWER, 1);
        		input_sync(sprd_kpad->input);
	        	key_ptr->count = 0 ;
	        	key_ptr->state = TB_KPD_PRESSED;
		}
		/* Timer continue check power button state */
		mod_timer(&s_pb_timer, jiffies + PB_TIMER_EXPIRE);
    	} else if (!kpd_getpbstate() && (TB_KPD_PRESSED == key_ptr->state)) {
        		/* Timer found power button state has been changed */
        		//DRV_Callback(TB_GPIO_INT,&pb_msg);
        		input_report_key(sprd_kpad->input, KEY_POWER, 0);
        		input_sync(sprd_kpad->input);

			key_ptr->state = TB_KPD_RELEASED;
        		// Clear the keypad INT status.
	    		REG_KPD_ICLR |= 1 << KPDICLR_PB_INT;
	    		//Enable PB INT;
	    		REG_INT_EN |= 1 << IRQ_MIX_INT;
	} else if (!kpd_getpbstate() && (TB_KPD_RELEASED == key_ptr->state)) {
			//Timer judge whether this is a shake ;
			if (key_ptr->count){
				key_ptr->count--;
				mod_timer(&s_pb_timer, jiffies + PB_TIMER_EXPIRE);
			} else {
				/* If counter is zero ,this interrupt must be a shake ;
				So close timer and reenable pb interrupt for waitting next int ;
				key_ptr->count == NULL && TB_KPD_RELEASED == key_ptr->state
            			Clear the keypad INT status */
				REG_KPD_ICLR |= 1 << KPDICLR_PB_INT;
				//Enable PB INT;
				REG_INT_EN |= 1 << IRQ_MIX_INT;
			}
	} else { //kpd_getpbstate() && (TB_KPD_PRESSED == key_ptr->state)
		mod_timer(&s_pb_timer, jiffies + PB_TIMER_EXPIRE);
	}
}

static irqreturn_t sprd_kpad_isr(int irq, void *dev_id)
{
        //struct platform_device *pdev = dev_id;
        //struct sprd_kpad_t *sprd_kpad = platform_get_drvdata(pdev);
        //struct input_dev *input = sprd_kpad->input;
        int i;
	unsigned short  key_code = 0;
	kpd_key_t *key_ptr = NULL;
	unsigned long found = 0, status; 
	unsigned long s_reg_status = REG_KPD_STS;

	/* check the type of INT */    
    	if (s_reg_status & KPDSTS_KPD) {
        	/* Keypad normal INT */
        	/* get current key code */
        	key_code = (s_reg_status  & (KPDSTS_ROW_COUNT | KPDSTS_COL_COUNT)) >> 2;
		/* if key_code is stored, don't seat the code again */
	    	for (i = 0; i< MAX_MUL_KEY_NUM; i++) {
    	        	key_ptr = &s_key[i];
    			if (key_ptr->key_code == key_code) {
	    			handle_key(key_code, key_ptr);	    	
	    			found = 1;
	    			break;
    			}
    		}
	    	    
	   	if (!found) {
			/* the key_code is fresh, try to find a seat other than exceed the seat limit */	        	   	
			for (i = 0; i < MAX_MUL_KEY_NUM; i++) {
				key_ptr = &s_key[i];
				status = handle_key(key_code, key_ptr);
				if (status == 0)
					break;
			}
		}
	        /* clear the keypad INT status */
		REG_KPD_ICLR |= KPDICLR_KPD_INT;
	} else {
        	/* keypad timeout INT */
        	/* clear the keypad time out INT status */
		REG_KPD_ICLR |= KPDICLR_TOUT_INT;
    	}
	
        return IRQ_HANDLED;
}

static irqreturn_t sprd_pb_isr(int irq, void *dev_id)
{
        struct platform_device *pdev = dev_id;
        struct sprd_kpad_t *sprd_kpad = platform_get_drvdata(pdev);
        struct input_dev *input = sprd_kpad->input;

	/* disable PB INT */
	REG_INT_EN_CLR |= (1 << IRQ_MIX_INT);
#if 0
	mod_timer(&s_pb_timer, jiffies + PB_TIMER_EXPIRE);
#else
	mod_timer(&s_pb_timer, jiffies + PB_TIMER_EXPIRE);
	sprd_pb_timer_modify(0);
#endif
	return IRQ_HANDLED;
}

static int __devinit sprd_kpad_probe(struct platform_device *pdev)
{
        struct sprd_kpad_platform_data *pdata;
        struct input_dev *input;
        int i, error;

	pdev->dev.platform_data = &sprd_kpad_data;
	pdata = pdev->dev.platform_data;
        if (!pdata->rows || !pdata->cols || !pdata->keymap) {
                dev_err(&pdev->dev, "no rows, cols or keymap from pdata\n");
               return -EINVAL;
        }

        if (!pdata->keymapsize ||
            pdata->keymapsize > (pdata->rows * pdata->cols)) {
                dev_err(&pdev->dev, "invalid keymapsize\n");
                return -EINVAL;
        }

        sprd_kpad = kzalloc(sizeof(struct sprd_kpad_t), GFP_KERNEL);
        if (!sprd_kpad)
                return -ENOMEM;

        platform_set_drvdata(pdev, sprd_kpad);

        /* Allocate memory for keymap followed by private LUT */
        sprd_kpad->keycode = kmalloc(pdata->keymapsize * sizeof(unsigned short) * 2, GFP_KERNEL);
        if (!sprd_kpad->keycode) {
                error = -ENOMEM;
                goto out;
        }

        if (!pdata->keyup_test_interval)
                sprd_kpad->keyup_test_jiffies = msecs_to_jiffies(50);
        else
                sprd_kpad->keyup_test_jiffies =
                       msecs_to_jiffies(pdata->keyup_test_interval);

        sprd_kpad->irq = platform_get_irq(pdev, 0);
        if (sprd_kpad->irq < 0) {
                error = -ENODEV;
                goto out2;
        }

        /* init sprd keypad controller */
        REG_INT_EN_CLR = (1 << IRQ_KPD_INT) | (1 << IRQ_MIX_INT);
        REG_GR_GEN0 |= BIT_8;
        sprd_config_keypad_pins();
        REG_KPD_ICLR = KPDICLR_KPD_INT | KPDICLR_TOUT_INT;
        REG_KPD_POLARITY = CFG_ROW_POLARITY | CFG_COL_POLARITY;
        REG_KPD_CLKDIV = 0x1 & 0xffff;
        REG_KPD_CTL = (1 << 2) | (1 << 0) | (CFG_KEY_TYPE);

        error = request_irq(sprd_kpad->irq, sprd_kpad_isr, 0, DRV_NAME, pdev);
        if (error) {
                dev_err(&pdev->dev, "unable to claim irq %d\n", sprd_kpad->irq);
                goto out2;
        }

        input = input_allocate_device();
        if (!input) {
                error = -ENOMEM;
                goto out3;
        }

        sprd_kpad->input = input;

        input->name = pdev->name;
        input->phys = "sprd-keys/input0";
        input->dev.parent = &pdev->dev;
	input_set_drvdata(input, sprd_kpad);

        input->id.bustype = BUS_HOST;
        input->id.vendor = 0x0001;
        input->id.product = 0x0001;
        input->id.version = 0x0100;

        input->keycodesize = sizeof(unsigned short);
        input->keycodemax = pdata->keymapsize;
        input->keycode = sprd_kpad->keycode;

        sprd_keycodecpy(sprd_kpad->keycode, pdata->keymap, pdata->keymapsize);
	/*printk("keycodesize = %d  keycodemax = %d\n", input->keycodesize, input->keycodemax);
	for (i = 0; i < input->keycodemax; i++) {
		printk("keycode = 0x%04x  colrow = 0x%04x\n", sprd_kpad->keycode[i], sprd_kpad->keycode[i + input->keycodemax]);	
	}*/

        /* setup input device */
        __set_bit(EV_KEY, input->evbit);

        if (pdata->repeat)
                __set_bit(EV_REP, input->evbit);

        for (i = 0; i < input->keycodemax; i++)
		__set_bit(sprd_kpad->keycode[i], input->keybit);

        __clear_bit(KEY_RESERVED, input->keybit);

        error = input_register_device(input);
        if (error) {
                dev_err(&pdev->dev, "unable to register input device\n");
                goto out4;
        }

        device_init_wakeup(&pdev->dev, 1);
	for (i = 0; i < MAX_MUL_KEY_NUM; i++) {
		/* clear Key state */
		clear_key(&s_key[i]);
		/* create a timer to check if key is released */
		setup_timer(&s_kpd_timer[i], sprd_kpad_timer, (unsigned long) &s_key[i]);
		s_key[i].timer_id = i;						        
	}

	REG_INT_EN |= 1 << IRQ_KPD_INT;
#ifdef PB_INT_ENABLE
	clear_key(&s_pb);
	__set_bit(KEY_POWER & KEY_MAX, input->keybit);
	REG_INT_EN_CLR |= 1 << IRQ_MIX_INT;
	//gpio_as_pbint();
#if 0
	setup_timer(&s_pb_timer, sprd_pb_timer, (unsigned long) 0);
#else
	setup_timer(&s_pb_timer, sprd_pb_timer_modify, (unsigned long) 0);
#endif
	error = request_irq(IRQ_MIX_INT, sprd_pb_isr, 0, DRV_NAME, pdev);
        if (error) {
                dev_err(&pdev->dev, "unable to claim power irq %d\n", IRQ_MIX_INT);
        }
	REG_KPD_ICLR |= 1 << KPDICLR_PB_INT;
	REG_INT_EN |= 1 << IRQ_MIX_INT;
	//print_kpad();
#endif

	return 0;

out4:
        input_free_device(input);
out3:
        free_irq(sprd_kpad->irq, pdev);
out2:
        kfree(sprd_kpad->keycode);
out:
        kfree(sprd_kpad);
        platform_set_drvdata(pdev, NULL);
        return error;
}

static int __devexit sprd_kpad_remove(struct platform_device *pdev)
{
	int i;
        //struct sprd_kpad_platform_data *pdata = pdev->dev.platform_data;
        struct sprd_kpad_t *sprd_kpad = platform_get_drvdata(pdev);

	for (i = 0; i < MAX_MUL_KEY_NUM; i++)
        	del_timer_sync(&s_kpd_timer[i]);

        free_irq(sprd_kpad->irq, pdev);
        input_unregister_device(sprd_kpad->input);

        kfree(sprd_kpad->keycode);
        kfree(sprd_kpad);
        platform_set_drvdata(pdev, NULL);

        /* disable sprd keypad controller */
        REG_INT_EN_CLR = (1 << IRQ_KPD_INT) | (1 << IRQ_MIX_INT);
        REG_KPD_ICLR = KPDICLR_KPD_INT | KPDICLR_TOUT_INT;
        REG_KPD_CTL &= ~((1 << 2) | (1 << 0));
        REG_GR_GEN0 &= ~BIT_8;

        return 0;
}

#ifdef CONFIG_PM
static int sprd_kpad_suspend(struct platform_device *pdev, pm_message_t state)
{
        struct sprd_kpad_t *sprd_kpad = platform_get_drvdata(pdev);

        return 0;
}

static int sprd_kpad_resume(struct platform_device *pdev)
{
        struct sprd_kpad_t *sprd_kpad = platform_get_drvdata(pdev);

        return 0;
}
#else
# define sprd_kpad_suspend NULL
# define sprd_kpad_resume  NULL
#endif

struct platform_driver sprd_kpad_device_driver = {
        .driver         = {
                .name   = DRV_NAME,
                .owner  = THIS_MODULE,
        },
        .probe          = sprd_kpad_probe,
        .remove         = __devexit_p(sprd_kpad_remove),
        .suspend        = sprd_kpad_suspend,
        .resume         = sprd_kpad_resume,
};

static int __init sprd_kpad_init(void)
{
        return platform_driver_register(&sprd_kpad_device_driver);
}

static void __exit sprd_kpad_exit(void)
{
        platform_driver_unregister(&sprd_kpad_device_driver);
}

module_init(sprd_kpad_init);
module_exit(sprd_kpad_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Richard Feng <Richard.Feng@spreadtrum.com>");
MODULE_DESCRIPTION("Keypad driver for spreadtrum Processors");
MODULE_ALIAS("platform:sprd-keys");
