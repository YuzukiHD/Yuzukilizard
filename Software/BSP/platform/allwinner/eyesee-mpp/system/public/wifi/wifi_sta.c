/* *******************************************************************************
 * Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 * *******************************************************************************/
/**
 * @file wifi_sta.c
 * @brief HAL for wifi control
 * @author id: KP0356
 * @version v0.1
 * @date 2016-08-28
 */

/******************************************************************************/
/*                             Include Files                                  */
/******************************************************************************/
#include "wpa_supplicant/wpa_ctrl.h"
#include "wifi_sta.h"

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
#define WPA_SUP_CFG_PATH        "/var/run/wpa_supplicant/wlan0"
#define WPA_SUP_CFG_FILE        "/var/run/wpa_supplicant/wpa.conf"
#define CMD_STA_LOAD            "/etc/firmware/sta_ctrl.sh %s load"
#define CMD_STA_UNLOAD          "/etc/firmware/sta_ctrl.sh %s unload"
#define CMD_WPA_RUN             "/usr/sbin/wpa_supplicant -Dnl80211 -i%s -c%s &"
#define CMD_KILL_WPA            "killall wpa_supplicant"

#define STA_STATUS_INITED       0x1
#define STA_STATUS_DEINITED     (~(STA_STATUS_INITED))
#define STA_STATUS_OPENDED      0x2
#define STA_STATUS_CLOSED       (~(STA_STATUS_OPENDED))
#define STA_STATUS_STARTED      0x4
#define STA_STATUS_STOPED       (~(STA_STATUS_STARTED))


/******************************************************************************/
/*                         Structure Declarations                             */
/******************************************************************************/
/* None */


/******************************************************************************/
/*                            Global Variables                                */
/******************************************************************************/

static pthread_mutex_t   g_scan_lock;
static pthread_mutex_t   g_operate_lock;
static int               g_sta_net_id     = -1;
static unsigned int      g_sta_opt_status = 0;
static void             *g_sta_pdata      = NULL;
static char              g_dev_name[64]   = {0};
static WIFI_STA_STATUS_E g_scan_status = WIFI_STA_STATUS_BOTTON;
static WIFI_STA_EVENT_E  g_sta_event   = WIFI_STA_EVENT_BOTTON;
static WIFI_STA_EVENT_CALLBACK g_fcb   = NULL;


/******************************************************************************/
/*                          Function Declarations                             */
/******************************************************************************/
/* None */


/******************************************************************************/
/*                          Function Definitions                              */
/******************************************************************************/
static int wpa_cli_cmd(char *cmd)
{
    int         ret         = 0;
    int         stat_len    = 0;
    char        buf[1024]   = {0};
    size_t      len         = 0;
    const char *pstatus     = NULL;
    struct wpa_ctrl *pst_wpaCtl = NULL;

    if (NULL == cmd) {
        STA_ERR_PRT("Input cmd is NULL!\n");
        return -1;
    }

    pst_wpaCtl = wpa_ctrl_open(WPA_SUP_CFG_PATH);
    if (NULL == pst_wpaCtl) {
        STA_ERR_PRT("Do wpa_ctrl_open error! PATH:%s\n", WPA_SUP_CFG_PATH);
        return -1;
    }

    len = sizeof(buf) - 1;
    ret = wpa_ctrl_request(pst_wpaCtl, cmd, strlen(cmd),
                            buf, &len, NULL);
    if (-2 == ret) {
        STA_ERR_PRT("Do wpa_ctrl_request time out! cmd:%s ret:%d\n", cmd, ret);
        goto GET_STATUA_EXIT;
    } else if (ret < 0) {
        STA_ERR_PRT("Do wpa_ctrl_request error! cmd:%s ret:%d\n", cmd, ret);
        goto GET_STATUA_EXIT;
    }

    STA_DB_PRT("cmd:%s  buf:%s len:%d\n", cmd, buf, len);

    if(len >= 2 && strncmp(buf, "OK", 2) == 0) {
        ret = 0;
        goto GET_STATUA_EXIT;
    } else {
        ret = -1;
        goto GET_STATUA_EXIT;
    }

    ret = 0;
GET_STATUA_EXIT:
    wpa_ctrl_close(pst_wpaCtl);
    return ret;
}


