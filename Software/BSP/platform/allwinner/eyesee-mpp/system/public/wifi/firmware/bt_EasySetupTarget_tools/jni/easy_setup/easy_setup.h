#ifndef __EASY_SETUP_H__
#define __EASY_SETUP_H__

#include "common.h"

#define EASY_SETUP_STATE_DONE (5)

/* wlan security */
enum {
    WLAN_SECURITY_NONE = 0,
    WLAN_SECURITY_WEP,
    WLAN_SECURITY_WPA,
    WLAN_SECURITY_WPA2,
};

/* easy setup protocols */
enum {
    EASY_SETUP_PROTO_BCAST = 0,
    EASY_SETUP_PROTO_NEEZE,
    EASY_SETUP_PROTO_AKISS,
    EASY_SETUP_PROTO_XIAOYI,
    EASY_SETUP_PROTO_CHANGHONG,
    EASY_SETUP_PROTO_JINGDONG,
    EASY_SETUP_PROTO_JD,
    EASY_SETUP_PROTO_MAX,
};

enum {
    EASY_SETUP_STOP = 0,
    EASY_SETUP_START,
    EASY_SETUP_RESTART,
};

typedef struct {      
    uint8 enable;    /* START/STOP/RESTART easy setup */ 
    uint16 protocol_mask;  /* set parameters to which protocol */ 
    tlv_t param; /* param blocks for each protocol */
} easy_setup_param_t;

typedef struct {
    uint8          state;
    uint8          security_key[64];     /* target AP's password */
    uint8          security_key_length;  /* length of password */
    ssid_t         ap_ssid;              /* target AP's name */
    mac_t          ap_bssid;             /* target AP's mac address */
} easy_setup_result_t;

/* internal interface */
void easy_setup_get_param(uint16 proto_mask, tlv_t** pptr);
void easy_setup_set_result(uint8 protocol, void* p);

/*
 * public interface here!!!
 */

/* 
 * Set decryption key, if you have set it in sender side
 * parameter
 *   key: null-terminated key string
 * returns
 *   0: succ, -1: fail
 */
int easy_setup_set_decrypt_key(char* key);

/* enable easy setup protocols */
void easy_setup_enable_bcast(); /* broadcom bcast */
void easy_setup_enable_neeze(); /* broadcom neeze */
void easy_setup_enable_akiss(); /* wechat airkiss */
void easy_setup_enable_qqcon(); /* qq connect */
void easy_setup_enable_changhong(); /* changhong smart link */
void easy_setup_enable_jingdong(); /* changhong old smart link */
void easy_setup_enable_jd(); /* jd joylink */

/* enable protocols one time by bitmask
 * 0x0001 - bcast
 * 0x0002 - neeze
 * 0x0004 - airkiss
 * 0x0010 - changhong
 * 0x0020 - jingdong
 * 0x0040 - jd joylink
 */
void easy_setup_enable_protocols(uint16 proto_mask);

/* 
 * Start easy setup 
 * returns
 *   0: succ, -1: fail
 */
int easy_setup_start();

/* 
 * Stop easy setup 
 * returns
 *   0: succ, -1: fail
 */
int easy_setup_stop();

/*
 * Query easy setup result. This function blocks for some time (see EASY_SETUP_QUERY_TIMEOUT)
 * returns
 *   0: succ, -1: fail
 */
int easy_setup_query();

int easy_setup_iovar(int set, void* param, int size);
int easy_setup_ioctl(int cmd, int set, void* param, int size);

/* 
 * Get ssid. Call this after easy_setup_query() succeeds.
 * returns
 *   0: succ, -1: fail
 */
int easy_setup_get_ssid(char buff[], int buff_len);

/* 
 * Get password. Call this after easy_setup_query() succeeds.
 * returns
 *   0: succ, -1: fail
 */
int easy_setup_get_password(char buff[], int buff_len);

/* get protocol shot */
int easy_setup_get_protocol(uint8* protocol);

#if 0
/* 
 * Get security of configured SSID
 * returns
 *   WLAN_SCURITY_NONE: NONE
 *   WLAN_SCURITY_WEP: WEP 
 *   WLAN_SCURITY_WPA: WPA
 *   
 */
int easy_setup_get_security(void);
#endif

#endif /* __EASY_SETUP_H__ */
