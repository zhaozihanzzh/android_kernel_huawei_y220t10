/*
 * USB device controllers have lots of quirks.  Use these macros in
 * gadget drivers or other code that needs to deal with them, and which
 * autoconfigures instead of using early binding to the hardware.
 *
 * This SHOULD eventually work like the ARM mach_is_*() stuff, driven by
 * some config file that gets updated as new hardware is supported.
 * (And avoiding all runtime comparisons in typical one-choice configs!)
 *
 * NOTE:  some of these controller drivers may not be available yet.
 * Some are available on 2.4 kernels; several are available, but not
 * yet pushed in the 2.6 mainline tree.
 */

#ifndef __GADGET_CHIPS_H
#define __GADGET_CHIPS_H

#ifdef CONFIG_USB_GADGET_NET2280
#define	gadget_is_net2280(g)	!strcmp("net2280", (g)->name)
#else
#define	gadget_is_net2280(g)	0
#endif

#ifdef CONFIG_USB_GADGET_AMD5536UDC
#define	gadget_is_amd5536udc(g)	!strcmp("amd5536udc", (g)->name)
#else
#define	gadget_is_amd5536udc(g)	0
#endif

#ifdef CONFIG_USB_GADGET_DUMMY_HCD
#define	gadget_is_dummy(g)	!strcmp("dummy_udc", (g)->name)
#else
#define	gadget_is_dummy(g)	0
#endif

#ifdef CONFIG_USB_GADGET_PXA25X
#define	gadget_is_pxa(g)	!strcmp("pxa25x_udc", (g)->name)
#else
#define	gadget_is_pxa(g)	0
#endif

#ifdef CONFIG_USB_GADGET_GOKU
#define	gadget_is_goku(g)	!strcmp("goku_udc", (g)->name)
#else
#define	gadget_is_goku(g)	0
#endif

#ifdef CONFIG_USB_GADGET_LH7A40X
#define	gadget_is_lh7a40x(g)	!strcmp("lh7a40x_udc", (g)->name)
#else
#define	gadget_is_lh7a40x(g)	0
#endif

#ifdef CONFIG_USB_GADGET_OMAP
#define	gadget_is_omap(g)	!strcmp("omap_udc", (g)->name)
#else
#define	gadget_is_omap(g)	0
#endif

/* various unstable versions available */
#ifdef CONFIG_USB_GADGET_PXA27X
#define	gadget_is_pxa27x(g)	!strcmp("pxa27x_udc", (g)->name)
#else
#define	gadget_is_pxa27x(g)	0
#endif

#ifdef CONFIG_USB_GADGET_ATMEL_USBA
#define gadget_is_atmel_usba(g)	!strcmp("atmel_usba_udc", (g)->name)
#else
#define gadget_is_atmel_usba(g)	0
#endif

#ifdef CONFIG_USB_GADGET_S3C2410
#define gadget_is_s3c2410(g)    !strcmp("s3c2410_udc", (g)->name)
#else
#define gadget_is_s3c2410(g)    0
#endif

#ifdef CONFIG_USB_GADGET_AT91
#define gadget_is_at91(g)	!strcmp("at91_udc", (g)->name)
#else
#define gadget_is_at91(g)	0
#endif

#ifdef CONFIG_USB_GADGET_IMX
#define gadget_is_imx(g)	!strcmp("imx_udc", (g)->name)
#else
#define gadget_is_imx(g)	0
#endif

#ifdef CONFIG_USB_GADGET_FSL_USB2
#define gadget_is_fsl_usb2(g)	!strcmp("fsl-usb2-udc", (g)->name)
#else
#define gadget_is_fsl_usb2(g)	0
#endif

/* Mentor high speed "dual role" controller, in peripheral role */
#ifdef CONFIG_USB_GADGET_MUSB_HDRC
#define gadget_is_musbhdrc(g)	!strcmp("musb_hdrc", (g)->name)
#else
#define gadget_is_musbhdrc(g)	0
#endif

#ifdef CONFIG_USB_GADGET_LANGWELL
#define gadget_is_langwell(g)	(!strcmp("langwell_udc", (g)->name))
#else
#define gadget_is_langwell(g)	0
#endif

#ifdef CONFIG_USB_GADGET_M66592
#define	gadget_is_m66592(g)	!strcmp("m66592_udc", (g)->name)
#else
#define	gadget_is_m66592(g)	0
#endif

/* Freescale CPM/QE UDC SUPPORT */
#ifdef CONFIG_USB_GADGET_FSL_QE
#define gadget_is_fsl_qe(g)	!strcmp("fsl_qe_udc", (g)->name)
#else
#define gadget_is_fsl_qe(g)	0
#endif

#ifdef CONFIG_USB_GADGET_CI13XXX
#define gadget_is_ci13xxx(g)	(!strcmp("ci13xxx_udc", (g)->name))
#else
#define gadget_is_ci13xxx(g)	0
#endif

