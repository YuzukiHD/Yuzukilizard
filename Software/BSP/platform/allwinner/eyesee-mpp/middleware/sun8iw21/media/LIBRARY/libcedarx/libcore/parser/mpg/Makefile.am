
include $(top_srcdir)/Makefile.inc

noinst_LTLIBRARIES = libcdx_mpg_parser.la

## set the source files.
libcdx_mpg_parser_la_SOURCES =  CdxMpgParser.c \
			CdxMpgParserImpl.c \
			dvdTitleIfo.c \
			mpgOpen.c \
			mpgRead.c \
			mpgTime.c \
			statusChange.c

libcdx_mpg_parser_la_CFLAGS = $(CFLAGS_CDXG)
LOCAL_INCLUDE = -I../include \
		-I../../include \
		-I../../base/include \
		-I../../stream/include \
		-I$(top_srcdir)/ \
		-I$(top_srcdir)/external/include/adecoder/ \
		-I$(top_srcdir)/../libcedarc/include

libcdx_mpg_parser_la_CFLAGS += $(LOCAL_INCLUDE)


libcdx_mpg_parser_la_LDFLAGS = $(LDFLAGS_CDXG)

libcdx_mpg_parser_la_LIBADD =

#LOCAL_LIB = -lpthread
libcdx_mpg_parser_la_LDFLAGS += $(LOCAL_LIB)

