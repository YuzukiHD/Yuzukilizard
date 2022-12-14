/*
* Copyright (c) 2020-2030 allwinnertech.com JetCui<cuiyuntao@allwinnertech.com>
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <memory.h>
#include <errno.h>

#include <sys/time.h>
#include <pthread.h>
#include <lwip/inet.h>
#include "lwip/sockets.h"
#include "lwip/tcpip.h"

#include "trans_header.h"
#include <rtthread.h>
#include "msh.h"
#include <finsh.h>
#include <finsh_api.h>
#include <shell.h>


#ifndef PQ_TCP_PORT
#define PQ_TCP_PORT 5005
#endif
#define MAX_CONNECT_COUNT 2
#define MAX_SOCKET_CONNECT_COUNT 4

#define EVENT_MSG_LEN 4096
#define HEART_INTERVAL_MS 3000
#define CLIENTSIZE 10

struct pollfd poll_fds[MAX_CONNECT_COUNT];
int alive_connect_thread = 0;
int con_fd[MAX_SOCKET_CONNECT_COUNT] = {-1,};
int write_fd;
int read_fd;
int can_close = 0;

#if PQ_DEBUG_LEVEL
static int debug_cout = 0;
#endif

void send_to_translate(struct trans_data* data);
int PQ_translate_init(void);
static void *start_pq_test(void *arg);
int init_pq(void);
int heart_count = 0;

void* connect_thread(void *arg)
{
	char *recv_buf = NULL;
	int client_fd, rev_cnt, tail;
	struct PQ_package *tmp_head, tempackage;
	struct trans_data *trans_da = NULL;
	int eventCount, recvlen, i, ret;
	unsigned int timewaitems = HEART_INTERVAL_MS * 2;
	recv_buf = (char *)arg;

	PQ_Printf("creat a connect thread to deal the socket.");

	recv_buf = (char *)malloc(EVENT_MSG_LEN);
	if (recv_buf == NULL) {
		PQ_Printf("alloc err for connect_thread...\n");
		exit(-1);
	}
	memset(&tempackage, 0, sizeof(tempackage));
	memset(recv_buf, 0, EVENT_MSG_LEN);
	tmp_head = NULL;

wait_event:
	for (i = 0; i < MAX_CONNECT_COUNT; i++) {
		timewaitems = HEART_INTERVAL_MS * 3;
		tmp_head = NULL;
		tail = 0;
        client_fd = poll_fds[i].fd;
        rt_thread_mdelay(200);
        if ((poll_fds[i].revents & POLLHUP) || (poll_fds[i].revents & POLLERR))
        {
            lwip_close(client_fd);
            poll_fds[i].fd = 0;
            PQ_Printf("revents error, delete connection: %d\n", client_fd);

            continue;
        }
		if (poll_fds[i].revents & POLLIN) {
receiv:
            rev_cnt = 0;
			recvlen = recv(client_fd, recv_buf, EVENT_MSG_LEN, MSG_DONTWAIT | MSG_NOSIGNAL);
			if(recvlen == -1)
			{
				heart_count++;
				if(heart_count == 21)
				{
					lwip_close(1);//put out usb ,close adb server socket
				}
			}
			else
			{
				heart_count = 0;
			}
			if (recvlen == EINTR || recvlen == EWOULDBLOCK || recvlen == EAGAIN)
				goto receiv;
			if (recvlen < 0 && heart_count <21) {
                continue;
			}
			if (recvlen == 0 || heart_count >= 21) {
				int t = 0;
				PQ_Printf("close fd = %d  i = %d\n",client_fd,i);
				heart_count = 0;
                lwip_close(client_fd);
                poll_fds[i].fd = 0;
                poll_fds[i].revents = 0;
                PQ_Printf("client close, delete connection: %d\n", client_fd);
				goto wait_event;
			}
#if PQ_DEBUG_LEVEL

			{
				struct PQ_package *ttmp_head = NULL;
				if (recvlen >= sizeof(struct PQ_package)) {
					ttmp_head = (struct PQ_package *)(recv_buf);
					PQ_Printf("recive:%c%c%c%c cmd:%d type:%d size:%d times:%dth",
						ttmp_head->head[0], ttmp_head->head[1], ttmp_head->head[2],
						ttmp_head->head[3], ttmp_head->cmd, ttmp_head->type, ttmp_head->size,
						    debug_cout);
					if (recvlen >= sizeof(struct PQ_package) + sizeof(int)) {
						PQ_Printf("id:%d", *(int*)(recv_buf + sizeof(struct PQ_package)));
					}
				}
#if (PQ_DEBUG_LEVEL >= 2)
				int t = 0;
				PQ_Printf("\npackage info[%d]:\n", debug_cout);
				while (t < recvlen) {
					printf("%02x ", *((char*)((recv_buf + rev_cnt)+t)));
					if (++t%30 == 0) {
						printf("\n");
					}
				}
				PQ_Printf("\n################end\n");
#endif
				PQ_Printf("deal the %dth of %d,and buffer size:%d", i, eventCount, recvlen);
			}
#endif

many_head:
			if (tmp_head == NULL) {
				/* little-end */
				tmp_head = (struct PQ_package *)(recv_buf + rev_cnt);
				if (recvlen >= sizeof(struct PQ_package)) {
					if (tmp_head->head[0] != 'A'
					    || tmp_head->head[1] != 'W'
					    || tmp_head->head[2] != 'P'
					    || tmp_head->head[3] != 'Q') {
						PQ_Printf("not a head str:%c%c%c%c,something wrong? receive %d?:",
							tmp_head->head[0],tmp_head->head[1],
							tmp_head->head[2],tmp_head->head[3], recvlen);
						int t = 0;
						printf("\nBad package...\n###################\n");
						while (t < recvlen) {
							printf("%02x ", *((char*)((recv_buf + rev_cnt)+t)));
							if (++t%30 == 0) {
								printf("\n");
							}
						}
						printf("\n###################\n");
						trans_da = (struct trans_data*)malloc(sizeof(struct PQ_package)
								+ sizeof(struct trans_reply) + sizeof(struct trans_data));
						tmp_head = (struct PQ_package *)&trans_da->data[0];
						tmp_head->head[0] = 'A';
						tmp_head->head[1] = 'W';
						tmp_head->head[2] = 'P';
						tmp_head->head[3] = 'Q';
						tmp_head->cmd = 255;
						tmp_head->size = sizeof(struct PQ_package)
								+ sizeof(struct trans_reply);
						tmp_head = NULL;
						
						recvlen = 0;
						goto send_trans;
					}

					trans_da = (struct trans_data*)malloc(tmp_head->size + sizeof(struct trans_data));
					if (trans_da == NULL) {
						PQ_Printf("malloc err...");
						continue;
					}
					if (recvlen >= tmp_head->size) {
						memcpy((char *)&trans_da->data[0], recv_buf + rev_cnt, tmp_head->size);
						tmp_head = (struct PQ_package *)&trans_da->data[0];
						recvlen -= tmp_head->size;
						rev_cnt += tmp_head->size;
						goto deal_data;
					} else {
						tmp_head = (struct PQ_package *)&trans_da->data[0];
						memcpy((char *)tmp_head, recv_buf + rev_cnt, recvlen);
						tail = recvlen;
						recvlen = 0;
						rev_cnt += recvlen;
						goto receiv;
					}
				}else{
					PQ_Printf("Not a head size %d, something wrong?", recvlen);
					goto deal_data;
				}
			}

			if (tmp_head == &tempackage) {
				memcpy(((char *)(&tempackage)) + tail, recv_buf, sizeof(struct PQ_package)- tail);
				trans_da = (struct trans_data*)malloc(tmp_head->size + sizeof(struct trans_data));
				tmp_head = (struct PQ_package *)&trans_da->data[0];
				memcpy(tmp_head, &tempackage, sizeof(struct PQ_package));
				rev_cnt += sizeof(struct PQ_package)- tail;
				recvlen -= (sizeof(struct PQ_package)- tail);
				if (recvlen >= (tmp_head->size - sizeof(struct PQ_package))) {
					memcpy(tmp_head->data, recv_buf + rev_cnt,
						tmp_head->size - sizeof(struct PQ_package));
					recvlen -= (tmp_head->size - sizeof(struct PQ_package));
					rev_cnt += (tmp_head->size - sizeof(struct PQ_package));
				} else {
					memcpy(tmp_head->data, recv_buf + rev_cnt, recvlen);
					rev_cnt += recvlen;
					recvlen = 0;
				}
			} else if (tmp_head != NULL) {
				int cpy_size = 0, half = 0; 
				if (recvlen + tail >= tmp_head->size) {
					cpy_size = tmp_head->size - tail;
					recvlen -= cpy_size;
				} else {
					cpy_size = rev_cnt;
					recvlen = 0;
					half = 1;
				}	
				memcpy(tmp_head->data + tail, recv_buf + rev_cnt, cpy_size);
				rev_cnt += cpy_size;
				if (half) {
					goto receiv;
				}
			}
