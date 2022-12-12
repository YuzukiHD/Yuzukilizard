TARGET_PATH :=$(call my-dir)
include $(ENV_CLEAR)

$(call copy-one-file, \
	$(TARGET_PATH)/bluetooth.default.so, \
	$(TARGET_OUT)/target/usr/lib/bluetooth.default.so \
)

$(call copy-one-file, \
	$(TARGET_PATH)/libbt-vendor.so, \
	$(TARGET_OUT)/target/usr/lib/libbt-vendor.so \
)
