/*
 * $ Copyright Broadcom Corporation $
 */

/** @file
 *
 */
#include "wiced.h"
#include "stdint.h"
#include "stddef.h"
#include "wwd_network_interface.h"
#include "wwd_buffer_interface.h"
#include "tlv.h"
#include "wiced_security.h"

#include "internal/wwd_sdpcm.h"
#include "wwd_wlioctl.h"
#include "wwd_events.h"

#include "easy_setup.h"

#define PARAM_MAX_LEN (256)
#define RESULT_MAX_LEN (256)

typedef struct
{
    easy_setup_result_t result;
    easy_setup_param_t param;
    wiced_semaphore_t es_complete;
    wiced_semaphore_t scan_complete;
    wiced_security_t security;
    uint8_t channel;
    int abort_scan;
    uint8_t protocol;
} easy_setup_workspace_t;

// Trigger custom scan to fetch the security info of the AP
#define SCAN_TO_GET_SECURITY

#ifdef SCAN_TO_GET_SECURITY
static wiced_scan_result_t  scan_result[1];
#endif

/* error code in WICED/WWD/include/wwd_constants.h */
#define easy_setup_assert(s, v) \
do {\
    if (v != WWD_SUCCESS) {\
        WPRINT_WICED_INFO(("%s, %s: %d\n", __FUNCTION__, s, v));\
        WICED_ASSERTION_FAIL_ACTION();\
    }\
} while (0)

#define CHECK_IOCTL_BUFFER( buff )  \
    if ( buff == NULL ) {  \
        easy_setup_assert("Allocation failed", WWD_BUFFER_UNAVAILABLE_TEMPORARY);\
        return WWD_BUFFER_ALLOC_FAIL; \
    }

static const wwd_event_num_t easy_setup_events[] = { WLC_E_WAKE_EVENT, WLC_E_NONE };

uint16_t g_protocol_mask = 0x3; /* bcast+neeze */
static uint8_t g_protocol = 0;

static void* easy_setup_result_handler( const wwd_event_header_t* event_header, 
    const uint8_t* event_data, 
    void* handler_user_data )
{
    easy_setup_workspace_t *ws = (easy_setup_workspace_t *)handler_user_data;

    if (event_header->status == WLC_E_STATUS_SUCCESS)
    {
        WPRINT_WICED_INFO( ("\r\nEvent Found => Success\r\n") );
        memcpy( &ws->result, event_data, sizeof(easy_setup_result_t) );
        wiced_rtos_set_semaphore( &ws->es_complete );
        //easy_setup_stop( );
    }
    else if (event_header->status == WLC_E_STATUS_TIMEOUT)
    {
        WPRINT_WICED_INFO( ("\r\nEvent Found => Timeout\r\n") );
    }
    else 
    {
        WPRINT_WICED_INFO( ("\r\nEvent Found => Unknown\r\n") );
    }
    return handler_user_data;
}

wwd_result_t easy_setup_restart( void )
{
    wiced_buffer_t buffer;
    wwd_result_t result;
    easy_setup_param_t *param;

    param = wwd_sdpcm_get_iovar_buffer( &buffer, sizeof(easy_setup_param_t), "easy_setup");
    CHECK_IOCTL_BUFFER( param );
    
    param->enable = EASY_SETUP_RESTART;

    result = wwd_sdpcm_send_iovar( SDPCM_SET, buffer, 0, WWD_STA_INTERFACE );
    easy_setup_assert("Failed to set IOCTL", result);
    return result;
}

int easy_setup_stop( void )
{
    wiced_buffer_t buffer;
    wwd_result_t result;
    easy_setup_param_t *p;

    p = wwd_sdpcm_get_iovar_buffer( &buffer, sizeof(easy_setup_param_t), "easy_setup");
    CHECK_IOCTL_BUFFER(p);
    
    p->enable = EASY_SETUP_STOP;

    result = wwd_sdpcm_send_iovar( SDPCM_SET, buffer, 0, WWD_STA_INTERFACE );
    easy_setup_assert("Failed to set IOCTL", result);
    return result;
}

wwd_result_t easy_setup_enable( uint16_t proto_mask, easy_setup_workspace_t *ws )
{
    wiced_buffer_t buffer;
    wwd_result_t result;
    easy_setup_param_t *p;

    result = wwd_management_set_event_handler( easy_setup_events, easy_setup_result_handler, ws, WWD_STA_INTERFACE );
    easy_setup_assert("Failed to set Event Handler", result);

    p = wwd_sdpcm_get_iovar_buffer( &buffer, PARAM_MAX_LEN, "easy_setup");
    CHECK_IOCTL_BUFFER(p);
    
    p->enable = EASY_SETUP_START;
    p->protocol_mask = proto_mask;

    int param_len = 0;
    param_len += ((uint8*)&p->param-(uint8*)p);

    tlv_t* t = &p->param;
    easy_setup_get_param(proto_mask, &t);

    result = wwd_sdpcm_send_iovar( SDPCM_SET, buffer, 0, WWD_STA_INTERFACE);
    easy_setup_assert("Failed to set IOCTL", result);
    return result;
}

