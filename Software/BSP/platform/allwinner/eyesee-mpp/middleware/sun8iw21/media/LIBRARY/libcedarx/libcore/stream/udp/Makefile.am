
include $(top_srcdir)/Makefile.inc

noinst_LTLIBRARIES = libcdx_udp_stream.la

## set the source files.
libcdx_udp_stream_la_SOURCES =  CdxUdpStream.c

libcdx_udp_stream_la_CFLAGS = $(CFLAGS_CDXG)
LOCAL_INCLUDE = -I../include \
		-I../../include \
		-I../../base/include \
		-I$(top_srcdir)/

libcdx_udp_stream_la_CFLAGS += $(LOCAL_INCLUDE)


libcdx_udp_stream_la_LDFLAGS = $(LDFLAGS_CDXG)

libcdx_udp_stream_la_LIBADD = 
#$(top_srcdir)/libcore/base/libcdx_base.la

#LOCAL_LIB = -lpthread
libcdx_udp_stream_la_LDFLAGS += $(LOCAL_LIB)

