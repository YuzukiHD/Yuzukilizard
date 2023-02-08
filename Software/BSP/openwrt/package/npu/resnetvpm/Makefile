include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/package.mk

PKG_NAME:=resnetvpm
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

define Package/$(PKG_NAME)
  SUBMENU:=NPU
  SECTION:=utils
  CATEGORY:=Allwinner
  TITLE:=resnet demo used to test npu 
  DEPENDS:=+libstdcpp +viplite-driver +vpm_run
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) -r ./vpmRun_resnet $(PKG_BUILD_DIR)
	$(CP) -fr ./resnet-board-test/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
endef

define Build/InstallDev
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/vpmRun_resnet $(1)/usr/bin
endef

define Package/$(PKG_NAME)-model
  SUBMENU:=NPU
  SECTION:=utils
  CATEGORY:=Allwinner
  TITLE:=resnet demo model
  DEPENDS:=@resnetvpm
endef

define Package/$(PKG_NAME)-model/install
	$(INSTALL_DIR) $(1)/etc/resnet-vpm-run-demo/
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/input_0.dat $(1)/etc/resnet-vpm-run-demo/input_0.dat
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/resnet50.nb $(1)/etc/resnet-vpm-run-demo/resnet50.nb
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/sample.txt $(1)/etc/resnet-vpm-run-demo/sample.txt
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
$(eval $(call BuildPackage,$(PKG_NAME)-model))