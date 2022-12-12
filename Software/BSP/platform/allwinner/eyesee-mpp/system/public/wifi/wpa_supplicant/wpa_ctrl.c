/* *******************************************************************************
 * Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 * *******************************************************************************/
/**
 * @file wpa_ctrl.c
 * @brief  Control  wpa_supplicant moudel
 * @author id:
 * @version v0.1
 * @date 2016-08-28
 */

/******************************************************************************/
/*                             Include Files                                  */
/******************************************************************************/
#include "wpa_ctrl.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/types.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>


/******************************************************************************/
/*                           Macros & Typedefs                                */
/******************************************************************************/
/* None */


/******************************************************************************/
/*                         Structure Declarations                             */
/******************************************************************************/
/* None */


/******************************************************************************/
/*                            Global Variables                                */
/******************************************************************************/
/* None */


/******************************************************************************/
/*                          Function Declarations                             */
/******************************************************************************/
/* None */


/******************************************************************************/
/*                          Function Definitions                              */
/******************************************************************************/
struct wpa_ctrl * wpa_ctrl_open(const char *ctrl_path)
{
    int    ret   = 0;
    int    tries = 0;
    int    flags = 0;
    size_t res   = 0;
    static int       counter = 0;
    struct wpa_ctrl *ctrl = NULL;

    ctrl = malloc(sizeof(struct wpa_ctrl));
    if (NULL == ctrl) {
        printf("[FUN]:%s  [LINE]:%d  Input wifi_name or pscan_status is NULL!\n",
                                    __func__, __LINE__);
        return NULL;
    }
    memset(ctrl, 0 , sizeof(struct wpa_ctrl));

    ctrl->s = socket(PF_UNIX, SOCK_DGRAM, 0);
    if (ctrl->s < 0) {
        printf("[FUN]:%s  [LINE]:%d  Fail to create PF_UNIX socket! errno[%d] errinfo[%s]\n",
                                    __func__, __LINE__, errno, strerror(errno));
        free(ctrl);
        return NULL;
    }
    ctrl->local.sun_family = AF_UNIX;
    counter++;

    do {
        ret = snprintf(ctrl->local.sun_path, sizeof(ctrl->local.sun_path),
                        "/tmp/wpa_ctrl_%d-%d", getpid(), counter);
        if (ret < 0 || (unsigned int) ret >= sizeof(ctrl->local.sun_path)) {
            close(ctrl->s);
            free(ctrl);
            return NULL;
        }
        tries++;

        unlink(ctrl->local.sun_path);
        ret = bind(ctrl->s, (struct sockaddr *)&ctrl->local, sizeof(ctrl->local));
        if (ret < 0) {
            printf("[FUN]:%s  [LINE]:%d  Fail to bind UNIX:%s socket! tries[%d] EADDRINUSE[%d] ret[%d] errno[%d] errinfo[%s]\n",
                                    __func__, __LINE__, ctrl->local.sun_path, tries, EADDRINUSE, ret, errno, strerror(errno));
            if (EADDRINUSE == errno && tries < 4) {
                printf("[FUN]:%s [LINE]:%d  Do bind next. tries:%d \n", __func__, __LINE__, tries);
                /*
                * getpid() returns unique identifier for this instance
                * of wpa_ctrl, so the existing socket file must have
                * been left by unclean termination of an earlier run.
                * Remove the file and try again.
                */
                unlink(ctrl->local.sun_path);
                usleep(10);
                continue;
            }
            close(ctrl->s);
            free(ctrl);
            return NULL;
        }
        printf("[FUN]:%s [LINE]:%d bind  UNIX:%s socket OK! ret:%d\n", __func__, __LINE__, ctrl->local.sun_path, ret);
        break;
    } while (1);

    ctrl->dest.sun_family = AF_UNIX;
    if (strlen(ctrl_path) >= sizeof(ctrl->dest.sun_path)) {
        printf("[FUN]:%s [LINE]:%d  error! ctrl_path:%s  sun_path:%s \n",
                                                    __func__, __LINE__, ctrl_path, ctrl->dest.sun_path);
        close(ctrl->s);
        free(ctrl);
        return NULL;
    }

    memset(ctrl->dest.sun_path, 0, sizeof(ctrl->dest.sun_path));
    strncpy(ctrl->dest.sun_path, ctrl_path, sizeof(ctrl->dest.sun_path));

