TARGET_PATH :=$(call my-dir)
TOP_PATH := $(TARGET_PATH)

# $(shell cp $(TARGET_PATH)/OnvifAdaptor.h $(TARGET_PATH)/src/include/onvif/)
$(info $(shell cp -v $(TARGET_PATH)/src/include/OnvifConnector.h $(TARGET_TOP)/custom_aw/include/onvif))
$(info $(shell cp -v $(TARGET_PATH)/libOnvif.*  $(TARGET_TOP)/custom_aw/lib))
$(shell arm-linux-gnueabi-strip $(TARGET_TOP)/custom_aw/lib/libOnvif.*)
