/* *******************************************************************************
 * Copyright (c), 2001-2016, Allwinner Tech. All rights reserved.
 * *******************************************************************************/
/**
 * @file    test.cpp
 * @brief   ${DESCRIPTION}
 * @author  id:826
 * @version v0.1
 * @date    2016-10-11
 */

#include "lua/lua_config_parser.h"
#include "lua.hpp"

#include <string>
#include <iostream>

using namespace std;

int main(int argc, char *argv[])
{
    LuaConfig *lua_cfg = new LuaConfig();

    lua_cfg->LoadFromFile("/tmp/data/ipc_config.lua");

    // Get Value
    string ssid = lua_cfg->GetStringValue("system.network.wifi[1].ssid");
    cout << "ssid: " << ssid << endl;

    int width = lua_cfg->GetIntegerValue("system.screen.width");
    cout << "width: " << width << endl;

    bool dhcp = lua_cfg->GetBoolValue("system.network.ethernet.dhcp");
    cout << "dhcp: " << dhcp << endl;

    // Set Value
    lua_cfg->SetStringValue("system.network.wifi[1].ssid", "wifi-hehe");
    lua_cfg->SetIntegerValue("system.screen.width", 1920);
    lua_cfg->SetBoolValue("system.network.ethernet.dhcp", false);

    // Get Value Again
    ssid = lua_cfg->GetStringValue("system.network.wifi[1].ssid");
    cout << "ssid: " << ssid << endl;

    width = lua_cfg->GetIntegerValue("system.screen.width");
    cout << "width: " << width << endl;

    dhcp = lua_cfg->GetBoolValue("system.network.ethernet.dhcp");
    cout << "dhcp: " << dhcp << endl;

    lua_cfg->SyncToFile("/tmp/data/ipc_config.lua");

    return 0;
}

