ifneq ($(CONFIG_STANDBY_MSGBOX), y)
obj-$(CONFIG_STANDBY) += standby.o pm_notify.o
else
obj-y += standby_e907.o pm_notify.o
endif
