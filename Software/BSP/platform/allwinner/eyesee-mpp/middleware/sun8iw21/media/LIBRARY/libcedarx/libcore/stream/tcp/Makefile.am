
include $(top_srcdir)/Makefile.inc

noinst_LTLIBRARIES = libcdx_tcp_stream.la

## set the source files.
libcdx_tcp_stream_la_SOURCES =  CdxTcpStream.c

libcdx_tcp_stream_la_CFLAGS = $(CFLAGS_CDXG)
LOCAL_INCLUDE = -I../include \
		-I../../include \
		-I../../base/include \
		-I$(top_srcdir)/

libcdx_tcp_stream_la_CFLAGS += $(LOCAL_INCLUDE)


libcdx_tcp_stream_la_LDFLAGS = $(LDFLAGS_CDXG)

libcdx_tcp_stream_la_LIBADD = 
#$(top_srcdir)/libcore/base/libcdx_base.la

#LOCAL_LIB = -lpthread
libcdx_tcp_stream_la_LDFLAGS += $(LOCAL_LIB)