deal_data:
			if (recvlen > 0 && recvlen < sizeof(struct PQ_package)) {
				memcpy(&tempackage, tmp_head, recvlen);
				tmp_head = &tempackage;
				tail = recvlen;
				goto receiv;
			}
send_trans:
#if PQ_DEBUG_LEVEL
			trans_da->count = debug_cout++;
#endif
			trans_da->client_fd = client_fd;
            PQ_Printf("start to send to translate");
			send_to_translate(trans_da);
			if (recvlen > 0) {
				trans_da = NULL;
				tmp_head = NULL;
				PQ_Printf("a buffer has seval head? or give a big buffer size");
				goto many_head;
			}
		}
	}

	goto wait_event;
}


static void *start_pq_test(void *arg)
{
	pthread_t pthread_handle;
	int i = 0, result = 0, pq_port = PQ_TCP_PORT;
	int sockfd;
	int client_fd;
	struct sockaddr_in clnt_addr;
	struct sockaddr_in serv_addr;
    socklen_t clnt_addr_size = sizeof(clnt_addr);
    int conncount = 0;

	result = PQ_translate_init();    // init PQ translate
	if (result) {
        printf("sunxi PQ PQ_translate err...\n");
        return NULL;
	}

    tcpip_init(NULL, NULL);    // init tcpip
    printf("aaaaaaaa tcpip init success\n");

	/* todo load the default config for  de */
	/* load dexxx.so  */
    memset(&poll_fds, 0, sizeof(poll_fds));
	pthread_create(&pthread_handle, NULL, connect_thread, NULL);
	pthread_setname_np(pthread_handle,"connect_thread");
	if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		PQ_Printf("sunxi PQ create socket error:%s(error:%d)\n",strerror(errno),errno);
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(0x7F000001); //inet_addr("127.0.0.1");
	serv_addr.sin_port = htons(pq_port);
	bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

waite_connect:
	PQ_Printf("sunxi PQ wait for client connect from tcp port:%d...\n", pq_port);
	if (listen(sockfd, MAX_SOCKET_CONNECT_COUNT) < 0) {
		PQ_Printf("sunxi PQ connect error:%s(errno:%d)\n",strerror(errno),errno);
		goto err;
	}
	client_fd =  accept(sockfd,(struct sockaddr*)&clnt_addr, &clnt_addr_size);

	if(client_fd < 0)
	{
		 rt_thread_mdelay(30);
		 goto waite_connect;
	}

    for (i = 0; i < MAX_CONNECT_COUNT; i++) {
        if (poll_fds[i].fd <= 0) {
            poll_fds[i].fd = client_fd;
            poll_fds[i].events = POLLIN  | POLLERR | POLLHUP;
            poll_fds[i].revents = 0;

            send(write_fd, &client_fd, sizeof(int), MSG_DONTWAIT | MSG_NOSIGNAL);
            break;
        }
    }

    int ret = poll(poll_fds, MAX_CONNECT_COUNT, -1);

    if (ret < 0)
    {
        printf("poll: %d, %s\n", errno, strerror(errno));
        return NULL;
    }
	printf("sunxi PQ has connectted for %d times and new:%d...\n", i, client_fd);
	goto waite_connect;
err:
	return NULL;
}

