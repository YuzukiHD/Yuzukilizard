# Makefile for eyesee-mpp/system/public/libion
CUR_PATH := .

#set source files here.
SRCCS := \
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

#include directories
INCLUDE_DIRS := \
    $(CUR_PATH)/groupsock/include \
	$(CUR_PATH)/BasicUsageEnvironment/include \
	$(CUR_PATH)/UsageEnvironment/include \
	$(CUR_PATH)/liveMedia/include \
	$(CUR_PATH)/liveMedia \
	$(CUR_PATH)/IPCProgram/interface

LOCAL_SHARED_LIBS := \

LOCAL_STATIC_LIBS :=

#set dst file name: shared library, static library, execute bin.
ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)
LOCAL_TARGET_DYNAMIC := libTinyServer
endif
ifeq ($(MPPCFG_COMPILE_STATIC_LIB), Y)
LOCAL_TARGET_STATIC := libTinyServer
endif
LOCAL_TARGET_BIN :=

COMPILE_OPTS =	-DSOCKLEN_T=socklen_t  \
                -D_LARGEFILE_SOURCE=1 -D_FILE_OFFSET_BITS=64 \
                -DSO_REUSEPORT -DALLOW_SERVER_PORT_REUSE \
                -DAW_RTSP_LIB_VERSION=\"$(lib_version)\" \
                -DBUILD_BY_WHO=\"$(who)\" \
                -DBUILD_TIME=\"$(build_time)\" \
                -DUSE_SIGNALS -DREUSE_FOR_TCP \

#generate include directory flags for gcc.
inc_paths := $(foreach inc,$(filter-out -I%,$(INCLUDE_DIRS)),$(addprefix -I, $(inc))) \
                $(filter -I%, $(INCLUDE_DIRS))
#Extra flags to give to the C compiler
LOCAL_CFLAGS := $(CFLAGS) $(inc_paths) -fPIC -Wall -Wno-error=format-security -fpermissive $(COMPILE_OPTS)
#Extra flags to give to the C++ compiler
LOCAL_CXXFLAGS := $(CXXFLAGS) $(inc_paths) -fPIC -Wall -Wno-error=format-security  -fpermissive $(COMPILE_OPTS)
#Extra flags to give to the C preprocessor and programs that use it (the C and Fortran compilers).
LOCAL_CPPFLAGS := $(CPPFLAGS)
#target device arch: x86, arm
LOCAL_TARGET_ARCH := $(ARCH)
#Extra flags to give to compilers when they are supposed to invoke the linker,‘ld’.
LOCAL_LDFLAGS := $(LDFLAGS)

LOCAL_DYNAMIC_LDFLAGS := $(LOCAL_LDFLAGS) -shared \
    -L $(CUR_PATH)/ \
    -Wl,-Bstatic \
    -Wl,--start-group $(foreach n, $(LOCAL_STATIC_LIBS), -l$(patsubst lib%,%,$(patsubst %.a,%,$(notdir $(n))))) -Wl,--end-group \
    -Wl,-Bdynamic \
    $(foreach y, $(LOCAL_SHARED_LIBS), -l$(patsubst lib%,%,$(patsubst %.so,%,$(notdir $(y)))))

#generate object files
OBJS := $(SRCCS:%=%.o) #OBJS=$(patsubst %,%.o,$(SRCCS))

#add dynamic lib name suffix and static lib name suffix.
target_dynamic := $(if $(LOCAL_TARGET_DYNAMIC),$(LOCAL_TARGET_DYNAMIC).so,)
target_static := $(if $(LOCAL_TARGET_STATIC),$(LOCAL_TARGET_STATIC).a,)

#generate exe file.
.PHONY: all
all: $(target_dynamic) $(target_static)
	@echo ===================================
	@echo build eyesee-mpp-system-private-rtsp done
	@echo ===================================

$(target_dynamic): $(OBJS)
	$(CXX) $+ $(LOCAL_DYNAMIC_LDFLAGS) -o $@
	@echo ----------------------------
	@echo "finish target: $@"
#	@echo "object files:  $+"
#	@echo "source files:  $(SRCCS)"
	@echo ----------------------------

$(target_static): $(OBJS)
	$(AR) -rcs -o $@ $+
	@echo ----------------------------
	@echo "finish target: $@"
#	@echo "object files:  $+"
#	@echo "source files:  $(SRCCS)"
	@echo ----------------------------

#patten rules to generate local object files
%.cpp.o: %.cpp
	$(CXX) $(LOCAL_CXXFLAGS) $(LOCAL_CPPFLAGS) -c -o $@ $<
%.cc.o: %.cc
	$(CXX) $(LOCAL_CXXFLAGS) $(LOCAL_CPPFLAGS) -c -o $@ $<

%.c.o: %.c
	$(CC) $(LOCAL_CFLAGS) $(LOCAL_CPPFLAGS) -c -o $@ $<

# clean all
.PHONY: clean
clean:
	-rm -f $(OBJS) $(target_dynamic) $(target_static)
