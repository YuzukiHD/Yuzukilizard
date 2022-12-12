#
# 1. Set the path and clear environment
# 	TARGET_PATH := $(call my-dir)
# 	include $(ENV_CLEAR)
#
# 2. Set the source files and headers files
#	TARGET_SRC := xxx_1.c xxx_2.c
#	TARGET_INc := xxx_1.h xxx_2.h
#
# 3. Set the output target
#	TARGET_MODULE := xxx
#
# 4. Include the main makefile
#	include $(BUILD_BIN)
#
# Before include the build makefile, you can set the compilaion
# flags, e.g. TARGET_ASFLAGS TARGET_CFLAGS TARGET_CPPFLAGS
#

TARGET_PATH :=$(call my-dir)

#########################################
include $(ENV_CLEAR)
TARGET_SRC := wifi_sta.c
TARGET_INC := ./wpa_supplicant
TARGET_SHARED_LIB :=
TARGET_CFLAGS += \
	-fPIC
TARGET_LDFLAGS += \
	-lpthread \
	-llog

TARGET_MODULE := libwifi_sta
include $(BUILD_STATIC_LIB)


#########################################
include $(ENV_CLEAR)
TARGET_SRC := wifi_ap.c 
TARGET_INC := ./wpa_supplicant
TARGET_SHARED_LIB :=
TARGET_CFLAGS += \
        -fPIC
TARGET_LDFLAGS += \
        -lpthread \
        -llog

TARGET_MODULE := libwifi_ap
include $(BUILD_STATIC_LIB)

$(shell cp $(TARGET_PATH)/wifi_sta.h $(TARGET_PATH)/../include/wifi)
$(shell cp $(TARGET_PATH)/wifi_ap.h $(TARGET_PATH)/../include/wifi)



#########################################
include $(ENV_CLEAR)

ifeq (${WIFI_CONFIG},8189ftv)
$(warning "The wifi is 8189ftv")
$(shell rm -rf $(TARGET_OUT)/target/etc/firmware)
$(call copy-files-under, \
        $(TARGET_PATH)/firmware/8189ftv, \
        $(TARGET_OUT)/target/etc/firmware \
)

endif

ifeq (${WIFI_CONFIG},ap6181)
$(warning "The wifi is ap6181")
$(shell rm -rf $(TARGET_OUT)/target/etc/firmware)
$(call copy-files-under, \
        $(TARGET_PATH)/firmware/ap6181, \
        $(TARGET_OUT)/target/etc/firmware \
)
endif

ifeq (${WIFI_CONFIG},ap6255)
$(warning "The wifi is ap6255")	
$(shell rm -rf $(TARGET_OUT)/target/etc/firmware)
$(call copy-files-under, \
        $(TARGET_PATH)/firmware/ap6255, \
        $(TARGET_OUT)/target/etc/firmware \
)
endif

ifeq (${WIFI_CONFIG},ap6335)
$(warning "The wifi is ap6335")	
$(shell rm -rf $(TARGET_OUT)/target/etc/firmware)
$(call copy-files-under, \
        $(TARGET_PATH)/firmware/ap6335, \
        $(TARGET_OUT)/target/etc/firmware \
)
endif

ifeq (${WIFI_CONFIG},xr819)
$(warning "The wifi is xr819")
$(shell rm -rf $(TARGET_OUT)/target/etc/firmware)
$(call copy-files-under, \
	$(TARGET_PATH)/firmware/xr819/*, \
	$(TARGET_OUT)/target/etc/firmware \
)
$(shell mkdir -p $(TARGET_OUT)/target/etc/firmware/xr819)
endif
$(call copy-files-under, \
        $(TARGET_PATH)/firmware/*.sh, \
        $(TARGET_OUT)/target/etc/firmware \
)

