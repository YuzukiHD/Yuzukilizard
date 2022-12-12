#define LOG_TAG "BT_SOCK"
#include "bt_util.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/socket.h>

#include <sys/un.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "bt.h"
#include <hardware/bt_sock.h>


#define SERVICE_SEC_NAME  "BluetoothChatSecure"

#define SOCK_SIGNAL_SIZE  16

static char my_sec_uuid[] = {0xfa, 0x87, 0xc0, 0xd0, 0xaf, 0xac, 0x11, 0xde, 0x8a, 0x39, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66};

typedef struct {
    sock_connect_signal_t sig;
    int ctl_fd;
} sock_connect_t;


static int socket_process_cmsg(int fd[], struct msghdr * pMsg)
{
    struct cmsghdr *cmsgptr;
    int count;
    int i;

    if (fd == NULL)
    {
        bdt_log("%s() fd[] is NULL.", __FUNCTION__);
        return -1;
    }

    for (cmsgptr = CMSG_FIRSTHDR(pMsg);
            cmsgptr != NULL; cmsgptr = CMSG_NXTHDR(pMsg, cmsgptr))
    {

        if (cmsgptr->cmsg_level != SOL_SOCKET)
        {
            continue;
        }

        if (cmsgptr->cmsg_type == SCM_RIGHTS)
        {
            int *pDescriptors = (int *)CMSG_DATA(cmsgptr);
            count = ((cmsgptr->cmsg_len - CMSG_LEN(0)) / sizeof(int));

            if (count < 0)
            {
                bdt_log("%s() invalid cmsg length", __FUNCTION__);
                return -1;
            }

            for (i = 0; i < count; i++)
            {
                fd[i] = pDescriptors[i];
            }
        }
    }

    return 0;
}


static int socket_read_all(int fd, void *buffer, int len, bool rec_signal)
{
    int ret;
    struct msghdr msg;
    struct iovec iv;
    unsigned char *buf = (unsigned char *)buffer;
    struct cmsghdr cmsgbuf[2*sizeof(cmsghdr) + 0x100];

    memset(&msg, 0, sizeof(msg));
    memset(&iv, 0, sizeof(iv));

    iv.iov_base = buf;
    iv.iov_len = len;

    msg.msg_iov = &iv;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);

    do
    {
        ret = recvmsg(fd, &msg, MSG_NOSIGNAL);
    } while (ret < 0 && errno == EINTR);

    if (ret < 0 && errno == EPIPE)
    {
        // Treat this as an end of stream
        return 0;
    }

    if (ret < 0)
    {
        bdt_log("%s() recv error.", __FUNCTION__);
        return -1;
    }

    if ((msg.msg_flags & (MSG_CTRUNC | MSG_OOB | MSG_ERRQUEUE)) != 0)
    {
        // To us, any of the above flags are a fatal error
        bdt_log("%s() Unexpected error or truncation during recvmsg()", __FUNCTION__);
        return -1;
    }

    if (rec_signal)
    {
        int fd[2];
        if (ret >= 0)
        {
            if (!socket_process_cmsg(fd, &msg))
                return fd[0];
        }
    }

    return ret;
}

static int sock_read_data(int fd, char buffer[], int len)
{
    int ret;
    fd_set input;

    FD_ZERO(&input);
    FD_SET(fd, &input);

    ret = select(fd + 1, &input, NULL, NULL, NULL);
    if (ret > 0)
    {
        if (FD_ISSET(fd, &input))
        {
            ret = recv(fd, buffer, len, MSG_NOSIGNAL);
            if (ret == 0)
                bdt_log("%s() recv() returned 0!", __FUNCTION__);
            return ret;
        }
    }
    else if (ret < 0)
    {
        bdt_log("%s() select() Failed", __FUNCTION__);
    }
    else if (ret == 0)
    {
        bdt_log("%s() Got a select() TIMEOUT", __FUNCTION__);
    }

    return ret;
}



static int sock_bind_listen(int fd)
{
    char port_buf[4];
    int port = 0;
    int ret;

    memset(port_buf, 0, sizeof(port_buf));
    ret = recv(fd, port_buf, sizeof(port_buf), MSG_NOSIGNAL);
    if (ret > 0)
    {
        port = (port_buf[3] << 24) | (port_buf[2] << 16) | (port_buf[1] << 8) | (port_buf[0]);
        //ALOGI("port num: %d", port);
    }

    return port;
}


static int wait_socket_signal(int fd, sock_connect_t *con)
{
    char buf[SOCK_SIGNAL_SIZE];
    int ret;
    int ctl_fd;

    memset(buf, 0, sizeof(buf));
    ctl_fd = socket_read_all(fd, buf, sizeof(buf), true);
    con->ctl_fd = ctl_fd;
    memcpy(&(con->sig), buf, sizeof(buf));

    return 0;
}




static void *socket_server_thread(void *)
{
    int sock_fd;
    int cl_fd;
    int ret;
    int port;

    bdt_log("entered socket_server_thread...");

    sock_fd = bluetooth_create_socket_channel_native(BTSOCK_RFCOMM, (char *)SERVICE_SEC_NAME, my_sec_uuid, -1, BTSOCK_FLAG_ENCRYPT | BTSOCK_FLAG_AUTH);
    if (sock_fd < 0)
    {
        bdt_log("%s() sock_fd < 0", __FUNCTION__);
        goto fail1;
    }

    // TODO: read port number.
    port = sock_bind_listen(sock_fd);
    bdt_log("%s() port number: %d", __FUNCTION__, port);

    // TODO: wait signal. block wait.
    sock_connect_t con;
    wait_socket_signal(sock_fd, &con);
    
    cl_fd = con.ctl_fd;
    if (cl_fd <= 0)
    {
        bdt_log("%s() cl_fd is invalid.", __FUNCTION__);
        goto fail;
    }
    bdt_log("%s() cl_fd = %d", __FUNCTION__, cl_fd);

    char buf[100];
    int i;
    // TODO: read data.
    bdt_log("waiting data...");
    while (1)
    {
        memset(buf, 0, sizeof(buf));
        ret = sock_read_data(cl_fd, buf, sizeof(buf));
        if (ret > 0)
        {
            for (i = 0; i < ret; i++)
                bdt_log("recv data: %c", buf[i]);
            // loop back
            ret = send(cl_fd, buf, ret, 0);
            if (ret > 0)
            {
                
            }
        } else {
            bdt_log("peer is invalid, may be the socket is abort.");
            break;
        }
    }


    close(cl_fd);
fail:
    close(sock_fd);
fail1:
    bdt_log("thread exit.");
    return NULL;
}



int sock_init()
{
    int ret;
    pthread_t tid;

    ret = pthread_create(&tid, NULL, socket_server_thread, NULL);
    if (ret != 0)
    {
        bdt_log("create socket_server_thread error...\n");
        return -1;
    }
    
    return 0;
}


