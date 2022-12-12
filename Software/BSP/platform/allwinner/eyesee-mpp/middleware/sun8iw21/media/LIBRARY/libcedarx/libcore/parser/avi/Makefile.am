
include $(top_srcdir)/Makefile.inc

noinst_LTLIBRARIES = libcdx_avi_parser.la

## set the source files.
libcdx_avi_parser_la_SOURCES =  CdxAviDepack.c \
						CdxAviDepackIndex.c \
						CdxAviDepackSequence.c \
						CdxAviIdx1.c \
						CdxAviOdmlIndx.c \
						CdxAviParser.c \
						CdxCheckStreamPara.c

libcdx_avi_parser_la_CFLAGS = $(CFLAGS_CDXG)
LOCAL_INCLUDE = -I../include \
		-I../../include \
		-I../../base/include \
		-I../../stream/include \
		-I$(top_srcdir)/ \
		-I$(top_srcdir)/external/include/adecoder/ \
		-I$(top_srcdir)/../libcedarc/include
libcdx_avi_parser_la_CFLAGS += $(LOCAL_INCLUDE)


libcdx_avi_parser_la_LDFLAGS = $(LDFLAGS_CDXG)

libcdx_avi_parser_la_LIBADD =

#LOCAL_LIB = -lpthread
libcdx_avi_parser_la_LDFLAGS += $(LOCAL_LIB)

