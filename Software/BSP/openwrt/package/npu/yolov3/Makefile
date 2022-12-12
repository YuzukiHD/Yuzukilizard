include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/package.mk

PKG_NAME:=yolov3
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

define Package/$(PKG_NAME)
  SUBMENU:=NPU
  SECTION:=utils
  CATEGORY:=Allwinner
  TITLE:=yolov3 demo
  DEPENDS:=+libjpeg-turbo +viplite-driver
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) -r ./src/* $(PKG_BUILD_DIR)/
endef

TARGET_LDFLAGS+=-L$(STAGING_DIR)/usr/lib/
define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR)/ \
		ARCH="$(TARGET_ARCH)" \
		AR="$(TARGET_AR)" \
		CC="$(TARGET_CC)" \
		CXX="$(TARGET_CXX)" \
		CFLAGS="$(TARGET_CFLAGS) $(EXTRA_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		TARGET="$(PKG_NAME)" \
		all
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/$(PKG_NAME) 	$(1)/usr/bin/
endef

define Package/$(PKG_NAME)-model
  SUBMENU:=NPU
  SECTION:=utils
  CATEGORY:=Allwinner
  TITLE:=yolov3 test demo model (37 MB)
  DEPENDS:=@yolov3
endef

define Package/$(PKG_NAME)-model/install
	$(INSTALL_DIR) $(1)/etc/models
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/yolov3_model.nb $(1)/etc/models/yolov3_model.nb
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
$(eval $(call BuildPackage,$(PKG_NAME)-model))
