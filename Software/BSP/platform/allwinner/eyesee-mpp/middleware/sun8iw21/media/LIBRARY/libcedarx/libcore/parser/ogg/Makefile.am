
include $(top_srcdir)/Makefile.inc

noinst_LTLIBRARIES = libcdx_ogg_parser.la

## set the source files.
libcdx_ogg_parser_la_SOURCES =  CdxOggParser.c \
			oggCore.c \
			oggparseogm.c \
			oggparsevorbis.c \
			oggparseopus.c

libcdx_ogg_parser_la_CFLAGS = $(CFLAGS_CDXG)
LOCAL_INCLUDE = -I../include \
		-I../../include \
		-I../../base/include \
		-I../../stream/include \
		-I$(top_srcdir)/ \
		-I$(top_srcdir)/external/include/adecoder/ \
		-I$(top_srcdir)/../libcedarc/include

libcdx_ogg_parser_la_CFLAGS += $(LOCAL_INCLUDE)


libcdx_ogg_parser_la_LDFLAGS = $(LDFLAGS_CDXG)

libcdx_ogg_parser_la_LIBADD =

#LOCAL_LIB = -lpthread
libcdx_ogg_parser_la_LDFLAGS += $(LOCAL_LIB)

