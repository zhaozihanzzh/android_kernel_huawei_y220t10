obj-y += io.o idle.o irq.o timer.o gpio.o\
	mfp_sprd.o vibrator.o \
	ts0710_mux.o mux_buffer.o \
	dma.o ldo.o power_manager.o

# Common code for board init
obj-y += common.o

obj-$(CONFIG_MACH_HISENSE) += board-hisense.o
obj-$(CONFIG_MACH_BIGPHONE) += board-bigphone.o

obj-$(CONFIG_PM)		+= pm.o standby.o
obj-$(CONFIG_CPU_FREQ_SC8800S) += cpufreq-sc8800s.o
obj-$(CONFIG_CPU_IDLE_SC8800S) += cpuidle-sc8800s.o