int easy_setup_query( easy_setup_workspace_t *ws )
{
    wiced_buffer_t buffer;
    wiced_buffer_t response;
    wwd_result_t ret;
    easy_setup_result_t* r = &ws->result;

    UNUSED_PARAMETER(ws);

    (void)wwd_sdpcm_get_iovar_buffer(&buffer, RESULT_MAX_LEN, "easy_setup");
    ret = wwd_sdpcm_send_iovar(SDPCM_GET, buffer, &response, WWD_STA_INTERFACE );
    if ( ret == WWD_SUCCESS )
    {
        tlv_t* t = (tlv_t*) host_buffer_get_current_piece_data_pointer(response);
        memcpy(r, t->value, sizeof(easy_setup_result_t));
        WPRINT_WICED_INFO( ("state [%d]\r\n", r->state) );
        if (r->state == EASY_SETUP_STATE_DONE) 
        {
            WPRINT_WICED_INFO(("Finished! protocol: %d\r\n", t->type));

            g_protocol = t->type;
            easy_setup_set_result(g_protocol, t->value);

            wiced_rtos_set_semaphore( &ws->es_complete );
        }

        host_buffer_release(response, WWD_NETWORK_RX);
    }

    return WICED_SUCCESS;    
}

#ifdef SCAN_TO_GET_SECURITY
static void scan_result_callback( wiced_scan_result_t** result_ptr, void* user_data, wiced_scan_status_t status  )
{
    easy_setup_workspace_t *ws = (easy_setup_workspace_t *)user_data;

    UNUSED_PARAMETER(status);
    if (ws->abort_scan == WICED_TRUE) {
        return;
    }
    
    if (NULL == result_ptr)
    {
        wiced_rtos_set_semaphore(&ws->scan_complete);
    }
    else
    {
    /*
        uint8_t* o = (*result_ptr)->BSSID.octet;
    WPRINT_WICED_INFO( ("scan cb, %02x:%02x:%02x:%02x:%02x:%02x\r\n", o[0], o[1], o[2], o[3], o[4], o[5]) );
    */
        /* Only look for a match on our locked BSSID */
        if (memcmp(ws->result.ap_bssid.octet, (*result_ptr)->BSSID.octet, sizeof(wiced_mac_t)) == 0)
        {
            ws->abort_scan = WICED_TRUE;
            ws->channel = (*result_ptr)->channel;
            ws->security = (*result_ptr)->security;
            wiced_rtos_set_semaphore(&ws->scan_complete);
        }
    }
}
#endif

static int hex_str_to_int( const char* hex_str )
{
    int n = 0;
    uint32_t value = 0;
    int shift = 7;
    while ( hex_str[n] != '\0' && n < 8 )
    {
        if ( hex_str[n] > 0x21 && hex_str[n] < 0x40 )
        {
            value |= (uint32_t)(( hex_str[n] & 0x0f ) << ( shift << 2 ));
        }
        else if ( ( hex_str[n] >= 'a' && hex_str[n] <= 'f' ) || ( hex_str[n] >= 'A' && hex_str[n] <= 'F' ) )
        {
            value |= (uint32_t)(( ( hex_str[n] & 0x0f ) + 9 ) << ( shift << 2 ));
        }
        else
        {
            break;
        }
        n++;
        shift--;
    }

    return (int)( value >> ( ( shift + 1 ) << 2 ) );
}