static int wpa_cli_get_status(WIFI_STA_STATUS_E *psta_status)
{
    int         ret         = 0;
    int         cnt         = 0;
    int         stat_len    = 0;
    char        buf[1024]   = {0};
    char        status[128] = {0};
    size_t      len         = 0;
    const char *cmd         = "STATUS";
    const char *pstatus     = NULL;
    struct wpa_ctrl *pst_wpaCtl = NULL;

    if (NULL == psta_status) {
        STA_ERR_PRT("Input psta_status is NULL!\n");
        return -1;
    }

    pst_wpaCtl = wpa_ctrl_open(WPA_SUP_CFG_PATH);
    if (NULL == pst_wpaCtl) {
        STA_ERR_PRT("Do wpa_ctrl_open error! PATH:%s\n", WPA_SUP_CFG_PATH);
        return -1;
    }

    len = sizeof(buf) - 1;
    ret = wpa_ctrl_request(pst_wpaCtl, cmd, strlen(cmd),
                            buf, &len, NULL);
    if (-2 == ret) {
        STA_ERR_PRT("Do wpa_ctrl_request time out! cmd:%s ret:%d\n", cmd, ret);
        goto GET_STATUA_EXIT;
    } else if (ret < 0) {
        STA_ERR_PRT("Do wpa_ctrl_request error! cmd:%s ret:%d\n", cmd, ret);
        goto GET_STATUA_EXIT;
    }

    STA_DB_PRT("buf:%s len:%d\n", buf, len);

    pstatus = strstr(buf, "wpa_state=");
    if (NULL == pstatus) {
        STA_ERR_PRT("Get wifi status error! cmd:%s \n", cmd);
        ret = -1;
        goto GET_STATUA_EXIT;
    }

    cnt       = 0;
    stat_len  = len - strlen("wpa_state=");
    pstatus  += strlen("wpa_state=");
    while (1) {
        if (cnt >= (sizeof(status) - 1) || cnt >= stat_len || (pstatus - buf) >= len) {
            break;
        }

        if (0x0a == *pstatus) {
            break;
        }

        if (*pstatus == '_' || (*pstatus <= 'Z' && *pstatus >= 'A') || (*pstatus <= '9' && *pstatus >= '0')) {
            status[cnt] = *pstatus;
            cnt++;
        }
        pstatus += 1;
    }

    STA_DB_PRT("status:%s  cnt:%d  len:%d \n", status, cnt, len);

    if(strcmp(status, "COMPLETED") == 0) {
        *psta_status = WIFI_STA_STATUS_CONNECTED;
    } else if(strcmp(status, "DISCONNECTED") == 0) {
        *psta_status = WIFI_STA_STATUS_DISCONNECTED;
    } else if(strcmp(status, "INACTIVE") == 0) {
        *psta_status = WIFI_STA_STATUS_INACTIVE;
    } else if (strcmp(status, "SCANNING") == 0
		|| strcmp(status, "4WAY_HANDSHAKE") == 0
		|| strcmp(status, "GROUP_HANDSHAKE") == 0
		|| strcmp(status, "ASSOCIATED") == 0
		|| strcmp(status, "ASSOCIATING") == 0){
        *psta_status = WIFI_STA_STATUS_CONNECTING;
    } else {
        *psta_status = WIFI_STA_STATUS_UNKNOW;
    }

    ret = 0;
GET_STATUA_EXIT:
    wpa_ctrl_close(pst_wpaCtl);
    return ret;
}


static int wpa_cli_add_network(void)
{
    int         ret         = 0;
    char        buf[1024]   = {0};
    size_t      len         = 0;
    const char *cmd         = NULL;
    struct wpa_ctrl *pst_wpaCtl = NULL;

    /* step 1.  Connect the wpa server */
    pst_wpaCtl = wpa_ctrl_open(WPA_SUP_CFG_PATH);
    if (NULL == pst_wpaCtl) {
        STA_ERR_PRT("Do wpa_ctrl_open error! PATH:%s\n", WPA_SUP_CFG_PATH);
        return -1;
    }

    /* step 2.  Send cmd:ADD_NETWORK   to wpa. return 0~9 ID */
    memset(buf, 0, sizeof(buf));
    len = sizeof(buf) - 1;
    cmd = "ADD_NETWORK";
    ret = wpa_ctrl_request(pst_wpaCtl, cmd, strlen(cmd),
                            buf, &len, NULL);
    if (-2 == ret) {
        STA_ERR_PRT("Do wpa_ctrl_request time out! cmd:%s ret:%d\n", cmd, ret);
        goto ADD_NETWORK_EXIT;
    } else if (ret < 0) {
        STA_ERR_PRT("Do wpa_ctrl_request error! cmd:%s ret:%d\n", cmd, ret);
        goto ADD_NETWORK_EXIT;
    }

    /* step 3. Now we just support 0~9 ID network, other is error! guixing */
    if (len > 3) {
        STA_ERR_PRT("Do cmd:ADD_NETWORK fail! buf:%s len:%d\n", buf, len);
        ret = -1;
        goto ADD_NETWORK_EXIT;
    }
    ret = buf[0] - '0';

ADD_NETWORK_EXIT:
    wpa_ctrl_close(pst_wpaCtl);
    return ret;
}


static int wpa_parse_apinfo(const char *buf_list, WIFI_STA_AP_INFO_S *pap_info)
{
    int   ret        = 0;
    int   signal     = 0;
    char *pret       = NULL;
    char  flags[128] = {0};

    if (NULL == buf_list || NULL == pap_info) {
        STA_ERR_PRT("Input event or pap_info is NULL!\n");
        return -1;
    }

    /*  Do scan results is
        >
        > scan_results
        bssid / frequency / signal level / flags / ssid
        d8:15:0d:48:27:74       2412    -51     [WPA-PSK-CCMP][WPA2-PSK-CCMP][ESS]      PD2-IPC-test
        d8:15:0d:09:85:66       2462    -51     [WPA-PSK-CCMP][WPA2-PSK-CCMP][ESS]      BU3-PD1-SPTH
        38:1c:1a:2e:b3:50       2462    -55     [WPA-EAP-CCMP][WPA2-EAP-CCMP][ESS]      Allwinner
        38:1c:1a:2e:b3:51       2462    -55     [WPA2-PSK-CCMP][ESS]    AW2
    */

    ret = strlen(buf_list);
    if (ret <= 24) {
        STA_ERR_PRT("Input buf_list too few! buf:%s\n", buf_list);
        return -1;
    }

    if (strstr(buf_list, "bssid") || strstr(buf_list, "frequency")) {
        STA_ERR_PRT("Input buf_list is title! buf:%s\n", buf_list);
        return -1;
    }

    ret = sscanf(buf_list, "%s %d %d %s %s", pap_info->bssid, &pap_info->frequency, &pap_info->db, flags, pap_info->ssid);
    if (ret < 0) {
        STA_ERR_PRT("buf_list:%s sscanf error:%s! ret:%d\n", buf_list, strerror(errno), ret);
        return -1;
    }

    if (strstr(flags, "WPA-PSK") || strstr(flags, "WPA2-PSK")) {
        pap_info->security = WIFI_STA_SECURITY_WPA_WPA2_PSK;
    } else if (strstr(flags, "WPA-EAP") || strstr(flags, "WPA2-EAP")) {
        pap_info->security = WIFI_STA_SECURITY_WPA_WPA2_EAP;
    } else if (strstr(flags, "WEP")) {
        pap_info->security = WIFI_STA_SECURITY_WEP;
    } else if (strstr(flags, "WAPI-CERT")) {
        pap_info->security = WIFI_STA_SECURITY_WAPI_CERT;
    } else if (strstr(flags, "WAPI-PSK")) {
        pap_info->security = WIFI_STA_SECURITY_WAPI_PSK;
    } else {
        pap_info->security = WIFI_STA_SECURITY_OPEN;
    }

    if (strstr(flags, "CCMP") && strstr(flags, "TKIP")) {
        pap_info->alg_type = WIFI_STA_ALG_CCMP_TKIP;
    } else if (strstr(flags, "CCMP")) {
        pap_info->alg_type = WIFI_STA_ALG_CCMP;
    } else if (strstr(flags, "TKIP")) {
        pap_info->alg_type = WIFI_STA_ALG_TKIP;
    } else {
        pap_info->alg_type = WIFI_STA_ALG_BOTTON;
    }

    return 0;
}