// CONFIG_USB_GADGET_SX2
// CONFIG_USB_GADGET_AU1X00
// ...

#ifdef CONFIG_USB_GADGET_R8A66597
#define	gadget_is_r8a66597(g)	!strcmp("r8a66597_udc", (g)->name)
#else
#define	gadget_is_r8a66597(g)	0
#endif

#ifdef CONFIG_USB_GADGET_SC8800H
#define	gadget_is_sprd_sc8800h(g)	!strcmp("sc8800_udc", (g)->name)
#else
#define	gadget_is_sprd_sc8800h(g)	0
#endif

#ifdef CONFIG_USB_GADGET_SC8800G
#define	gadget_is_sprd_sc8800g(g)	!strcmp("dwc_otg", (g)->name)
#else
#define	gadget_is_sprd_sc8800g(g)	0
#endif

#ifdef CONFIG_USB_GADGET_SC8810
#define	gadget_is_sprd_sc8810(g)	!strcmp("dwc_otg", (g)->name)
#else
#define	gadget_is_sprd_sc8810(g)	0
#endif

#ifdef CONFIG_USB_SPRD_DWC
#define gadget_is_sprd_dwc(g)	(gadget_is_sprd_sc8800g(g) || \
				gadget_is_sprd_sc8810(g))
#else
#define gadget_is_sprd_dwc(g)		0
#endif

#ifdef CONFIG_USB_S3C_HSOTG
#define gadget_is_s3c_hsotg(g)    (!strcmp("s3c-hsotg", (g)->name))
#else
#define gadget_is_s3c_hsotg(g)    0
#endif


/**
 * usb_gadget_controller_number - support bcdDevice id convention
 * @gadget: the controller being driven
 *
 * Return a 2-digit BCD value associated with the peripheral controller,
 * suitable for use as part of a bcdDevice value, or a negative error code.
 *
 * NOTE:  this convention is purely optional, and has no meaning in terms of
 * any USB specification.  If you want to use a different convention in your
 * gadget driver firmware -- maybe a more formal revision ID -- feel free.
 *
 * Hosts see these bcdDevice numbers, and are allowed (but not encouraged!)
 * to change their behavior accordingly.  For example it might help avoiding
 * some chip bug.
 */
static inline int usb_gadget_controller_number(struct usb_gadget *gadget)
{
	if (gadget_is_net2280(gadget))
		return 0x01;
	else if (gadget_is_dummy(gadget))
		return 0x02;
	else if (gadget_is_pxa(gadget))
		return 0x03;
	else if (gadget_is_goku(gadget))
		return 0x06;
	else if (gadget_is_omap(gadget))
		return 0x08;
	else if (gadget_is_lh7a40x(gadget))
		return 0x09;
	else if (gadget_is_pxa27x(gadget))
		return 0x11;
	else if (gadget_is_s3c2410(gadget))
		return 0x12;
	else if (gadget_is_at91(gadget))
		return 0x13;
	else if (gadget_is_imx(gadget))
		return 0x14;
	else if (gadget_is_musbhdrc(gadget))
		return 0x16;
	else if (gadget_is_atmel_usba(gadget))
		return 0x18;
	else if (gadget_is_fsl_usb2(gadget))
		return 0x19;
	else if (gadget_is_amd5536udc(gadget))
		return 0x20;
	else if (gadget_is_m66592(gadget))
		return 0x21;
	else if (gadget_is_fsl_qe(gadget))
		return 0x22;
	else if (gadget_is_ci13xxx(gadget))
		return 0x23;
	else if (gadget_is_langwell(gadget))
		return 0x24;
	else if (gadget_is_r8a66597(gadget))
		return 0x25;
	else if (gadget_is_s3c_hsotg(gadget))
		return 0x26;
	else if (gadget_is_sprd_sc8800h(gadget))
		return 0x27;
	else if (gadget_is_sprd_sc8800g(gadget))
		return 0x28;
	else if (gadget_is_sprd_sc8810(gadget))
		return 0x29;
	return -ENOENT;
}


/**
 * gadget_supports_altsettings - return true if altsettings work
 * @gadget: the gadget in question
 */
static inline bool gadget_supports_altsettings(struct usb_gadget *gadget)
{
	/* PXA 21x/25x/26x has no altsettings at all */
	if (gadget_is_pxa(gadget))
		return false;

	/* PXA 27x and 3xx have *broken* altsetting support */
	if (gadget_is_pxa27x(gadget))
		return false;

	/* Everything else is *presumably* fine ... */
	return true;
}

/**
 * gadget_dma32 - return true if we want buffer aligned on 32 bits (for dma)
 * @gadget: the gadget in question
 */
static inline bool gadget_dma32(struct usb_gadget *gadget)
{
        if (gadget_is_sprd_dwc(gadget))
                return true;
        return false;
}
#endif /* __GADGET_CHIPS_H */