int easy_setup_start()
{
    easy_setup_workspace_t* ws = NULL;
    easy_setup_result_t *result = NULL;
    wiced_bool_t led_flag;
    wiced_config_ap_entry_t* dct_ap_entry;
#ifdef SCAN_TO_GET_SECURITY
    wiced_mac_t bogus_scan_mac;
    wiced_ssid_t ssid;
    wiced_scan_extended_params_t extparam = { 5, 110, 110, 50 };
    wiced_scan_result_t* result_ptr = (wiced_scan_result_t *) &scan_result;
    int retry_times;
#endif
#ifdef AUTO_TEST
    int timeout_count = 0;
#endif

    if (!g_protocol_mask) {
        WPRINT_WICED_INFO(("No protocol enabled.\r\n"));
        return WICED_ERROR;
    }

    ws = malloc_named( "es", sizeof(easy_setup_workspace_t) );
    if ( ws == NULL )
    {
        return WICED_OUT_OF_HEAP_SPACE;
    }
    memset(ws, 0, sizeof(easy_setup_workspace_t));

    ws->abort_scan = WICED_FALSE;
    wiced_rtos_init_semaphore( &ws->es_complete);

    easy_setup_enable(g_protocol_mask, ws);

    WPRINT_WICED_INFO( ("Enabled, waiting ...\r\n") );

    led_flag = WICED_FALSE;
    while (wiced_rtos_get_semaphore( &ws->es_complete, 500 * MILLISECONDS ) == WICED_TIMEOUT)
    {
        /* LED show status, could be removed */
        if ( led_flag == WICED_FALSE )
        {
            led_flag = WICED_TRUE;
            wiced_gpio_output_high( WICED_LED1 );
        }
        else
        {
            led_flag = WICED_FALSE;
            wiced_gpio_output_low( WICED_LED1 );
        }

        easy_setup_query( ws );
        
#ifdef AUTO_TEST
        timeout_count++;
        /* if each takes 500ms, then 60 means wait time is 500ms x 60 = 30s */
        if (timeout_count == 60)
        {
            wiced_rtos_set_semaphore( &ws->es_complete );
            easy_setup_stop( );            
            wiced_rtos_deinit_semaphore( &ws->es_complete );
            return WWD_TIMEOUT;
        }
#endif
    }    
    wwd_management_set_event_handler( easy_setup_events, NULL, NULL, WWD_STA_INTERFACE );
    wiced_rtos_deinit_semaphore( &ws->es_complete );
    easy_setup_stop( );
    /* LED show status, could be removed */
    wiced_gpio_output_high( WICED_LED1 );

    result = &ws->result;
    result->security_key[result->security_key_length] = 0;
    WPRINT_WICED_INFO( ("SSID        : %s\r\n", result->ap_ssid.val) );
    WPRINT_WICED_INFO( ("PASSWORD    : %.64s\r\n", result->security_key) );
    WPRINT_WICED_INFO( ("BSSID       : %02x:%02x:%02x:%02x:%02x:%02x\r\n", 
                    (uint)result->ap_bssid.octet[0], (uint)result->ap_bssid.octet[1],
                    (uint)result->ap_bssid.octet[2], (uint)result->ap_bssid.octet[3],
                    (uint)result->ap_bssid.octet[4], (uint)result->ap_bssid.octet[5]));

    wiced_rtos_init_semaphore(&ws->scan_complete);
    wiced_rtos_get_semaphore( &ws->scan_complete, 200 * MILLISECONDS );

#ifdef SCAN_TO_GET_SECURITY
    memcpy( &bogus_scan_mac, &result->ap_bssid, sizeof(wiced_mac_t) );
    memcpy( &ssid, &result->ap_ssid, sizeof(wiced_ssid_t) );
    
    wiced_rtos_init_semaphore( &ws->scan_complete );
    result_ptr = &scan_result[0];

    /* we need to scan and get security & channel, will retry several times */
    retry_times = 3;
    do {
        wwd_wifi_scan( WICED_SCAN_TYPE_ACTIVE, WICED_BSS_TYPE_INFRASTRUCTURE, &ssid, NULL, NULL, &extparam, scan_result_callback, (wiced_scan_result_t **) &result_ptr, ws, WWD_STA_INTERFACE  );
        wiced_rtos_get_semaphore( &ws->scan_complete, 2000 * MILLISECONDS );
        retry_times--;
    } while ((ws->channel == 0) && (retry_times > 0));
    WPRINT_WICED_INFO( ("AP Channel  : %d\r\n", ws->channel) );
    WPRINT_WICED_INFO( ("AP Security : %s ", 
                    ws->security == WICED_SECURITY_OPEN ? "Open" :
                    ws->security == WICED_SECURITY_WEP_PSK ? "WEP-OPEN" :
                    ws->security == WICED_SECURITY_WEP_SHARED ? "WEP-SHARED" :
                    ws->security == WICED_SECURITY_WPA_TKIP_PSK ? "WPA-PSK TKIP" :
                    ws->security == WICED_SECURITY_WPA_AES_PSK ? "WPA-PSK AES" :
                    ws->security == WICED_SECURITY_WPA2_AES_PSK ? "WPA2-PSK AES" :
                    ws->security == WICED_SECURITY_WPA2_TKIP_PSK ? "WPA2-PSK TKIP" :
                    ws->security == WICED_SECURITY_WPA2_MIXED_PSK ? "WPA2-PSK Mixed" :
                     "Unknown" ) );
     WPRINT_WICED_INFO( ("\r\n") );
#endif /* SCAN_TO_GET_SECURITY */

    /* Store AP credentials into DCT */
    WPRINT_WICED_INFO( ("Storing received credentials in DCT\r\n") );    

    wiced_dct_read_lock( (void**) &dct_ap_entry, WICED_TRUE, DCT_WIFI_CONFIG_SECTION, offsetof(platform_dct_wifi_config_t, stored_ap_list), sizeof(wiced_config_ap_entry_t) );

    /* Setup the AP details */
    dct_ap_entry->details.security = ws->security;
    dct_ap_entry->details.channel = ws->channel;
    dct_ap_entry->details.band = WICED_802_11_BAND_2_4GHZ;
    dct_ap_entry->details.bss_type = WICED_BSS_TYPE_INFRASTRUCTURE;

    memcpy(&dct_ap_entry->details.SSID, &result->ap_ssid, sizeof(wiced_ssid_t));
    memcpy(dct_ap_entry->details.BSSID.octet, result->ap_bssid.octet, sizeof(wiced_mac_t));
    
    /* Store the AP security into DCT */
    if (ws->security == WICED_SECURITY_WPA_TKIP_PSK ||
        ws->security == WICED_SECURITY_WPA_AES_PSK ||
        ws->security == WICED_SECURITY_WPA2_AES_PSK ||
        ws->security == WICED_SECURITY_WPA2_TKIP_PSK ||
        ws->security == WICED_SECURITY_WPA2_MIXED_PSK )
    {
        dct_ap_entry->security_key_length = result->security_key_length;
        memset(dct_ap_entry->security_key, 0, 64);
        memcpy(dct_ap_entry->security_key, result->security_key, 64);
    }
    else if ( ws->security == WICED_SECURITY_WEP_PSK ||
              ws->security == WICED_SECURITY_WEP_SHARED )
    {
        uint8_t wep_key_buffer[64];
        wiced_wep_key_t* temp_wep_key = (wiced_wep_key_t*)wep_key_buffer;
        uint8_t* security_key;
        uint8_t key_length;
        char temp_string[3];
        int a;

        temp_string[2] = 0;
        key_length = result->security_key_length;
        
        if ( key_length == 5 || key_length == 13 || key_length == 16 )
        {
            /* Setup WEP key 0 */
            temp_wep_key[0].index = 0;
            temp_wep_key[0].length = key_length;
            memcpy(temp_wep_key[0].data, result->security_key, key_length);
        }
        else if ( key_length == 10 || key_length == 26 || key_length == 32 )
        {
            key_length = key_length/2;
            /* Setup WEP key 0 */
            temp_wep_key[0].index = 0;
            temp_wep_key[0].length = key_length;
            for ( a = 0; a < temp_wep_key[0].length; ++a )
            {
                memcpy(temp_string, &result->security_key[a*2], 2);
                temp_wep_key[0].data[a] = (uint8_t)hex_str_to_int(temp_string);
            }
        }
        else
        {
            WPRINT_APP_INFO(( "Invalid wep key length\r\n" ));
            return WWD_INVALID_KEY;
        }

        /* Setup WEP keys 1 to 3 */
        memcpy(wep_key_buffer + 1*(2 + key_length), temp_wep_key, (uint8_t)(2 + key_length));
        memcpy(wep_key_buffer + 2*(2 + key_length), temp_wep_key, (uint8_t)(2 + key_length));
        memcpy(wep_key_buffer + 3*(2 + key_length), temp_wep_key, (uint8_t)(2 + key_length));
        wep_key_buffer[1*(2 + key_length)] = 1;
        wep_key_buffer[2*(2 + key_length)] = 2;
        wep_key_buffer[3*(2 + key_length)] = 3;
        
        security_key = wep_key_buffer;
        key_length = (uint8_t)(4*(2 + key_length));

        dct_ap_entry->security_key_length = key_length;
        memcpy(dct_ap_entry->security_key, security_key, 64);
    }

#ifndef AUTO_TEST
    /* Call API to store into DCT */
    wiced_dct_write( dct_ap_entry, DCT_WIFI_CONFIG_SECTION, offsetof(platform_dct_wifi_config_t, stored_ap_list), sizeof(wiced_config_ap_entry_t) );
#endif

    free( ws );
    ws = NULL;

    return WWD_SUCCESS;
}

int easy_setup_get_protocol(uint8_t* protocol) {
    *protocol = g_protocol;
    return 0;
}

