/*
 ****************************************************************
 *
 * Component = Linux console driver on top of the nanokernel
 *
 * Copyright (C) 2002-2005 Jaluna SA.
 *
 * This program is free software;  you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * #ident  "@(#)console.c 1.1     07/10/18 VirtualLogix"
 *
 * Contributor(s):
 *   Vladimir Grouzdev (grouzdev@jaluna.com) Jaluna SA
 *   Francois Armand (francois.armand@jaluna.com) Jaluna SA
 *   Guennadi Maslov (guennadi.maslov@jaluna.com) Jaluna SA
 *   Gilles Maigne (gilles.maigne@jaluna.com) Jaluna SA
 *   Chi Dat Truong <chidat.truong@jaluna.com>
 *
 ****************************************************************
 */

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/major.h>
#include <linux/console.h>
#include <linux/serial.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/spinlock.h>

#include <asm/uaccess.h>
#include <asm/nkern.h>
#include <nk/nkern.h>

//#define __TRACE__

#ifdef  __TRACE__
#define CON_TRACE printk
#else
#define CON_TRACE(...)
#endif

//#define CON_DEBUG

#ifdef  CON_DEBUG
#define CON_PRINT printk
#else
#define CON_PRINT(...)
#endif

# ifdef CONFIG_ARCH_SC8800G
//#define CONFIG_TS0710_MUX_UART
# endif /* CONFIG_ARCH_SC8800G */

#define SERIAL_NK_NAME	  "ttyNK"
#define SERIAL_NK_MAJOR	  220 /* GMv 254 */
#define SERIAL_NK_MINOR	  0

#define	NKLINE(tty)	((tty)->index)
#define	NKPORT(tty) ( (NkPort*)((tty)->driver_data))

#define	SERIAL_NK_TIMEOUT	(10 * HZ)	/* timeout */
#define	SERIAL_NK_RXLIMIT	256	/* no more than 256 characters */
typedef struct NkPort NkPort;
#define MAX_BUF			128

typedef struct {
    volatile int	reader;
    volatile int	writer;
    int			size;
    char		buffer[MAX_BUF];
} Fifo;


#ifdef CONFIG_TS0710_MUX_UART
extern struct mux_ringbuffer rbuf;
struct tty_driver *serial_for_mux_driver = NULL;
static int serial_mux_guard = 0;
struct tty_struct *serial_for_mux_tty = NULL;
extern ssize_t mux_ringbuffer_write(struct mux_ringbuffer *rbuf, const u8 *buf, size_t len);
extern ssize_t mux_ringbuffer_free(struct mux_ringbuffer *rbuf);
extern int cmux_opened(void);
extern int is_cmux_mode(void);
void (*serial_mux_dispatcher)(struct tty_struct *tty) = NULL;
void (*serial_mux_sender)(void) = NULL;
#endif

struct NkPort {
    struct timer_list	timer;
    unsigned int	poss;    
    char		buf[MAX_BUF];
    volatile char	stoprx;    
    volatile char	stoptx;    
    volatile char	wakeup;
    unsigned short	count;
    unsigned short	sz;
    NkOsId		id;
    int			(*write_room)(NkPort*);
    void		(*flush_input)(NkPort*);
    int			(*write)(NkPort*, const u_char* buf, int count);
    Fifo*		rxfifo;
    Fifo*		txfifo;
    spinlock_t		lock;
    struct tty_struct*	tty;
    NkDevVlink*		vlink;
    NkXIrqId		xid;
};

#define MAX_PORT 4
struct NkPort serial_port[MAX_PORT];

static struct tty_driver     serial_driver;
static struct tty_operations serial_ops;
static struct tty_struct*    serial_table[MAX_PORT];
static struct ktermios*      serial_termios[MAX_PORT];
static struct ktermios*      serial_termios_locked[MAX_PORT];

/*
 * the console driver is in three configurations :
 *	1/ to display console history with /dev/ttyNK1
 *	2/ with vlink based console driver (allowing to be notified with interrupt)
 *	3/ with traditional console driver with input based on timeout
 *
 * To use the driver in this various configurations, one use some `port ops'
 *      1/ The hist_ops for the history
 *	2/ The vcons_ops for the vlink based driver
 *	3/ The cons_ops for the traditional driver
 */


