/*
 * Copyright 2014, Broadcom Corporation
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************
 *                      Macros
 ******************************************************/

/******************************************************
 *                    Constants
 ******************************************************/

/*
 * AP settings in this file are stored in the DCT. These
 * settings may be overwritten at manufacture when the
 * DCT is written with the final production configuration
 */

/* This is the soft AP used for device configuration */
#define CONFIG_AP_SSID       "NOT USED FOR THIS APP"
#define CONFIG_AP_PASSPHRASE "NOT USED FOR THIS APP"
#define CONFIG_AP_SECURITY   WICED_SECURITY_OPEN
#define CONFIG_AP_CHANNEL    1

/* This is the soft AP available for normal operation (if used)*/
#define SOFT_AP_SSID         "WICED UDP Receive App"
#define SOFT_AP_PASSPHRASE   "abcd1234"
#define SOFT_AP_SECURITY     WICED_SECURITY_OPEN  /* WICED_SECURITY_WPA2_AES_PSK */
#define SOFT_AP_CHANNEL      1

/* This is the default AP the device will connect to (as a client)*/
#define CLIENT_AP_SSID       "NOT USED FOR THIS APP"
#define CLIENT_AP_PASSPHRASE "NOT USED FOR THIS APP"
#define CLIENT_AP_BSS_TYPE   WICED_BSS_TYPE_INFRASTRUCTURE
#define CLIENT_AP_SECURITY   WICED_SECURITY_WPA2_MIXED_PSK
#define CLIENT_AP_CHANNEL    1
#define CLIENT_AP_BAND       WICED_802_11_BAND_2_4GHZ

/* Override default country code */
#define WICED_COUNTRY_CODE    WICED_COUNTRY_CHINA

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/

/******************************************************
 *                    Structures
 ******************************************************/

/******************************************************
 *                 Global Variables
 ******************************************************/

/******************************************************
 *               Function Declarations
 ******************************************************/

#ifdef __cplusplus
} /*extern "C" */
#endif
