obj-y += main.o
obj-y += TinyMaix/tm_layers_O1.o TinyMaix/tm_layers_fp8.o TinyMaix/tm_layers.o TinyMaix/tm_model.o TinyMaix/tm_stat.o
obj-y += examples/cifar10/main.o
obj-y += examples/mbnet/main.o examples/mbnet/label.o 
obj-y += examples/vww/main.o
obj-y += examples/mnist/main.o


ifeq ($(CONFIG_FF_TRACE),y)
subdir-ccflags-y += -finstrument-functions
endif