/*
 * history related ops
 */

    static int 
hist_write_room(NkPort* port)
{
    return 0x1000;
}

    static int
hist_poll(NkPort* port, int* pc)
{
    int c;
    unsigned long flags;

    for (;;) {
	hw_local_irq_save(flags);
	c = os_ctx->hgetc(os_ctx, port->poss);
	hw_local_irq_restore(flags);
	if (c) {
	    break;
	}
	port->poss++;
    }

    if (c > 0) {
	port->poss++;
	*pc = c;
    }

    return c;
}


    static void
hist_flush_input(NkPort* port)
{
    unsigned int		size = SERIAL_NK_RXLIMIT;
    int				c;
    struct tty_struct*		tty  = port->tty;

    while (!port->stoprx && size && hist_poll(port, &c) > 0 ) {
	tty_insert_flip_char(tty, c, TTY_NORMAL);
	size--;
    }

    if (size < SERIAL_NK_RXLIMIT) {
	tty_flip_buffer_push(tty);
    }
}

    int
hist_write(NkPort* port, const u_char* buf, int count)
{
    return count;
}


    static void
hist_timeout(unsigned long data)
{
    struct tty_struct*	tty  = (struct tty_struct*)data;
    NkPort*          port = NKPORT(tty);

    hist_flush_input(port);

    port->timer.expires = jiffies + SERIAL_NK_TIMEOUT;
    add_timer(&(port->timer));
}

    static int
hist_open(NkPort* port) 
{
    port->write_room = hist_write_room;
    port->flush_input = hist_flush_input;
    port->write = hist_write;

    init_timer(&port->timer);
    port->timer.data     = (unsigned long)port->tty;
    port->timer.function = hist_timeout;
    port->timer.expires  = jiffies + SERIAL_NK_TIMEOUT;
    add_timer(&port->timer);

    return 0;
}

    static void
hist_close(NkPort* port)
{
    if (port->timer.data) {
	del_timer(&(port->timer));
    }
    port->tty = 0;
}


/*
 * traditional console ops
 */
    static int
cons_poll(NkPort* port, int* c)
{
    int res;
    char ch;

    res = os_ctx->cops.read(port->id, &ch, 1);

    *c = ch;

    return res;
}

#define ROOM(port)  (MAX_BUF - (port)->sz - 1)

    static int
cons_write_room (NkPort* port)
{
    return ROOM(port);
}

    static void
cons_flush_input(NkPort* port)
{
    unsigned int		size = SERIAL_NK_RXLIMIT;
    int				c;
    struct tty_struct*		tty  = port->tty;

    while (!port->stoprx && size && cons_poll(port, &c) > 0 ) {
	tty_insert_flip_char(tty, c, TTY_NORMAL);
	size--;
    }

    if (size < SERIAL_NK_RXLIMIT) {
	tty_flip_buffer_push(tty);
    }
}
    static int
cons_flush_chars (NkPort*	port)
{
    if (port->sz) {
	int		sz;
	sz = os_ctx->cops.write(port->id, port->buf , port->sz);
	if (sz > 0) {
	    port->sz -= sz;
	    if (port->sz) {
		memcpy(port->buf, port->buf + sz, port->sz);
	    }
	}
    }
    return port->sz;
}

    static int
cons_write (NkPort*	port, const u_char* buf, int count)
{
    int				res = 0;
    unsigned long		flags;

    spin_lock_irqsave(&port->lock, flags);
    if (cons_flush_chars(port) == 0) {
	res = os_ctx->cops.write(port->id, buf, count);
    } 

    if ( res < count ) {
	int		room = ROOM(port);

	if ( (count - res) < room  ) {
	    room = count - res;	
	}

	memcpy(port->buf + port->sz, buf + res, room); 
	port->sz += room;
	res += room;

    }
    spin_unlock_irqrestore(&port->lock, flags);
    return res;
}

    static void
cons_timeout (unsigned long data)
{
    struct tty_struct*	tty  = (struct tty_struct*)data;
    NkPort*          	port = NKPORT(tty);
    unsigned long	flags;

    if (port->count == 0) {
        return;
    }

    cons_flush_input(port);

    spin_lock_irqsave(&port->lock, flags);
    if (port->sz) {
	cons_flush_chars(port);
	tty_wakeup(port->tty);
    }
    spin_unlock_irqrestore(&port->lock, flags);

    port->timer.expires = jiffies + SERIAL_NK_TIMEOUT;
    add_timer(&(port->timer));
}

    static int 
cons_open(NkPort*	port)
{

    port->flush_input = cons_flush_input;
    port->write = cons_write;
    port->write_room = cons_write_room;

    os_ctx->cops.open(port->id);

    /*
     * no vlink found so use timer to poll character and restart output
     */
    init_timer(&port->timer);
    port->timer.data     = (unsigned long)port->tty;
    port->timer.function = cons_timeout;
    port->timer.expires  = jiffies + SERIAL_NK_TIMEOUT;
    add_timer(&port->timer);

    return 0;
}

    static int
cons_close(NkPort* port)
{
    unsigned long		flags;
    if (port->timer.data) {
	del_timer(&(port->timer));
    }

    spin_lock_irqsave(&port->lock, flags);
    cons_flush_input(port);
    cons_flush_chars(port);
    spin_unlock_irqrestore(&port->lock, flags);
    os_ctx->cops.close(port->id);

    port->tty = 0;

    return 0;
}



/*
 * vlink related console ops
 */

    static inline int
vcons_rxfifo_count(NkPort* port)
{
    return port->rxfifo->writer - port->rxfifo->reader;
}


    static inline int
vcons_write_room(NkPort* port)
{
    Fifo*	fifo = port->txfifo;
    int		res;
    res = fifo->size - (fifo->writer - fifo->reader);

    return res;
}

    static int
vcons_write(NkPort*	     	   port, 
	    const u_char*      buf,
	    int                count)
{
    int				res;
    unsigned long		flags;

    spin_lock_irqsave(&port->lock, flags);
    res = os_ctx->cops.write(port->id, buf, count);
    if (vcons_write_room(port) > 0) {
	if (port->tty) 
	    tty_wakeup(port->tty);
    }
    spin_unlock_irqrestore(&port->lock, flags);

    return res;
}

    static void
vcons_rx_intr (NkPort* port)
{
    unsigned int		size;
    int				i;
    int				count = 0;

#ifdef CONFIG_TS0710_MUX_UART
    int				line = NKLINE(port->tty);
#endif

    while ((size = vcons_rxfifo_count(port))) {
	if (port->count == 0) {
	    return;
	}

	if (port->stoprx) {
	    break;
	}

#ifdef CONFIG_TS0710_MUX_UART
	if ( ( 3 == line ) && cmux_opened() ) {
	    int 		num;
	    unsigned long	flags;  
	    num =0; 	
		
	    hw_local_irq_save(flags);

	    if  ( (num = os_ctx->cops.read(port->id, port->buf, size)) ) {
		mux_ringbuffer_write(&rbuf, port->buf, num);
	    }

	    count += num;
	    hw_local_irq_restore(flags);

	    CON_TRACE("func[%s]:rev_num=%d\n",__FUNCTION__,num);
	    continue;
	}
#endif

	/* ask how many characters we can read */
	size = tty_buffer_request_room(port->tty, size);
	if (size == 0) {
	    return;
	}
	
	size = os_ctx->cops.read(port->id, port->buf, size);
	count += size;
	for (i = 0; i < size; i++) {
	    tty_insert_flip_char(port->tty, port->buf[i], TTY_NORMAL);
	}
    }

    if (count > 0) {
#ifdef CONFIG_TS0710_MUX_UART
	if (line == 3) {
	    unsigned long flags;   

	    hw_local_irq_save(flags);
	    if (serial_mux_dispatcher && is_cmux_mode()) {
		serial_mux_dispatcher(port->tty);
	    }
	    hw_local_irq_restore(flags);
	    return;
	} 
#endif
	tty_schedule_flip(port->tty);
    }
}

    static void
