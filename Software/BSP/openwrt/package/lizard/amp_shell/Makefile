include $(TOPDIR)/rules.mk

PKG_NAME:=amp_shell
PKG_VERSION:=1.0.1
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/kernel.mk

define Package/$(PKG_NAME)
  SECTION:=utils
  CATEGORY:=Allwinner
  TITLE:=OpenAMP Console for E907 Core: amp_shell
endef

define Build/Prepare
	# mkdir -p $(PKG_BUILD_DIR)
	# $(CP) ./src/* $(PKG_BUILD_DIR)
	$(CP) ./bin/* $(PKG_BUILD_DIR)
endef

define Build/Compile
	# $(MAKE) -C $(PKG_BUILD_DIR)/ \
	# 	ARCH="$(TARGET_ARCH)" \
	# 	AR="$(TARGET_AR)" \
	# 	CC="$(TARGET_CC)" \
	# 	CFLAGS="$(TARGET_CFLAGS)" \
	# 	LDFLAGS="$(TARGET_LDFLAGS)" \
	# 	KERNEL_DIR="$(LINUX_DIR)" \
	# 	all
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin
	# $(INSTALL_BIN) $(PKG_BUILD_DIR)/usr/bin/* $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/amp_shell $(1)/usr/bin
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
