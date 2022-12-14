ccflags-y += -I$(srctree)/include/melis/kernel/drivers

obj-y += startup.o
obj-y += va2pa.o
obj-y += blobdata.o
obj-$(CONFIG_ARCH_SUN20IW3) += sun20iw3/
obj-$(CONFIG_CPUFREQ_SUPPORT) += cpufreq.o

#force build blobdata.S .FORCE will always be create so blobdata would re-create
#every time.
$(obj)/blobdata.o: .FORCE

.FORCE:
