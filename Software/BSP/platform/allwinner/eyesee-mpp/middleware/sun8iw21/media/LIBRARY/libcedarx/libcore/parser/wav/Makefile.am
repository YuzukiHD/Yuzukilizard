
include $(top_srcdir)/Makefile.inc

noinst_LTLIBRARIES = libcdx_wav_parser.la

## set the source files.
libcdx_wav_parser_la_SOURCES =  CdxWavParser.c

libcdx_wav_parser_la_CFLAGS = $(CFLAGS_CDXG)
LOCAL_INCLUDE = -I../include \
		-I../../include \
		-I../../base/include \
		-I../../stream/include \
		-I$(top_srcdir)/ \
		-I$(top_srcdir)/external/include/adecoder/ \
		-I$(top_srcdir)/../libcedarc/include

libcdx_wav_parser_la_CFLAGS += $(LOCAL_INCLUDE)


libcdx_wav_parser_la_LDFLAGS = $(LDFLAGS_CDXG)

libcdx_wav_parser_la_LIBADD =

#LOCAL_LIB = -lpthread
libcdx_wav_parser_la_LDFLAGS += $(LOCAL_LIB)