vcons_tx_intr (NkPort* port)
{
    unsigned long		flags;
    

    spin_lock_irqsave(&port->lock, flags);

    if (vcons_write_room(port)) {
	    /* restart tty when some space has been freed */
	if (!port->stoptx) 
	    tty_wakeup(port->tty);
    }
    spin_unlock_irqrestore(&port->lock, flags);

#ifdef CONFIG_TS0710_MUX_UART
    int				line;
    line = NKLINE(port->tty);
    if ((3==line) && cmux_opened()){
	CON_PRINT("func[%s]:serial_mux_sender\n",__FUNCTION__);
	if(serial_mux_sender){
	    serial_mux_sender();	
	}	
    }
#endif

}

    static void 
vcons_intr(void* data, NkXIrq irq)
{
    NkPort*	port = (NkPort*) data;
    vcons_rx_intr(port);
    vcons_tx_intr(port);
}


/*
 * lookup a vlink for this tty
 */
    NkDevVlink* 
vcons_lookup(struct tty_struct* tty)
{
    NkPhAddr	plink = 0;
    NkDevVlink*	vlink;
    /* lookup vcons for the given tty */
    while ((plink = nkops.nk_vlink_lookup("vcons", plink))) {
	vlink = (NkDevVlink*) nkops.nk_ptov(plink);

	if (vlink->s_id == nkops.nk_id_get()) {

	    int   minor = 0;
	    if (vlink->s_info) {
		char*	resinfo;
		char*   info;
		int	t;

		info = (char*) nkops.nk_ptov(vlink->s_info);

		t = simple_strtoul(info, &resinfo, 0);
		if (resinfo != info) {
		    minor = t;	
		}
	    }

	    if (vlink->link == nkops.nk_id_get() && 
		(tty->index != 0)) {
		/* this should never occurs */
		continue;
	    }

	    if (minor == tty->index) {
		return vlink;
	    }
	}
    }
    return 0;
}

    static int
vcons_open(NkPort*	port, NkDevVlink* vlink)
{
    NkXIrq		irq;
    NkPhAddr	paddr;
    NkPhAddr	plink;

    /* vlink physical address */
    plink = nkops.nk_vtop(vlink);

    /* save the vlink */
    port->vlink = vlink;

    /* initialize port ops for vlink based driver */
    port->flush_input = vcons_rx_intr;
    port->write = vcons_write;
    port->write_room = vcons_write_room;

    /* 
     * otherwise one use the underlying link number as channel number
     */
    port->id = vlink->link;

    /*
     * alloc a cross interrupt so as to be notified when a character
     * has been received
     */
    irq = nkops.nk_pxirq_alloc(plink, 1, vlink->s_id, 1);
    if (!irq) {
	return -ENOMEM;
    }

    /*
     * allocate  transmit fifo
     */
    paddr = nkops.nk_pdev_alloc(plink, 1, sizeof(Fifo));
    if (!paddr) {
	return -ENOMEM;
    }

    port->txfifo = nkops.nk_ptov(paddr);
    if (port->txfifo == 0) {
	return -ENOMEM;
    }

    /*
     * allocate  the rx fifo
     */
    paddr = nkops.nk_pdev_alloc(plink, 2, sizeof(Fifo));
    if (!paddr) {
	return -ENOMEM;
    }

    port->rxfifo = nkops.nk_ptov(paddr);
    if (port->rxfifo == 0) {
	return -ENOMEM;
    }

    os_ctx->cops.open(vlink->link);

    port->xid = nkops.nk_xirq_attach(irq, 
				     vcons_intr, 
				     port);
    if (port->xid == 0) {
	return -ENOMEM;
    }

#ifdef DEBUG
    printk("Open ttyNK<%d> portid %x irq %x", NKLINE(port->tty), port->id, irq);
    printk("vlink %p vlink->link %x\n", vlink, vlink->link);
#endif
    return 0;

}


    static int
vcons_close(NkPort* port)
{
#ifdef DEBUG
    printk("Close ttyNK<%d> portid %x", NKLINE(port->tty), port->id);
    printk("vlink %p vlink->link %x\n", vlink, vlink->link);
#endif
     /* mask xirq */
    if (port->xid) {
	nkops.nk_xirq_detach(port->xid);
	port->xid = 0;
    }

    port->tty = 0;
    os_ctx->cops.close(port->vlink->link);
    return 0;
}



/*
 * Here is the implementation of the generic tty driver interface
 */

    static int
serial_write_room (struct tty_struct* tty)
{
    NkPort*	port = NKPORT(tty);
    return port->write_room(port);
}

    static void
serial_send_xchar (struct tty_struct* tty, char c)
{
}

    static void
serial_throttle (struct tty_struct* tty)
{
    NKPORT(tty)->stoprx = 1;
}

    static void
serial_unthrottle (struct tty_struct* tty)
{
    NkPort*		port = NKPORT(tty);
    port->stoprx = 0;
    port->flush_input(port);
}

    static int
serial_write (struct tty_struct* tty,
	      const u_char*      buf,
	      int                count)
{
    NkPort*			port = NKPORT(tty);
    return port->write(port, buf, count);
}


    static int
serial_chars_in_buffer (struct tty_struct* tty)
{
    int		res;
    NkPort*	port = NKPORT(tty);

    if (port->vlink) {
	Fifo*	fifo = port->txfifo;
	res = fifo->writer - fifo->reader;
    } else {
	res = NKPORT(tty)->sz;
    }

    return res;
}

    static void
serial_flush_buffer (struct tty_struct* tty)
{
    unsigned long	flags;
    NkPort*		port = NKPORT(tty);
    spin_lock_irqsave(&port->lock, flags);
    if (!port->vlink) {
	NKPORT(tty)->sz = 0; 
    }
    spin_unlock_irqrestore(&port->lock, flags);
    tty_wakeup(tty);
}

    static void
serial_set_termios (struct tty_struct* tty, struct ktermios* old)
{
}

    static void
serial_stop (struct tty_struct* tty)
{
    NKPORT(tty)->stoptx = 1;
}

    static void
serial_start(struct tty_struct* tty)
{
    NKPORT(tty)->stoptx = 0;
}

    static int
serial_open (struct tty_struct* tty, struct file* filp)
{
    int			line;
    NkPort* 		port;
    NkDevVlink*		vlink;

    line = NKLINE(tty);
    port = line + serial_port;
	
    printk("serial_open tty addr = 0x%x, filp = 0x%x, port = 0x%x\n", tty, filp,port);
#ifdef CONFIG_TS0710_MUX_UART
	if(line == 3){

	        if( serial_mux_guard ) {
			serial_mux_guard++;
			printk("Fail to open ttyNK3, it's busy!\n");
			return -EBUSY;
		} else {
			serial_mux_guard++;
			serial_for_mux_tty = tty;
			printk("=========serial_for_mux_tty=%p========\n",serial_for_mux_tty);
		}
	}
#endif

    if (line >= MAX_PORT) {
	return -ENODEV;
    }

    port->count++;

    if (port->count > 1) {
	return 0;
    }

    port->id		= os_ctx->id;
    tty->driver_data	= port;
    port->stoptx	= 0;
    port->stoprx	= 0;
    port->wakeup	= 0;
    port->sz		= 0;
    port->poss		= 0;
    port->tty		= tty;
    port->vlink		= 0;
    port->xid		= 0;
    port->timer.data 	= 0;

    spin_lock_init(&port->lock);

    /*
     * console specific initialization.
     */
    if (line == 1) {
	/* history initialization */
	return hist_open(port);
    } else {
	/* use vcons driver if possible */
	vlink = vcons_lookup(tty);
	if (vlink) {
	    return vcons_open(port, vlink);
	} else {
	    return cons_open(port);
	}
    }
}

    static void
