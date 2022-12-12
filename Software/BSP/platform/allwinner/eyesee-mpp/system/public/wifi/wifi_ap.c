/* *******************************************************************************
 * Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 * *******************************************************************************/
/**
 * @file wifi_ap.c
 * @brief HAL for wifi ap control
 * @author id: KP0356
 * @version v0.1
 * @date 2016-08-28
 */

/******************************************************************************/
/*                             Include Files                                  */
/******************************************************************************/
#include "wpa_supplicant/wpa_ctrl.h"
#include "wifi_ap.h"

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <pthread.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <errno.h>
#include <net/route.h>
#include <stdio.h>


/******************************************************************************/
/*                           Macros & Typedefs                                */
/******************************************************************************/
#define HOSTAPD_CFG_FILE       "/var/run/hostapd/hostapd.conf"
#define HOSTAPD_CFG_DIRC       "/var/run/hostapd"
#define HOSTAPD_RUN_CMD        "/usr/sbin/hostapd -B %s &"
#define HOSTAPD_KILL_CMD       "/usr/bin/killall hostapd"

#define AP_STATUS_INITED       0x1
#define AP_STATUS_DEINITED     (~(AP_STATUS_INITED))
#define AP_STATUS_OPENDED      0x2
#define AP_STATUS_CLOSED       (~(AP_STATUS_OPENDED))
#define AP_STATUS_STARTED      0x4
#define AP_STATUS_STOPED       (~(AP_STATUS_STARTED))

#define CMD_KILL_HOSTAPD       "killall hostapd"
#define CMD_KILL_UDHCPD        "killall udhcpd"
#define CMD_AP_LOAD            "/etc/firmware/ap_ctrl.sh %s load"
#define CMD_AP_UNLOAD          "/etc/firmware/ap_ctrl.sh %s unload"

#define HOSTAPD_BASE_CONF      "interface=wlan0\n"\
                               "driver=nl80211\n"\
                               "ctrl_interface=/var/run/hostapd\n"\
                               "ssid=%s\n"\
                               "channel=%d\n"\
                               "hw_mode=g\n"\
                               "ieee80211n=1\n"\
                               "wowlan_triggers=any\n"

#define HOSTAPD_5G_MOD_CONF    "interface=wlan0\n"\
                               "driver=nl80211\n"\
                               "ctrl_interface=/var/run/hostapd\n"\
                               "ssid=%s\n"\
                               "channel=44\n"\
                               "hw_mode=a\n"\
                               "ieee80211n=1\n"

#define HOSTAPD_5G_6255_CONF   "interface=wlan0\n"\
                               "driver=nl80211\n"\
                               "ctrl_interface=/var/run/hostapd\n"\
                               "ssid=%s\n"\
                               "channel=149\n"\
                               "hw_mode=a\n"\
                               "ieee80211n=1\n"\
                               "ignore_broadcast_ssid=0\n"

#define HOSTAPD_CCMP_CONF      "wpa=2\n"\
                               "rsn_pairwise=CCMP\n"\
                               "wpa_passphrase=%s\n"

#define HOSTAPD_WPAPSK_CONF    "wpa=3\n"\
                               "wpa_key_mgmt=WPA-PSK\n"\
                               "wpa_pairwise=TKIP CCMP\n"\
                               "wpa_passphrase=%s\n"

#define HOSTAPD_WPAEAP_CONF    "wpa=3\n"\
                               "wpa_key_mgmt=WPA-EAP\n"\
                               "wpa_pairwise=TKIP CCMP\n"\
                               "wpa_passphrase=%s\n"

#define HOSTAPD_WEP_CONF       "wep_default_key=0\n"\
                               "wep_key0=%s\n"

/******************************************************************************/
/*                         Structure Declarations                             */
/******************************************************************************/
/* None */


/******************************************************************************/
/*                            Global Variables                                */
/******************************************************************************/
static char              g_dev_name[64]  = {0};
static pthread_mutex_t   g_operate_lock;
static unsigned int      g_ap_opt_status = 0;
static WIFI_AP_CFG_S     g_ap_cfg;


