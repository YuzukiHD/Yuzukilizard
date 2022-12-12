#include "log_handle.h"
#include "./server/server_api.h"
#include "./server/server_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SERVER_PORT          8848
#define BUILD_VERSION        "Build Version: 0.9.2"
#define DEFAULT_PATH         "/tmp/"
int main(int argc, const char *argv[])
{
	int ret = -1;
	int port = SERVER_PORT;
	char path[50], command[70];
	struct ToolsIniTuning_cfg ini_cfg;
	ini_cfg.enable = 0;
	sprintf(ini_cfg.base_path, "%s", DEFAULT_PATH);
	sprintf(path, "%sisp_tuning", ini_cfg.base_path);

#if SERVER_DEBUG_EN
	if (argc >= 2) {
		init_logger(argv[1], "wb");
	} else {
		init_logger("/data/hawkview_server.log", "wb");
	}
#endif

	LOG(                  "==================================================================================\n"
		"                  ==========   Welcome to Hawkview Tools Tuning Server                    ==========\n"
		"                  ==========   %s, %s %s                 ==========\n"
		"                  ==========   Copyright (c) 2017 by Allwinnertech Co., Ltd.              ==========\n"
		"                  ==========   http://www.allwinnertech.com                               ==========\n",
		BUILD_VERSION, __TIME__, __DATE__
		);

	if (argc == 2) {
		port = atoi(argv[1]);
	} else if (argc == 3) {
		port = atoi(argv[1]);
		ini_cfg.enable = atoi(argv[2]);
	} else if (argc >= 4) {
		port = atoi(argv[1]);
		ini_cfg.enable = atoi(argv[2]);
		sprintf(ini_cfg.base_path, "%s", argv[3]);
		sprintf(path, "%sisp_tuning", ini_cfg.base_path);
	}
	SetIniTuningEn(ini_cfg);

	if (ini_cfg.enable) {
		if (access(path, F_OK) != 0) {
			sprintf(command, "touch %s", path);
			system(command);
			LOG("Create isptuning file: %s\n", path);
		} else {
			LOG("isptuning file already existed\n");
		}
	}

	ret = init_server(port);
	if (!ret) {
		LOG(              "==========   Hawkview Tools Tuning Server Starts up, Enjoy tuning now!  ==========\n"
		//"                  ==================================================================================\n"
			);
		ret = run_server();
	}

	exit_server();
	LOG(                  "==========   Hawkview Tools Tuning Server Exits, Bye-bye!               ==========\n"
		"                  ==================================================================================\n"
		);

#if SERVER_DEBUG_EN
	close_logger();
#endif

	return 0;
}