static int wpa_parse_event(const char *event, WIFI_STA_EVENT_E *psta_event)
{
    char *pret = NULL;

    if (NULL == event) {
        STA_ERR_PRT("Input event is NULL!\n");
        return -1;
    }

    pret = strstr(event, "CTRL-EVENT-");
    if (NULL == pret) {
        return -1;
    }

    if (strstr(event, WPA_EVENT_SCAN_RESULTS)) {
        *psta_event = WIFI_STA_EVENT_SCAN_END;
    } else if (strstr(event, WPA_EVENT_CONNECTED)) {
        *psta_event = WIFI_STA_EVENT_CONNECTED;
    } else if (strstr(event, WPA_EVENT_DISCONNECTED)) {
        *psta_event = WIFI_STA_EVENT_DISCONNECTED;
    } else if (strstr(event, WPA_EVENT_TERMINATING)) {
        *psta_event = WIFI_STA_EVENT_SUPP_STOPPED;
    } else {
        *psta_event = WIFI_STA_EVENT_BOTTON;
        //STA_ERR_PRT("Unknow event:%s !\n", event);
        return -1;
    }

    return 0;
}


static void wpa_event_callback(WIFI_STA_EVENT_E event, void *pdata)
{
    STA_DB_PRT("wpa_event_callback  event:%d \n", event);
}


static void * sta_event_proccess(void *para)
{
    int  ret       = 0;
    int  len       = 0;
    char buf[1024] = {0};
    struct wpa_ctrl   *pst_wpaCtl = NULL;
    WIFI_STA_EVENT_E   sta_event  = WIFI_STA_EVENT_BOTTON;

    prctl(PR_SET_NAME, "sta_event_proccess", 0, 0, 0);

    pthread_mutex_lock(&g_operate_lock);
    g_sta_opt_status |= STA_STATUS_STARTED;
    pthread_mutex_unlock(&g_operate_lock);

    while ((g_sta_opt_status & STA_STATUS_STARTED)) {
        pst_wpaCtl = wpa_ctrl_open(WPA_SUP_CFG_PATH);
        if (NULL == pst_wpaCtl) {
            STA_ERR_PRT("Do wpa_ctrl_open error! PATH:%s\n", WPA_SUP_CFG_PATH);
            sleep(2);
            continue;
        }

        ret = wpa_ctrl_attach(pst_wpaCtl);
        if (ret) {
            STA_ERR_PRT("Do wpa_ctrl_attach error! ret:%d\n", ret);
            wpa_ctrl_close(pst_wpaCtl);
            pst_wpaCtl = NULL;
            sleep(2);
            continue;
        }

        while ((g_sta_opt_status & STA_STATUS_STARTED)) {
            ret = wpa_ctrl_pending(pst_wpaCtl, 2, 0);

            /* client slecet to wpa error. */
            if (ret < 0) {
                STA_ERR_PRT("select recv error:%s! ret:%d\n", strerror(errno), ret);
                break;
            }

            /* client slecet to wpa time out. */
            if(0 == ret) {
                //STA_DB_PRT("[LINE]:%d  Select send timeout!\n");
                sleep(1);
                continue;
            }

            len = sizeof(buf) - 1;
            memset(buf, 0, sizeof(buf));
            ret = wpa_ctrl_recv(pst_wpaCtl, buf, &len);
            if (ret < 0) {
                STA_ERR_PRT("Do wpa_ctrl_recv error! ret:%d\n", ret);
                break;
            }

            ret = wpa_parse_event(buf, &sta_event);
            if (ret) {
                //STA_ERR_PRT("Do wpa_ctrl_parse_event error! buf:%s\n", buf);
                continue;
            }

            pthread_mutex_lock(&g_scan_lock);
            if (WIFI_STA_EVENT_SCAN_END == sta_event) {
                g_scan_status = WIFI_STA_STATUS_SCAN_END;
            }
            pthread_mutex_unlock(&g_scan_lock);

            if (WIFI_STA_EVENT_BOTTON != sta_event) {
                if (NULL != g_fcb) {
                    g_fcb(sta_event, g_sta_pdata);
                }
            }
            /* No need mutex lock it */
            g_sta_event = sta_event;

            STA_DB_PRT("wpa_event:%s  sta_event:%d\n", buf, sta_event);
        }

        if (NULL != pst_wpaCtl) {
            wpa_ctrl_close(pst_wpaCtl);
            pst_wpaCtl = NULL;
        }
        sleep(2);
    }

    if (NULL != g_fcb) {
        g_fcb(WIFI_STA_EVENT_DOWN, g_sta_pdata);
    }

    STA_DB_PRT(" Wifi sta proccess end!\n");

    return NULL;
}


