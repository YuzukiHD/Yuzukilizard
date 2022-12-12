include $(top_srcdir)/Makefile.inc

lib_LTLIBRARIES = libcdx_stream.la

## set the source files.
libcdx_stream_la_SOURCES =  CdxStream.c

libcdx_stream_la_CFLAGS = $(CFLAGS_CDXG)
LOCAL_INCLUDE = -I../include \
		-I../../include \
		-I../../base/include \
		-I$(top_srcdir)/

libcdx_stream_la_CFLAGS += $(LOCAL_INCLUDE)


libcdx_stream_la_LDFLAGS = $(LDFLAGS_CDXG)

libcdx_stream_la_LIBADD = $(top_srcdir)/libcore/stream/file/libcdx_file_stream.la \
			$(top_srcdir)/libcore/stream/http/libcdx_http_stream.la \
			$(top_srcdir)/libcore/stream/udp/libcdx_udp_stream.la \
			$(top_srcdir)/libcore/stream/tcp/libcdx_tcp_stream.la \
			$(top_srcdir)/libcore/base/libcdx_base.la

LOCAL_LIB = -lz
libcdx_stream_la_LDFLAGS += $(LOCAL_LIB)



