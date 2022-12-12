#include "socket_protocol.h"
#include "log_handle.h"

#define BLOCK_MAX_LENGTH     (1<<15)

/*
 * operate with timeout, not includes the real operation
 * fd: file descriptor
 * timeout: timeout in seconds, non-positive means never timeout
 * returns 0 if 0K, -1 if timeout
 */
static int read_timeout(int fd, int timeout)
{
	int ret = -1;
	if (timeout > 0) {
		fd_set fs;
		struct timeval t;

		FD_ZERO(&fs);
		FD_SET(fd, &fs);
		t.tv_sec = timeout;
		t.tv_usec = 0;

		do {
			// select will block until detects event or timeout
			// if detects event, the operation will be nonblock
			ret = select(fd + 1, &fs, NULL, NULL, &t);
		} while (ret < 0 && EINTR == errno);

		if (0 == ret) {
			errno = ETIMEDOUT;
			return -1;
		} else if (1 == ret) {
			return 0;
		} else {
			return -1;
		}
	}

	return 0;
}
static int write_timeout(int fd, int timeout)
{
	int ret = -1;
	if (timeout > 0) {
		fd_set fs;
		struct timeval t;

		FD_ZERO(&fs);
		FD_SET(fd, &fs);
		t.tv_sec = timeout;
		t.tv_usec = 0;

		do {
			// select will block until detects event or timeout
			// if detects event, the operation will be nonblock
			ret = select(fd + 1, NULL, &fs, NULL, &t);
		} while (ret < 0 && EINTR == errno);

		if (0 == ret) {
			errno = ETIMEDOUT;
			return -1;
		} else if (1 == ret) {
			return 0;
		} else {
			return -1;
		}
	}

	return 0;
}


int sock_read(int sock_fd, void *buf, int len, int timeout)
{
	int recv_len = 0, recv_total = 0, recv_ret = 0;
	char *data_ptr = (char *)buf;

    while (len > 0) {
		if (timeout > 0 && read_timeout(sock_fd, timeout) != 0) {
			//LOG("%s: time out(%d, %s)\n", __FUNCTION__, errno, strerror(errno));
			break;
		}

		recv_len = len > BLOCK_MAX_LENGTH ? BLOCK_MAX_LENGTH : len;
		recv_ret = recv(sock_fd, (void *)data_ptr, recv_len, 0);
		if (0 == recv_ret) {  // the peer has performed an orderly shutdown
			errno = ENOTCONN;
			LOG("%s: %d has performed an orderly shutdown *********\n", __FUNCTION__, sock_fd);
			break;
		} else if (recv_ret < 0) {
			break;
		}

		recv_total += recv_ret;
		len        -= recv_ret;
		data_ptr   += recv_ret;
	}

	return recv_total;
}

int sock_write(int sock_fd, const void *buf, int len, int timeout)
{
	int send_len = 0, send_total = 0, send_ret = 0;
	const char *data_ptr = (const char *)buf;

	while (len > 0) {
		if (timeout > 0 && write_timeout(sock_fd, timeout) != 0) {
			LOG("%s: time out for sock %d(%d, %s)\n", __FUNCTION__,
				sock_fd, errno, strerror(errno));
			break;
		}

		send_len = len > BLOCK_MAX_LENGTH ? BLOCK_MAX_LENGTH : len;
		send_ret = send(sock_fd, (const void *)data_ptr, send_len, 0);
		if (send_ret <= 0) {
			LOG("%s: failed to send(sock %d, %d)\n", __FUNCTION__, sock_fd, send_ret);
			break;
		}

		send_total += send_ret;
		len        -= send_ret;
		data_ptr   += send_ret;
	}

	return send_total;
}

int socket_accept(int sock_fd, struct sockaddr *addr, socklen_t *addrlen, int timeout)
{
	if (timeout > 0 && read_timeout(sock_fd, timeout) != 0) {
		//LOG("%s: time out(%d, %s)\n", __FUNCTION__, errno, strerror(errno));
		return -1;
	}

	return accept(sock_fd, addr, addrlen);
}


#define SERVER_SOCK_HEAD     "AWhw"

/*
 * check socket packet
 * returns 0 if OK, <0 if something went wrong
 */
int check_sock_packet(const sock_packet *packet, sock_command_code sock_cmd)
{
	// check head flag
	if (strncmp((const char *)packet->head_flag, SERVER_SOCK_HEAD, sizeof(packet->head_flag))) {
		return -1;
	}

	// check command
	if (packet->cmd_ids[0] != sock_cmd) {
		return -2;
	}

	return 0;	
}

