/* *******************************************************************************
 * Copyright (c), 2001-2016, Allwinner Tech. All rights reserved.
 * *******************************************************************************/
/**
 * @file    lua_config_parser.cpp
 * @brief   lua 配置解析类
 * @author  id:826
 * @version v0.1
 * @date    2016-10-11
 */


#include "lua/lua_config_parser.h"
#include "lua.hpp"

#include <sstream>
#include <unistd.h>

#define DEBUG 1

#define LOG_TAG "LuaConfig"

#define LUA_ERROR(msg) \
    luaL_error(lua_context_, "\n\t%s: <line[%d] %s> %s\n", \
            LOG_TAG, __LINE__, __FUNCTION__, msg)

#define ERROR_PRINT(msg, args...) \
    fprintf(stderr, "%s: <line[%d] %s> " msg "\n", \
            LOG_TAG, __LINE__, __FUNCTION__, ##args)

LuaConfig::LuaConfig()
{
    lua_context_ = lua_open();
    luaL_openlibs(lua_context_);
}

LuaConfig::~LuaConfig()
{
    lua_close(lua_context_);
}

lua_State *LuaConfig::GetContext()
{
    return lua_context_;
}

int LuaConfig::LoadFromFile(const std::string &filepath)
{
    std::lock_guard<std::mutex> lock(lock_);

    int ret = luaL_dofile(lua_context_, filepath.c_str());
    if (ret != 0) {
        ERROR_PRINT("%s", lua_tostring(lua_context_, -1));
        lua_pop(lua_context_, 1);
        return -1;
    }

    ret = luaL_dostring(lua_context_, "persistence = require\"persistence\"");
    if (ret != 0) {
        ERROR_PRINT("%s", lua_tostring(lua_context_, -1));
        lua_pop(lua_context_, 1);
        return -1;
    }

    return 0;
}

int LuaConfig::SyncConfigToFile(const std::string &filepath, const std::string &table)
{
    std::lock_guard<std::mutex> lock(lock_);

    std::stringstream lua_str;
    lua_str << "persistence.tofile" << "(\"" << filepath << "\"," << table << ",\"" << table << "\")";

    if (luaL_dostring(lua_context_, lua_str.str().c_str()) != 0) {
        ERROR_PRINT("%s", lua_tostring(lua_context_, -1));
        lua_pop(lua_context_, 1);
        return -1;
    }

    sync();

    return 0;
}

int LuaConfig::SyncToFile(const std::string &filepath)
{
    std::lock_guard<std::mutex> lock(lock_);

    std::stringstream lua_str;
    lua_str << "persistence.tofile" << "(\"" << filepath << "\")";

    if (luaL_dostring(lua_context_, lua_str.str().c_str()) != 0) {
        ERROR_PRINT("%s", lua_tostring(lua_context_, -1));
        lua_pop(lua_context_, 1);
        return -1;
    }

    sync();

    return 0;
}

int LuaConfig::LoadValueToStack(const std::string &path)
{
    std::lock_guard<std::mutex> lock(lock_);

    std::string lua_str = "value=" + path;
    if (luaL_dostring(lua_context_, lua_str.c_str()) != 0) {
        ERROR_PRINT("%s", lua_tostring(lua_context_, -1));
        lua_pop(lua_context_, 1);
        return -1;
    }
    lua_getglobal(lua_context_, "value");

    return 0;
}

int LuaConfig::SetValueToLua(const std::string &path, const std::string &value)
{
    std::lock_guard<std::mutex> lock(lock_);

    std::stringstream lua_str;
    lua_str << path << "=" << value;

    if (luaL_dostring(lua_context_, lua_str.str().c_str()) != 0) {
        ERROR_PRINT("%s", lua_tostring(lua_context_, -1));
        lua_pop(lua_context_, 1);
        return -1;
    }

    return 0;
}


std::string LuaConfig::GetStringValue(const std::string &path)
{
    return GetStringValue(path, "");
}

std::string LuaConfig::GetStringValue(const std::string &path, const std::string &default_value)
{
    std::string str = "";

    int res = LoadValueToStack(path);
    if (res == -1) return default_value;

    if (lua_isstring(lua_context_, -1)) {
        str = lua_tostring(lua_context_, -1);
    } else {
        ERROR_PRINT("get value failed, used default[%s=%s]", path.c_str(), default_value.c_str());
        str = default_value;
    }

    lua_pop(lua_context_, 1);

    return str;
}

int LuaConfig::GetIntegerValue(const std::string &path)
{
    return GetIntegerValue(path, -1);
}

int LuaConfig::GetIntegerValue(const std::string &path, const int default_value)
{
    int ret = -1;

    int res = LoadValueToStack(path);
    if (res == -1) return default_value;

    if (lua_isnumber(lua_context_, -1)) {
        ret = (int)lua_tointeger(lua_context_, -1);
    } else {
        ERROR_PRINT("get value failed, used default[%s=%d]", path.c_str(), default_value);
        ret = default_value;
    }

    lua_pop(lua_context_, 1);

    return ret;
}

double LuaConfig::GetDoubleValue(const std::string &path)
{
    return GetDoubleValue(path, 0);
}

double LuaConfig::GetDoubleValue(const std::string &path, const double default_value)
{
    double ret = 0.0f;

    int res = LoadValueToStack(path);
    if (res == -1) return default_value;

    if (lua_isnumber(lua_context_, -1)) {
        ret = lua_tonumber(lua_context_, -1);
    } else {
        ERROR_PRINT("get value failed, used default[%s=%lf]", path.c_str(), default_value);
        ret = default_value;
    }

    lua_pop(lua_context_, 1);

    return ret;
}

bool LuaConfig::GetBoolValue(const std::string &path)
{
    return GetBoolValue(path, false);
}

bool LuaConfig::GetBoolValue(const std::string &path, const bool default_value)
{
    bool ret = false;

    int res = LoadValueToStack(path);
    if (res == -1) return default_value;

    if (lua_isboolean(lua_context_, -1)) {
        ret = (bool)lua_toboolean(lua_context_, -1);
    } else {
        ERROR_PRINT("get value failed, used default[%s=%s]", path.c_str(), default_value?"true":"false");
        ret = default_value;
    }

    lua_pop(lua_context_, 1);

    return ret;
}

int LuaConfig::SetStringValue(const std::string &path, const std::string &value)
{
    std::stringstream str_str;
    str_str << "\"" << value << "\"";

    return SetValueToLua(path, str_str.str());
}

int LuaConfig::SetIntegerValue(const std::string &path, const int &value)
{
    std::stringstream stream;
    stream << value;

    return SetValueToLua(path, stream.str());
}

int LuaConfig::SetDoubleValue(const std::string &path, const double &value)
{
    std::stringstream stream;
    stream << value;

    return SetValueToLua(path, stream.str());
}

int LuaConfig::SetBoolValue(const std::string &path, const bool &value)
{
    if (value) {
        return SetValueToLua(path, "true");
    } else {
        return SetValueToLua(path, "false");
    }
}

void LuaConfig::RegisterTableFiled(const std::string &table_name, const std::string &filed, const int &value)
{
    lua_getglobal(lua_context_, table_name.c_str());
    lua_pushstring(lua_context_, filed.c_str());
    lua_pushnumber(lua_context_, value);
    lua_settable(lua_context_, -3);
    lua_setglobal(lua_context_, table_name.c_str());
}