/******************************************************************************/
/*                          Function Declarations                             */
/******************************************************************************/
/* None */


/******************************************************************************/
/*                          Function Definitions                              */
/******************************************************************************/

static int get_cmd_result(const char *cmd, char *buf, uint16_t size)
{
    FILE *fp = NULL;

    fp = popen(cmd, "r");
    if (fp == NULL) {
        perror("popen failed");
        pclose(fp);
        return -errno;
    }

    while(fgets(buf, size, fp) != NULL) {
        buf[strlen(buf) - 1]= '\0';
    }

    pclose(fp);

    return 0;
}

static int up_net_dev(const char * wifi_name)
{
    int fd = -1;
    struct ifreq ifr;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd <= 0) {
        AP_ERR_PRT("Fail to create socket! errno:%d err:%s\n", errno, strerror(errno));
        return -1;
    }

    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, wifi_name, sizeof(ifr.ifr_name));
    ifr.ifr_addr.sa_family = AF_INET;
    if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
        AP_ERR_PRT("Fail to ioctl SIOCGIFFLAGS. wifi_name:%s errno:%d err:%s\n",
                                            wifi_name, errno, strerror(errno));
        close(fd);
        return -1;
    }

    ifr.ifr_flags |= IFF_UP;

    if (ioctl(fd, SIOCSIFFLAGS, &ifr) < 0) {
        AP_ERR_PRT("Fail to ioctl SIOCSIFFLAGS. wifi_name:%s errno:%d err:%s\n",
                                            wifi_name, errno, strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}


static int down_net_dev(const char * wifi_name)
{
    int fd = -1;
    struct ifreq ifr;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd <= 0) {
        AP_ERR_PRT("Fail to create socket! errno:%d err:%s\n", errno, strerror(errno));
        return -1;
    }

    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, wifi_name, sizeof(ifr.ifr_name));
    ifr.ifr_addr.sa_family = AF_INET;
    if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
        AP_ERR_PRT("Fail to ioctl SIOCGIFFLAGS. wifi_name:%s errno:%d err:%s\n",
                                            wifi_name, errno, strerror(errno));
        close(fd);
        return -1;
    }

    ifr.ifr_flags &= (~IFF_UP);

    if (ioctl(fd, SIOCSIFFLAGS, &ifr) < 0) {
        AP_ERR_PRT("Fail to ioctl SIOCSIFFLAGS. wifi_name:%s errno:%d err:%s\n",
                                            wifi_name, errno, strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}


static void * ap_proccess(void *para)
{
    return NULL;
}


int wifi_ap_init(void)
{
    int  ret = 0;
    char buf[1024] = {0};

    if ((g_ap_opt_status & AP_STATUS_INITED)) {
        AP_DB_PRT("The wifi ap mode have inited yet!\n");
        return 0;
    }

    pthread_mutex_init(&g_operate_lock, NULL);

    /* step 1.  load wifi AP driver. */
    memset(g_dev_name, 0, sizeof(g_dev_name));
    if ((ret = access("/etc/firmware/ap6255", F_OK)) == 0) {
        strcpy(g_dev_name, "ap6255");
    }else if ((ret = access("/etc/firmware/ap6335", F_OK)) == 0) {
        strcpy(g_dev_name, "ap6335");
    }else if((ret = access("/etc/firmware/ap6181", F_OK)) == 0){
        strcpy(g_dev_name, "ap6181");
    }else if((ret = access("/etc/firmware/8189ftv", F_OK)) == 0){
        strcpy(g_dev_name, "8189ftv");
    }else if((ret = access("/etc/firmware/xr819", F_OK)) == 0){
        strcpy(g_dev_name, "xr819");
    }

    memset(buf, 0, sizeof(buf));
    ret = snprintf(buf, sizeof(buf)-1, CMD_AP_LOAD, g_dev_name);
    if (ret <= 0) {
        AP_ERR_PRT ("Do snprintf CMD_AP_LOAD fail! ret:%d\n", ret);
        return -1;
    }
    system(buf);

    usleep(200 * 1000);

    int check_cnt = 10;
    memset(buf, 0, sizeof(buf));
    while(check_cnt--) {
        get_cmd_result("cat /proc/net/dev | grep wlan0", buf, sizeof(buf));
        if (strstr(buf, "wlan0")) break;
        AP_DB_PRT("wait load driver done...check count[%d]\n", check_cnt);
        usleep(200 * 1000);
    }

    if (check_cnt == 0) {
        AP_ERR_PRT("wifi ap init failed, load driver failed\n");
        return -1;
    }

    g_ap_opt_status  = 0;
    g_ap_opt_status |= AP_STATUS_INITED;

    return 0;
}


int wifi_ap_exit(void)
{
    int ret = 0;
    char  buf[1024] = {0};

    /* step 1.  Unload this wifi AP driver */
    memset(g_dev_name, 0, sizeof(g_dev_name));
    if ((ret = access("/etc/firmware/ap6255", F_OK)) == 0) {
        strcpy(g_dev_name, "ap6255");
    } else if ((ret = access("/etc/firmware/ap6335", F_OK)) == 0) {
        strcpy(g_dev_name, "ap6335");
    }else if((ret = access("/etc/firmware/ap6181", F_OK)) == 0){
        strcpy(g_dev_name, "ap6181");
    }else if((ret = access("/etc/firmware/8189ftv", F_OK)) == 0){
        strcpy(g_dev_name, "8189ftv");
    }else if((ret = access("/etc/firmware/xr819", F_OK)) == 0){
        strcpy(g_dev_name, "xr819");
    }

    memset(buf, 0, sizeof(buf));
    ret = snprintf(buf, sizeof(buf)-1, CMD_AP_UNLOAD, g_dev_name);
    if (ret <= 0) {
        AP_ERR_PRT ("Do snprintf CMD_AP_UNLOAD fail! ret:%d\n", ret);
        return -1;
    }
    system(buf);

    sleep(1);

    g_ap_opt_status = 0;
    pthread_mutex_destroy(&g_operate_lock);

    return 0;
}


int wifi_ap_open(const char * wifi_name)
{
    int ret = 0;

    /* step 1.  Check input para */
    if (NULL == wifi_name) {
        AP_ERR_PRT ("Input wifi_name is NULL!\n");
        return -1;
    }

    /* step 2.  Update operate status*/
    pthread_mutex_lock(&g_operate_lock);
    if ((g_ap_opt_status & AP_STATUS_OPENDED)) {
        AP_DB_PRT("The wifi ap mode have opened yet!\n");
        pthread_mutex_unlock(&g_operate_lock);
        return 0;
    }

    g_ap_opt_status |= AP_STATUS_OPENDED;
    pthread_mutex_unlock(&g_operate_lock);

    return 0;
}


int wifi_ap_close(const char * wifi_name)
{
    int ret = 0;

    /* step 1.  Check input para */
    if (NULL == wifi_name) {
        AP_ERR_PRT ("Input wifi_name is NULL!\n");
        return -1;
    }

    /* step 2.  Update operate status */
    pthread_mutex_lock(&g_operate_lock);
    if (0 == (g_ap_opt_status & AP_STATUS_OPENDED)) {
        AP_DB_PRT("The wifi ap mode have closed yet!\n");
        pthread_mutex_unlock(&g_operate_lock);
        return 0;
    }

    /* step 3.  ifc down this AP device */
    ret = down_net_dev(wifi_name);
    if (ret < 0) {
        AP_ERR_PRT("Do down_net_dev fail! wifi:%s ret:%d\n", wifi_name, ret);
        pthread_mutex_unlock(&g_operate_lock);
        return -1;
    }

    g_ap_opt_status &= AP_STATUS_CLOSED;
    pthread_mutex_unlock(&g_operate_lock);

    return 0;
}


int wifi_ap_start(const char *wifi_name, WIFI_AP_CFG_S *ap_cfg)
{
    int            ret = 0;
    char           buf[1024] = {0};
    FILE          *fp = NULL;
    pthread_t      thread_id;
    pthread_attr_t attr;

    /* step 1.  Check input para */
    if (NULL == wifi_name || NULL == ap_cfg) {
        AP_ERR_PRT ("Input wifi_name or ap_cfg is NULL!\n");
        return -1;
    }

    pthread_mutex_lock(&g_operate_lock);
    if ((g_ap_opt_status & AP_STATUS_STARTED)) {
        AP_DB_PRT ("The %s already start!\n", wifi_name);
        pthread_mutex_unlock(&g_operate_lock);
        return 0;
    }

    /* step 2.  Set ap cfg, and init data */
    memset(&g_ap_cfg, 0, sizeof(g_ap_cfg));

    if (access(HOSTAPD_CFG_FILE, F_OK) == 0) {
        ret = unlink(HOSTAPD_CFG_FILE);
        if (ret) {
            AP_ERR_PRT ("Unlink file:%s fail! errno[%d] errinfo[%s]\n",
                    HOSTAPD_CFG_FILE, errno, strerror(errno));
        }
    }

    /* step 3.  Creat sta server proccess  */
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&thread_id, &attr, ap_proccess, NULL);
    if (ret) {
        AP_ERR_PRT ("Create pthread fail! ret:%d  error:%s\n", ret, strerror(errno));
        pthread_attr_destroy(&attr);
        pthread_mutex_unlock(&g_operate_lock);
        return ret;
    }
    pthread_attr_destroy(&attr);
    usleep(10);

    /* step 4.  Creat hostapd_cli connect hostapd, for operate wifi_ap. */
    fp = fopen(HOSTAPD_CFG_FILE, "wb+");
    if (NULL == fp) {
        AP_ERR_PRT ("Create %s fail! errno:%d  error:%s\n",
                                    HOSTAPD_CFG_FILE, errno, strerror(errno));
        pthread_mutex_unlock(&g_operate_lock);
        return -1;
    }

    /* Write base ap config to file */
    memset(buf, 0, sizeof(buf));
    if (0 == ap_cfg->frequency) {
        /* Set 2.4G work mode */
        ret = snprintf(buf, sizeof(buf)-1, HOSTAPD_BASE_CONF, ap_cfg->ssid, ap_cfg->channel);
    } else {
        /* Set 5G work mode */
        if (0 == strcmp(g_dev_name, "ap6255")) {
            ret = snprintf(buf, sizeof(buf)-1, HOSTAPD_5G_6255_CONF, ap_cfg->ssid);
        } else {
            ret = snprintf(buf, sizeof(buf)-1, HOSTAPD_5G_MOD_CONF, ap_cfg->ssid);
        }
    }
    if (ret <= 0) {
        AP_ERR_PRT ("Do snprintf HOSTAPD_BASE_CONF fail! ret:%d\n", ret);
        goto AP_START_OUT;
    }
    ret = fwrite(buf, ret, 1, fp);
    if (ret <= 0) {
        AP_ERR_PRT ("fwrite fail! errno:%d  error:%s\n", errno, strerror(errno));
        goto AP_START_OUT;
    }

    /* Write security type config to file */
    memset(buf, 0, sizeof(buf));
    switch (ap_cfg->security) {
        case WIFI_AP_SECURITY_OPEN:
            /* open security, so do nothing. */
            break;

        case WIFI_AP_SECURITY_WEP:
            ret = snprintf(buf, sizeof(buf)-1, HOSTAPD_WEP_CONF, ap_cfg->pswd);
            if (ret <= 0) {
                AP_ERR_PRT ("Do snprintf HOSTAPD_WPAPSK_CONF fail! ret:%d\n", ret);
                goto AP_START_OUT;
            }
            ret = fwrite(buf, ret, 1, fp);
            if (ret <= 0) {
                AP_ERR_PRT ("fwrite fail! errno:%d  error:%s\n", errno, strerror(errno));
                goto AP_START_OUT;
            }
            break;

        case WIFI_AP_SECURITY_WPA_WPA2_PSK:
            ret = strlen(ap_cfg->pswd);
            if (ret < 8) {
                AP_ERR_PRT ("This password len < 8. error!\n");
                goto AP_START_OUT;
            }
            ret = snprintf(buf, sizeof(buf)-1, HOSTAPD_WPAPSK_CONF, ap_cfg->pswd);
            if (ret <= 0) {
                AP_ERR_PRT ("Do snprintf HOSTAPD_WPAPSK_CONF fail! ret:%d\n", ret);
                goto AP_START_OUT;
            }
            ret = fwrite(buf, ret, 1, fp);
            if (ret <= 0) {
                AP_ERR_PRT ("fwrite fail! errno:%d  error:%s\n", errno, strerror(errno));
                goto AP_START_OUT;
            }
            break;

        case WIFI_AP_SECURITY_WPA_WPA2_EAP:
            ret = strlen(ap_cfg->pswd);
            if (ret < 8) {
                AP_ERR_PRT ("This password len < 8. error!\n");
                goto AP_START_OUT;
            }
            ret = snprintf(buf, sizeof(buf)-1, HOSTAPD_WPAEAP_CONF, ap_cfg->pswd);
            if (ret <= 0) {
                AP_ERR_PRT ("Do snprintf HOSTAPD_WPAEAP_CONF fail! ret:%d\n", ret);
                goto AP_START_OUT;
            }
            ret = fwrite(buf, ret, 1, fp);
            if (ret <= 0) {
                AP_ERR_PRT ("fwrite fail! errno:%d  error:%s\n", errno, strerror(errno));
                goto AP_START_OUT;
            }
            break;

        case WIFI_AP_SECURITY_WAPI_CERT:
        case WIFI_AP_SECURITY_WAPI_PSK:
            AP_ERR_PRT ("Now, we don't support this [WAPI_CERT/WAPI_PSK] security type.\n");
            ret = -1;
            goto AP_START_OUT;
            break;

        default:
            AP_ERR_PRT ("Input security type:%d error!\n", ap_cfg->security);
            ret = -1;
            goto AP_START_OUT;
            break;
    }

    fclose(fp);
    fp = NULL;

    sync();

    /* Step 5. Run hostapd by call system. */
    memset(buf, 0, sizeof(buf));
    ret = snprintf(buf, sizeof(buf)-1, HOSTAPD_RUN_CMD, HOSTAPD_CFG_FILE);
    if (ret <= 0) {
        AP_ERR_PRT ("Do snprintf HOSTAPD_RUN_CMD fail! ret:%d\n", ret);
        goto AP_START_OUT;
    }
    AP_DB_PRT("Do [%s] ......\n", buf);
    int i;
    for (i = 3; i < getdtablesize(); i++) {
        int flags;
        if ((flags = fcntl(i, F_GETFD)) != -1)
            fcntl(i, F_SETFD, flags | FD_CLOEXEC);
    }
    ret = system(buf);
    usleep(20);

    g_ap_opt_status |= AP_STATUS_STARTED;
    memcpy(&g_ap_cfg, ap_cfg, sizeof(g_ap_cfg));
    ret = 0;

AP_START_OUT:
    if (fp != NULL) {
        fclose(fp);
        fp = NULL;
    }

    sleep(1);

    memset(buf, 0, sizeof(buf));
    snprintf(buf, sizeof(buf), "%s/%s", HOSTAPD_CFG_DIRC, wifi_name);
    int check_cnt = 10;
    while(check_cnt--) {
        if (!access(buf, F_OK)) break;
        AP_DB_PRT("wait run hostapd done...check count[%d]\n", check_cnt);
        usleep(200 * 1000);
    }

    if (check_cnt == 0) ret = -1;

    pthread_mutex_unlock(&g_operate_lock);

    return ret;
}