int wifi_sta_init(void)
{
    int ret = 0;
    char  buf[1024] = {0};

    if ((g_sta_opt_status & STA_STATUS_INITED)) {
        STA_DB_PRT("The wifi sta mode have inited yet!\n");
        return 0;
    }

    pthread_mutex_init(&g_scan_lock, NULL);
    pthread_mutex_init(&g_operate_lock, NULL);

    /* step 1.  Exit wpa_supplicant sevice proccess */
    system(CMD_KILL_WPA);
    usleep(2*1000);

    /* step 2.  load wifi sta driver. */
    memset(g_dev_name, 0, sizeof(g_dev_name));
    if ((ret = access("/etc/firmware/ap6255", F_OK)) == 0) {
        strcpy(g_dev_name, "ap6255");
    } else if ((ret = access("/etc/firmware/ap6335", F_OK)) == 0) {
        strcpy(g_dev_name, "ap6335");
    }else if((ret = access("/etc/firmware/ap6181", F_OK)) == 0){
        strcpy(g_dev_name, "ap6181");
    }else if((ret = access("/etc/firmware/xr819", F_OK)) == 0){
        strcpy(g_dev_name, "xr819");
    }

    memset(buf, 0, sizeof(buf));
    ret = snprintf(buf, sizeof(buf)-1, CMD_STA_LOAD, g_dev_name);
    if (ret <= 0) {
        STA_ERR_PRT ("Do snprintf CMD_STA_LOAD fail! ret:%d\n", ret);
        return -1;
    }
    system(buf);

    /* step 3.  check wifi sta driver is station mode? if not change it. */
    g_sta_opt_status  = 0;
    g_sta_opt_status |= STA_STATUS_INITED;

    return 0;
}


int wifi_sta_exit(void)
{
    int ret = 0;
    char  buf[1024] = {0};

    if (0 == g_sta_opt_status) {
        STA_DB_PRT ("The wifi sta mode have exit yet!\n");
        return 0;
    }

    g_sta_opt_status  = 0;
    /* step 1.  Unload this wifi sta driver */
    memset(g_dev_name, 0, sizeof(g_dev_name));
    if ((ret = access("/etc/firmware/ap6255", F_OK)) == 0) {
        strcpy(g_dev_name, "ap6255");
    } else if ((ret = access("/etc/firmware/ap6335", F_OK)) == 0) {
        strcpy(g_dev_name, "ap6335");
    }else if((ret = access("/etc/firmware/ap6181", F_OK)) == 0){
        strcpy(g_dev_name, "ap6181");
    }

    memset(buf, 0, sizeof(buf));
    ret = snprintf(buf, sizeof(buf)-1, CMD_STA_UNLOAD, g_dev_name);
    if (ret <= 0) {
        STA_ERR_PRT ("Do snprintf CMD_STA_UNLOAD fail! ret:%d\n", ret);
        return -1;
    }
    system(buf);

    pthread_mutex_destroy(&g_scan_lock);
    pthread_mutex_destroy(&g_operate_lock);

    return 0;
}


int wifi_sta_open(const char * wifi_name)
{
    int   ret       = 0;
    int   cnt       = 0;
    char  buf[1024] = {0};

    /* step 1.  Check input para */
    if (NULL == wifi_name) {
        STA_ERR_PRT ("Input wifi_name is NULL!\n");
        return -1;
    }

    /* step 2.  Update operate status*/
    pthread_mutex_lock(&g_operate_lock);
    if ((g_sta_opt_status & STA_STATUS_OPENDED)) {
        STA_DB_PRT ("The wifi sta mode have opened yet!\n");
        pthread_mutex_unlock(&g_operate_lock);
        return 0;
    }

    memset(buf, 0, sizeof(buf));
    ret = snprintf(buf, sizeof(buf)-1, CMD_WPA_RUN, wifi_name, WPA_SUP_CFG_FILE);
    if (ret <= 0) {
        STA_ERR_PRT ("Do snprintf CMD_WPA_RUN fail! ret:%d\n", ret);
        pthread_mutex_unlock(&g_operate_lock);
        return -1;
    }
    int i;
    for (i = 3; i < getdtablesize(); i++) {
        int flags;
        if ((flags = fcntl(i, F_GETFD)) != -1)
            fcntl(i, F_SETFD, flags | FD_CLOEXEC);
    }
    system(buf);

    cnt = 40;
    while (cnt--) {
        ret = access(WPA_SUP_CFG_PATH, 0);
        if (0 == ret) {
            break;
        }
        usleep(200 * 1000); /* 200ms */
    }
    if (cnt <= 0) {
        STA_ERR_PRT ("Create wpa_supplicant time out!\n");
        pthread_mutex_unlock(&g_operate_lock);
        return -1;
    }

    STA_DB_PRT("Create wpa_supplicant success! cnt:%d\n", cnt);

    g_sta_opt_status |= STA_STATUS_OPENDED;
    pthread_mutex_unlock(&g_operate_lock);

    return 0;
}


int wifi_sta_close(const char * wifi_name)
{
    /* step 1.  Check input para */
    if (NULL == wifi_name) {
        STA_ERR_PRT ("Input wifi_name is NULL!\n");
        return -1;
    }

    /* step 2.  Update operate status */
    pthread_mutex_lock(&g_operate_lock);
    if (0 == (g_sta_opt_status & STA_STATUS_OPENDED)) {
        STA_ERR_PRT ("The wifi sta mode have closed yet!\n");
        pthread_mutex_unlock(&g_operate_lock);
        return 0;
    }

    /* step 3.  Exit wpa_supplicant sevice proccess */
    system(CMD_KILL_WPA);
    usleep(2*1000);

    g_sta_opt_status &= STA_STATUS_CLOSED;
    pthread_mutex_unlock(&g_operate_lock);

    return 0;
}


