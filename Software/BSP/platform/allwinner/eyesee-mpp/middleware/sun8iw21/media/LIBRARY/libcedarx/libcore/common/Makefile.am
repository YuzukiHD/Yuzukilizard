
include $(top_srcdir)/Makefile.inc

lib_LTLIBRARIES = libcdx_common.la

libcdx_common_la_SOURCES =   \
        ./iniparser/dictionary.c   \
        ./iniparser/iniparserapi.c \
        ./iniparser/iniparser.c    \
        ./plugin/cdx_plugin.c

libcdx_common_la_CFLAGS = $(CFLAGS_CDXG)
LOCAL_INCLUDE = -I../include \
		-I./iniparser \
		-I./plugin \
		-I../base/include \
		-I$(top_srcdir)/

libcdx_common_la_CFLAGS += $(LOCAL_INCLUDE)


libcdx_common_la_LDFLAGS = $(LDFLAGS_CDXG)

libcdx_common_la_LIBADD =  $(top_srcdir)/libcore/base/libcdx_base.la

LOCAL_LIB = -lpthread -ldl
libcdx_common_la_LDFLAGS += $(LOCAL_LIB)

