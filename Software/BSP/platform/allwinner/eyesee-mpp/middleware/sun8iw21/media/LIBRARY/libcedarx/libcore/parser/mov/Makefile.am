
include $(top_srcdir)/Makefile.inc

noinst_LTLIBRARIES = libcdx_mov_parser.la

## set the source files.
libcdx_mov_parser_la_SOURCES =  CdxMovParser.c \
				CdxMovList.c \
				CdxMovAtom.c \
				CdxMovSample.c \
				CdxMovParserImpl.c \
				mpeg4Vol.c

libcdx_mov_parser_la_CFLAGS = $(CFLAGS_CDXG)
LOCAL_INCLUDE = -I../include \
		-I../../include \
		-I../../base/include \
		-I../../stream/include \
		-I$(top_srcdir)/ \
		-I$(top_srcdir)/external/include/adecoder/ \
		-I$(top_srcdir)/external/include/zlib/ \
		-I$(top_srcdir)/../libcedarc/include \
		-I$(top_srcdir)/libcore/parser/base/id3base
libcdx_mov_parser_la_CFLAGS += $(LOCAL_INCLUDE)


libcdx_mov_parser_la_LDFLAGS = $(LDFLAGS_CDXG)

libcdx_mov_parser_la_LIBADD =

#LOCAL_LIB = -lpthread
libcdx_mov_parser_la_LDFLAGS += $(LOCAL_LIB)

