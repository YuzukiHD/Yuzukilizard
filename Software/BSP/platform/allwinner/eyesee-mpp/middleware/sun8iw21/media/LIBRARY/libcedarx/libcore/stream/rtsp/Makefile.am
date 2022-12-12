
include $(top_srcdir)/Makefile.inc

noinst_LTLIBRARIES = libcdx_rtsp_stream.la

## set the source files.
libcdx_rtsp_stream_la_SOURCES =  CdxRtspStream.cpp

libcdx_rtsp_stream_la_CFLAGS = $(CFLAGS_CDCG)
libcdx_rtsp_stream_la_CPPFLAGS = $(CPPFLAGS_CDXG)
LOCAL_INCLUDE = -I../include \
		-I../../include \
		-I../../base/include \
		-I../../parser/include \
		-I../../../external/include/zlib \
		-I../../../external/include/live \
		-I../../../external/include/adecoder \
		-I$(top_srcdir)/../libcedarc/include \
		-I$(top_srcdir)/

libcdx_rtsp_stream_la_CFLAGS += $(LOCAL_INCLUDE)
libcdx_rtsp_stream_la_CPPFLAGS += $(LOCAL_INCLUDE)

libcdx_rtsp_stream_la_LDFLAGS = $(LDFLAGS_CDCG)

libcdx_rtsp_stream_la_LIBADD = $(top_srcdir)/libcore/base/libcdx_base.la

#LOCAL_LIB = -lpthread
libcdx_rtsp_stream_la_LDFLAGS += $(LOCAL_LIB)

