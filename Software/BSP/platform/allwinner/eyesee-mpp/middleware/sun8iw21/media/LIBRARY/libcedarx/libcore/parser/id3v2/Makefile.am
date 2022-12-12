
include $(top_srcdir)/Makefile.inc

noinst_LTLIBRARIES = libcdx_id3v2_parser.la

## set the source files.
libcdx_id3v2_parser_la_SOURCES =  CdxId3v2Parser.c

libcdx_id3v2_parser_la_CFLAGS = $(CFLAGS_CDXG)
LOCAL_INCLUDE = -I../include \
		-I../../include \
		-I../../base/include \
		-I../../stream/include \
		-I$(top_srcdir)/ \
		-I$(top_srcdir)/external/include/adecoder/ \
		-I$(top_srcdir)/../libcedarc/include \
		-I$(top_srcdir)/libcore/parser/base/id3base

libcdx_id3v2_parser_la_CFLAGS += $(LOCAL_INCLUDE)


libcdx_id3v2_parser_la_LDFLAGS = $(LDFLAGS_CDXG)

libcdx_id3v2_parser_la_LIBADD =

#LOCAL_LIB = -lpthread
libcdx_id3v2_parser_la_LDFLAGS += $(LOCAL_LIB)

