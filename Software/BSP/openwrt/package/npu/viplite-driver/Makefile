include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/package.mk

PKG_NAME:=viplite-driver
PKG_VERSION:=vip-1.8.0
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

define Package/$(PKG_NAME)
  SUBMENU:=NPU
  SECTION:=utils
  CATEGORY:=Allwinner
  TITLE:=viplite driver for NPU 
  DEPENDS:=+libstdcpp
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)/lib
	$(CP) -fr ./lib/* $(PKG_BUILD_DIR)/lib
	mkdir -p $(PKG_BUILD_DIR)/include
	$(CP) -fr ./include/* $(PKG_BUILD_DIR)/include
	$(CP) -fr ./algo_aw $(PKG_BUILD_DIR)/
endef

define Build/Compile
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib/
	$(INSTALL_DIR) $(1)/usr/include
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/lib/libVIPlite.so $(1)/usr/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/lib/libVIPuser.so $(1)/usr/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/include/*.h $(1)/usr/include
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/algo_aw/awnn_det.h $(1)/usr/include/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/algo_aw/cross_utils.h $(1)/usr/include/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/algo_aw/libawnn_det.a $(1)/usr/lib/
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/lib/libVIPlite.so $(1)/usr/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/lib/libVIPuser.so $(1)/usr/lib/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
