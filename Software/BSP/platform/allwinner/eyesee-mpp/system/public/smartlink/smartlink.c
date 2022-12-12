/************************************************************************************************/
/* Copyright (C), 2001-2017, Allwinner Tech. Co., Ltd.                                          */
/************************************************************************************************/
/**
 * @file smartlink.c
 * @brief 
 * @author id: guixing
 * @version v0.1
 * @date 2017-02-14
 */

/************************************************************************************************/
/*                                      Include Files                                           */
/************************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

#include <sys/socket.h>
#include <linux/socket.h>
#include <linux/if_ether.h>
#include <netpacket/packet.h>
#include <net/if.h>
#include <netinet/in.h>

#include "smartlink.h"


/************************************************************************************************/
/*                                     Macros & Typedefs                                        */
/************************************************************************************************/
#define WIFI_NAME     "wlan0"

#define MAX_WIFI_CHN  196

#define ENABLE_ENCRYPT

#define CAL_CHECK_NUM  3

#define CRC8_INIT   0x0
#define CRC8_POLY   0x31

#define BROADCAST_ADDR  "255.255.255.255"
#define BROADCAST_PORT  10001


#ifdef SMARTLINK_DEBUG
#define SL_DB_PRT(fmt, args...)                                          \
    do {                                                                  \
        printf("[FUN]%s [LINE]%d  " fmt, __FUNCTION__, __LINE__, ##args); \
    } while (0)
#else
#define SL_DB_PRT(fmt, args...)                                          \
    do {} while (0)
#endif

#ifdef SMARTLINK_ERROR
#define SL_ERR_PRT(fmt, args...)                                                                        \
    do {                                                                                                 \
        printf("\033[0;32;31m ERROR! [FUN]%s [LINE]%d  " fmt "\033[0m", __FUNCTION__, __LINE__, ##args); \
    } while (0)
#else
#define SL_ERR_PRT(fmt, args...)                                          \
            do {} while (0)
#endif



/************************************************************************************************/
/*                                    Structure Declarations                                    */
/************************************************************************************************/
typedef struct tag_DHD_PRIV_CMD_S {
    char *buf;
    int   used_len;
    int   total_len;
} DHD_PRIV_CMD_S;

typedef struct tag_SSID_PWD_CALC_S {
    char calc_cnt[64+256];
    char ssid_pwd_val[CAL_CHECK_NUM][64+256];
    char ssid[64];
    char pwd[256];

    char ssid_crc_cnt;
    char ssid_crc_val[CAL_CHECK_NUM];
    char ssid_crc;

    char pwd_crc_cnt;
    char pwd_crc_val[CAL_CHECK_NUM];
    char pwd_crc;
} SSID_PWD_CALC_S;


/************************************************************************************************/
/*                                      Global Variables                                        */
/************************************************************************************************/
static unsigned short g_protocol = 0x0003;

static int g_smartlink_chn  = 0;

static int g_ssid_pwd_size  = 0;
static int g_ssid_size      = 0;
static int g_pwd_size       = 0;
static int g_pwd_recv_flag  = 0;

static unsigned char g_ssid_pwd_val[192]  = {0};
static unsigned char g_ssid_crc      = 0;
static unsigned char g_pwd_crc       = 0;

static unsigned char g_random_num     = 0;
static unsigned char g_max_packet_num = 0;

SSID_PWD_CALC_S g_ssid_pwd_calc;

static short int g_wifi_support_chn[64] = {0};


/************************************************************************************************/
/*                                    Function Declarations                                     */
/************************************************************************************************/
/* None */


/************************************************************************************************/
/*                                     Function Definitions                                     */
/************************************************************************************************/

static unsigned char cal_crc8(const unsigned char* in, int num)
{
    unsigned char crc = CRC8_INIT;
    int i = 0, j = 0;

    for(i = 0; i < num; i++ ) {
        crc ^= in[i];
        for(j = 0; j < 8; j++) {
            if(crc & 0x1)
                crc = (crc >> 1) ^ CRC8_POLY;
            else
                crc = crc >> 1;
        }
    }
    return crc;
}

static int dhd_priv_ioctrl(const char *wifi_name, char *cmd, char *data)
{
    struct ifreq ifr;
    DHD_PRIV_CMD_S priv_cmd;
    int ret = 0;
    int ioctl_sock; /* socket for ioctl() use */
    char buf[512]={0};
    int len = 0, i = 0;

    if (NULL == wifi_name) {
        SL_ERR_PRT("Input wifi_name is NULL!\n");
        return -1;
    }

    if (NULL == cmd) {
        SL_ERR_PRT("Input cmd is NULL!\n");
        return -1;
    }

    if (NULL == data) {
        SL_ERR_PRT("Input data is NULL!\n");
        return -1;
    }

    /* Setup 1. fill private cmd and data for ioctrl */
    memset(buf, '\0', sizeof(buf));
    len = 0;
    for (i = 0; len < sizeof(buf) && cmd[i] != '\0'; i++, len++) {
        buf[len] = cmd[i];
    }

    if (len == (sizeof(buf) - 1)) {
        SL_ERR_PRT("Input cmd:%s too long > %d !\n", cmd, (int)sizeof(buf));
        return -1;
    } else {
        buf[len] = ' ';
        len++;
    }

    for (i = 0; len < sizeof(buf) && data[i] != '\0'; i++, len++) {
        buf[len] = data[i];
    }
    buf[len] = ' ';

    SL_DB_PRT("Do cmd:%s\n", buf);

    /* Setup 2. Create private  socket */
    ioctl_sock = socket(PF_INET, SOCK_DGRAM, 0);
        if (ioctl_sock < 0) {
        SL_ERR_PRT("Create socket(PF_INET,SOCK_DGRAM) fail! error[%d]:%s \n", errno, strerror(errno));
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    memset(&priv_cmd, 0, sizeof(priv_cmd));
    strncpy(ifr.ifr_name, wifi_name, sizeof(ifr.ifr_name));

    priv_cmd.buf       = buf;
    priv_cmd.used_len  = sizeof(buf);
    priv_cmd.total_len = sizeof(buf);
    ifr.ifr_data       = (char*)&priv_cmd;

    /* Setup 3. Do dhd wifi private cmd. */
    ret = 0;
    if ((ret = ioctl(ioctl_sock, SIOCDEVPRIVATE + 1, &ifr)) < 0) {
        SL_ERR_PRT("Do ioctl cmd:SIOCDEVPRIVATE fail! error[%d]:%s \n", errno, strerror(errno));
    } else {
        SL_DB_PRT("Do %s ret_len:%d, %d\n", buf, ret, strlen(buf));
        strncpy(data, buf, strlen(buf));
    }

    close(ioctl_sock);

    return ret;
}


static int set_wifi_channel(const char *wifi_name, int chn)
{
    int ret = 0;
    char data_buf[16];

    if (NULL == wifi_name) {
        SL_ERR_PRT("Input wifi_name is NULL!\n");
        return -1;
    }

    if (chn < 1 || chn > MAX_WIFI_CHN) {
        SL_ERR_PRT("Input chn:%d is error! 1<=chn<=%d is ok!\n", chn, MAX_WIFI_CHN);
        return -1;
    }

    memset(data_buf, '\0', sizeof(data_buf));
    if (chn < 10) {
        data_buf[0] = '0' + chn;
    } else if (chn >= 10 && chn < 100) {
        data_buf[0] = '0' + chn/10;
        data_buf[1] = '0' + chn%10;
    } else if (chn >= 100 && chn < 1000) {
        data_buf[0] = '0' + chn/100;
        data_buf[1] = '0' + (chn%100)/10;
        data_buf[2] = '0' + (chn%100)%10;
    }

    /*command:"channel" is differential in Linux 4.4 and Linux 3.10.[Linux4.4:"channel"][Linux3.10:"set_channel"] */
    ret = dhd_priv_ioctrl(wifi_name, "channel", data_buf);
    if (ret) {
        SL_ERR_PRT("Do dhd_priv_ioctrl %s set_channel %s fail! ret:%d\n", wifi_name, data_buf, ret);
        return -1;
    }

    return 0;
}


static int get_wifi_channel(const char *wifi_name, int *chn)
{
    int ret = 0;
    char data_buf[128];

    if (NULL == wifi_name) {
        SL_ERR_PRT("Input wifi_name is NULL!\n");
        return -1;
    }

    memset(data_buf, '\0', sizeof(data_buf));
    ret = dhd_priv_ioctrl(wifi_name, "get_channel", data_buf);
    if (ret) {
        SL_ERR_PRT("Do dhd_priv_ioctrl %s get_channel %s fail! ret:%d\n", wifi_name, data_buf, ret);
        return -1;
    }

    sscanf(data_buf, "channel %d", chn);
    SL_DB_PRT("data_buf:%s  The current wifi channel:%d\n", data_buf, *chn);

    return 0;
}


int set_wifi_monitor(const char *wifi_name, int monitor_enable)
{
    int ret = 0;
    char data_buf[16] = {0};

    if (NULL == wifi_name) {
        SL_ERR_PRT("Input wifi_name is NULL!\n");
        return -1;
    }

    memset(data_buf, '\0', sizeof(data_buf));
    if (monitor_enable) {
        data_buf[0] = '1';
    } else {
        data_buf[0] = '0';
    }

    ret = dhd_priv_ioctrl(wifi_name, "monitor", data_buf);
    if (ret) {
        SL_ERR_PRT("Do dhd_priv_ioctrl %s monitor %s fail! ret:%d\n", wifi_name, data_buf, ret);
        return -1;
    }

    return 0;
}


static int create_monitor_socket(const char *wifi_name)
{
    struct ifreq ifr;
    struct sockaddr_ll ll;
    int fd = -1;

    if (NULL == wifi_name) {
        SL_ERR_PRT("Input wifi_name is NULL!\n");
        return -1;
    }

    /* Setup 1. Create Raw socket */
    fd = socket(PF_PACKET, SOCK_RAW, htons(g_protocol));
    if (fd < 0) {
        SL_ERR_PRT("Create PF_PACKET, SOCK_RAW, htons(0x%x)=0x%x fd:%d fail! error[%d]:%s\n",
                                          g_protocol, htons(g_protocol), fd, errno, strerror(errno));
        return -1;
    } else {
        SL_DB_PRT("Create socket PF_PACKET, SOCK_RAW, htons(0x%x)=0x%x fd:%d ok!\n",
                                                        g_protocol, htons(g_protocol), fd);
    }

    /* Setup 2.  Setting net device, for socket. this is wifi wlan0. */
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, wifi_name, sizeof(ifr.ifr_name));
    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
        SL_ERR_PRT("Do ioctl:SIOCGIFINDEX fail! fd:%d error[%d]:%s\n", fd, errno, strerror(errno));
        close(fd);
        return -1;
    }

    /* Setup 3. Bind wifi dev for monitor wriless packet. */
    memset(&ll, 0, sizeof(ll));
    ll.sll_family   = PF_PACKET;
    ll.sll_ifindex  = ifr.ifr_ifindex;
    ll.sll_protocol = htons(g_protocol);
    if (bind(fd, (struct sockaddr *) &ll, sizeof(ll)) < 0) {
        SL_ERR_PRT("Do bind fd:%d fail! error[%d]:%s\n", fd, errno, strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}


static int delete_monitor_socket(int fd)
{
    if (fd > 0) {
        close(fd);
        return 0;
    } else {
        SL_ERR_PRT("Input monitor socket fd:%d error!\n", fd);
        return -1;
    }
}


static void init_smartlink_data(void)
{
    g_smartlink_chn  = 0;

    g_ssid_pwd_size  = 0;
    g_ssid_size      = 0;
    g_pwd_size       = 0;
    g_pwd_recv_flag  = 0;

    memset(g_ssid_pwd_val, 0, sizeof(g_ssid_pwd_val));
    g_ssid_crc       = 0;
    g_pwd_crc        = 0;

    g_random_num     = 0;
    g_max_packet_num = 0;

    memset(&g_ssid_pwd_calc,    0, sizeof(g_ssid_pwd_calc));
    memset(&g_wifi_support_chn, 0, sizeof(g_wifi_support_chn));

    /* In most country, 12,13,14  2.4G wifi channel is forbidden to using. */
    g_wifi_support_chn[0] = 1;
    g_wifi_support_chn[1] = 2;
    g_wifi_support_chn[2] = 3;
    g_wifi_support_chn[3] = 4;
    g_wifi_support_chn[4] = 5;
    g_wifi_support_chn[5] = 6;
    g_wifi_support_chn[6] = 7;
    g_wifi_support_chn[7] = 8;
    g_wifi_support_chn[8] = 9;
    g_wifi_support_chn[9] = 10;
    g_wifi_support_chn[10] = 11;

    /* In china, same 5G wifi channel is used. */
    g_wifi_support_chn[11] = 36;
    g_wifi_support_chn[12] = 40;
    g_wifi_support_chn[13] = 44;
    g_wifi_support_chn[14] = 48;
    g_wifi_support_chn[15] = 149;
    g_wifi_support_chn[16] = 153;
    g_wifi_support_chn[17] = 157;
    g_wifi_support_chn[18] = 161;
    g_wifi_support_chn[19] = 165;
    g_wifi_support_chn[20] = 0;
    g_wifi_support_chn[21] = 0;
}


static int parser_smartlink_data(const unsigned char *data, int len, unsigned char *da_mac)
{
    int  ret = 0;
    int  cnt = 0, tmp = 0;
    unsigned char pack_num = 0;
    unsigned char crc = 0;

    if (NULL == data) {
        SL_ERR_PRT("Input data is NULL!\n");
        return -1;
    }

    /*if not a Data-packet discare it*/
    if(data[0] != 0x88){
        return -1;
    }

    if (len >= 188 || len <= 36) {
        return -1;
    }

    memset(da_mac, 0, 6);

    /*multicase_head could in address1[4~9] or adress3[16~21] */
    if(0x01 == data[16] && 0x00 == data[17] && 0x5e == data[18]){
        for (cnt = 0; cnt < 6; cnt++) {
            da_mac[cnt] = data[16+cnt];
        }
    }
    else if (0x01 == data[4] && 0x00 == data[5] && 0x5e == data[6]) {
        for (cnt = 0; cnt < 6; cnt++) {
            da_mac[cnt] = data[4+cnt];
        }
    }
    else {
        return -1;
    }

    /* Check data_num : (1<= num <=127) */
    pack_num = da_mac[3] & 0x7f;
    if (pack_num > 128) {
        //printf("  pack_num error! num:%d \n\n", pack_num);
        return -1;
    }

    if (g_max_packet_num > 6 && pack_num > g_max_packet_num) {
        //printf(" pack_num:%d > max_pack_num:%d error! \n\n", pack_num, g_max_packet_num);
        return -1;
    }

    /* calculate the packt crc, and check */
    if (pack_num < 5) {
        crc = cal_crc8(&da_mac[4], 1);
        if (crc != da_mac[5])
            return -1;
    }

    /* parser smartlink data */
    switch (pack_num) {
        case 1:
            if (da_mac[4] > MAX_WIFI_CHN) {
                return -1;
            }
            break;
        case 2:
            if (da_mac[4] > 192) {
                return -1;
            }
            g_ssid_pwd_size = da_mac[4];
            if (g_pwd_recv_flag) {
                g_ssid_size = g_ssid_pwd_size - g_pwd_size;
            }
            g_max_packet_num = 4 + g_ssid_pwd_size/2 + g_ssid_pwd_size%2 + 1;
            break;
        case 3:
            g_pwd_size = da_mac[4];
            g_pwd_recv_flag = 1;
            if (g_ssid_pwd_size > 0) {
                g_ssid_size = g_ssid_pwd_size - g_pwd_size;
            }
            break;
        case 4:
            g_random_num = da_mac[4];
            break;

        default:
            if (g_max_packet_num > 6 && pack_num == g_max_packet_num) {
                g_pwd_crc  = da_mac[4];
                g_ssid_crc = da_mac[5];
            } else {
                tmp = (pack_num-5)*2;
                if (tmp >= sizeof(g_ssid_pwd_val)) {
                    return -1;
                }
                g_ssid_pwd_val[tmp] = da_mac[4];
                g_ssid_pwd_val[tmp+1] = da_mac[5];
            }
            break;
    }

    return 0;
}


static int parser_smartlink_data_debug(unsigned char *data, int len, char *da_mac)
{
    int  ret = 0;
    unsigned char crc = 0;

    if (NULL == data) {
        SL_ERR_PRT("Input data is NULL!\n");
        return -1;
    }

    memset(da_mac, 0, 6);

    if (len >= 128 || len <= 32) {
        return -1;
    }

    // TODO: Get MAC:DA from 802.2 SNAP packt head. 

    int i = 0;
    unsigned char pack_num = 0;
    for (i = 0; i < (len - 24); i++) {
        if (0x01 == data[i] && 0x00 == data[i+1] && 0x5e == data[i+2]) {
            printf(" [%X:%X:%X] ", data[i], data[i+1], data[i+2]);

            /* Check data_num is ok ? (1<= num <=127) */
            pack_num = data[i+3] & 0x7f;
            if (pack_num > 128) {
                printf("  pack_num error! num:%d \n\n", pack_num);
                return -1;
            }

            if (g_max_packet_num > 6 && pack_num > g_max_packet_num) {
                printf(" pack_num:%d > max_pack_num:%d error! \n\n", pack_num, g_max_packet_num);
                return -1;
            }

            /* calculate the packt crc. */
            crc = cal_crc8(&data[i+4], 1);

            switch (pack_num) {
                case 1:
                    printf(" Cnt:%d  channel :%d  CRC:%d-%d  ", pack_num, data[i+4], data[i+5], crc);
                    break;
                case 2:
                    printf(" Cnt:%d  pwd+ssid:%d  CRC:%d-%d  ", pack_num, data[i+4], data[i+5], crc);
                    break;
                case 3:
                    printf(" Cnt:%d  pwd size:%d  CRC:%d-%d  ", pack_num, data[i+4], data[i+5], crc);
                    break;
                case 4:
                    printf(" Cnt:%d  randome :%d  CRC:%d-%d  ", pack_num, data[i+4], data[i+5], crc);
                    break;
                default:
                    if (g_max_packet_num > 6 && pack_num == g_max_packet_num) {
                        printf(" Cnt:%d  pwd_crc:%d  ssid_crc:%d  ", pack_num, data[i+4], data[i+5]);
                    } else {
                        printf(" Cnt:%d  data:\'%c\'  date:\'%c\'  ", pack_num, data[i+4], data[i+5]);
                    }
                    break;
            }
            i += 5;

            len = i + 24 + 12;  /////////for testing!!!!!!!
        } else {
            printf("%02x ", data[i]);
        }
    }

    return 0;
}


static int check_receive_result(SMARTLINK_AP_S *ap_info)
{
    int i = 0;
    unsigned char crc = 0;

    if (NULL == ap_info) {
        SL_ERR_PRT("Input ap_info is NULL!\n");
        return -1;
    }

    if (g_smartlink_chn < 1 || g_smartlink_chn > MAX_WIFI_CHN) {
        return -1;
    }

    if (0 == g_random_num || 0xff == g_random_num) {
        return -1;
    }

    if (0 == g_max_packet_num) {
        return -1;
    }

    if (g_pwd_recv_flag) {
        if (g_ssid_pwd_size != (g_ssid_size + g_pwd_size))
            return -1;
    } else {
        return -1;
    }

    for (i = 0; i < g_ssid_pwd_size; i++) {
        if (0 == g_ssid_pwd_val[i] || 0xff == g_ssid_pwd_val[i]) {
            return -1;
        }
    }

    if (g_pwd_size > 0) {
        crc = cal_crc8(g_ssid_pwd_val, g_pwd_size);
        if (crc != g_pwd_crc) {
            return -1;
        }
    }

    crc = cal_crc8(&g_ssid_pwd_val[g_pwd_size], g_ssid_size);
    if (crc != g_ssid_crc) {
        return -1;
    }

    for (i = 0; i < g_pwd_size; i++) {
        ap_info->psswd[i] = g_ssid_pwd_val[i];
    }
    for (i = 0; i < g_ssid_size; i++) {
        ap_info->ssid[i] = g_ssid_pwd_val[g_pwd_size+i];
    }
    ap_info->random_num = g_random_num;

    printf("[FUN]:%s [LINE]:%d  wifi_chn:%d  pwd_ssid_size:%d  pwd_size:%d  random:%d  ssid:%s  pwd:%s  ssid_crc:%d  pwd_crc:%d  max_pack:%d\n", __func__, __LINE__,
                     g_smartlink_chn, g_ssid_pwd_size, g_pwd_size, g_random_num, ap_info->ssid, ap_info->psswd, g_ssid_crc, g_pwd_crc, g_max_packet_num);

    return 0;
}


int lock_smartlink_wifi_chn(const char *wifi_name, int lock_time_out, int chn_parser_time)
{
    int ret = 0, chn_cnt = 0, tmp_chn = 0;
    int wifi_chn = 0, fd = 0;
    int recv_chn_cnt[64] = {0};
    int lock_flag = 0, use_time_ms = 0, lock_loop = 0;
    char crc = 0;
    struct timeval parser_start_tv, chn_start_tv, tmp_tv;
    fd_set inputfs, rfds;

    int res = 0, res2 = 0, chn_loop = 0;
    unsigned char buf[2300] = {0};
    unsigned char da_mac[6] = {0};
    char          multicase_head[3] = {0x01,0x00,0x5e};
    struct sockaddr_ll ll;
    socklen_t fromlen;

    if (NULL == wifi_name) {
        SL_ERR_PRT("Input wifi_name is NULL!\n");
        return -1;
    }

    if (lock_time_out < (chn_parser_time * 32) || chn_parser_time < 10) {
        SL_ERR_PRT("Input lock_time_out chn_parser_time error!\n");
        return -1;
    }

    init_smartlink_data();
    memset(recv_chn_cnt, 0, sizeof(recv_chn_cnt));
    gettimeofday(&parser_start_tv, NULL);

    lock_flag = 0;
    lock_loop = 8888888;
    while (lock_loop--) {
        for (chn_cnt = 0; 0 != g_wifi_support_chn[chn_cnt]; chn_cnt++) {

            /* Setting wifi channel, for loop all wifi channel.  */
            ret = set_wifi_channel(wifi_name, g_wifi_support_chn[chn_cnt]);
            if (ret) {
                SL_ERR_PRT("Do set_wifi_channel fail! ret:%d\n", ret);
                continue;
            }
            //usleep(1000);

            /* Read current wifi channel data, for make sure send wifi channel. */
            fd = create_monitor_socket(wifi_name);
            if (fd <= 0) {
                SL_ERR_PRT("Do create_monitor_socket fail! ret:%d\n", fd);
                return -1;
            }

            chn_loop = 0;
            gettimeofday(&chn_start_tv, NULL);
            FD_ZERO(&rfds);
            FD_SET(fd, &rfds);

            /*start receive and analyze data*/
            memset(&ll, 0, sizeof(ll));
            while (1) {
                inputfs = rfds;
                tmp_tv.tv_sec = 0;
                tmp_tv.tv_usec = chn_parser_time * 1000;

                ret = select(fd+1, &inputfs, (fd_set *)0, (fd_set *)0, &tmp_tv);
                if (0 == ret) {
                    printf ("[FUN]:%s  [LINE]:%d  ==>> wifi_chn:%d recv time out!\n",
                                                __func__,__LINE__, g_wifi_support_chn[chn_cnt]);
                    //continue;
                } else if (ret < 0) {
                    printf ("[FUN]:%s  [LINE]:%d  ==>> wifi_chn:%d recv error!\n",
                                                __func__,__LINE__, g_wifi_support_chn[chn_cnt]);
                    //continue;
                } else {
                    fromlen = sizeof(ll);
                    res = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&ll, &fromlen);
                    if (res < 0) {
                        SL_ERR_PRT("Do recvfrom fail! fd:%d res:%d error[%d]:%s\n", fd, res, errno, strerror(errno));
                    } else {
                        ret = parser_smartlink_data(buf, res, da_mac);

                        if (0 == ret) {
                            /* Calculate every recver chn data num. */
                            recv_chn_cnt[chn_cnt] += 1;

                            /* if is NO. 1 packet, and wifi_chn is make sure, so find wifi_chn */
                            if (1 == (da_mac[3] & 0x7f) && da_mac[4] > 0 && recv_chn_cnt[chn_cnt] > 3) {
                                g_smartlink_chn = da_mac[4];
                                printf("[FUN]:%s [LINE]:%d   I finded smartlink channel:%d !\n", 
                                                                        __func__, __LINE__, g_smartlink_chn);
                                delete_monitor_socket(fd);
                                return 0;
                            }

                            /* If receive right data, just loop twice channel lock test. */
                            if (0 == lock_flag && recv_chn_cnt[chn_cnt] > 3) {
                                printf("[FUN]:%s [LINE]:%d   Recver right data, just loop Twice again!!! chn_cnt:%d  recv_chn_cnt:%d\n",
                                                                        __func__, __LINE__, chn_cnt, recv_chn_cnt[chn_cnt]);
                                lock_flag = 1;
                                lock_loop = 1;
                                chn_cnt = 31;
                                break;
                            }
                        }
                    }
                }

                /*check timeout*/
                chn_loop++;
                if (0 == chn_loop%3) {
                    gettimeofday(&tmp_tv, NULL);
                    use_time_ms = (tmp_tv.tv_sec - chn_start_tv.tv_sec) * 1000 + 
                                                (tmp_tv.tv_usec - chn_start_tv.tv_usec) / 1000;
                    if (use_time_ms > chn_parser_time) {
                        SL_DB_PRT("Currcut will change chn:%d, chn_parser_time:%d use_time %dms loop:%d\n",
                                                    g_wifi_support_chn[chn_cnt], chn_parser_time, use_time_ms, chn_loop);
                        break;
                    }
                }
            }//end of while(1) : parser every channle

            ret = delete_monitor_socket(fd);
            if (ret) {
                SL_ERR_PRT("Do delete_monitor_socket fail! ret:%d\n", ret);
                return -1;
            }
        }

        /* Lock smartlink channel operation time out! So don't finded  smartlink channel return error! */
        if (0 == lock_flag) {
            gettimeofday(&tmp_tv, NULL);
            use_time_ms = (tmp_tv.tv_sec - parser_start_tv.tv_sec) * 1000 +
                                        (tmp_tv.tv_usec - parser_start_tv.tv_usec) / 1000;
            if (use_time_ms > lock_time_out && 0 == lock_flag) {
                printf("Lock smartlink channel time out! lock_time:%dms use_time %dms lock_loop:%d\n",
                                                                        lock_time_out, use_time_ms, lock_loop);
                return -1;
            }
        }
    }//end of while(lock_loop)

    /* If ios and same andriod system don't get wifi channel, so we will check witch smartlink channel is. */
    if (g_smartlink_chn <= 0) {

        /* find max packet in wifi channel */
        res = 0;
        for (chn_cnt = 0; 0 != g_wifi_support_chn[chn_cnt]; chn_cnt++) {
            if (recv_chn_cnt[chn_cnt] > res) {
                res = recv_chn_cnt[chn_cnt];
                tmp_chn = g_wifi_support_chn[chn_cnt];
            }
        }

        /* check again in most pack_num wifi channel */
        if(res > 1 && res <30){
            printf("[FUN]:%s [LINE]:%d Try to monitor channel:%d, please wait 3 second!\n",
                        __func__, __LINE__,tmp_chn);
            /* Setting wifi channel, for loop all wifi channel.  */
            ret = set_wifi_channel(wifi_name, tmp_chn);
            if (ret) {
                SL_ERR_PRT("Do set_wifi_channel fail! ret:%d\n", ret);
            }

            /* Read current wifi channel data, for make sure send wifi channel. */
            fd = create_monitor_socket(wifi_name);
            if (fd <= 0) {
                SL_ERR_PRT("Do create_monitor_socket fail! ret:%d\n", fd);
                return -1;
            }

            memset(&ll, 0, sizeof(ll));

            chn_loop = 0;
            chn_parser_time = 3000;
            FD_ZERO(&rfds);
            FD_SET(fd, &rfds);
            gettimeofday(&chn_start_tv, NULL);
            while(1){
                inputfs = rfds;
                tmp_tv.tv_sec = 0;
                tmp_tv.tv_usec = 500 * 1000;

                ret = select(fd+1, &inputfs, (fd_set *)0, (fd_set *)0, &tmp_tv);
                if (0 == ret) {
                    printf ("[FUN]:%s  [LINE]:%d  ==>> wifi_chn:%d recv time out!\n",
                                                __func__,__LINE__,tmp_chn);
                    //continue;
                } else if (ret < 0) {
                    printf ("[FUN]:%s  [LINE]:%d  ==>> wifi_chn:%d recv error!\n",
                                                __func__,__LINE__,tmp_chn);
                    //continue;
                } else{
                    fromlen = sizeof(ll);
                    res2 = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&ll, &fromlen);
                    if (res2 < 0) {
                        SL_ERR_PRT("Do recvfrom fail! fd:%d res2:%d error[%d]:%s\n", fd, res2, errno, strerror(errno));
                    }

                    ret = parser_smartlink_data(buf, res2, da_mac);
                    if (0 == ret) {
                        /* Calculate every recver chn data num. */
                        res += 1;

                        if(res >=30){
                            break;
                        }
                    }
                }

                /*check timeout*/
                chn_loop++;
                if (0 == chn_loop%5) {
                    gettimeofday(&tmp_tv, NULL);
                    use_time_ms = (tmp_tv.tv_sec - chn_start_tv.tv_sec) * 1000 +
                                                (tmp_tv.tv_usec - chn_start_tv.tv_usec) / 1000;
                    if (use_time_ms > chn_parser_time) {
                        SL_DB_PRT("Currcut will change chn:%d, chn_parser_time:%d use_time %dms loop:%d\n",
                                                    g_wifi_support_chn[chn_cnt], chn_parser_time, use_time_ms, chn_loop);
                        break;
                    }
                }
            }//end of while(1)

            delete_monitor_socket(fd);
        }

        if (res >= 30) {
            g_smartlink_chn = tmp_chn;
            printf("[FUN]:%s [LINE]:%d  I finded g_smartlink_chn:%d  tmp_chn:%d  res:%d\n",
                                    __func__, __LINE__, g_smartlink_chn, tmp_chn, res);
        } else {
            printf("[FUN]:%s [LINE]:%d  Don't finded smartlink chn! res:%d\n",
                                    __func__, __LINE__, res);
            return -1;
        }
    }

    return 0;
}


static int decrypt_smartlink_data(SMARTLINK_AP_S *ap_info)
{
    int i = 0, tmp = 0;
    if (NULL == ap_info) {
        SL_ERR_PRT("Input ap_info is NULL!\n");
        return -1;
    }

    if (0 == g_random_num || 0xff == g_random_num) {
        SL_ERR_PRT("g_random_num:%d is no recieve!\n", g_random_num);
        return -1;
    }

    tmp = g_random_num + g_ssid_pwd_size;
    for (i = 0; i < g_ssid_size; i++) {
        ap_info->ssid[i] = ap_info->ssid[i] ^ tmp;
    }

    for (i = 0; i < g_pwd_size; i++) {
        ap_info->psswd[i] = ap_info->psswd[i] ^ tmp;
    }

    return 0;
}


int parser_smartlink_config(const char *wifi_name, int time_out, SMARTLINK_AP_S *ap_info)
{
    int ret = 0, fd = 0;
    int chn_cnt = 0, use_time_ms = 0;
    struct timeval parser_start_tv, tmp_tv;

    int res = 0;
    unsigned char buf[2300] = {0};
    unsigned char da_mac[6] = {0};
    struct sockaddr_ll ll;
    socklen_t fromlen;
    fd_set inputfs, rfds;

    if (NULL == wifi_name) {
        SL_ERR_PRT("Input wifi_name is NULL!\n");
        return -1;
    }

    if (NULL == ap_info) {
        SL_ERR_PRT("Input ap_info is NULL!\n");
        return -1;
    }

    if (time_out < 100) {
        SL_ERR_PRT("Input time_out:%d too little!\n", time_out);
        return -1;
    }

    /* Step 1. Check wifi channel is locked? */
    if (g_smartlink_chn <= 0 || g_smartlink_chn > MAX_WIFI_CHN) {
        SL_ERR_PRT("Current smartlink wifi channle:%d error!\n", g_smartlink_chn);
        return -1;
    }

    /* Step 2. Set right lock wifi channel. */
    ret = set_wifi_channel(wifi_name, g_smartlink_chn);
    if (ret) {
        SL_ERR_PRT("Do set_wifi_channel fail! ret:%d\n", ret);
        return -1;
    }
    usleep(1000);

    /* Step 3. Parser smartlink config data packet. */
    gettimeofday(&parser_start_tv, NULL);

    /* Read current wifi channel data, for make sure send wifi channel. */
    fd = create_monitor_socket(wifi_name);
    if (fd <= 0) {
        SL_ERR_PRT("Do create_monitor_socket fail! ret:%d\n", fd);
        return -1;
    }

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    while (1) {
        inputfs = rfds;
        tmp_tv.tv_sec = 0;
        tmp_tv.tv_usec = 400 * 1000;

        ret = select(fd+1, &inputfs, (fd_set *)0, (fd_set *)0, &tmp_tv);
        if (0 == ret) {
            printf ("[FUN]:%s  [LINE]:%d  ==>> wifi_chn:%d recv time out!\n",
                                        __func__,__LINE__, g_wifi_support_chn[chn_cnt]);
            //continue;
        } else if (ret < 0) {
            printf ("[FUN]:%s  [LINE]:%d  ==>> wifi_chn:%d recv error!\n",
                                        __func__,__LINE__, g_wifi_support_chn[chn_cnt]);
            //continue;
        } else {
            memset(&ll, 0, sizeof(ll));
            fromlen = sizeof(ll);
            res = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&ll, &fromlen);
            if (res < 0) {
                SL_ERR_PRT("Do recvfrom fail! fd:%d res:%d error[%d]:%s\n", fd, res, errno, strerror(errno));
            } else {
                ret = parser_smartlink_data(buf, res, da_mac);
                if (0 == ret) {
                    /* Calculate every recver chn data num. */
                    ret = check_receive_result(ap_info);
                    if (0 == ret) {

                        #ifdef ENABLE_ENCRYPT
                        ret = decrypt_smartlink_data(ap_info);
                        if (ret) {
                            SL_ERR_PRT("Do decrypt_smartlink_data fail! ret:%d\n", ret);
                            break;
                        }
                        #endif

                        printf("[FUN]:%s [LINE]:%d  Receive smarlink complete! ssid:%s  psswd:%s  random:%d\n",
                                        __func__, __LINE__, ap_info->ssid, ap_info->psswd, ap_info->random_num);
                        break;
                    }
                }
            }
        }

        /* Calculate parser smartlink data time out for return this function. */
        gettimeofday(&tmp_tv, NULL);
        use_time_ms = (tmp_tv.tv_sec - parser_start_tv.tv_sec) * 1000 +
                                        (tmp_tv.tv_usec - parser_start_tv.tv_usec) / 1000;
        if (use_time_ms > time_out) {
            printf("Parser smartlink config data packet time out! time_out:%dms use_time %dms\n",
                                                                    time_out, use_time_ms);
            delete_monitor_socket(fd);
            return -1;
        }
    }

    ret = delete_monitor_socket(fd);
    if (ret) {
        SL_ERR_PRT("Do delete_monitor_socket fail! ret:%d\n", ret);
        return -1;
    }

    return 0;
}


