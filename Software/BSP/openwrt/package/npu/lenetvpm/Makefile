include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/package.mk

PKG_NAME:=lenetvpm
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

define Package/$(PKG_NAME)
  SUBMENU:=NPU
  SECTION:=utils
  CATEGORY:=Allwinner
  TITLE:=lenet demo used to test npu (less memory)
  DEPENDS:=+libstdcpp +viplite-driver +vpm_run
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) -r ./vpmRun_lenet $(PKG_BUILD_DIR)
	$(CP) -fr ./lenet-board-test/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
endef

define Build/InstallDev
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/vpmRun_lenet $(1)/usr/bin
endef

define Package/$(PKG_NAME)-model
  SUBMENU:=NPU
  SECTION:=utils
  CATEGORY:=Allwinner
  TITLE:=lenet demo vpm model
  DEPENDS:=@lenetvpm
endef

define Package/$(PKG_NAME)-model/install
	$(INSTALL_DIR) $(1)/etc/lenet-vpm-run-demo/
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/actual_output_tensor.dat $(1)/etc/lenet-vpm-run-demo/actual_output_tensor.dat
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/input_0.dat $(1)/etc/lenet-vpm-run-demo/input_0.dat
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/output0_10_1.dat $(1)/etc/lenet-vpm-run-demo/output0_10_1.dat
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/sample.txt $(1)/etc/lenet-vpm-run-demo/sample.txt
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/lenet.nb $(1)/etc/lenet-vpm-run-demo/lenet.nb
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
$(eval $(call BuildPackage,$(PKG_NAME)-model))