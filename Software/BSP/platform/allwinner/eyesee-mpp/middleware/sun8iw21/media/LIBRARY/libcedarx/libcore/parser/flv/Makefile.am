
include $(top_srcdir)/Makefile.inc

noinst_LTLIBRARIES = libcdx_flv_parser.la

## set the source files.
libcdx_flv_parser_la_SOURCES =  CdxFlvParser.c

libcdx_flv_parser_la_CFLAGS = $(CFLAGS_CDXG)
LOCAL_INCLUDE = -I../include \
		-I../../include \
		-I../../base/include \
		-I../../stream/include \
		-I$(top_srcdir)/ \
		-I$(top_srcdir)/external/include/adecoder/ \
		-I$(top_srcdir)/../libcedarc/include

libcdx_flv_parser_la_CFLAGS += $(LOCAL_INCLUDE)


libcdx_flv_parser_la_LDFLAGS = $(LDFLAGS_CDXG)

libcdx_flv_parser_la_LIBADD =

#LOCAL_LIB = -lpthread
libcdx_flv_parser_la_LDFLAGS += $(LOCAL_LIB)