int send_broadcast_val(unsigned char random_num)
{
    int ret = 0, num = 0;
    int fd = -1, opt = 1;
    struct sockaddr_in server; /* server's address information */

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        SL_ERR_PRT("Create socket SOCK_DGRAM fd:%d fail! error[%d]:%s\n", fd, errno, strerror(errno));
        return -1;
    }

    opt = 1;
    ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (ret < 0) {
        SL_ERR_PRT("setsockopt SO_REUSEADDR fd:%d fail! ret:%d error[%d]:%s\n", fd, ret, errno, strerror(errno));
       goto cleanup;
    }

    opt = 1;
    ret = setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
    if (ret < 0) {
        SL_ERR_PRT("setsockopt SO_BROADCAST fd:%d fail! ret:%d error[%d]:%s\n", fd, ret, errno, strerror(errno));
        goto cleanup;
    }

    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port   = htons(BROADCAST_PORT);
    server.sin_addr.s_addr = INADDR_BROADCAST;

    socklen_t len;
    len = sizeof(struct sockaddr_in);
    num = 10;
    while (num--) {
        ret = sendto(fd, &random_num, sizeof(random_num), 0, (struct sockaddr *)&server, len);
        if (ret < 0) {
            SL_ERR_PRT("sendto random_num fd:%d fail! ret:%d error[%d]:%s\n", fd, ret, errno, strerror(errno));
        }
        usleep(100 * 1000);
    }

    ret = 0;
cleanup:
    if (fd > 0) {
        close(fd);
    }
    return ret;
}


#if 0
int main_test()
{
    int ret = 0, cnt = 0;
    SMARTLINK_AP_S ap_info;

    ret = set_wifi_monitor(WIFI_NAME, 1);
    if (ret) {
        SL_ERR_PRT("Do set_wifi_monitor fail! ret:%d\n", ret);
        return -1;
    }

    ret = lock_smartlink_wifi_chn(WIFI_NAME, 60*1000, 100);
    if (ret) {
        SL_ERR_PRT("Do lock_smartlink_wifi_chn fail! ret:%d\n", ret);
        goto main_exit;
    }

    printf("\n\n");

    memset(&ap_info, 0, sizeof(ap_info));
    ret = parser_smartlink_config(WIFI_NAME, 20*1000, &ap_info);
    if (ret) {
        SL_ERR_PRT("Do parser_smartlink_config fail! ret:%d\n", ret);
        goto main_exit;
    }

main_exit:
    ret = set_wifi_monitor(WIFI_NAME, 0);
    if (ret) {
        SL_ERR_PRT("Do set_wifi_monitor fail! ret:%d\n", ret);
        return -1;
    }

    return 0;
}
#endif