int wifi_sta_start(const char * wifi_name)
{
    int            ret = 0;
    pthread_t      thread_id;
    pthread_attr_t attr;

    /* step 1.  Check input para */
    if (NULL == wifi_name) {
        STA_ERR_PRT ("Input wifi_name is NULL!\n");
        return -1;
    }

    pthread_mutex_lock(&g_operate_lock);
    if ((g_sta_opt_status & STA_STATUS_STARTED)) {
        STA_DB_PRT ("The %s already start!\n", wifi_name);
        pthread_mutex_unlock(&g_operate_lock);
        return 0;
    }
    pthread_mutex_unlock(&g_operate_lock);

    /* step 2.  Creat sta server proccess  */
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&thread_id, &attr, sta_event_proccess, NULL);
    if (ret) {
        STA_ERR_PRT ("Create pthread fail! ret:%d  error:%s\n", ret, strerror(errno));
        pthread_attr_destroy(&attr);
		return ret;
    }
    pthread_attr_destroy(&attr);
    usleep(10);

    /* step 3.  Creat wpa_cli connect wpa_supplicant, for operate wifi. */

    return 0;
}


int wifi_sta_stop(const char *wifi_name)
{
    /* step 1.  Check input para */
    if (NULL == wifi_name) {
        STA_ERR_PRT (" Input wifi_name is NULL!\n");
        return -1;
    }

    pthread_mutex_lock(&g_operate_lock);
    g_sta_opt_status &= STA_STATUS_STOPED;
    pthread_mutex_unlock(&g_operate_lock);

    /* step 2.  Exit wpa_cli connect. */


    /* step 3.  Exit the sta server proccess. */

    return 0;
}


int wifi_sta_start_scan(const char *wifi_name)
{
    int ret = 0;

    /* step 1.  Check input para */
    if (NULL == wifi_name) {
        STA_ERR_PRT (" Input wifi_name is NULL!\n");
        return -1;
    }

    pthread_mutex_lock(&g_scan_lock);
    if (WIFI_STA_STATUS_SCANING == g_scan_status) {
        STA_DB_PRT ("SCANING!\n");
        pthread_mutex_unlock(&g_scan_lock);
        return 0;
    } else {
        g_scan_status  = WIFI_STA_STATUS_SCANING;
    }
    pthread_mutex_unlock(&g_scan_lock);

    /* step 2.  Send scan cmd to wpa server */
    ret = wpa_cli_cmd("SCAN");
    if (ret < 0) {
        STA_ERR_PRT ("Do cmd:SCAN fail! ret:%d\n", ret);
    }

    /* step 3.  Wait scan results */

    return ret;
}


int wifi_sta_get_scan_status(const char *wifi_name, WIFI_STA_STATUS_E *pscan_status)
{
    int ret = 0;

    /* step 1.  Check input para */
    if (NULL == wifi_name || NULL == pscan_status) {
        STA_ERR_PRT (" Input wifi_name or pscan_status is NULL!\n");
        return -1;
    }

    ret = 0;
    pthread_mutex_lock(&g_scan_lock);
    if (WIFI_STA_STATUS_SCAN_END != g_scan_status &&
        WIFI_STA_STATUS_SCANING  != g_scan_status) {
        //g_scan_status = WIFI_STA_STATUS_BOTTON;
        ret = -1;
    }
    *pscan_status = g_scan_status;
    pthread_mutex_unlock(&g_scan_lock);

    return ret;
}


int wifi_sta_get_scan_results(const char *wifi_name, WIFI_STA_AP_LIST_S *pap_list)
{
    int         ret         = 0;
    int         cnt         = 0;
    int         i           = 0;
    char        buf[4096]   = {0};
    char        line[512]   = {0};
    size_t      len         = 0;
    const char *cmd         = "SCAN_RESULTS";
    struct wpa_ctrl    *pst_wpaCtl = NULL;
    WIFI_STA_AP_INFO_S  ap_info;

    if (NULL == wifi_name || NULL == pap_list) {
        STA_ERR_PRT(" Input psta_status is NULL!\n");
        return -1;
    }

    memset(pap_list, 0, sizeof(WIFI_STA_AP_LIST_S));
    pthread_mutex_lock(&g_scan_lock);
    if (WIFI_STA_STATUS_SCAN_END != g_scan_status) {
        ret = 0;
    }
    pthread_mutex_unlock(&g_scan_lock);

    pst_wpaCtl = wpa_ctrl_open(WPA_SUP_CFG_PATH);
    if (NULL == pst_wpaCtl) {
        STA_ERR_PRT(" Do wpa_ctrl_open error! PATH:%s\n", WPA_SUP_CFG_PATH);
        return -1;
    }

    memset(buf, 0, sizeof(buf));
    len = sizeof(buf) - 1;
    ret = wpa_ctrl_request(pst_wpaCtl, cmd, strlen(cmd),
                            buf, &len, NULL);
    if (-2 == ret) {
        STA_ERR_PRT(" Do wpa_ctrl_request time out! cmd:%s ret:%d\n", cmd, ret);
        goto GET_SCAN_EXIT;
    } else if (ret < 0) {
        STA_ERR_PRT(" Do wpa_ctrl_request error! cmd:%s ret:%d\n", cmd, ret);
        goto GET_SCAN_EXIT;
    }

    while (cnt < len && cnt < sizeof(buf)-1) {
        memset(line, 0, sizeof(line));
        for(i = 0; ; i++, cnt++) {
            if (0xa == buf[cnt] || \
                i   >= sizeof(line) || \
                cnt >= len || \
                cnt >= sizeof(buf)-1) {
                break;
            }
            line[i] = buf[cnt];
        }
        cnt++;

        memset(&ap_info, 0, sizeof(WIFI_STA_AP_INFO_S));
        ret = wpa_parse_apinfo(line, &ap_info);
        if (ret) {
            continue;
        }

        memcpy(&(pap_list->ap_list[pap_list->ap_list_num]), &ap_info, sizeof(WIFI_STA_AP_INFO_S));
        pap_list->ap_list_num++;
        if (pap_list->ap_list_num >= MAX_AP_LIST_SIZE) {
            break;
        }
    }

    for (cnt = 0; cnt < pap_list->ap_list_num; cnt++) {
        STA_DB_PRT("wifi_sta_get_scan_results cnt:%d  ap_list_num:%d  bssid:%s  db:%d  freq:%d  security:%d  alg_type:%d  ssid:%s \n",
                cnt, pap_list->ap_list_num, pap_list->ap_list[cnt].bssid, pap_list->ap_list[cnt].db, pap_list->ap_list[cnt].frequency,
                pap_list->ap_list[cnt].security, pap_list->ap_list[cnt].alg_type,pap_list->ap_list[cnt].ssid);
    }

    ret = 0;
GET_SCAN_EXIT:
    wpa_ctrl_close(pst_wpaCtl);
    return ret;
}


