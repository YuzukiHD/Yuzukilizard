# common.mk
ifneq ($(__target/linux/generic/common.mk_inc),1)
__target/linux/generic/common.mk_inc=1

TARGET_PREINIT_SUPPRESS_STDERR :=
TARGET_PREINIT_TIMEOUT :=
TARGET_PREINIT_IFNAME :=
TARGET_PREINIT_IP :=
TARGET_PREINIT_NETMASK :=
TARGET_PREINIT_BROADCAST :=
TARGET_PREINIT_SHOW_NETMSG :=
TARGET_PREINIT_SUPPRESS_FAILSAFE_NETMSG :=
TARGET_INIT_PATH :=
TARGET_INIT_ENV :=
TARGET_INIT_CMD :=
TARGET_INIT_SUPPRESS_STDERR :=

endif #__target/linux/generic/common.mk_inc