serial_close (struct tty_struct* tty, struct file* filp)
{ 
    NkPort* port = (NkPort*)tty->driver_data;
#ifdef CONFIG_TS0710_MUX_UART
	int line = NKLINE(tty);
	if (line == 3) {
		serial_mux_guard--;
	}
#endif

    printk("serial_close tty addr = 0x%x, filp = 0x%x, port->count=0x%x\n", tty, filp, port->count);
    port->count--;
    if (port->count == 0) {

	if (NKLINE(tty) == 1) {
	    hist_close(port);
	} else if (port->vlink) {
	    vcons_close(port);
	} else {
	    cons_close(port);
	}

#ifdef DEBUG
	printk("Closing ttyNK<%d>\n", tty->index);
#endif
    }
}

    static void
serial_wait_until_sent (struct tty_struct* tty, int timeout)
{
}

    static int __init
serial_init (void)
{
    serial_driver.owner           = THIS_MODULE;
    serial_driver.magic           = TTY_DRIVER_MAGIC;
    serial_driver.driver_name     = "nkconsole";
    serial_driver.name            = SERIAL_NK_NAME;
    /* GMv    serial_driver.devfs_name      = SERIAL_NK_NAME; */
    serial_driver.major           = SERIAL_NK_MAJOR;
    serial_driver.minor_start     = SERIAL_NK_MINOR;
    serial_driver.num             = MAX_PORT;
    serial_driver.type            = TTY_DRIVER_TYPE_SERIAL;
    serial_driver.subtype         = SERIAL_TYPE_NORMAL;
    serial_driver.init_termios    = tty_std_termios;
    serial_driver.init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
    serial_driver.flags           = TTY_DRIVER_REAL_RAW;
    kref_init(&serial_driver.kref);
    serial_driver.ttys            = serial_table;
    serial_driver.termios         = serial_termios;
    serial_driver.termios_locked  = serial_termios_locked;
    serial_driver.ops             = &serial_ops;

    serial_ops.open            	  = serial_open;
    serial_ops.close           	  = serial_close;
    serial_ops.write           	  = serial_write;
    serial_ops.write_room      	  = serial_write_room;
    serial_ops.chars_in_buffer 	  = serial_chars_in_buffer;
    serial_ops.flush_buffer    	  = serial_flush_buffer;
    serial_ops.throttle        	  = serial_throttle;
    serial_ops.unthrottle      	  = serial_unthrottle;
    serial_ops.send_xchar      	  = serial_send_xchar;
    serial_ops.set_termios     	  = serial_set_termios;
    serial_ops.stop            	  = serial_stop;
    serial_ops.start           	  = serial_start;
    serial_ops.wait_until_sent 	  = serial_wait_until_sent;

    if (tty_register_driver(&serial_driver)) {
	printk(KERN_ERR "Couldn't register NK console driver\n");
    }

#ifdef CONFIG_TS0710_MUX_UART
	serial_for_mux_driver = &serial_driver;
	printk("=========serial_for_mux_driver=%p========\n",serial_for_mux_driver);
	//serial_for_mux_tty = &serial_tty;
	serial_mux_guard = 0;
#endif
	return 0;

}

    static void __exit
serial_fini(void)
{
    unsigned long flags;

    local_irq_save(flags);

    if (tty_unregister_driver(&serial_driver)) {
	printk(KERN_ERR "Unable to unregister NK console driver\n");
    }

    local_irq_restore(flags);
}

    void
printnk (const char* fmt, ...)
{
    va_list args;
    int     size;
    char    buf[256];

    va_start(args, fmt);
    size = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (size >= sizeof(buf)) {
	size = sizeof(buf)-1;
    }

    os_ctx->cops.write_msg(os_ctx->id, buf, size);
}


    static int
nk_console_setup (struct console* c, char* unused)
{
    return 1;
}

    static void
nk_console_write (struct console* c, const char* buf, unsigned int count)
{
    os_ctx->cops.write_msg(os_ctx->id, buf, count);
}

    static struct tty_driver*
nk_console_device (struct console* c, int* index)
{
    *index = c->index;
    return &serial_driver;
}

static struct console nkcons =
{
	name:	SERIAL_NK_NAME,
	write:	nk_console_write,
	device:	nk_console_device,
	setup:	nk_console_setup,
	flags:	CON_ENABLED,
	index:	-1,
};

    void __init
nk_console_init (void)
{
    register_console(&nkcons);
}

module_init(serial_init);
module_exit(serial_fini);

