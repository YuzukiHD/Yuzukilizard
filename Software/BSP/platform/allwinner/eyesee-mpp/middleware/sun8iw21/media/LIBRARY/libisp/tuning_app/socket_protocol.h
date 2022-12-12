#ifndef _SOCKET_PROTOCOL_H_V100_
#define _SOCKET_PROTOCOL_H_V100_

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include "../isp_version.h"

#define SOCK_DEFAULT_TIMEOUT 30  // s


typedef struct sock_comm_packet_s
{
	unsigned char head_flag[4];  // for data check
	unsigned char cmd_ids[8];    // 1 byte - command, 1 byte - group id, 4 bytes - cfg ids
	int           ret;
	int           data_length;
	int           reserved[4]; 
	int	      framecount;
	int	      index;
} sock_packet;

typedef enum sock_command_code_e
{
	/*
	 * for get/set config
	 * cmd_ids[0] - command, cmd_ids[1] - get/set cmd, cmd_ids[2] - group id, cmd_ids[3-6] - cfg ids
	 */
	SOCK_CMD_TUNING          = 0x01,   // tuning
	SOCK_CMD_GET_CFG,
	SOCK_CMD_SET_CFG,
	SOCK_CMD_UPDATE_CFG,
	SOCK_CMD_ISP_SEL,                  // select isp
	/*
	 * for capture
	 * cmd_ids[0] - command
	 */
	SOCK_CMD_CAPTURE,                  // capture
	/*
	 * for preview
	 * cmd_ids[0] - command
	 */
	SOCK_CMD_PREVIEW,                  // preview
	/*
	 * for get statistics
	 * cmd_ids[0] - command, cmd_ids[1] - cmd type
	 */
	SOCK_CMD_GET_STAT,                 // get statistics
	SOCK_CMD_STAT_AE,
	SOCK_CMD_STAT_AWB,
	SOCK_CMD_STAT_AF,
	/*
	 * for shell script
	 * cmd_ids[0] - command
	 */
	SOCK_CMD_SCRIPT,                   // run script
	/*
	 * for heart jump
	 * cmd_ids[0] - command
	 */
	SOCK_CMD_HEART_JUMP,               // heart jump
	/*
	 * for close socket
	 * cmd_ids[0] - command
	 */
	SOCK_CMD_CLOSE,                    // close socket
	/*
	 * for exit
	 * cmd_ids[0] - command
	 */
	SOCK_CMD_EXIT,                     // app exit
	/*
	 * for ack
	 * cmd_ids[0] - command, cmd_ids[1] - sub cmd, cmd_ids[2] - socket type
	 */
	SOCK_CMD_ACK,                      // app ack
	SOCK_CMD_ACK_QUESTION,             // ack sub cmd
	SOCK_CMD_ACK_ANSWER,               // ack sub cmd
	SOCK_CMD_ACK_RET,                  // ack sub cmd
	SOCK_CMD_ACK_STATUS,               // ack sub cmd
	/*
	 * for register operation
	 * cmd_ids[0] - command, cmd_ids[1] - sub cmd
	 */
	SOCK_CMD_REGOPT,                   // reg opt
	SOCK_CMD_REG_SETCFG,               // set register config
	SOCK_CMD_REG_READ,                 // read register
	SOCK_CMD_REG_WRITE,                // write register
	/*
	 * for ae lv
	 */
	SOCK_CMD_AE_LV,
	/*
	 * set sensor input
	 */
	SOCK_CMD_SET_INPUT,
	/*
	 * get raw flow
	 */
	SOCK_CMD_RAW_FLOW,
	SOCK_CMD_RAW_FLOW_START,           // start raw flow
	SOCK_CMD_RAW_FLOW_STOP,            // stop raw flow
	SOCK_CMD_RAW_FLOW_GET,             // get one raw data

	/*
	* VENC
	*/
	SOCK_CMD_VENC_GET,		//get venc configure 
	SOCK_CMD_VENC_SET,		//set venc configure
	SOCK_CMD_VENC_UPDATE_CFG,//update venc configure
	SOCK_CMD_VENC_SEL,		//select venc

	/* isp version*/
	SOCK_CMD_ISP_VERSION,

	/*
	* preview vencode
	*/
	SOCK_CMD_VENCODE,			//preview vencode
	SOCK_CMD_VENCODE_PPSSPS,	//get pps sps data
	SOCK_CMD_VENCODE_STREAM,	//get h264 stream
	SOCK_CMD_VENCODE_STOP,		//stop vencode
	/*
	*for gray block info
	*/
	SOCK_CMD_BLOCKINFO,
	SOCK_CMD_SET_REGION,
	SOCK_CMD_GET_BLOCKINFO,
	/*
	* preview vencode for get encpp yuv
	*/
	SOCK_CMD_VENCODE_ENCPP_YUV,
	/*
	* for get some isp log
	*/
	SOCK_CMD_GET_LOG,
	/*
	* for get statistics
	* cmd_ids[1] - cmd type
	*/
	SOCK_CMD_STAT_TDNF,

	SOCK_CMD_COUNT
} sock_command_code;

