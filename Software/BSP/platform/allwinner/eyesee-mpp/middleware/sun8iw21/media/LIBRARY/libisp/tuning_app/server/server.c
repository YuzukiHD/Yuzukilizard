#include "server_api.h"

#if 0
int main(int argc, const char *argv[])
{
	int ret = -1;
	
#if SERVER_DEBUG_EN
	if (argc >= 2) {
		init_logger(argv[1], "wb");
	} else {
		init_logger("/data/hawkview_server.log", "wb");
	}
#endif
	
	ret = init_server();
	if (!ret) {
		ret = run_server();
	}

	exit_server();

#if SERVER_DEBUG_EN
	close_logger();
#endif

	return 0;
}
#endif