int wifi_sta_connect(const char *wifi_name, WIFI_STA_AP_INFO_S *pap_info)
{
    int  ret        = 0;
    int  net_id     = -1;
    char line[256]  = {0};

    /* step 1.  Check input para */
    if (NULL == wifi_name || NULL == pap_info) {
        STA_ERR_PRT(" Input wifi_name or pap_info is NULL!\n");
        return -1;
    }

    /* step 2.  Send cmd:ADD_NETWORK   to wpa. guixing */
    ret = wpa_cli_add_network();
    if (ret < 0) {
        STA_ERR_PRT(" Do wpa_cli_add_network fail! ret:%d\n", ret);
        return -1;
    }
    net_id = ret;
    STA_DB_PRT("Do wpa_cli_add_network id:%d \n", ret);

    /* step 3. Set network[ID] dst ap ssid. */
    memset(line, 0, sizeof(line));
    snprintf(line, sizeof(line)-1, "SET_NETWORK %d ssid \"%s\"", net_id, pap_info->ssid);
    ret = wpa_cli_cmd(line);
    if (ret < 0) {
        STA_ERR_PRT(" Do wpa_cli_add_network fail! ret:%d\n", ret);
        return -1;
    }

    /* step 4. Set network[ID] dst ap key and password. */
    switch (pap_info->security) {
        case WIFI_STA_SECURITY_OPEN:
            memset(line, 0, sizeof(line));
            snprintf(line, sizeof(line)-1, "SET_NETWORK %d key_mgmt NONE", net_id);
            ret = wpa_cli_cmd(line);
            if (ret < 0) {
                STA_ERR_PRT(" Do %s fail! ret:%d\n", line, ret);
                return -1;
            }
            break;

        case WIFI_STA_SECURITY_WPA_WPA2_PSK:
            memset(line, 0, sizeof(line));
            snprintf(line, sizeof(line)-1, "SET_NETWORK %d psk \"%s\"", net_id, pap_info->psswd);
            ret = wpa_cli_cmd(line);
            if (ret < 0) {
                STA_ERR_PRT(" Do %s fail! ret:%d\n", line, ret);
                return -1;
            }
            break;

        case WIFI_STA_SECURITY_WPA_WPA2_EAP:
            memset(line, 0, sizeof(line));
            snprintf(line, sizeof(line)-1, "SET_NETWORK %d eap \"%s\"", net_id, pap_info->psswd);
            ret = wpa_cli_cmd(line);
            if (ret < 0) {
                STA_ERR_PRT(" Do %s fail! ret:%d\n", line, ret);
                return -1;
            }
            break;

        case WIFI_STA_SECURITY_WEP:
            memset(line, 0, sizeof(line));
            snprintf(line, sizeof(line)-1, "SET_NETWORK %d key_mgmt NONE", net_id);
            ret = wpa_cli_cmd(line);
            if (ret < 0) {
                STA_ERR_PRT(" Do %s fail! ret:%d\n", line, ret);
                return -1;
            }

            memset(line, 0, sizeof(line));
            snprintf(line, sizeof(line)-1, "SET_NETWORK %d auth_alg OPEN SHARED", net_id);
            ret = wpa_cli_cmd(line);
            if (ret < 0) {
                STA_ERR_PRT(" Do %s fail! ret:%d\n", line, ret);
                return -1;
            }

            memset(line, 0, sizeof(line));
            snprintf(line, sizeof(line)-1, "SET_NETWORK %d wep_key0 \"%s\"", net_id, pap_info->psswd);
            ret = wpa_cli_cmd(line);
            if (ret < 0) {
                STA_ERR_PRT(" Do %s fail! ret:%d\n", line, ret);
                return -1;
            }

            memset(line, 0, sizeof(line));
            snprintf(line, sizeof(line)-1, "SET_NETWORK %d wep_tx_keyidx \"0\"", net_id);
            ret = wpa_cli_cmd(line);
            if (ret < 0) {
                STA_ERR_PRT(" Do %s fail! ret:%d\n", line, ret);
                return -1;
            }
            break;

        default:
            STA_ERR_PRT(" Don't support this security:[%d]\n", pap_info->security);
            return -1;
            break;
    }

    /* step 5. Enable network[ID]. */
    memset(line, 0, sizeof(line));
    snprintf(line, sizeof(line), "ENABLE_NETWORK %d", net_id);
    ret = wpa_cli_cmd(line);
    if (ret < 0) {
        STA_ERR_PRT(" Do %s fail! ret:%d\n", line, ret);
        return -1;
    }

    /* step 6. Do RECONNECT , like reassociate, but only takes effect if already disconnected. */
    ret = wpa_cli_cmd("RECONNECT");
    if (ret < 0) {
        STA_ERR_PRT(" Do %s fail! ret:%d\n", line, ret);
    }

    pthread_mutex_lock(&g_operate_lock);
    g_sta_net_id = net_id;
    pthread_mutex_unlock(&g_operate_lock);

    return 0;
}


