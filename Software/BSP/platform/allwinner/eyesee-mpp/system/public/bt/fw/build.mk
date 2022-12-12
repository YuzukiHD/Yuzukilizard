TARGET_PATH :=$(call my-dir)
include $(ENV_CLEAR)

$(call copy-one-file, \
	$(TARGET_PATH)/ap6335/bcm4339a0.hcd, \
	$(TARGET_OUT)/target/etc/bcm4339a0.hcd \
)
