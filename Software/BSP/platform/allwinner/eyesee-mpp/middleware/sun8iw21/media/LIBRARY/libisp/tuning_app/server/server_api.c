#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <signal.h>

#include "../socket_protocol.h"
#include "../thread_pool.h"
#include "../log_handle.h"

#include "server_api.h"
#include "server_core.h"
#include "capture_image.h"
#include "mini_shell.h"
#include "isp_handle.h"
#include "raw_flow_opt.h"


#define SERVER_THREADS_MAX   20

enum server_flag_e {
	SERVER_FLAG_INIT_NOT     = 0x00,
	SERVER_FLAG_INIT_YES     = 0x01,
	SERVER_FLAG_STOP_NOT     = 0x02,
	SERVER_FLAG_STOP_YES     = 0x03,
};

static int                   g_server_init_flag = SERVER_FLAG_INIT_NOT;
static int                   g_server_socket_fd = -1;
static int                   g_server_stop_flag = SERVER_FLAG_STOP_NOT;

/*
 *length of question/answer cannot be greater than sock_packet.reserved in bytes
 * for current, the lenght are both 16, sock_packet.reserved is 16
 */
#define SERVER_SOCK_QUESTION "wHo Am i? woOOo"
#define SERVER_SOCK_ANSWER   "WHos yoUR daddY"

static int client_handle(int sock_fd)
{
	int ret = -1;
	static sock_packet comm_packet;
	thread_work_func thread_func_ptr = NULL;

	LOG("%s: starts - %d\n", __FUNCTION__, sock_fd);

	// 1. ack question
	pack_packet(&comm_packet);
	comm_packet.cmd_ids[0] = SOCK_CMD_ACK;
	comm_packet.cmd_ids[1] = SOCK_CMD_ACK_QUESTION;
	comm_packet.ret = htonl(SOCK_CMD_RET_OK);
	strcpy((char *)comm_packet.reserved, SERVER_SOCK_QUESTION);

	ret = sock_write_check_packet(__FUNCTION__, sock_fd, &comm_packet, SOCK_CMD_ACK, SOCK_DEFAULT_TIMEOUT);
	if (ret != SOCK_RW_CHECK_OK) {
		LOG("%s: failed to write and check\n", __FUNCTION__);
		close(sock_fd);
		return -1;
	}

	if (comm_packet.cmd_ids[1] != SOCK_CMD_ACK_ANSWER ||
		strncmp((const char *)comm_packet.reserved, SERVER_SOCK_ANSWER, sizeof(comm_packet.reserved))) {
		((char *)comm_packet.reserved)[sizeof(comm_packet.reserved) - 1] = '\0';
		LOG("%s: wrong answer: %d, %s\n", __FUNCTION__, comm_packet.cmd_ids[1], (const char *)comm_packet.reserved);
		close(sock_fd);
		return -1;
	}

	switch (comm_packet.cmd_ids[2]) {
	case SOCK_TYPE_HEART_JUMP:
		thread_func_ptr = &sock_handle_heart_jump_thread;
		break;
	case SOCK_TYPE_PREVIEW:
		thread_func_ptr = &sock_handle_preview_thread;
		break;
	case SOCK_TYPE_CAPTURE:
		thread_func_ptr = &sock_handle_capture_thread;
		break;
	case SOCK_TYPE_TUNING:
		thread_func_ptr = &sock_handle_tuning_thread;
		break;
	case SOCK_TYPE_STATISTICS:
		thread_func_ptr = &sock_handle_statistics_thread;
		break;
	case SOCK_TYPE_SCRIPT:
		thread_func_ptr = &sock_handle_script_thread;
		break;
	case SOCK_TYPE_REGOPT:
		thread_func_ptr = &sock_handle_register_thread;
		break;
	case SOCK_TYPE_AELV:
		thread_func_ptr = &sock_handle_aelv_thread;
		break;
	case SOCK_TYPE_SET_INPUT:
		thread_func_ptr = &sock_handle_set_input_thread;
		break;
	case SOCK_TYPE_RAW_FLOW:
		thread_func_ptr = &sock_handle_raw_flow_thread;
		break;
	case SOCK_TYPE_ISP_VERSION:
		thread_func_ptr = &sock_handle_isp_version_thread;
		break;
	case SOCK_TYPE_PREVIEW_VENCODE:
#ifdef ANDROID_VENCODE
		thread_func_ptr = &sock_handle_preview_vencode_thread;
		break;
#endif
		LOG("App in board does't have preview vencode feature, please rebuild App with ANDROID_VENCODE macro define\n");
		close(sock_fd);
		return -1;
#if 1
	case SOCK_TYPE_BLOCK_INFO:
		thread_func_ptr = &sock_handle_blockinfo_thread;
		break;
	case SOCK_TYPE_LOG:
		thread_func_ptr = &sock_handle_log_thread;
		break;
	case SOCK_CMD_EXIT:
		LOG("%s: recv exit\n", __FUNCTION__);
		g_server_stop_flag = SERVER_FLAG_STOP_YES;
		close(sock_fd);
		return -1;
#endif
	default:
		LOG("%s: unknown request(%d)\n", __FUNCTION__, comm_packet.cmd_ids[2]);
		close(sock_fd);
		return -1;
	}

	// 3. add work
	ret = add_work(thread_func_ptr, (void *)(intptr_t)sock_fd);

	// 4. reply
	//pack_packet(&comm_packet);
	//comm_packet.cmd_ids[0] = SOCK_CMD_ACK;
	comm_packet.cmd_ids[1] = SOCK_CMD_ACK_RET;
	if (ret < 0) {
		comm_packet.ret = htonl(SOCK_CMD_RET_FAILED);
		ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
		close(sock_fd);
		LOG("%s: threads out of range\n", __FUNCTION__);
		return -1;
	} else {
		comm_packet.ret = htonl(SOCK_CMD_RET_OK);
		ret = sock_write(sock_fd, (const void *)&comm_packet, sizeof(comm_packet), SOCK_DEFAULT_TIMEOUT);
	}

	return 0;
}