    ret = connect(ctrl->s, (struct sockaddr *) &ctrl->dest, sizeof(ctrl->dest));
    if (ret < 0) {
        printf("[FUN]:%s  [LINE]:%d  Fail connect! ret:%d errno[%d] errinfo[%s]\n",
                                        __func__, __LINE__, ret, errno, strerror(errno));
        close(ctrl->s);
        unlink(ctrl->local.sun_path);
        free(ctrl);
        return NULL;
    }

    return ctrl;
}


void wpa_ctrl_close(struct wpa_ctrl *ctrl)
{
    unlink(ctrl->local.sun_path);
    close(ctrl->s);
    free(ctrl);
}


int wpa_ctrl_request(struct wpa_ctrl *ctrl, const char *cmd, size_t cmd_len,
                            char *reply, size_t *reply_len,
                            void (*msg_cb)(char *msg, size_t len))
{
    int   res = 0;
    const char *_cmd = NULL;
    fd_set rfds;
    size_t _cmd_len;
    struct timeval tv;

    _cmd     = cmd;
    _cmd_len = cmd_len;
    if (send(ctrl->s, _cmd, _cmd_len, 0) < 0) {
        printf("[FUN]:%s  [LINE]:%d  Fail connect! errno[%d] errinfo[%s]\n", __func__, __LINE__,
                errno, strerror(errno));
        return -1;
    }

    for (;;) {
        tv.tv_sec  = 10;
        tv.tv_usec = 0;
        FD_ZERO(&rfds);
        FD_SET(ctrl->s, &rfds);
        res = select(ctrl->s + 1, &rfds, NULL, NULL, &tv);
        if (res < 0) {
            return res;
        }

        if (FD_ISSET(ctrl->s, &rfds)) {
            res = recv(ctrl->s, reply, *reply_len, 0);
            if (res < 0) {
                printf("[FUN]:%s  [LINE]:%d  Fail connect! errno[%d] errinfo[%s]\n",
                                    __func__, __LINE__, errno, strerror(errno));
                return res;
            }

            if (res > 0 && reply[0] == '<') {
                /* This is an unsolicited message from
                * wpa_supplicant, not the reply to the
                * request. Use msg_cb to report this to the
                * caller. */
                if (msg_cb) {
                    /* Make sure the message is nul
                    * terminated. */
                    if ((size_t) res == *reply_len) {
                        res = (*reply_len) - 1;
                    }
                    reply[res] = '\0';
                    msg_cb(reply, res);
                }
                continue;
            }

            *reply_len = res;
            break;
        } else {
            return -2;
        }
    }

    return 0;
}


static int wpa_ctrl_attach_helper(struct wpa_ctrl *ctrl, int attach)
{
    int    ret     = 0;
    char   buf[10] = {0};
    size_t len     = 10;

	ret = wpa_ctrl_request(ctrl, attach ? "ATTACH" : "DETACH", 6,
			       buf, &len, NULL);
	if (ret < 0) {
        return ret;
	}

    if (3 == len && memcmp(buf, "OK\n", 3) == 0) {
        return 0;
    }

    return -1;
}


int wpa_ctrl_attach(struct wpa_ctrl *ctrl)
{
    return wpa_ctrl_attach_helper(ctrl, 1);
}


int wpa_ctrl_detach(struct wpa_ctrl *ctrl)
{
    return wpa_ctrl_attach_helper(ctrl, 0);
}


int wpa_ctrl_recv(struct wpa_ctrl *ctrl, char *reply, int *reply_len)
{
    int res;

    res = recv(ctrl->s, reply, *reply_len, 0);
    if (res < 0) {
        printf("[FUN]:%s  [LINE]:%d  Fail connect! res:%d errno[%d] errinfo[%s]\n",
                                __func__, __LINE__, res, errno, strerror(errno));
        return res;
    }

    *reply_len = res;
    return 0;
}

int wpa_ctrl_pending(struct wpa_ctrl *ctrl, int sec, int usec)
{
    int ret = 0;
    struct timeval tv;
    fd_set rfds;
    tv.tv_sec  = sec;
    tv.tv_usec = usec;
    FD_ZERO(&rfds);
    FD_SET(ctrl->s, &rfds);

    if (0 == sec && 0 == usec) {
        ret = select(ctrl->s + 1, &rfds, NULL, NULL, NULL);
    } else {
        ret = select(ctrl->s + 1, &rfds, NULL, NULL, &tv);
    }

    if (ret < 0) {
        return -1;
    } else if(0 == ret) {
        return 0;
    }

    return FD_ISSET(ctrl->s, &rfds);
}


int wpa_ctrl_get_fd(struct wpa_ctrl *ctrl)
{
    return ctrl->s;
}
