/*
 * LuaConfigHelper.cpp
 *
 *  Created on: 2017年5月3日
 *      Author: liuyangcheng
 */

#include "lua/lua_config_parser.h"
#include "lua/LuaConfigHelper.h"

using namespace std;

LuaConfigHelper::LuaConfigHelper(LuaConfig& luaConfig) :
        luaConfig_(luaConfig)
{
}

LuaConfigHelper::~LuaConfigHelper()
{
}

LuaConfigHelper& LuaConfigHelper::operator [](char* fieldStr)
{
    if (0 != exprSStream_.rdbuf()->in_avail()) {
        exprSStream_ << '.';
    }
    exprSStream_ << fieldStr;

    return *this;
}

LuaConfigHelper& LuaConfigHelper::operator [](int index)
{
    index++;
    exprSStream_ << '[' << index << ']';

    return *this;
}

void LuaConfigHelper::Clear()
{
    exprSStream_.str("");
}

int LuaConfigHelper::IntValue()
{
    int ret = luaConfig_.GetIntegerValue(exprSStream_.str());
    Clear();

    return ret;
}

bool LuaConfigHelper::BoolValue()
{
    bool ret = luaConfig_.GetBoolValue(exprSStream_.str());
    Clear();

    return ret;
}

string LuaConfigHelper::StringValue()
{
    const string& ret = luaConfig_.GetStringValue(exprSStream_.str());
    Clear();

    return ret;
}

void LuaConfigHelper::SetValue(int value)
{
    luaConfig_.SetIntegerValue(exprSStream_.str(), value);
    Clear();
}

void LuaConfigHelper::SetValue(const string& value)
{
    luaConfig_.SetStringValue(exprSStream_.str(), value);
    Clear();
}

void LuaConfigHelper::SetValue(bool value)
{
    luaConfig_.SetBoolValue(exprSStream_.str(), value);
    Clear();
}