static void reset_server_params()
{
	g_server_init_flag = SERVER_FLAG_INIT_NOT;
	g_server_socket_fd = -1;
	g_server_stop_flag = SERVER_FLAG_STOP_NOT;
}

/*
 * get ip address by socket
 * returns 0 if OK, -1 if something went wrong
 */
int get_sock_ip(int sock_fd, char *ip_addr)
{
	struct ifconf if_config = { 0 };
	struct ifreq *if_req = NULL;
	char buf[512] = { 0 }, *ip0 = NULL;
	int i = 0, ret = -1;

	if (sock_fd < 0 || !ip_addr) {
		return -1;
	}

	if_config.ifc_len = sizeof(buf);
	if_config.ifc_buf = buf;

	if (ioctl(sock_fd, SIOCGIFCONF, &if_config) < 0) {
		return -1;
	}

	if_req = if_config.ifc_req;
	for (i = if_config.ifc_len / sizeof(struct ifreq); i > 0; i--) {
		ip0 = inet_ntoa(((struct sockaddr_in *)&(if_req->ifr_addr))->sin_addr);
		LOG("    ip: %s, name: %s\n", ip0, if_req->ifr_name);
		if (ret < 0 && strncmp(ip0, "127.0", 4) && !strncmp(if_req->ifr_name, "eth", 3)) {
			strcpy(ip_addr, ip0);
			ret = 0;
		}
		if_req++;
	}

	return ret;
}

/*
 * init server
 * returns 0 if init success, <0 if something went wrong
 */
