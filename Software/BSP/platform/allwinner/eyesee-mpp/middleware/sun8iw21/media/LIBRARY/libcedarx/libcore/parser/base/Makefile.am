include $(top_srcdir)/Makefile.inc

lib_LTLIBRARIES = libcdx_parser.la

## set the source files.
libcdx_parser_la_SOURCES =  CdxParser.c \
			VideoSpecificData.c

libcdx_parser_la_SOURCES += \
    ./id3base/Id3Base.c \
    ./id3base/StringContainer.c \
    ./id3base/CdxUtfCode.c \
    ./id3base/CdxMetaData.c
    
libcdx_parser_la_CFLAGS = $(CFLAGS_CDXG)
LOCAL_INCLUDE = -I../include \
		-I../../include \
		-I../../base/include \
		-I../../stream/include \
		-I$(top_srcdir)/external/include/adecoder \
		-I$(top_srcdir)/../libcedarc/include \
		-I$(top_srcdir)/ \
		-I./id3base/

libcdx_parser_la_CFLAGS += $(LOCAL_INCLUDE)


libcdx_parser_la_LDFLAGS = $(LDFLAGS_CDXG)

libcdx_parser_la_LIBADD = $(top_srcdir)/libcore/parser/asf/libcdx_asf_parser.la \
			$(top_srcdir)/libcore/parser/mov/libcdx_mov_parser.la \
			$(top_srcdir)/libcore/parser/remux/libcdx_remux_parser.la \
			$(top_srcdir)/libcore/parser/flv/libcdx_flv_parser.la \
			$(top_srcdir)/libcore/parser/avi/libcdx_avi_parser.la \
			$(top_srcdir)/libcore/parser/ts/libcdx_ts_parser.la \
			$(top_srcdir)/libcore/parser/hls/libcdx_hls_parser.la \
			$(top_srcdir)/libcore/parser/mkv/libcdx_mkv_parser.la \
			$(top_srcdir)/libcore/parser/ogg/libcdx_ogg_parser.la \
			$(top_srcdir)/libcore/parser/mpg/libcdx_mpg_parser.la \
			$(top_srcdir)/libcore/parser/ape/libcdx_ape_parser.la \
			$(top_srcdir)/libcore/parser/flac/libcdx_flac_parser.la \
			$(top_srcdir)/libcore/parser/AwRawStream/libcdx_rawstream_parser.la \
			$(top_srcdir)/libcore/parser/amr/libcdx_amr_parser.la \
			$(top_srcdir)/libcore/parser/mp3/libcdx_mp3_parser.la \
			$(top_srcdir)/libcore/parser/aac/libcdx_aac_parser.la \
			$(top_srcdir)/libcore/parser/wav/libcdx_wav_parser.la \
			$(top_srcdir)/libcore/parser/id3v2/libcdx_id3v2_parser.la \
			$(top_srcdir)/libcore/parser/pls/libcdx_pls_parser.la \
			$(top_srcdir)/libcore/stream/base/libcdx_stream.la \
			$(top_srcdir)/libcore/base/libcdx_base.la

LOCAL_LIB = -lpthread -lrt -lcrypto -lz
libcdx_parser_la_LDFLAGS += $(LOCAL_LIB)