int wifi_sta_disconnect(const char *wifi_name)
{
    int  ret        = 0;
    char line[256]  = {0};

    /* step 1.  Check input para */
    if (NULL == wifi_name) {
        STA_ERR_PRT("Input wifi_name is NULL!\n");
        return -1;
    }

    /* step 2.  Disconnect the ap */
    ret = wpa_cli_cmd("DISCONNECT");
    if (ret < 0) {
        STA_ERR_PRT("Do DISCONNECT fail! ret:%d\n", ret);
        return -1;
    }

    /* step 3.  Delete the network [ID] */
    pthread_mutex_lock(&g_operate_lock);
    memset(line, 0, sizeof(line));
    snprintf(line, sizeof(line), "REMOVE_NETWORK %d", g_sta_net_id);
    ret = wpa_cli_cmd(line);
    if (ret < 0) {
        STA_ERR_PRT("Do DISCONNECT fail! ret:%d\n", ret);
        pthread_mutex_unlock(&g_operate_lock);
        return -1;
    }
    g_sta_net_id = -1;
    pthread_mutex_unlock(&g_operate_lock);

    return 0;
}


int wifi_sta_get_connect_status(const char *wifi_name, WIFI_STA_CONNECT_STATUS_S *pconnect_status)
{
    int         ret         = 0;
    int         cnt         = 0;
    int         i           = 0;
    char       *pline       = NULL;
    char        buf[1024]   = {0};
    char        line[512]   = {0};
    char        val[128]    = {0};
    size_t      len         = 0;
    struct wpa_ctrl *pst_wpaCtl = NULL;

    /* step 1.  Check input para */
    if (NULL == wifi_name || NULL == pconnect_status) {
        STA_ERR_PRT("Input wifi_name or pconnect_status is NULL!\n");
        return -1;
    }

    pst_wpaCtl = wpa_ctrl_open(WPA_SUP_CFG_PATH);
    if (NULL == pst_wpaCtl) {
        STA_ERR_PRT("Do wpa_ctrl_open error! PATH:%s\n", WPA_SUP_CFG_PATH);
        return -1;
    }

    len = sizeof(buf) - 1;
    ret = wpa_ctrl_request(pst_wpaCtl, "STATUS", strlen("STATUS"),
                            buf, &len, NULL);
    if (-2 == ret) {
        STA_ERR_PRT("Do wpa_ctrl_request time out! cmd:STATUS ret:%d\n", ret);
    } else if (ret < 0) {
        STA_ERR_PRT("Do wpa_ctrl_request error! cmd:STATUS ret:%d\n", ret);
    }

    wpa_ctrl_close(pst_wpaCtl);

    STA_DB_PRT(" buf:\n%s len:%d\n", buf, len);

    while (cnt < len && cnt < sizeof(buf)-1) {
        memset(line, 0, sizeof(line));
        memset(val,  0, sizeof(val));
        for(i = 0; ; i++, cnt++) {
            if (0xa == buf[cnt] || \
                i   >= sizeof(line) || \
                cnt >= len || \
                cnt >= sizeof(buf)-1) {
                break;
            }
            line[i] = buf[cnt];
        }
        cnt++;

        /* Search wifi sta status */
        if(strstr(line, "wpa_state=COMPLETED") == 0) {
            pconnect_status->state = WIFI_STA_STATUS_CONNECTED;
        } else if(strstr(line, "wpa_state=DISCONNECTED") == 0) {
            pconnect_status->state = WIFI_STA_STATUS_DISCONNECTED;
        } else if(strstr(line, "wpa_state=INACTIVE") == 0) {
            pconnect_status->state = WIFI_STA_STATUS_INACTIVE;
        } else if (strstr(line, "wpa_state=SCANNING") == 0
    		|| strstr(line, "wpa_state=4WAY_HANDSHAKE") == 0
    		|| strstr(line, "wpa_state=GROUP_HANDSHAKE") == 0
    		|| strstr(line, "wpa_state=ASSOCIATED") == 0
    		|| strstr(line, "wpa_state=ASSOCIATING") == 0){
            pconnect_status->state = WIFI_STA_STATUS_CONNECTING;
        } /* search wifi sta connect info */
        else if (strstr(line, "bssid=")) {
            sscanf(line, "bssid=%s", pconnect_status->ap_info.bssid);
        } else if (strstr(line, "ssid=")) {
            sscanf(line, "ssid=%s", pconnect_status->ap_info.ssid);
        } else if (strstr(line, "key_mgmt=WPA-PSK")) {
            pconnect_status->ap_info.security = WIFI_STA_SECURITY_WPA_WPA2_PSK;
        } else if (strstr(line, "key_mgmt=WPA-PSK")) {
            pconnect_status->ap_info.security = WIFI_STA_SECURITY_WPA_WPA2_PSK;
        }
    }

    return ret;
}


