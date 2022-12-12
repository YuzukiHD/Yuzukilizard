
include $(top_srcdir)/Makefile.inc

lib_LTLIBRARIES = libcdx_base.la

libcdx_base_la_SOURCES =  ./AwPool.c \
			  ./AwMessageQueue.c \
                          ./CdxBitReader.c \
                          ./CdxBuffer.c \
                          ./CdxDebug.c \
                          ./CdxList.c \
                          ./CdxMessage.c \
                          ./CdxMeta.c \
                          ./CdxParseUrl.c \
                          ./CdxQueue.c \
                          ./CdxSocketUtil.c \
                          ./CdxStrUtil.c \
                          ./CdxSysinfo.c \
                          ./CdxTime.c \
                          ./CdxKeyedVector.c \
                          ./CdxISOLang.c \
                          ./cdx_log.c \
                          ./SmartDnsService.c


libcdx_base_la_CFLAGS = $(CFLAGS_CDXG)
LOCAL_INCLUDE = -I./include \
		-I../include \
		-I$(top_srcdir)/

libcdx_base_la_CFLAGS += $(LOCAL_INCLUDE)


libcdx_base_la_LDFLAGS = $(LDFLAGS_CDXG)

libcdx_base_la_LIBADD = 

LOCAL_LIB = -lpthread -lrt
libcdx_base_la_LDFLAGS += $(LOCAL_LIB)

