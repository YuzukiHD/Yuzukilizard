subdir-cxxflags-y += -DTEST_FOR_CPP_SUBDIR

CFLAGS_REMOVE_crt_init.o  += 	-Werror=implicit-int \
				-Werror=strict-prototypes \
				-Wno-declaration-after-statement \
				-Wno-old-style-declaration \
				-Wno-strict-prototypes \
				-Wno-discarded-qualifiers


obj-y += crt_init.o
obj-$(CONFIG_CPLUSPLUS_TEST) += test/