int wifi_sta_register_eventcall(const char *wifi_name, WIFI_STA_EVENT_CALLBACK event_callback, void *pdata)
{
    if (NULL == wifi_name || NULL == event_callback) {
        STA_ERR_PRT ("Input wifi_name or event_callback is NULL!\n");
        return -1;
    }

    pthread_mutex_lock(&g_operate_lock);
    g_fcb       = event_callback;
    g_sta_pdata = pdata;
    pthread_mutex_unlock(&g_operate_lock);

    return 0;
}


int wifi_sta_unregister_eventcall(const char *wifi_name)
{
    if (NULL == wifi_name) {
        STA_ERR_PRT ("Input wifi_name is NULL!\n");
        return -1;
    }

    pthread_mutex_lock(&g_operate_lock);
    g_fcb       = NULL;
    g_sta_pdata = NULL;
    pthread_mutex_unlock(&g_operate_lock);

    return 0;
}


int wifi_sta_get_event(const char *wifi_name, WIFI_STA_EVENT_E *psta_event)
{
    if (NULL == wifi_name || NULL == psta_event) {
        STA_ERR_PRT ("Input wifi_name or psta_event is NULL!\n");
        return -1;
    }

    /* No need mutex lock */
    *psta_event = g_sta_event;

    return 0;
}


int wifi_sta_do_dhcp(const char *wifi_name)
{
    int  ret      = 0;
    char cmd[256] = {0};

    /* step 1.  Check input para */
    if (NULL == wifi_name) {
        STA_ERR_PRT ("Input wifi_name is NULL!\n");
        return -1;
    }

    /* step 2.  Kill old udhcpc process. */
    ret = system("killall udhcpc");

    /* step 3.  Creat new udhcpc process for wlan0 wifi deviece. */
    snprintf(cmd, sizeof(cmd)-1, "udhcpc -i %s -b -R", wifi_name);
    ret = system(cmd);

    return 0;
}


int main_wifi_sta_test()
{
    int ret = 0;
    int cnt = 0;
    int i   = 0;
    WIFI_STA_STATUS_E   sta_status;
    WIFI_STA_STATUS_E   scan_status;
    WIFI_STA_AP_LIST_S  ap_list;
    WIFI_STA_EVENT_E    wpa_event;

    ret = wifi_sta_init();
    if (ret) {
        STA_ERR_PRT ("Do wifi_sta_init fail! ret:%d\n", ret);
        return -1;
    }

    ret = wifi_sta_open("wlan0");
    if (ret) {
        STA_ERR_PRT ("Do wifi_sta_open fail! ret:%d\n", ret);
        return -1;
    }

    ret = wifi_sta_start("wlan0");
    if (ret) {
        STA_ERR_PRT ("Do wifi_sta_start fail! ret:%d\n", ret);
        return -1;
    }

    sleep(1);

    ret = wifi_sta_register_eventcall("wlan0", wpa_event_callback, NULL);
    if (ret) {
        STA_ERR_PRT ("Do wifi_sta_register_enventcall fail! ret:%d\n", ret);
        return -1;
    }

    ret = wifi_sta_start_scan("wlan0");
    if (ret) {
        STA_ERR_PRT ("Do wifi_sta_start_scan fail! ret:%d\n", ret);
        return -1;
    }

    while (1) {
        ret = wifi_sta_get_scan_status("wlan0", &scan_status);
        if (ret) {
            STA_ERR_PRT ("Do wifi_sta_get_scan_status fail! ret:%d\n", ret);
            sleep(1);
            continue;
        }

        if (WIFI_STA_STATUS_SCANING == scan_status) {
            STA_DB_PRT ("Waitting scaning......\n");
            sleep(1);
            continue;
        } else if (WIFI_STA_STATUS_SCAN_END == scan_status) {
            STA_DB_PRT ("Scan AP list is end!\n");
            break;
        }
    }

    ret = wifi_sta_get_scan_results("wlan0", &ap_list);
    if (ret) {
        STA_ERR_PRT ("Do wifi_sta_get_scan_results fail! ret:%d\n", ret);
        return -1;
    }

    sleep(1);

    WIFI_STA_AP_INFO_S ap_info;
    memset(&ap_info, 0, sizeof(ap_info));
    //strcpy(ap_info.ssid, "PD2-IPC-test");
    //ap_info.security = WIFI_STA_SECURITY_OPEN;
    strcpy(ap_info.ssid, "BU3-PD2-IPC");
    strcpy(ap_info.psswd, "awt.1235");
    ap_info.security = WIFI_STA_SECURITY_WPA_WPA2_PSK;

    ret = wifi_sta_connect("wlan0", &ap_info);
    if (ret) {
        STA_ERR_PRT ("Do wifi_sta_connect fail! ret:%d\n", ret);
        return -1;
    }

    cnt = 8;
    while (cnt--) {
        ret = wifi_sta_get_event("wlan0", &wpa_event);
        if (ret) {
            STA_ERR_PRT ("Do wifi_sta_get_event fail! ret:%d\n", ret);
        }
        STA_DB_PRT ("\n cnt:%d  Do wifi_sta_get_event :%d \n", cnt, wpa_event);
        sleep(8);

        ret = wpa_cli_get_status(&sta_status);
        if (ret) {
            STA_ERR_PRT ("Do wpa_cli_get_status fail! ret:%d\n", ret);
        }
        STA_DB_PRT ("cnt:%d  Do wpa_cli_get_status :%d \n\n", cnt, sta_status);
        sleep(8);
    }

    ret = wifi_sta_disconnect("wlan0");
    if (ret) {
        STA_ERR_PRT ("Do wifi_sta_disconnect fail! ret:%d\n", ret);
        return -1;
    }

    wifi_sta_unregister_eventcall("wlan0");

    wifi_sta_stop("wlan0");

    wifi_sta_close("wlan0");

    wifi_sta_exit();

    sleep(10);

    return 0;
}

