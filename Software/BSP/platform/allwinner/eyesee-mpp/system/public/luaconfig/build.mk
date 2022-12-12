TARGET_PATH :=$(call my-dir)

include $(ENV_CLEAR)

TARGET_SRC := \
	lua_config_parser.cpp \
	LuaConfigHelper.cpp

TARGET_CPPFLAGS += -fPIC

TARGET_SHARED_LIB := \
    liblua

TARGET_MODULE := libluaconfig

include $(BUILD_SHARED_LIB)

include $(ENV_CLEAR)

TARGET_SRC := \
    lua_config_parser.cpp \
    LuaConfigHelper.cpp

TARGET_CPPFLAGS += -fPIC

TARGET_STATIC_LIB := \
    liblua

TARGET_MODULE := libluaconfig

include $(BUILD_STATIC_LIB)

#######################################

# include $(ENV_CLEAR)

# TARGET_SRC := lua_config_parser.cpp test.cpp

# TARGET_CPPFLAGU +=

# TARGET_SHARED_LIB := \
    # liblua

# TARGET_LDFLAGS := \
    # -ldl

# TARGET_MODULE := test-luaconfig

# include $(BUILD_BIN)

# copy fils as a lua package
include $(ENV_CLEAR)
$(call copy-one-file, \
    $(TARGET_PATH)/persistence.lua, \
    $(TARGET_OUT)/target/usr/share/lua/5.1/persistence.lua \
)
