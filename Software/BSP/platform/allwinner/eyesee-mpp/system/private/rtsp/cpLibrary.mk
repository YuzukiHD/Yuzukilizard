TARGET_PATH :=$(call my-dir)
TOP_PATH := $(TARGET_PATH)

$(info $(shell cp -v $(TARGET_PATH)/IPCProgram/interface/TinyServer.h $(TARGET_TOP)/custom_aw/include/rtsp))
$(info $(shell cp -v $(TARGET_PATH)/IPCProgram/interface/MediaStream.h $(TARGET_TOP)/custom_aw/include/rtsp))
$(info $(shell cp -v $(TARGET_PATH)/libTinyServer.* $(TARGET_TOP)/custom_aw/lib))
$(shell arm-linux-gnueabi-strip $(TARGET_TOP)/custom_aw/lib/libTinyServer.so)
