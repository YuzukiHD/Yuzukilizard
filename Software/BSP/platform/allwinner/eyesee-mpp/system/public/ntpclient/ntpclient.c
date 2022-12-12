/**
 * @file ntpclient.c
 * @brief ntp时间同步客户端
 * @author yinzh
 * @version v0.1
 * @date 2016-03-09
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <poll.h>
#include <arpa/inet.h>
#include <netdb.h> /* gethostbyname */

#include "ntpclient.h"

static int send_packet(int sockfd)
{
    unsigned int ntp_head[12];
    memset(ntp_head, 0, sizeof(ntp_head));

    ntp_head[0] = htonl(DEF_NTP_HEAD_LI | DEF_NTP_HEAD_VN | DEF_NTP_HEAD_MODE |
                DEF_NTP_HEAD_STARTUM | DEF_NTP_HEAD_POLL | DEF_NTP_HEAD_PRECISION);
    ntp_head[1] = htonl(1<<16);  /* Root Delay (seconds) */
    ntp_head[2] = htonl(1<<16);  /* Root Dispersion (seconds) */

    struct timeval now;
    gettimeofday(&now, NULL);

    /* 将当前时间写到transmit timestamp */
    ntp_head[10] = htonl(now.tv_sec + OFFSET_1900_TO_1970); /* 高32位 */
    ntp_head[11] = htonl(NTPFRAC(now.tv_usec));             /* 低32位 */

    if (send(sockfd, ntp_head, sizeof(ntp_head), 0) == -1) {
        printf("send data failed: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

static int connect_to_server(const char *ntp_server)
{
    int sockfd;
    const char *server;

    if (ntp_server != NULL) {
        server = ntp_server;
    } else {
        server = DEF_NTP_SERVER;
    }

    /* struct sockaddr_in addr_src; */
    struct sockaddr_in addr_dst;

    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1 ) {
        printf("create socket failed: %s\n", strerror(errno));
        return -1;
    }

    struct hostent *host;
    host = gethostbyname(server);
    if (host == NULL) {
        herror(server);
        return -1;
    }

    memset(&addr_dst, 0, sizeof(addr_dst));
    memcpy(&(addr_dst.sin_addr.s_addr), host->h_addr_list[0], sizeof(addr_dst.sin_addr));
    addr_dst.sin_family = AF_INET;
    addr_dst.sin_port = htons(DEF_NTP_PORT);

    printf("ntp server %s address:%s, port:%d\n", \
            server, inet_ntoa(addr_dst.sin_addr), DEF_NTP_PORT);

    if (connect(sockfd, (struct sockaddr*)&addr_dst, sizeof(addr_dst)) == -1) {
        printf("connect to server failed:%s\n", strerror(errno));
        close(sockfd);
        return -1;
    }

    return sockfd;
}

static int get_time_from_server(int sockfd, struct timeval *time)
{
    ntp_timestamp_t origtime, recvtime, transtime, desttime;

    uint32_t ntp_data[12];

    memset(ntp_data, 0, sizeof(ntp_data));
    if (recv(sockfd, ntp_data, sizeof(ntp_data), 0) == -1) {
        printf("recv failed:%s\n", strerror(errno));
        return -1;
    }

    struct timeval now;
    gettimeofday(&now, NULL);

    /* 目的时间戳， 表示客户端接收到数据的时间 */
    desttime.integer   = now.tv_sec + OFFSET_1900_TO_1970;
    desttime.fraction  = NTPFRAC(now.tv_usec);

    /* 原始时间戳， 表示客户端请求数据的时间 */
    origtime.integer   = ntohl(ntp_data[6]);
    origtime.fraction  = ntohl(ntp_data[7]);

    /* 接收时间戳， 表示服务器接收到数据的时间 */
    recvtime.integer   = ntohl(ntp_data[8]);
    recvtime.fraction  = ntohl(ntp_data[9]);

    /* 发送时间戳， 表示服务器发送数据的时间 */
    transtime.integer  = ntohl(ntp_data[10]);
    transtime.fraction = ntohl(ntp_data[11]);

/* 转换ntp时间戳到微秒 */
#define NTPTIME_TO_USEC(ntptime) (((long long)ntptime.integer) * 1000000 + USEC(ntptime.fraction))

    int64_t origus, recvus, transus, destus, offus, delayus;

    origus  = NTPTIME_TO_USEC(origtime);
    recvus  = NTPTIME_TO_USEC(recvtime);
    transus = NTPTIME_TO_USEC(transtime);
    destus  = NTPTIME_TO_USEC(desttime);

    /* 总的网络延迟，包括发送和接收过程 */
    offus = ((recvus - origus) + (transus - destus))/2;
    /* 这个公式的前提是，假设发送过程和接收过程的网络延迟相同 */
    delayus = (recvus - origus) + (destus - transus);
    printf("offus: %lld, delayus: %lld\n", offus, delayus);

    struct timeval newtime;

    uint64_t new_usec = (uint64_t)now.tv_sec * 1000000 + now.tv_usec + offus;
    printf("new_usec: %llu\n", new_usec);

    newtime.tv_sec    = new_usec / 1000000;
    newtime.tv_usec   = new_usec % 1000000;

    printf("nowtime.tv_sec:%ld, nowtime.tv_usec:%ld\n", now.tv_sec, now.tv_usec);
    printf("newtime.tv_sec:%ld, newtime.tv_usec:%ld\n", newtime.tv_sec, newtime.tv_usec);

    memcpy(time, &newtime, sizeof(struct timeval));

    return 0;
}

int main(int argc, char *argv[])
{
    int sockfd;
    int ret;

    char need_set = 0;
    char *server = NULL;

    if (argc >= 3 && !strcmp(argv[1], "-s")) {
        need_set = 1;
        server = argv[2];
    } else if (argc >= 2) {
        server = argv[1];
    }

    while(1) {
        /* query_server("0.cn.poll.ntp.org"); */
        if ( (sockfd = connect_to_server(server)) == -1) {
                printf("try again\n");
                sleep(3);

                continue;
        }

        send_packet(sockfd);

        struct pollfd pfd;

        pfd.fd = sockfd;
        pfd.events = POLLIN;

        ret = poll(&pfd, 1, 300);

        if (ret == 1) {

            /* Looks good, try setting time on host ... */
            struct timeval time;

            if (get_time_from_server(sockfd, &time) == -1) {

                close(sockfd);
                sleep(1);

                continue;
            }

            if (need_set) {

                struct timezone zone;
                zone.tz_minuteswest = timezone/60 - 60*daylight;
                zone.tz_dsttime = 0;

                if (settimeofday(&time, &zone) < 0) {
                    fprintf(stderr, "set time failed, %s, try again\n", strerror(errno));

                    close(sockfd);
                    sleep(5);
                    system("date 2016-01-01\\ 00:00 ; hwclock -w");

                    continue;
                }
            }

            close(sockfd);

            return 0;          /* All done! :) */
        } else if (ret == 0) {
            printf("timeout retry!\n");

            close(sockfd);
            sleep(1);

            continue;
        }
    }

    close(sockfd);

    return 0;
}