typedef enum sock_command_ret_e {
	SOCK_CMD_RET_OK          = 0x01,
	SOCK_CMD_RET_FAILED,
} sock_command_ret;

typedef enum sock_type_e {
	SOCK_TYPE_HEART_JUMP     = 0x0001,
	SOCK_TYPE_PREVIEW,
	SOCK_TYPE_CAPTURE,
	SOCK_TYPE_TUNING,
	SOCK_TYPE_STATISTICS,
	SOCK_TYPE_SCRIPT,
	SOCK_TYPE_REGOPT,
	SOCK_TYPE_AELV,
	SOCK_TYPE_SET_INPUT,
	SOCK_TYPE_RAW_FLOW,
	SOCK_TYPE_VENC_TUNING,
	SOCK_TYPE_ISP_VERSION,
	SOCK_TYPE_PREVIEW_VENCODE,
	SOCK_TYPE_BLOCK_INFO,
	SOCK_TYPE_RESERVE_EXIT,
	SOCK_TYPE_LOG,
} sock_type;

/*
 * socket read and write
 * sock_fd: socket fd to read/write
 * buf: buffer to read/write
 * len: max length of buffer to read/write
 * timeout: timeout in seconds, <=0 means never timeout
 * returns length of read/write in bytes
 */
int sock_read(int sock_fd, void *buf, int len, int timeout);
int sock_write(int sock_fd, const void *buf, int len, int timeout);
/*
 * accept timeout
 * sock_fd: socket fd
 * addr: the address of the peer socket
 * addrlen: the size of addr
 * timeout: timeout in seconds, non-positive means never timeout
 * return a descriptor for the accepted socket, <0 if something went wrong
 */
int socket_accept(int sock_fd, struct sockaddr *addr, socklen_t *addrlen, int timeout);

/*
 * check socket packet
 * returns 0 if OK, <0 if something went wrong
 */
int check_sock_packet(const sock_packet *packet, sock_command_code sock_cmd);
/*
 * package a packet
 * only fill in the head_flag
 */
void pack_packet(sock_packet *packet);

typedef enum sock_rw_check_ret_e {
	SOCK_RW_CHECK_OK        = 0x00,
	SOCK_RW_WRITE_ERROR     = 0x01,
	SOCK_RW_READ_ERROR      = 0x02,
	SOCK_RW_CHECK_ERROR     = 0x03,
	SOCK_RW_RECV_CLOSE      = 0x04,
	SOCK_RW_RET_ERROR       = 0x05,
	SOCK_RW_TRY_AGAIN       = 0x06,
	SOCK_RW_ACK_STATUS      = 0x07,
} sock_rw_check_ret;

#define SHOULD_TRY_AGAIN() \
	(0 == errno || EAGAIN == errno || EWOULDBLOCK == errno || EINTR == errno || ETIMEDOUT == errno)

#define SHOULD_CLOSE_SOCK(ret) \
	(SOCK_RW_RECV_CLOSE == ret || SOCK_RW_WRITE_ERROR == ret || SOCK_RW_READ_ERROR == ret)


/*
 * socket read/write and check
 * for write, it write packet first, then wait for reply and check it
 * func_name: log tag, called function name
 * sock_fd: sockect fd
 * comm_packet: socket packet to read/write
 * sock_cmd: socket command
 * timeout: timeout in seconds
 * returns sock_rw_check_ret
 */
int sock_read_check_packet(const char *func_name, int sock_fd, sock_packet *comm_packet, 
	sock_command_code sock_cmd, int timeout);
int sock_write_check_packet(const char *func_name, int sock_fd, sock_packet *comm_packet, 
	sock_command_code sock_cmd, int timeout);



#endif /* _SOCKET_PROTOCOL_H_V100_ */
