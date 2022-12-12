/*
 * Copyright 2015, Broadcom Corporation
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 */

/** @file
 *
 * Broadcom EasySetup demo
 *
 * This application snippet demonstrates how to start Broadcom EasySetup
 *
 * Features demonstrated
 *  - Start Cooee/AirKiss/QQConnect
 *
 * Application Instructions
 *   1. Start this application
 *   2. Start sender (with Cooee/AirKiss/QQConnect application)
 *   3. On sender application, fill password and click send
 *
 * You should see SSID/password printed on wiced console,
 * then it will connect to AP. 
 * For AirKiss, after sucessfully connected, 
 * received random byte will be broadcast 20 times to notify
 * sender done.
 *
 */

#include "wiced.h"
#include "bcast.h"
#include "akiss.h"
#include "changhong.h"
#include "jingdong.h"
#include "jd.h"

#define AKISS_PORT                (10000)  /* akiss port */

static wiced_result_t send_akiss_random(uint8_t random)
{
    wiced_result_t ret;
    wiced_udp_socket_t  udp_socket;
    wiced_packet_t*          packet;
    char*                    tx_data;
    uint16_t                 available_data_length;
    const wiced_ip_address_t INITIALISER_IPV4_ADDRESS( target, MAKE_IPV4_ADDRESS(255, 255, 255, 255));

    /* Create UDP socket */
    ret = wiced_udp_create_socket( &udp_socket, 0, WICED_STA_INTERFACE );
    if (ret != WICED_SUCCESS) {
        WPRINT_APP_INFO( ("UDP socket creation failed: %d\n", ret) );
        return WICED_ERROR;
    }

    /* Create the UDP packet. Memory for the tx data is automatically allocated */
    if (wiced_packet_create_udp(&udp_socket, 1, &packet, (uint8_t**)&tx_data, &available_data_length) != WICED_SUCCESS)
    {
        WPRINT_APP_INFO(("UDP tx packet creation failed\r\n"));
        return WICED_ERROR;
    }

    tx_data[0] = random;

    /* Set the end of the data portion of the packet */
    wiced_packet_set_data_end(packet, (uint8_t*)tx_data + 1);

    /* Send the UDP packet */
    if (wiced_udp_send(&udp_socket, &target, AKISS_PORT, packet) != WICED_SUCCESS)
    {
        WPRINT_APP_INFO(("UDP packet send failed\r\n"));
        wiced_packet_delete(packet);  /* Delete packet, since the send failed */
    }

    /*
     * NOTE : It is not necessary to delete the packet created above, the packet
     *        will be automatically deleted *AFTER* it has been successfully sent
     */

    return WICED_SUCCESS;
}

void application_start(void)
{
    wiced_config_ap_entry_t* dct_ap_entry;
    wiced_result_t result;

    WPRINT_APP_INFO(("Broadcom EasySetup demo v3.4.0\r\n"));

    /* Initialise the device and WICED framework */
    wiced_init( );

    /* enable protocols */
    easy_setup_enable_bcast(); /* broadcom bcast */
    easy_setup_enable_neeze(); /* broadcom neeze */
    //easy_setup_enable_akiss(); /* wechat akiss */
    //easy_setup_enable_changhong(); /* changhong */
    //easy_setup_enable_jingdong(); /* jingdong */
    //easy_setup_enable_jd(); /* jd-joy */

    /* set bcast key */
    //bcast_set_key("1111111111111111");

    /* set airkiss key */
    //akiss_set_key("1111111111111111");

    /* set jingdong key */
    //jingdong_set_key("1111111111111111");

    /* set jd-joy key */
    //jd_set_key("1111111111111111");

    /* start easy setup */
    if (easy_setup_start() != WICED_SUCCESS)
    {
        WPRINT_APP_INFO(("easy setup failed.\r\n"));
    }
    else
    {
        WPRINT_APP_INFO(("easy setup done.\r\n"));
    }

    /* Bring up the network interface */
    int up_tries = 3;
    wiced_gpio_output_low(WICED_LED2);
    while (up_tries--) {
        result = wiced_network_up( WICED_STA_INTERFACE, WICED_USE_EXTERNAL_DHCP_SERVER, NULL );
        if (result == WICED_SUCCESS) {
            break;
        } else {
            WPRINT_APP_INFO(("Network up failed, try again (%d left)\r\n", up_tries));
        }
    }
    if ( result != WICED_SUCCESS )
    {
        wiced_dct_read_lock( (void**) &dct_ap_entry, 
        WICED_TRUE, 
        DCT_WIFI_CONFIG_SECTION, 
        offsetof(platform_dct_wifi_config_t, stored_ap_list), 
        sizeof(wiced_config_ap_entry_t) );
        if (dct_ap_entry->details.security == WICED_SECURITY_WEP_PSK ) // Now try shared instead of open authentication
        {
            dct_ap_entry->details.security = WICED_SECURITY_WEP_SHARED;
            wiced_dct_write( dct_ap_entry, 
                    DCT_WIFI_CONFIG_SECTION, 
                    offsetof(platform_dct_wifi_config_t, stored_ap_list), 
                    sizeof(wiced_config_ap_entry_t) );
            result = wiced_network_up( WICED_STA_INTERFACE, WICED_USE_EXTERNAL_DHCP_SERVER, NULL );
            if ( result != WICED_SUCCESS ) // Restore old value
            {
                wiced_dct_read_lock( (void**) &dct_ap_entry, 
                        WICED_TRUE, 
                        DCT_WIFI_CONFIG_SECTION, 
                        offsetof(platform_dct_wifi_config_t, stored_ap_list), 
                        sizeof(wiced_config_ap_entry_t) );
                dct_ap_entry->details.security = WICED_SECURITY_WEP_PSK;
                wiced_dct_write( dct_ap_entry, 
                        DCT_WIFI_CONFIG_SECTION, 
                        offsetof(platform_dct_wifi_config_t, stored_ap_list), 
                        sizeof(wiced_config_ap_entry_t) );
            }
        }
    }

    if ( result != WICED_SUCCESS )
    {
        WPRINT_APP_INFO( ("Network up failed\n") );
        return;
    }
    else
    {
        WPRINT_APP_INFO( ("Network up success\n") );
        wiced_gpio_output_high(WICED_LED2);
    }    

    /* if shot protocol is akiss, get random byte and send it 20 times */
    uint8_t protocol;
    if (!easy_setup_get_protocol(&protocol) && protocol == EASY_SETUP_PROTO_AKISS) {
        uint8_t random;
        if (!akiss_get_random(&random)) {
            int tries = 20;
            WPRINT_APP_INFO( ("send random (0x%02x) %d times.\r\n", random, tries) );
            while (tries--) {
                if (send_akiss_random(random) != WICED_SUCCESS) break;
                tx_thread_sleep(10 * SYSTICK_FREQUENCY/1000);
            }

            WPRINT_APP_INFO( ("send random done.\r\n") );
        }
    }
}

