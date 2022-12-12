include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/package.mk

PKG_NAME:=vpm_run
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

define Package/$(PKG_NAME)
  SUBMENU:=NPU
  SECTION:=utils
  CATEGORY:=Allwinner
  TITLE:=vpm model runner 
  DEPENDS:=+viplite-driver
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)/src
	$(CP) -fr ./src/* $(PKG_BUILD_DIR)
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR)
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/vpm_run $(1)/usr/bin/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
