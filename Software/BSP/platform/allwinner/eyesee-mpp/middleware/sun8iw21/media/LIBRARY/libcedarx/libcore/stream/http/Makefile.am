
include $(top_srcdir)/Makefile.inc

noinst_LTLIBRARIES = libcdx_http_stream.la

## set the source files.
libcdx_http_stream_la_SOURCES =  CdxHttpStream.c \
				CdxHttpField.c \
				CdxHttpUtils.c

libcdx_http_stream_la_CFLAGS = $(CFLAGS_CDXG)
LOCAL_INCLUDE = -I../include \
		-I../../include \
		-I../../base/include \
		-I../../../external/include/zlib \
		-I$(top_srcdir)/

libcdx_http_stream_la_CFLAGS += $(LOCAL_INCLUDE)


libcdx_http_stream_la_LDFLAGS = $(LDFLAGS_CDXG)

libcdx_http_stream_la_LIBADD = $(top_srcdir)/libcore/base/libcdx_base.la

#LOCAL_LIB = -lpthread
libcdx_http_stream_la_LDFLAGS += $(LOCAL_LIB)

