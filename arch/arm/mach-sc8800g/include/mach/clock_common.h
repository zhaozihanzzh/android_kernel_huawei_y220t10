/*
  *  Copyright (C) 2010 Spreadtrum Inc.
  * 
  *   Wang Liwei.  <levee.wang@spreadtrum.com>
  *
  *
  */
#ifndef __ARCH_ARM_SC8800G2_CLOCK_COMMON_H
#define __ARCH_ARM_SC8800G2_CLOCK_COMMON_H

#include <mach/clock_stub.h>


/* control registers. */
#define 	PLL_SCR		GR_PLL_SCR
#define 	GEN0		GR_GEN0
#define 	GEN1		GR_GEN1
#define 	GEN2		GR_GEN2
#define 	GEN3		GR_GEN3
#define 	GEN4		GR_GEN4
#define 	GEN5		GR_CLK_GEN5
#define		PLLMN_CTRL	GR_MPLL_MN
#define		CLK_DLY		GR_CLK_DLY
#define		PCTRL		GR_PCTL
#define		CLK_EN		GR_CLK_EN
//#define		AHB_CTL0	AHB_CTL0


/* clock enable bit. */
#define		CCIR_MCLK_EN_SHIFT	14
#define		CLK_CCIR_EN_SHIFT	2
#define		CLK_DCAM_EN_SHIFT	1
#define		CLK_VSP_EN_SHIFT	13
#define		CLK_LCDC_EN_SHIFT	3
#define		CLK_SDIO_EN_SHIFT	4
#define		CLK_UART0_EN_SHIFT	20
#define		CLK_UART1_EN_SHIFT	21
#define		CLK_UART2_EN_SHIFT	22
#define		CLK_SPI_EN_SHIFT	17
#define		CLK_IIS_EN_SHIFT	12
#define		CLK_ADI_M_EN_SHIFT	29
#define		CLK_AUX0_EN_SHIFT	10
#define		CLK_AUX1_EN_SHIFT	11
#define		CLK_PWM0_EN_SHIFT	21
#define		CLK_PWM1_EN_SHIFT	22
#define		CLK_PWM2_EN_SHIFT	23
#define		CLK_PWM3_EN_SHIFT	24
#define		CLK_USB_REF_EN_SHIFT	5


/* clock source mask. */
#define		CCIR_MCLK_CLKSEL_MASK 	(0x3UL << 18)
#define		CLK_DCAM_CLKSEL_MASK 	(0x3UL << 4)
#define		CLK_VSP_CLKSEL_MASK 	(0x3UL << 2)
#define		CLK_LCDC_CLKSEL_MASK 	(0x3UL << 6)
#define		CLK_SDIO_CLKSEL_MASK 	(0x3UL << 17)
#define		CLK_UART0_CLKSEL_MASK 	(0x3UL << 20)
#define		CLK_UART1_CLKSEL_MASK 	(0x3UL << 22)
#define		CLK_UART2_CLKSEL_MASK 	(0x3UL << 24)
#define		CLK_SPI_CLKSEL_MASK 	(0x3UL << 26)
#define		CLK_IIS_CLKSEL_MASK 	(0x3UL << 8)
#define		CLK_ADI_M_CLKSEL_MASK 	(0x1UL << 28)
#define		CLK_AUX0_CLKSEL_MASK 	(0x3UL << 10)
#define		CLK_AUX1_CLKSEL_MASK 	(0x3UL << 12)
#define		CLK_PWM0_CLKSEL_MASK 	(0x1UL << 25)
#define		CLK_PWM1_CLKSEL_MASK 	(0x1UL << 26)
#define		CLK_PWM2_CLKSEL_MASK 	(0x1UL << 27)
#define		CLK_PWM3_CLKSEL_MASK 	(0x1UL << 28)
#define		CLK_USB_REF_CLKSEL_MASK 	(0x1UL << 0)


/* clock divisor mask */
#define		CCIR_MCLK_CLKDIV_MASK	(0x3UL << 24)
#define		CLK_LCDC_CLKDIV_MASK	(0x7UL << 0)
#define		CLK_UART0_CLKDIV_MASK	(0x7UL << 0)
#define		CLK_UART1_CLKDIV_MASK	(0x7UL << 3)
#define		CLK_UART2_CLKDIV_MASK	(0x7UL << 6)
#define		CLK_SPI_CLKDIV_MASK		(0x7UL << 21)
#define		CLK_IIS_CLKDIV_MASK		(0xffUL << 24)
#define		CLK_AUX0_CLKDIV_MASK	(0xffUL << 0)
#define		CLK_AUX1_CLKDIV_MASK	(0xffUL << 22)



struct clk;

struct clksel_rate {
	u32		val;
	u32		div;
	u32		flags;
};

struct clksel {
	struct clk 					*parent;
	u32							val;
	const struct clksel_rate	*rates;
};
struct clkops {
	int			(*enable)(struct clk *);
	void		(*disable)(struct clk *);
	void		(*find_idlest)(struct clk *, void __iomem **, u8 *);
	void		(*find_companion)(struct clk *, void __iomem **, u8 *);
};

struct clk {
	struct list_head 	node;
	const struct clkops 	*ops;


	/************************/
	/* members moved to pmem. 
	     for many reasons, not all infomation can
	     be shared between OSes, for exmaple, callback 
	     function pointer, different virtrual address for same
	     I/O address, so we only share neccessary ones.
	*/
	
	const char  		*name;
	u32				flags;
	u32				usecount;
	/************************/


	int 				id;
	struct clk			*parent;
	struct list_head	children;
	struct list_head	sibling;
	unsigned long 		rate;
	long				(*round_rate)(struct clk *, unsigned long);
	void				(*init)(struct clk *);
	unsigned long		(*recalc)(struct clk *);
	int 				(*set_rate)(struct clk *, unsigned long);
	const char		*clkdm_name;
	void __iomem		*clksel_reg;
	u32				clksel_mask;
	const struct clksel	*clksel;

	void __iomem		*enable_reg;
	u32				enable_bit;

	void __iomem		*clkdiv_reg;
	u32				clkdiv_mask;
	int 				divisor;
	int				(*set_divisor)(struct clk *clk, int divisor);
	int				(*get_divisor)(struct clk *clk);
	struct clock_stub 	*pstub;
};


struct clk_functions {
	int		(*clk_enable)(struct clk *clk);
	void	(*clk_disable)(struct clk *clk);
	int		(*clk_set_rate)(struct clk *clk, unsigned long rate);
	int		(*clk_set_divisor)(struct clk *clk, int divisor);
	int		(*clk_get_divisor)(struct clk *clk);
	int		(*clk_set_parent)(struct clk *clk, struct clk *parent);
	long	(*clk_round_rate)(struct clk *clk, unsigned long rate);
	void	(*clk_allow_idle)(struct clk *clk);
	void	(*clk_deny_idle)(struct clk *clk);
	void	(*clk_disable_unused)(struct clk *clk);
#ifdef CONFIG_CPU_FREQ
	//void	(*clk_init_cpufreq_table)(struct cpufreq_frequency_table **);
#endif
};


int __init clk_init(struct clk_functions *custom_clocks);
void clk_preinit(struct clk *clk);
int clk_register(struct clk *clk);
void propagate_rate(struct clk *tclk);
void clk_enable_init_clocks(void);
void recalculate_root_clocks(void);
void sc88xx_init_clksel_parent(struct clk *clk);
void clk_reparent(struct clk *child, struct clk *parent);
void clk_print_all(void);
int	clk_set_divisor(struct clk *clk, int divisor);
int	clk_get_divisor(struct clk *clk);

#endif