int wifi_ap_stop(const char *wifi_name)
{
    /* step 1.  Check input para */
    if (NULL == wifi_name) {
        AP_ERR_PRT (" Input wifi_name is NULL!\n");
        return -1;
    }

    pthread_mutex_lock(&g_operate_lock);
    if (0 == (g_ap_opt_status & AP_STATUS_STARTED)) {
        AP_DB_PRT("The wifi ap mode have stoped yet!\n");
        pthread_mutex_unlock(&g_operate_lock);
        return 0;
    }

    /* step 2.  Exit the AP server proccess. */
    system(HOSTAPD_KILL_CMD);
    usleep(80);

    if (access(HOSTAPD_CFG_FILE, F_OK) == 0) {
        unlink(HOSTAPD_CFG_FILE);
    }
    usleep(80);

    char buf[128] = {0};
    snprintf(buf, sizeof(buf), "%s/%s", HOSTAPD_CFG_DIRC, wifi_name);
    if (access(buf, F_OK) == 0) {
        unlink(buf);
    }

    /* step 3.  Update operate status. */
    g_ap_opt_status &= AP_STATUS_STOPED;
    pthread_mutex_unlock(&g_operate_lock);

    return 0;
}


int wifi_ap_get_config(const char *wifi_name, WIFI_AP_CFG_S *ap_cfg)
{
    pthread_mutex_lock(&g_operate_lock);
    memcpy(ap_cfg, &g_ap_cfg, sizeof(g_ap_cfg));
    pthread_mutex_unlock(&g_operate_lock);

    return 0;
}