/*
 * package a packet
 * only fill in the head_flag
 */
void pack_packet(sock_packet *packet)
{
	memset(packet, 0, sizeof(sock_packet));
	memcpy(packet->head_flag, SERVER_SOCK_HEAD, sizeof(packet->head_flag));
}

/*
 * socket read/write and check
 * func_name: log tag, called function name
 * sock_fd: sockect fd
 * comm_packet: socket packet to read/write
 * sock_cmd: socket command
 * timeout: timeout in seconds
 * returns sock_rw_check_ret
 */
int sock_read_check_packet(const char *func_name, int sock_fd, sock_packet *comm_packet, 
	sock_command_code sock_cmd, int timeout)
{
	int ret = -1;
	// 1. read
	memset(comm_packet, 0, sizeof(sock_packet));
	ret = sock_read(sock_fd, (void *)comm_packet, sizeof(sock_packet), timeout);
	if (ret != sizeof(sock_packet)) {
		if (SHOULD_TRY_AGAIN()) {
			//LOG("%s#%s: failed to read, try again\n", __FUNCTION__, func_name);
			return SOCK_RW_TRY_AGAIN;
		} else {
			LOG("%s#%s: failed to read sock %d(%d, %s)\n", __FUNCTION__, func_name, 
				sock_fd, errno, strerror(errno));
			return SOCK_RW_READ_ERROR;
		}
	}

	// 2. check close
	ret = check_sock_packet(comm_packet, SOCK_CMD_CLOSE);
	if (0 == ret) {
		LOG("%s#%s: recv close sock %d\n", __FUNCTION__, func_name, sock_fd);
		return SOCK_RW_RECV_CLOSE;
	}

	// 3. check ack status
	ret = check_sock_packet(comm_packet, SOCK_CMD_ACK);
	if (0 == ret && SOCK_CMD_ACK_STATUS == comm_packet->cmd_ids[1]) {
		ret = sock_write(sock_fd, (const void *)comm_packet, sizeof(sock_packet), timeout);
		if (ret != sizeof(sock_packet)) {
			if (SHOULD_TRY_AGAIN()) {
				LOG("%s#%s: failed to write ACK to sock %d(%d, %s), try again\n", __FUNCTION__, func_name,
					sock_fd, errno, strerror(errno));
				return SOCK_RW_TRY_AGAIN;
			} else {
				LOG("%s#%s: failed to write ACK to sock %d(%d, %s)\n", __FUNCTION__, func_name,
					sock_fd, errno, strerror(errno));
				return SOCK_RW_WRITE_ERROR;
			}
		}
		//LOG("%s#%s: check ack status OK\n", __FUNCTION__, func_name);
		return SOCK_RW_ACK_STATUS;
	}

	// 4. check command
	ret = check_sock_packet(comm_packet, sock_cmd);
	if (ret < 0) {
		LOG("%s#%s: check error(sock %d, %d)\n", __FUNCTION__, func_name, sock_fd, ret);
		return SOCK_RW_CHECK_ERROR;
	}

	// 5. check ret
	ret = ntohl(comm_packet->ret);
	if (ret != SOCK_CMD_RET_OK) {
		LOG("%s#%s: reply ret error(sock %d, %d)\n", __FUNCTION__, func_name, sock_fd, ret);
		return SOCK_RW_RET_ERROR;
	}	

	return SOCK_RW_CHECK_OK;
}

int sock_write_check_packet(const char *func_name, int sock_fd, sock_packet *comm_packet, 
	sock_command_code sock_cmd, int timeout)
{
	int ret = -1;
	// 1. write to socket
	ret = sock_write(sock_fd, (const void *)comm_packet, sizeof(sock_packet), timeout);
	if (ret != sizeof(sock_packet)) {
		if (SHOULD_TRY_AGAIN()) {
			LOG("%s#%s: failed to write, try again\n", __FUNCTION__, func_name);
			return SOCK_RW_TRY_AGAIN;
		} else {
			LOG("%s#%s: failed to write(%d, %s)\n", __FUNCTION__, func_name, errno, strerror(errno));
			return SOCK_RW_WRITE_ERROR;
		}
	}

	// 2. wait for reply
	return sock_read_check_packet(func_name, sock_fd, comm_packet, sock_cmd, timeout);
}


