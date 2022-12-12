TARGET_PATH:= $(call my-dir)
include $(ENV_CLEAR)

LIB_ROOT:=$(TARGET_PATH)/../..
include $(LIB_ROOT)/../config.mk
include $(LIB_ROOT)/stream/config.mk

TARGET_SRC = \
		$(notdir $(wildcard $(TARGET_PATH)/*.c))

TARGET_INC:= $(LIB_ROOT)/base/include \
	$(LIB_ROOT)/stream/include  \
	$(LIB_ROOT)/../             \
	$(LIB_ROOT)/include/             \

TARGET_CFLAGS += $(CDX_CFLAGS) -fPIC

TARGET_MODULE:= libcdx_stream

TARGET_SHARED_LIB := libdl libcdx_base

TARGET_STATIC_LIB = \
	libcdx_file_stream \

#	libcdx_rtsp_stream \
#	libcdx_tcp_stream \
#	libcdx_http_stream \
#	libcdx_udp_stream \
#	libcdx_customer_stream \
#	libcdx_ssl_stream \
#	libcdx_aes_stream \
#	libcdx_bdmv_stream \
#	libcdx_widevine_stream \
#	libcdx_videoResize_stream \
#	libcdx_datasource_stream \
#	libcdx_dtmb_stream 	
#	libcdx_mms_stream \
#	libcdx_rtmp_stream \
#	libcdx_dtmb_stream \

include $(BUILD_SHARED_LIB)

######################################################################
######################################################################

