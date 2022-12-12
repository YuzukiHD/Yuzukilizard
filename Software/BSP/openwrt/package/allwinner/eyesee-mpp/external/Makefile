##############################################
# OpenWrt Makefile for eyesee-mpp-framework
#
#
# Most of the variables used here are defined in
# the include directives below. We just need to
# specify a basic description of the package,
# where to build our program, where to find
# the source files, and where to install the
# compiled program on the router.
#
# Be very careful of spacing in this file.
# Indents should be tabs, not spaces, and
# there should be no trailing whitespace in
# lines that are not commented.
#
##############################################
include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

# Name and release number of this package
PKG_NAME:=eyesee-mpp-external
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKG_MAINTAINER:=AW-PDC-PD5
# This specifies the directory where we're going to build the program.
# The root build directory, $(BUILD_DIR), is by default the build_mipsel
# directory in your OpenWrt SDK directory
PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)
SRC_COMPILE_DIR := $(PKG_BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)
SRC_CODE_DIR := $(LICHEE_PLATFORM_DIR)/allwinner/eyesee-mpp/external

# This specifies the directory where file depends. when files in it update, increasing compile will be enable.
PKG_FILE_DEPENDS:= $(SRC_CODE_DIR)

include $(INCLUDE_DIR)/package.mk

############################################################################
#below items are used for mpp configuration of previous EyeseeLinux,
#for smooth migration to tina, we reserve these config items, define them here and export.
#ref to mpp_config.mk of EyeseeLinux.
#[config item], valid in makefile.
#config chip version: AW1721
ifeq ($(TARGET_CHIP), sun8iw12p1)
    MPPCFG_CHIP := AW1721
else ifeq ($(TARGET_CHIP), sun8iw16p1)
    MPPCFG_CHIP := AW1816
else
#    $(error fatal error! $(TARGET_CHIP) is not support by eyesee-mpp!)
endif
export MPPCFG_CHIP
#config write file method: LINUX_VFS, DIRECT_IO
ifeq ($(CONFIG_mpp_fwrite_method_vfs),y)
    MPPCFG_FILE_SYSTEM := LINUX_VFS
else ifeq ($(CONFIG_mpp_fwrite_method_directIO),y)
    MPPCFG_FILE_SYSTEM := DIRECT_IO
else
    MPPCFG_FILE_SYSTEM := LINUX_VFS
endif
export MPPCFG_FILE_SYSTEM
#config if use hw display method
ifeq ($(CONFIG_mpp_hw_display),y)
    MPPCFG_HW_DISPLAY := Y
else
    MPPCFG_HW_DISPLAY := N
endif
export MPPCFG_HW_DISPLAY
#config if include libmpp_ise.so and enable ise function.
ifeq ($(CONFIG_mpp_ise),y)
    MPPCFG_ISE := Y
else
    MPPCFG_ISE := N
endif
export MPPCFG_ISE
#config if include libmpp_ise.so and enable ise mo module.
ifeq ($(CONFIG_mpp_ise_mo),y)
    MPPCFG_ISE_MO := Y
else
    MPPCFG_ISE_MO := N
endif
export MPPCFG_ISE_MO
#config if include libmpp_ise.so and enable ise bi module.
ifeq ($(CONFIG_mpp_ise_bi),y)
    MPPCFG_ISE_BI := Y
else
    MPPCFG_ISE_BI := N
endif
export MPPCFG_ISE_BI
#config if include libmpp_ise.so and enable ise bi soft module.
ifeq ($(CONFIG_mpp_ise_bi_soft),y)
    MPPCFG_ISE_BI_SOFT := Y
else
    MPPCFG_ISE_BI_SOFT := N
endif
export MPPCFG_ISE_BI_SOFT
#config if include libmpp_ise.so and enable ise sti module.
ifeq ($(CONFIG_mpp_ise_sti),y)
    MPPCFG_ISE_STI := Y
else
    MPPCFG_ISE_STI := N
endif
export MPPCFG_ISE_STI
#config if include libmpp_eis.so and enable eis function.(Electronic Image Stabilization)
ifeq ($(CONFIG_mpp_eis),y)
    MPPCFG_EIS := Y
else
    MPPCFG_EIS := N
endif
export MPPCFG_EIS
#config if include libai_MOD.so and enable MOD(Motion Object Detection) detect.
ifeq ($(CONFIG_mpp_mod),y)
    MPPCFG_MOD := Y
else
    MPPCFG_MOD := N
endif
export MPPCFG_MOD
#config if include libeve_face.so and enable face detect.
ifeq ($(CONFIG_mpp_eveface),y)
    MPPCFG_EVEFACE := Y
else
    MPPCFG_EVEFACE := N
endif
export MPPCFG_EVEFACE
#config if include libai_VLPR.so and enable VLPR(Vehicle License Plate Recognition) detect.
ifeq ($(CONFIG_mpp_vlpr),y)
    MPPCFG_VLPR := Y
else
    MPPCFG_VLPR := N