int init_server(int port)
{
	struct sockaddr_in server_socket_addr = { 0 };
	char ip_addr[64] = { 0 }, *ip = NULL;
	int ret = -1;
	int optVal = 1;

#ifdef FUNCTION_END_FLAG
#undef FUNCTION_END_FLAG
#endif
#define FUNCTION_END_FLAG    _##__FUNCTION__##_END_


	if (SERVER_FLAG_INIT_YES == g_server_init_flag) {
		exit_server();
	}

	reset_server_params();

	server_socket_addr.sin_family = AF_INET;
	server_socket_addr.sin_port = htons(port);
	server_socket_addr.sin_addr.s_addr = htons(INADDR_ANY);

	// creat sever socket
	ret = socket(AF_INET, SOCK_STREAM, 0);
	if (-1 == ret) {
		LOG("%s: failed to create socket(%d, %s)\n", __FUNCTION__, errno, strerror(errno));
		return -1;
	}
	g_server_socket_fd = ret;

	if (get_sock_ip(g_server_socket_fd, ip_addr) == 0) {
		ip = ip_addr;
	} else {
		ip = "unknown host";
	}

	LOG("%s: ready to start server: %s:%d ++++++++++++++++++++++++\n", __FUNCTION__, ip, port)

	// ignoral pipe error
	signal(SIGPIPE, SIG_IGN);

	setsockopt(g_server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &optVal, sizeof(optVal));

	// bind
	ret = bind(g_server_socket_fd, (struct sockaddr*)&server_socket_addr, sizeof(server_socket_addr));
	if (-1 == ret) {
		LOG("%s: failed to bind socket(%d, %s)\n", __FUNCTION__, errno, strerror(errno));
		goto FUNCTION_END_FLAG;
	}

	//listen
	ret = listen(g_server_socket_fd, SERVER_THREADS_MAX);
	if (-1 == ret) {
		LOG("%s: failed to listen socket(%d, %s)\n", __FUNCTION__, errno, strerror(errno));
		goto FUNCTION_END_FLAG;
	}

	LOG("%s: server starts:         %s:%d ++++++++++++++++++++++++\n", __FUNCTION__, ip, port);

	ret = init_thread_pool();
	if (ret < 0) {
		LOG("%s: failed to init thread pool\n", __FUNCTION__);
		goto FUNCTION_END_FLAG;
	}

	ret = init_capture_module();
	if (CAP_ERR_NONE != ret) {
		LOG("%s: failed to init capture module\n", __FUNCTION__);
	}

	//ret = init_isp_module();
	//if (ret < 0) {
	//	LOG("%s: failed to init isp module\n", __FUNCTION__);
	//}
	
	//ret = select_isp(0);
	//if (ret < 0) {
	//	LOG("%s: failed to select isp 0\n", __FUNCTION__);
	//}


	/////
#if 0
	msleep(1000);
	LOG("%s: ready to select isp 0\n", __FUNCTION__);
	ret = select_isp(0);
	if (ret < 0) {
		LOG("%s: failed to select isp 0\n", __FUNCTION__);
	}
	LOG("%s: ready to select isp 0, DONE\n", __FUNCTION__);

#endif

	ret = init_mini_shell(NULL);
	if (0 != ret) {
		LOG("%s: failed to init mini shell\n", __FUNCTION__);
	}
	
	g_server_init_flag = SERVER_FLAG_INIT_YES;
	LOG("%s: init done\n", __FUNCTION__);
	return 0;

FUNCTION_END_FLAG:
	close(g_server_socket_fd);
	g_server_socket_fd = -1;
	g_server_init_flag = SERVER_FLAG_INIT_NOT;
	return ret;
}


/*
 * run server
 * this function will be running until get some exit message from client/server
 * returns 0 if exit normal, <0 if something went wrong
 */
int run_server()
{
	int ret = -1;
	int client_socket = -1;
	int try_times = 0;
	struct sockaddr_in client_socket_addr;
	socklen_t client_socket_addr_len = sizeof(client_socket_addr);

	if (SERVER_FLAG_INIT_YES == g_server_init_flag) {
		while (1) {
			if (SERVER_FLAG_STOP_YES == g_server_stop_flag) {
				LOG("%s: recv stop flag\n", __FUNCTION__);
				return 0;
			}
			ret = CheckThreadsStatus();
			if (ret & TH_STATUS_SERVER_QUIT) {
				LOG("%s: recv STOP, ready to stop server(%08x)...\n", __FUNCTION__, ret);
				try_times = 0;
				while ((try_times++ < 100) && (ret & 0x00FFFFFF)) {
					msleep(32);
					ret = CheckThreadsStatus();
				}
				LOG("%s: exits(%d, %08x)\n", __FUNCTION__, try_times, ret);
				return 0;
			}
			
			memset(&client_socket_addr, 0, sizeof(client_socket_addr));
			client_socket = socket_accept(g_server_socket_fd,
				(struct sockaddr*)&client_socket_addr, &client_socket_addr_len, SOCK_DEFAULT_TIMEOUT);
			if (client_socket >= 0) {
				ret = client_handle(client_socket);
			} else {
				if (!SHOULD_TRY_AGAIN()) {
					LOG("%s: failed to accept(%d, %s)\n", __FUNCTION__, errno, strerror(errno));
					return -1;
				} else {
					errno = 0;
				}
			}

			msleep(1);
		}
	}

	LOG("%s: not init yet\n", __FUNCTION__);
	return -1;
}

/*
 * exit server
 * clean and exit
 */
void exit_server()
{
	struct ToolsIniTuning_cfg ini_cfg = GetIniTuningEn();
	g_server_stop_flag = SERVER_FLAG_STOP_YES;
	if (SERVER_FLAG_INIT_YES == g_server_init_flag) {
		close(g_server_socket_fd);
		g_server_socket_fd = -1;
		g_server_init_flag = SERVER_FLAG_INIT_NOT;
	}

	exit_mini_shell();
	if (!ini_cfg.enable) {
		exit_isp_module();
	}
	exit_capture_module();
	exit_thread_pool();

	LOG("%s: exits\n", __FUNCTION__);
}


