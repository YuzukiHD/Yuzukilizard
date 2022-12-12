#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "easy_setup.h"

#define ETHER_ADDR_LEN    6
#define DOT11_BSSTYPE_ANY 2

#define htod32(n) (n)
#define dtoh32(n) (n)
#define dtoh16(n) (n)

#define WLC_SCAN 50
#define WLC_SCAN_RESULTS 51

typedef struct wlc_ssid {
    uint32      SSID_len;
    uint8       SSID[32];
} wlc_ssid_t;

struct    ether_addr {
    uint8 octet[ETHER_ADDR_LEN];
};

typedef struct wl_scan_params {
    wlc_ssid_t ssid;
    struct ether_addr bssid;
    int8 bss_type;
    uint8 scan_type;
    int32 nprobes;
    int32 active_time;
    int32 passive_time;    
    int32 home_time;
    int32 channel_num;
    uint16 channel_list[1];
} wl_scan_params_t;

typedef struct wl_bss_info_107 {
    uint32      version;
    uint32      length;
    struct ether_addr BSSID;
    uint16      beacon_period;
    uint16      capability;
    uint8       SSID_len;
    uint8       SSID[32];
    struct {
        uint    count;
        uint8   rates[16];
    } rateset;
    uint8       channel;
    uint16      atim_window;
    uint8       dtim_period; 
    int16       RSSI;
    int8        phy_noise;
    uint32      ie_length;
} wl_bss_info_107_t;

typedef struct wl_bss_info {
    uint32      version;
    uint32      length;
    struct ether_addr BSSID;
    uint16      beacon_period;
    uint16      capability;
    uint8       SSID_len;
    uint8       SSID[32];
    struct {
        uint    count;
        uint8   rates[16];
    } rateset;
    uint16      chanspec;
    uint16      atim_window;
    uint8       dtim_period;
    int16       RSSI;
    int8        phy_noise;

    uint8       n_cap;
    uint32      nbss_cap;
    uint8       ctl_ch;
    uint8       padding1[3];
    uint16      vht_rxmcsmap;
    uint16      vht_txmcsmap;
    uint8       flags;
    uint8       vht_cap;
    uint8       reserved[2];
    uint8       basic_mcs[16];

    uint16      ie_offset;
    uint32      ie_length;
    int16       SNR;
} wl_bss_info_t;

typedef struct wl_scan_results {
    uint32 buflen;
    uint32 version;
    uint32 count;
    wl_bss_info_t bss_info[1];
} wl_scan_results_t; 

static int scan_and_get_security_internal(int ch, uint8* bssid, ssid_t* ssid) {
    int sec = -1;

    int ret = 0;
    wl_scan_params_t p;

    memset(&p, 0, sizeof(p));
    p.bss_type = DOT11_BSSTYPE_ANY;
    p.scan_type = 0;
    p.nprobes = -1;
    p.active_time = 100;
    p.passive_time = -1;
    p.home_time = -1;
    p.channel_num = 0;

    if (ch > 0) {
        p.channel_num = 1;
        p.channel_list[0] = ch;
    }

    if (bssid) {
        memcpy(&p.bssid, bssid, ETHER_ADDR_LEN);
    } else {
        uint8 bcast[ETHER_ADDR_LEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
        memcpy(&p.bssid, bcast, sizeof(bcast));
    }

    if (ssid) {
        p.ssid.SSID_len = ssid->len;
        memcpy(p.ssid.SSID, ssid->val, sizeof(ssid->val));
    }

    p.nprobes = htod32(p.nprobes);
    p.active_time = htod32(p.active_time);
    p.passive_time = htod32(p.passive_time);
    p.home_time = htod32(p.home_time);

    if ((ret = easy_setup_ioctl(WLC_SCAN, 1, &p, sizeof(p))) < 0) {
        return -1;
    }

    int size = 1024;
    uint8* ptr = malloc(size);
    wl_scan_results_t* r;
    r = (wl_scan_results_t*) ptr;
    int tries = 20;
    int delay_ms = 150;
    while (tries--) {
        memset(ptr, 0, size);
        r->buflen = size;
        ret = easy_setup_ioctl(WLC_SCAN_RESULTS, 0, r, size);
        if (!ret && r->count) break;

        //LOGD("sleep %dms\n", delay_ms);
        usleep(delay_ms*1000);
    }

    if (!ret && r->count) {
        int i=0; 
        wl_bss_info_t* bi = r->bss_info;

        if (dtoh32(bi->version) == 107) {
            wl_bss_info_107_t *old = (wl_bss_info_107_t *)bi;
            bi->ie_length = old->ie_length;
            bi->ie_offset = sizeof(wl_bss_info_107_t);
        }

        uint16 cap = dtoh16(bi->capability);
        int totlen = dtoh32(bi->ie_length);
        int wpa = 0;
        if (totlen) {
            uint8* ie = (uint8 *)(((uint8 *)bi) + dtoh16(bi->ie_offset));
            while (totlen>=2) {
                int type = ie[0];
                int len = ie[1];

                if (type == 221 && len >= 4) {
                    int v = (ie[2]<<24) | (ie[3]<<16) | (ie[4]<<8) | (ie[5]);
                    if (v == 0x0050f201) {
                        sec = WLAN_SECURITY_WPA;
                        break;
                    }
                }

                if (type == 48) {
                    sec = WLAN_SECURITY_WPA2;
                    break;
                }

                totlen -= (len+2);
                ie += (len+2);
            }
        }

        if (sec != WLAN_SECURITY_WPA && sec != WLAN_SECURITY_WPA2) {
            if (cap & 0x10) {
                sec = WLAN_SECURITY_WEP;
            } else {
                sec = WLAN_SECURITY_NONE;
            }
        }
    } else {
        LOGD("scan gets no result(ret: %d, count: %d).\n", ret, (!ret)?r->count:0);
        sec = -1;
    }

    free(ptr);

    return sec;
}

int scan_and_get_security(ssid_t* ssid) {
    return scan_and_get_security_internal(-1, NULL, ssid);
}

