#ifndef __EASY_SETUP_COMMON_H__
#define __EASY_SETUP_COMMON_H__

#include <stdio.h>

#define uint8 unsigned char
#define uint16 unsigned short
#define uint32 unsigned int

#define int8 char
#define int16 short
#define int32 int

/* direct log message to your system utils, default printf */
extern int debug_enable;
#define LOGE(fmt, arg...) do {if (1 | debug_enable) printf(fmt, ##arg);} while (0);
#define LOGD(fmt, arg...) do {if (debug_enable) printf(fmt, ##arg);} while (0);

typedef struct {
    uint8 type;
    uint8 length;
    uint8 value[0];
} tlv_t;

/**
 * Structure for storing a Service Set Identifier (i.e. Name of Access Point)
 */
typedef struct {
    uint8 len;     /**< SSID length */
    uint8 val[32]; /**< SSID name (AP name)  */
} ssid_t;

/**
 * Structure for storing a MAC address (Wi-Fi Media Access Control address).
 */
typedef struct {
    uint8 octet[6]; /**< Unique 6-byte MAC address */
} mac_t;

/**
 * Structure for storing a IP address.
 */
typedef struct {
    uint8 version;   /**< IPv4 or IPv6 type */

    union {
        uint32 v4;   /**< IPv4 IP address */
        uint32 v6[4];/**< IPv6 IP address */
    } ip;
} ip_address_t;

#endif /* __EASY_SETUP_COMMON_H__ */
