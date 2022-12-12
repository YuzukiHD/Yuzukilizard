LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

include $(LOCAL_PATH)/../../Config.mk

LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES := \
	cedarx_demux.c \
	aw_demux.cpp \

#	epdk_demux/epdk_demux.c \
#	music_demux.c \

		
LOCAL_C_INCLUDES := \
	${CEDARX_TOP}/include/include_platform/CHIP_$(CEDARX_CHIP_VERSION) \
	${CEDARX_TOP}/include/include_cedarv \
	${CEDARX_TOP}/include/include_camera \
	${CEDARX_TOP}/include \
	${CEDARX_TOP}/framework \
	${CEDARX_TOP}/libdemux/epdk_demux \
	${CEDARX_TOP}/../LIBRARY \
	${CEDARX_TOP}/../LIBRARY/DEMUX/PARSER/include \
	${CEDARX_TOP}/../LIBRARY/DEMUX/BASE/include \
	${CEDARX_TOP}/../LIBRARY/DEMUX/STREAM/include \
	${CEDARX_TOP}/../LIBRARY/CODEC/VIDEO/DECODER/include \
	${CEDARX_TOP}/../LIBRARY/CODEC/AUDIO/DECODER/include \
	${CEDARX_TOP}/../LIBRARY/CODEC/SUBTITLE/DECODER/include \
	${CEDARX_TOP}/include/include_demux \
	$(CEDARX_TOP)/include/include_writer \
    ${CEDARX_TOP}/include/include_stream \
    ${CEDARX_TOP}/include/include_omx \
	${CEDARX_TOP}/libutil \
	${CEDARX_TOP}/libstream \
	${CEDARX_TOP}/libcodecs/include \
	${CEDARX_TOP}/libcodecs/libvdcedar \
	${CEDARX_TOP}/librender \
	${CEDARX_TOP}/include/include_cedarv \
	${CEDARX_TOP}/libsub/include \
	$(TOP)/frameworks/${AV_BASE_PATH}/media/libstagefright/include 

LOCAL_MODULE_TAGS := optional
 
LOCAL_CFLAGS += -D__OS_ANDROID -D__CDX_ENABLE_NETWORK
LOCAL_CFLAGS += $(CEDARX_EXT_CFLAGS)

#LOCAL_SHARED_LIBRARIES += libcdx_parser

LOCAL_MODULE:= libcedarxdemuxers

include $(BUILD_STATIC_LIBRARY)

include $(call all-makefiles-under,$(LOCAL_PATH))
