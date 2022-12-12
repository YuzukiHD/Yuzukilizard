INCLUDES = -I../UsageEnvironment/include -I../groupsock/include \
			-I../liveMedia/include -I../BasicUsageEnvironment/include \
			-I/opt/libjpeg-turbo/include -I./interface
# Default library filename suffixes for each library that we link with.  The "config.*" file might redefine these later.
libliveMedia_LIB_SUFFIX = $(LIB_SUFFIX)
libBasicUsageEnvironment_LIB_SUFFIX = $(LIB_SUFFIX)
libUsageEnvironment_LIB_SUFFIX = $(LIB_SUFFIX)
libgroupsock_LIB_SUFFIX = $(LIB_SUFFIX)
##### Change the following for your environment:
COMPILE_OPTS =		$(INCLUDES) -I.  -DSOCKLEN_T=socklen_t -D_LARGEFILE_SOURCE=1 -D_FILE_OFFSET_BITS=64 \
					-DALLOW_SERVER_PORT_REUSE  -DLOCALE_NOT_USED
C =			c
C_COMPILER =		gcc
C_FLAGS =		$(COMPILE_OPTS) $(CPPFLAGS) $(CFLAGS) -g
CPP =			cpp
CPLUSPLUS_COMPILER =	g++
CPLUSPLUS_FLAGS =	$(COMPILE_OPTS) -Wall -DBSD=1 $(CPPFLAGS) $(CXXFLAGS) -DDEBUG -g
OBJ =			o
LINK =			g++ -o
LINK_OPTS =		-L. $(LDFLAGS) -g
CONSOLE_LINK_OPTS =	$(LINK_OPTS)
LIBRARY_LINK =		ar cr
LIBRARY_LINK_OPTS =
LIB_SUFFIX =			a
LIBS_FOR_CONSOLE_APPLICATION =
LIBS_FOR_GUI_APPLICATION =
EXE =
##### End of variables to change

PREFIX = /usr/local
ALL = IPCVideoStreamerH264$(EXE) IPCVideoStreamerH265$(EXE)
all: $(ALL)

.$(C).$(OBJ):
	$(C_COMPILER) -c $(C_FLAGS) $<
.$(CPP).$(OBJ):
	$(CPLUSPLUS_COMPILER) -c $(CPLUSPLUS_FLAGS) $<

IPC_STREAMER_OBJS = my_sws.$(OBJ) IPCSource.$(OBJ) JPEGDecodec.$(OBJ) CameraV4l2.$(OBJ)
H264_OBJ = IPCVideoStreamerH264.$(OBJ) encodeH264.$(OBJ)
H265_OBJ = IPCVideoStreamerH265.$(OBJ) encodeH265.$(OBJ)

USAGE_ENVIRONMENT_DIR = ../UsageEnvironment
USAGE_ENVIRONMENT_LIB = $(USAGE_ENVIRONMENT_DIR)/libUsageEnvironment.$(libUsageEnvironment_LIB_SUFFIX)
BASIC_USAGE_ENVIRONMENT_DIR = ../BasicUsageEnvironment
BASIC_USAGE_ENVIRONMENT_LIB = $(BASIC_USAGE_ENVIRONMENT_DIR)/libBasicUsageEnvironment.$(libBasicUsageEnvironment_LIB_SUFFIX)
LIVEMEDIA_DIR = ../liveMedia
LIVEMEDIA_LIB = $(LIVEMEDIA_DIR)/libliveMedia.$(libliveMedia_LIB_SUFFIX)
GROUPSOCK_DIR = ../groupsock
GROUPSOCK_LIB = $(GROUPSOCK_DIR)/libgroupsock.$(libgroupsock_LIB_SUFFIX)
LOCAL_LIBS = ./interface/libTinyServer.a
LIBS = $(LIBS_FOR_CONSOLE_APPLICATION) -lx264 -lavcodec -lavutil -lswscale -lavformat -lm -lturbojpeg\
            -lpthread -L/opt/libjpeg-turbo/lib64 -L./interface -lTinyServer

IPCVideoStreamerH264$(EXE):	$(IPC_STREAMER_OBJS) $(H264_OBJ) libTinyServer.a
	$(LINK)$@ $(CONSOLE_LINK_OPTS) $(IPC_STREAMER_OBJS) $(H264_OBJ) -Xlinker "-(" $(LIBS) -Xlinker "-)"

IPCVideoStreamerH265$(EXE):	$(IPC_STREAMER_OBJS) $(H265_OBJ) libTinyServer.a
	$(LINK)$@ $(CONSOLE_LINK_OPTS) $(IPC_STREAMER_OBJS) $(H265_OBJ) -Xlinker "-(" $(LIBS) -Xlinker "-)"

libTinyServer.a:
	make -C interface all

clean:
	rm -rf *.$(OBJ) $(ALL) core *.core *~ include/*~ ; \
	make -C interface clean

install: $(ALL)
	  install -d $(DESTDIR)$(PREFIX)/bin
	  install -m 755 $(ALL) $(DESTDIR)$(PREFIX)/bin

##### Any additional, platform-specific rules come here:
