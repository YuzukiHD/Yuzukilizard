include $(ALIB_PATH)/config_and_prepare.mk

LIBSRC = \
	adecoder.c \
	src/AudioDec_Decode.c \
	src/cedar_abs_packet_hdr.c \
	tools/audiomix/src/audiomix.c \

CFLAGS=-Wall
CFLAGS+= \
	-I./include/ \
	-I./tools/audiomix/src/ \
	-I$(ALIB_PATH)/osal/

CFLAGS += -D__OS_LINUX
CFLAGS += $(EXTRA_FLAG)

ifeq ($(MID_LAYER_DEBUG_CONFIG), true)
CFLAGS += -D__AD_CEDAR_DBG_WRITEOUT_BITSTREAM
CFLAGS += -D__AD_CEDAR_DBG_WRITEOUT_PCM_BUFFER
endif

ifeq ($(MID_LAYER_CHECK_MEMORY_CONFIG), true)
	CFLAGS += -DMEMCHECK
	LIBSRC += tools/memcheck/memcheck.c
	CFLAGS += -I./tools/memcheck
endif

LIB=$(LIBPATH)/libadecoder$(LIB_SUFFIX).so
CFLAGS+=-fPIC

include $(ALIB_PATH)/generate.mk