endif
export MPPCFG_VLPR
#config if include libai_HCNT.so and enable HCNT(human count) detect.
ifeq ($(CONFIG_mpp_hcnt),y)
    MPPCFG_HCNT := Y
else
    MPPCFG_HCNT := N
endif
export MPPCFG_HCNT
#config if include libai_BDII.so and enable BDII() detect.
ifeq ($(CONFIG_mpp_bdii),y)
    MPPCFG_BDII := Y
else
    MPPCFG_BDII := N
endif
export MPPCFG_BDII
#config if include libmpp_uvc.so
ifeq ($(CONFIG_mpp_uvc),y)
    MPPCFG_UVC := Y
else
    MPPCFG_UVC := N
endif
export MPPCFG_UVC
#prebuild all AI libs and component by static
ifeq ($(CONFIG_mpp_compile_static_lib),y)
    MPPCFG_COMPILE_STATIC_LIB := Y
else
    MPPCFG_COMPILE_STATIC_LIB := N
endif
export MPPCFG_COMPILE_STATIC_LIB
#build component to dynamic so if possible
ifeq ($(CONFIG_mpp_compile_dynamic_lib),y)
    MPPCFG_COMPILE_DYNAMIC_LIB := Y
else
    MPPCFG_COMPILE_DYNAMIC_LIB := N
endif
export MPPCFG_COMPILE_DYNAMIC_LIB

############################################################################
#below items are used for mpp external configuration

ifeq ($(CONFIG_mpp_external_sqlite),y)
    MPPEXTERNALCFG_SQLITE := Y
else
    MPPEXTERNALCFG_SQLITE := N
endif
export MPPEXTERNALCFG_SQLITE

ifeq ($(CONFIG_mpp_external_civetweb),y)
    MPPEXTERNALCFG_CIVETWEB := Y
else
    MPPEXTERNALCFG_CIVETWEB := N
endif
export MPPEXTERNALCFG_CIVETWEB

ifeq ($(CONFIG_mpp_external_lz4),y)
    MPPEXTERNALCFG_LZ4 := Y
else
    MPPEXTERNALCFG_LZ4 := N
endif
export MPPEXTERNALCFG_LZ4

ifeq ($(CONFIG_mpp_external_uvoice),y)
    MPPEXTERNALCFG_UVOICE := Y
else
    MPPEXTERNALCFG_UVOICE := N
endif
export MPPEXTERNALCFG_UVOICE
ifeq ($(CONFIG_mpp_external_jsoncpp),y)
    MPPEXTERNALCFG_JSONCPP := Y
else
    MPPEXTERNALCFG_JSONCPP := N
endif
export MPPEXTERNALCFG_JSONCPP

# Specify package information for this program.
# The variables defined here should be self explanatory.
# If you are running Kamikaze, delete the DESCRIPTION
# variable below and uncomment the Kamikaze define
# directive for the description below
define Package/$(PKG_NAME)
  SECTION:=eyesee-mpp-external
  CATEGORY:=Allwinner
  SUBMENU:=eyesee-mpp
  TITLE:=eyesee-mpp-external for Tina Linux
  DEPENDS:= +mpp_external_sqlite:libsqlite3 +glog
endef

define Package/$(PKG_NAME)/config
source "$(SOURCE)/Config.in"
endef

# Uncomment portion below for Kamikaze and delete DESCRIPTION variable above
define Package/$(PKG_NAME)/description
  eyesee-mpp-external contains some external libs which are needed by other mpp modules.
endef

# Specify what needs to be done to prepare for building the package.
# In our case, we need to copy the source files to the build directory.
# This is NOT the default.  The default uses the PKG_SOURCE_URL and the
# PKG_SOURCE which is not defined here to download the source from the web.
# In order to just build a simple program that we have just written, it is
# much easier to do it this way.

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	if [ -d $(SRC_COMPILE_DIR) ]; then \
		rm -rf $(SRC_COMPILE_DIR); \
	fi
	cp -rf $(SRC_CODE_DIR) $(SRC_COMPILE_DIR)
endef

define Build/Configure
endef

define Build/Compile
	$(MAKE) -C $(SRC_COMPILE_DIR) -f tina.mk \
		ARCH="$(TARGET_ARCH)" \
		AR="$(TARGET_AR)" \
		CC="$(TARGET_CC)" \
		CXX="$(TARGET_CXX)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		CXXFLAGS="$(TARGET_CXXFLAGS)" \
		CPPFLAGS="$(TARGET_CPPFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		all
endef

define Build/Clean
	if [ -d $(SRC_COMPILE_DIR) ]; then \
		MPPCFG_COMPILE_DYNAMIC_LIB=Y \
		MPPCFG_COMPILE_STATIC_LIB=Y \
		$(MAKE) -C $(SRC_COMPILE_DIR) -f tina.mk \
			ARCH="$(TARGET_ARCH)" \
			AR="$(TARGET_AR)" \
			CC="$(TARGET_CC)" \
			CXX="$(TARGET_CXX)" \
			CFLAGS="$(TARGET_CFLAGS)" \
			CXXFLAGS="$(TARGET_CXXFLAGS)" \
			CPPFLAGS="$(TARGET_CPPFLAGS)" \
			LDFLAGS="$(TARGET_LDFLAGS)" \
			clean; \
	else \
		echo "$(SRC_COMPILE_DIR) is not exist!"; \
	fi
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib/eyesee-mpp
	$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/external
ifeq ($(CONFIG_PACKAGE_SQLiteCpp),)
	$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/external/SQLiteCpp
	$(CP) -r $(SRC_COMPILE_DIR)/SQLiteCpp/include $(1)/usr/include/eyesee-mpp/external/SQLiteCpp
	find $(SRC_COMPILE_DIR)/SQLiteCpp -maxdepth 1 \( -name "libSQLiteCpp.a" -o -name "libSQLiteCpp.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
endif
	$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/external/jsoncpp-0.8.0
	$(CP) -r $(SRC_COMPILE_DIR)/jsoncpp-0.8.0/include $(1)/usr/include/eyesee-mpp/external/jsoncpp-0.8.0
	find $(SRC_COMPILE_DIR)/jsoncpp-0.8.0 -maxdepth 1 \( -name "libjsoncpp0.8.a" -o -name "libjsoncpp0.8.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;

	$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/external/civetweb
	$(CP) -r $(SRC_COMPILE_DIR)/civetweb/include $(1)/usr/include/eyesee-mpp/external/civetweb
	find $(SRC_COMPILE_DIR)/civetweb -maxdepth 1 \( -name "libcivetweb.a" -o -name "libcivetweb.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;

ifeq ($(CONFIG_PACKAGE_liblz4),)
	$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/external/lz4-1.7.5/lib
	$(CP) $(SRC_COMPILE_DIR)/lz4-1.7.5/lib/*.h $(1)/usr/include/eyesee-mpp/external/lz4-1.7.5/lib
	find $(SRC_COMPILE_DIR)/lz4-1.7.5 -maxdepth 1 -name "liblz4.a" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
endif
	$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/external/uvoice
	$(CP) $(SRC_COMPILE_DIR)/uvoice/*.h $(1)/usr/include/eyesee-mpp/external/uvoice
	if [ -d $(SRC_COMPILE_DIR)/uvoice/out ]; then \
		find $(SRC_COMPILE_DIR)/uvoice/out \( -name "*.a" -o -name "*.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
endef

# We do not need to define Build/Configure or Build/Compile directives
# The defaults are appropriate for compiling a simple program such as this one


# Specify where and how to install the program. Since we only have one file,
# the helloworld executable, install it by copying it to the /bin directory on
# the router. The $(1) variable represents the root directory on the router running
# OpenWrt. The $(INSTALL_DIR) variable contains a command to prepare the install
# directory if it does not already exist.  Likewise $(INSTALL_BIN) contains the
# command to copy the binary file from its current location (in our case the build
# directory) to the install directory.
define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/lib/eyesee-mpp
	$(INSTALL_DIR) $(1)/usr/bin
ifeq ($(CONFIG_PACKAGE_SQLiteCpp),)
	find $(SRC_COMPILE_DIR)/SQLiteCpp -maxdepth 1 -type f -name "libSQLiteCpp.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
endif
	find $(SRC_COMPILE_DIR)/jsoncpp-0.8.0 -maxdepth 1 -name "libjsoncpp0.8.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
ifeq ($(CONFIG_PACKAGE_civetweb),)
	find $(SRC_COMPILE_DIR)/civetweb -maxdepth 1 -type f -name "libcivetweb.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
#	find $(SRC_COMPILE_DIR)/civetweb -maxdepth 1 -type f -name "civetweb" -exec $(CP) {} $(1)/usr/bin \;
endif
#	find $(SRC_COMPILE_DIR)/fsck_msdos -maxdepth 1 -type f -name "fsck_msdos" -exec $(CP) {} $(1)/usr/bin \;
	if [ -d $(SRC_COMPILE_DIR)/uvoice/out ]; then \
		find $(SRC_COMPILE_DIR)/uvoice/out -name "*.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
		find $(SRC_COMPILE_DIR)/uvoice/out -name "kws.bin" -exec $(CP) {} $(1)/usr/bin \;; \
	fi
#	$(INSTALL_BIN) $(SRC_COMPILE_DIR)/logcat/alogcat.sh $(1)/bin/alogcat
	if [ -d $(SRC_COMPILE_DIR)/sound_controler/out ]; then \
		find $(SRC_COMPILE_DIR)/sound_controler/out -name "*.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
endef

#define Package/$(PKG_NAME)/InstallDev
#endef

# This line executes the necessary commands to compile our program.
# The above define directives specify all the information needed, but this
# line calls BuildPackage which in turn actually uses this information to
# build a package.

$(eval $(call BuildPackage,$(PKG_NAME)))
