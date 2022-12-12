TARGET_PATH :=$(call my-dir)
TOP_PATH := $(TARGET_PATH)

LOCAL_SRC := \
	 		 liveMedia/MediaSource.cpp \
			 liveMedia/FramedSource.cpp \
			 liveMedia/Media.cpp \
			 UsageEnvironment/HashTable.cpp \
			 UsageEnvironment/UsageEnvironment.cpp \
			 UsageEnvironment/strDup.cpp \
			 liveMedia/RTSPServer.cpp \
			 liveMedia/BitVector.cpp \
			 liveMedia/GenericMediaServer.cpp \
			 liveMedia/ServerMediaSession.cpp \
			 liveMedia/RTCP.cpp \
			 liveMedia/RTPSink.cpp \
			 liveMedia/BasicUDPSink.cpp \
			 liveMedia/MediaSink.cpp \
			 liveMedia/RTPInterface.cpp \
			 groupsock/Groupsock.cpp \
			 groupsock/GroupsockHelper.cpp \
			 groupsock/NetInterface.cpp \
			 groupsock/NetAddress.cpp \
			 groupsock/GroupEId.cpp \
			 groupsock/inet.c \
			 liveMedia/RTPSource.cpp \
			 liveMedia/RTSPCommon.cpp \
			 liveMedia/DigestAuthentication.cpp \
			 liveMedia/H264VideoRTPSink.cpp \
			 liveMedia/H265VideoRTPSink.cpp \
			 liveMedia/H264or5VideoRTPSink.cpp \
			 liveMedia/VideoRTPSink.cpp \
			 liveMedia/MultiFramedRTPSink.cpp \
			 liveMedia/MultiFramedRTPSource.cpp \
			 liveMedia/MPEG4GenericRTPSink.cpp \
			 liveMedia/FramedFilter.cpp \
			 liveMedia/H264or5VideoStreamDiscreteFramer.cpp \
			 liveMedia/H264VideoStreamDiscreteFramer.cpp \
			 liveMedia/H265VideoStreamDiscreteFramer.cpp \
			 liveMedia/H264or5VideoStreamFramer.cpp \
			 liveMedia/H264VideoRTPSource.cpp \
			 liveMedia/MPEGVideoStreamParser.cpp \
			 liveMedia/MPEGVideoStreamFramer.cpp \
			 liveMedia/PassiveServerMediaSubsession.cpp \
			 liveMedia/OnDemandServerMediaSubsession.cpp \
			 liveMedia/Base64.cpp \
			 liveMedia/StreamParser.cpp \
			 liveMedia/Locale.cpp \
			 liveMedia/rtcp_from_spec.c \
			 liveMedia/ourMD5.cpp \
			 BasicUsageEnvironment/BasicUsageEnvironment.cpp \
			 BasicUsageEnvironment/BasicUsageEnvironment0.cpp \
			 BasicUsageEnvironment/BasicTaskScheduler.cpp \
			 BasicUsageEnvironment/BasicTaskScheduler0.cpp \
			 BasicUsageEnvironment/BasicHashTable.cpp \
			 BasicUsageEnvironment/DelayQueue.cpp \
			 IPCProgram/interface/UnicastVideoMediaSubsession.cpp \
			 IPCProgram/interface/UnicastAudioMediaSubsession.cpp \
			 IPCProgram/interface/TinySource.cpp \
			 IPCProgram/interface/MediaStream.cpp \
			 IPCProgram/interface/FrameNaluParser.cpp \
			 liveMedia/H264or5VideoNaluFramer.cpp \
			 IPCProgram/interface/TinyServer.cpp

LOCAL_INC := \
    $(TARGET_PATH)/groupsock/include \
	$(TARGET_PATH)/BasicUsageEnvironment/include \
	$(TARGET_PATH)/UsageEnvironment/include \
	$(TARGET_PATH)/liveMedia/include \
	$(TARGET_PATH)/liveMedia \
	$(TARGET_PATH)/IPCProgram/interface\

lib_version = "$(shell git --git-dir=$(TARGET_PATH)/.git log -n 1 | grep commit | cut -d ' ' -f 2)"
build_time = "$(shell date)"
who = "$(shell whoami)"

#########################################
include $(ENV_CLEAR)

TARGET_SRC := $(LOCAL_SRC)
TARGET_INC := $(LOCAL_INC)

TARGET_CFLAGS += -O2 -g -W -Wall -fPIC -Wno-unused-parameter -mfloat-abi=softfp -mfpu=neon-vfpv4
TARGET_CPPFLAGS += -DSOCKLEN_T=socklen_t  \
                   -D_LARGEFILE_SOURCE=1 -D_FILE_OFFSET_BITS=64 \
                   -DSO_REUSEPORT -DALLOW_SERVER_PORT_REUSE \
                   -DAW_RTSP_LIB_VERSION=\"$(lib_version)\" \
                   -DBUILD_BY_WHO=\"$(who)\" \
                   -DBUILD_TIME=\"$(build_time)\" \
                   -DUSE_SIGNALS -DREUSE_FOR_TCP \
                   -Wall -DBSD=1 -O2 -g -W -Wall -fexceptions -fPIC -Wno-unused-parameter -Wno-unused-but-set-parameter

TARGET_LDFLAGS +=

TARGET_MODULE := libTinyServer
include $(BUILD_STATIC_LIB)

#########################################
include $(ENV_CLEAR)

TARGET_SRC := $(LOCAL_SRC)
TARGET_INC := $(LOCAL_INC)

TARGET_CFLAGS += -O2 -g -W -Wall -fPIC -mfloat-abi=softfp -mfpu=neon-vfpv4
TARGET_CPPFLAGS += -DSOCKLEN_T=socklen_t  \
                   -D_LARGEFILE_SOURCE=1 -D_FILE_OFFSET_BITS=64 \
                   -DSO_REUSEPORT -DALLOW_SERVER_PORT_REUSE \
                   -DAW_RTSP_LIB_VERSION=\"$(lib_version)\" \
                   -DBUILD_BY_WHO=\"$(who)\" \
                   -DBUILD_TIME=\"$(build_time)\" \
                   -DUSE_SIGNALS -DREUSE_FOR_TCP \
                   -Wall -DBSD=1 -O2 -g -W -Wall -fexceptions -fPIC

TARGET_LDFLAGS += -shared

TARGET_MODULE := libTinyServer
include $(BUILD_SHARED_LIB)
