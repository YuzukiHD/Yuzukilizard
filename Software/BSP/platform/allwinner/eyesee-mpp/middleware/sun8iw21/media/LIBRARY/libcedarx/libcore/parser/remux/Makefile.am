
include $(top_srcdir)/Makefile.inc

noinst_LTLIBRARIES = libcdx_remux_parser.la

## set the source files.
libcdx_remux_parser_la_SOURCES =  CdxRemuxParser.c \
			mpeg4Vol.c

libcdx_remux_parser_la_CFLAGS = $(CFLAGS_CDXG)
LOCAL_INCLUDE = -I../include \
		-I../../include \
		-I../../base/include \
		-I../../stream/include \
		-I../../stream/rtsp \
		-I$(top_srcdir)/ \
		-I$(top_srcdir)/external/include/adecoder/ \
		-I$(top_srcdir)/../libcedarc/include

libcdx_remux_parser_la_CFLAGS += $(LOCAL_INCLUDE)


libcdx_remux_parser_la_LDFLAGS = $(LDFLAGS_CDXG)

libcdx_remux_parser_la_LIBADD =

#LOCAL_LIB = -lpthread
libcdx_remux_parser_la_LDFLAGS += $(LOCAL_LIB)

