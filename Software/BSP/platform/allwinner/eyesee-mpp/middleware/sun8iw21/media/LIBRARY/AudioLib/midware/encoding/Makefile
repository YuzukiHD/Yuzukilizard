include $(ALIB_PATH)/config_and_prepare.mk

LIBSRC = \
	aencoder.c \
	pcm_enc.c

CFLAGS=-Wall

CFLAGS+= \
	-I./ \
	-I./include \
	-I$(ALIB_PATH)/osal/

CFLAGS += -fPIC -D__OS_LINUX
CFLAGS += $(EXTRA_FLAG)

EXTERN_LIBS += $(LIBPATH)/libaacenc$(LIB_SUFFIX).a
EXTERN_LIBS += $(LIBPATH)/libmp3enc$(LIB_SUFFIX).a
EXTERN_LIBS += $(LIBPATH)/libpcmenc$(LIB_SUFFIX).a

LIB=$(LIBPATH)/libaencoder$(LIB_SUFFIX).so

include $(ALIB_PATH)/generate.mk