static int main_wifi_ap_test()
{
    int ret = 0;
    int cnt = 0;
    int i   = 0;
    WIFI_AP_CFG_S ap_cfg;

    ret = wifi_ap_init();
    if (ret) {
        AP_ERR_PRT ("Do wifi_ap_init fail! ret:%d\n", ret);
        return -1;
    }

    ret = wifi_ap_open("wlan0");
    if (ret) {
        AP_ERR_PRT ("Do wifi_ap_open fail! ret:%d\n", ret);
        return -1;
    }

    memset(&ap_cfg, 0, sizeof(ap_cfg));
    strncpy(ap_cfg.ssid,  "ipc_guixing", sizeof(ap_cfg.ssid)-1);
    strncpy(ap_cfg.bssid, "a0:0b:ba:b4:af:3e", sizeof(ap_cfg.bssid)-1);
    strncpy(ap_cfg.pswd,  "12345678", sizeof(ap_cfg.pswd)-1);
    ap_cfg.channel  = 8;
    ap_cfg.security = WIFI_AP_SECURITY_WPA_WPA2_PSK;
    //ap_cfg.security = WIFI_AP_SECURITY_WEP;
    //ap_cfg.security = WIFI_AP_SECURITY_OPEN;
    //ap_cfg.security = WIFI_AP_SECURITY_WPA_WPA2_EAP;
    ap_cfg.hidden_ssid = 0;

    ret = wifi_ap_start("wlan0", &ap_cfg);
    if (ret) {
        AP_ERR_PRT ("Do wifi_ap_start fail! ret:%d\n", ret);
        return -1;
    }

    sleep(888);

    ret = wifi_ap_stop("wlan0");
    if (ret) {
        AP_ERR_PRT ("Do wifi_ap_stop fail! ret:%d\n", ret);
        return -1;
    }
    usleep(80);

    ret = wifi_ap_close("wlan0");
    if (ret) {
        AP_ERR_PRT ("Do wifi_ap_close fail! ret:%d\n", ret);
    }
    usleep(80);

    ret = wifi_ap_exit();
    if (ret) {
        AP_ERR_PRT ("Do wifi_ap_exit fail! ret:%d\n", ret);
    }
    usleep(80);

    return 0;
}