#if CONFIG_COMMAND_PQD_TEST
unsigned char pqd_exist = 0;
int cmd_pqd(int argc, char *argv[])
{
	if(!pqd_exist)
	{
		printf("pqd test start\n");
		pqd_exist = 1;
		pthread_t pqd_entery;
		pthread_create(&pqd_entery, NULL, start_pq_test, NULL);
		pthread_setname_np(pqd_entery,"pqd");

	}
	else
	{
		PQ_Printf("pqd is running");
	}
    return 0;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_pqd, pqd, pqd service);
#endif


int cmd_init_pq(int argc, char *argv[])
{
    init_pq();
    return 0;
}

FINSH_FUNCTION_EXPORT_CMD(cmd_init_pq, init_pq, take effect pq config);

#if 0
extern int query_thread(int argc, const char **argv);

int cmd_pqd_state(int argc, char *argv[])
{
  char name1[RT_NAME_MAX] = "query_thread";
  char name2[RT_NAME_MAX];
  char ** argv_pq = (char **)malloc(sizeof(char *) * 2);
      argv_pq[0] = (char *)malloc(RT_NAME_MAX);
      argv_pq[1] = (char *)malloc(RT_NAME_MAX);

  memset(argv_pq[0],0x0,RT_NAME_MAX);
  memset(argv_pq[1],0x0,RT_NAME_MAX);
  rt_snprintf(name2, sizeof(name2), "pth_%08x", connect_thread);
  memcpy((void *)argv_pq[0],name1,sizeof(name1));
  memcpy((void *)argv_pq[1] ,name2,sizeof(name2));
  query_thread(2, argv_pq);
  free(argv_pq[0]);
  free(argv_pq[1]);
  free(argv_pq);
  argv_pq[0] = NULL;
  argv_pq[1] = NULL;
  argv_pq     = NULL;
    return 0;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_pqd_state, pqd_state, pqd_state check);
#endif
