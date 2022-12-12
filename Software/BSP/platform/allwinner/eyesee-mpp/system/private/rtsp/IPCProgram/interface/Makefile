#SRC_FILES := $(wildcard src/*.cpp)

SRC_FILES := ../../liveMedia/MediaSource.cpp \
			 ../../liveMedia/FramedSource.cpp \
			 ../../liveMedia/Media.cpp \
			 ../../UsageEnvironment/HashTable.cpp \
			 ../../UsageEnvironment/UsageEnvironment.cpp \
			 ../../UsageEnvironment/strDup.cpp \
			 ../../liveMedia/RTSPServer.cpp \
			 ../../liveMedia/BitVector.cpp \
			 ../../liveMedia/GenericMediaServer.cpp \
			 ../../liveMedia/ServerMediaSession.cpp \
			 ../../liveMedia/RTCP.cpp \
			 ../../liveMedia/RTPSink.cpp \
			../../liveMedia/BasicUDPSink.cpp \
			 ../../liveMedia/MediaSink.cpp \
			 ../../liveMedia/RTPInterface.cpp \
			 ../../groupsock/Groupsock.cpp \
			 ../../groupsock/GroupsockHelper.cpp \
			 ../../groupsock/NetInterface.cpp \
			 ../../groupsock/NetAddress.cpp \
			 ../../groupsock/GroupEId.cpp \
			 ../../groupsock/inet.c \
			 ../../liveMedia/RTPSource.cpp \
			 ../../liveMedia/RTSPCommon.cpp \
			 ../../liveMedia/DigestAuthentication.cpp \
			 ../../liveMedia/H264VideoRTPSink.cpp \
			 ../../liveMedia/H265VideoRTPSink.cpp \
			 ../../liveMedia/H264or5VideoRTPSink.cpp \
			 ../../liveMedia/VideoRTPSink.cpp \
			 ../../liveMedia/MultiFramedRTPSink.cpp \
			 ../../liveMedia/MultiFramedRTPSource.cpp \
			 ../../liveMedia/FramedFilter.cpp \
			 ../../liveMedia/H264or5VideoStreamDiscreteFramer.cpp \
			 ../../liveMedia/H264or5VideoStreamFramer.cpp \
			 ../../liveMedia/H264or5VideoNaluFramer.cpp \
			 ../../liveMedia/H264VideoRTPSource.cpp \
			 ../../liveMedia/MPEGVideoStreamParser.cpp \
			 ../../liveMedia/MPEGVideoStreamFramer.cpp \
			 ../../liveMedia/PassiveServerMediaSubsession.cpp \
			 ../../liveMedia/OnDemandServerMediaSubsession.cpp \
			 ../../liveMedia/Base64.cpp \
			 ../../liveMedia/StreamParser.cpp \
			 ../../liveMedia/Locale.cpp \
			 ../../liveMedia/rtcp_from_spec.c \
			 ../../liveMedia/ourMD5.cpp \
			 ../../BasicUsageEnvironment/BasicUsageEnvironment.cpp \
			 ../../BasicUsageEnvironment/BasicUsageEnvironment0.cpp \
			 ../../BasicUsageEnvironment/BasicTaskScheduler.cpp \
			 ../../BasicUsageEnvironment/BasicTaskScheduler0.cpp \
			 ../../BasicUsageEnvironment/BasicHashTable.cpp \
			 ../../BasicUsageEnvironment/DelayQueue.cpp \
			 UnicastVideoMediaSubsession.cpp \
			 UnicastAudioMediaSubsession.cpp \
			 FrameNaluParser.cpp \
			 TinySource.cpp \
			 MediaStream.cpp \
			 TinyServer.cpp


INC_DIRS := ../../liveMedia/include \
			../../liveMedia \
			../../BasicUsageEnvironment/include \
			../../groupsock/include \
			../../UsageEnvironment/include


CPP_OBJ_FILES := $(patsubst %.cpp, %.o, $(filter %.cpp, $(SRC_FILES)))
C_OBJ_FILES := $(patsubst %.c, %.o, $(filter %.c, $(SRC_FILES)))
SRC_DIRS := $(dir $(SRC_FILES))
D_FILES := $(CPP_OBJ_FILES:.o=.d)
D_FILES += $(C_OBJ_FILES:.o=.d)

D_FILES_TMP := $(addsuffix .*, $(D_FILES))

CC_FLAGS := $(foreach dir, $(INC_DIRS), -I$(dir))
LD_FLAGS :=
CC_FLAGS += -DSOCKLEN_T=socklen_t  \
			-D_LARGEFILE_SOURCE=1 -D_FILE_OFFSET_BITS=64 \
			-Wall -DBSD=1 -DALLOW_SERVER_PORT_REUSE -g

TARGET_NAME := libTinyServer
TARGET_SHARED_LIB := $(addsuffix .so, $(TARGET_NAME))
TARGET_STATIC_LIB := $(addsuffix .a, $(TARGET_NAME))

build-share := f
build-static := t

ALL :=

ifeq ($(build-share), t)
	ALL += $(TARGET_SHARED_LIB)
	LD_FLAGS += -shared
	CC_FLAGS += -fPIC
endif

ifeq ($(build-static), t)
	ALL += $(TARGET_STATIC_LIB)
	LD_FLAGS +=
	CC_FLAGS +=
endif

all: $(ALL)

$(TARGET_STATIC_LIB) : $(CPP_OBJ_FILES) $(C_OBJ_FILES)
	ar crv $(LD_FLAGS) -o $@ $^

$(TARGET_SHARED_LIB) : $(CPP_OBJ_FILES) $(C_OBJ_FILES)
	g++ $(LD_FLAGS) -o $@ $^

%.o : %.cpp
	g++ $(CC_FLAGS) -c -o $@ $<

%.o : %.c
	gcc $(CC_FLAGS) -c -o $@ $<

%.d : %.cpp
	@set -e; \
	gcc -MM $(CC_FLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

%.d : %.c
	@set -e; \
	gcc -MM $(CC_FLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

-include $(D_FILES)

clean:
	rm -rf $(TARGET_STATIC_LIB) $(TARGET_SHARED_LIB) \
			$(CPP_OBJ_FILES) $(C_OBJ_FILES) \
			$(D_FILES) $(D_FILES_TMP)


