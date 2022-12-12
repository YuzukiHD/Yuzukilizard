#ifndef _BLUETOOTH_H
#define _BLUETOOTH_H

#include <hardware/hardware.h>
#include <hardware/bluetooth.h>


const bt_interface_t* get_bluetooth_interface();

int bluetooth_init();
int bluetooth_cleanup();
int bluetooth_enable();
int bluetooth_disable();
int bluetooth_start_discovery();
int bluetooth_cancel_discovery();
int bluetooth_create_bond(bt_bdaddr_t *address);
int bluetooth_remove_bond(bt_bdaddr_t *address);
int bluetooth_cancel_bond(bt_bdaddr_t *address);
int bluetooth_set_adapter_property(int type, char value[]);
int bluetooth_get_adapter_property(int type);
int bluetooth_pin_reply(bt_bdaddr_t *address, int accept, int len, char pinArray[]);
int bluetooth_ssp_reply_native(bt_bdaddr_t *address, int type, int accept, int passkey);

// client socket
int bluetooth_connect_socket_native(bt_bdaddr_t *addr, int type, char uuid[], int channel, int flag);
// server socket
int bluetooth_create_socket_channel_native(int type, char *service_name, char uuid[], int channel, int flag);

int adapter_get_adapter_state();
int adapter_get_adapter_conn_state();
int adapter_is_discovering();

void adapter_get_adapter_name(char name[]);
void adapter_get_adapter_addr(bt_bdaddr_t *addr);
void adapter_set_scan_mode(bt_scan_mode_t mode);
void adapter_set_name(char name[]);
void adapter_get_property(int type);


extern int bdaddr_compare(const bt_bdaddr_t *a, const bt_bdaddr_t *b);


#define MAX_DEVICE   20
#define MAX_UUID     10
typedef struct {
    bt_bdaddr_t addr;
    char name[100];
    uint32_t cod;
    uint8_t tod;
    int8_t rssi;
    int is_bond;
    int is_found;
    int is_connected;
    bt_uuid_t uuid[MAX_UUID];
} bt_device_t;


typedef enum {
    BT_TRANSPORT_UNKNOWN = 0,
    BT_TRANSPORT_BR_EDR,
    BT_TRANSPORT_LE
} bt_transport_t;


typedef void (*bond_changed_callback)(bt_status_t status, bt_bdaddr_t *bd_addr, bt_bond_state_t state);
void bluetooth_register_bond_state_changed_callback(bond_changed_callback cb);

typedef void (*state_changed_callback)(int state);
void bluetooth_register_adapter_state_changed_callback(state_changed_callback cb);

typedef void (*con_state_changed_callback)(bt_status_t status, bt_bdaddr_t *bd_addr, bt_acl_state_t state);
void bluetooth_register_conn_state_changed_callback(con_state_changed_callback cb);

bt_device_t *get_bt_device(bt_bdaddr_t *bd);
int find_empty_device_index();
bt_device_t *get_last_bonded_device();

void make_adapter_can_be_found(bool found);

#define COD_UNCLASSIFIED                    ((0x1F) << 8)
#define COD_HID_KEYBOARD                    0x0540
#define COD_HID_POINTING                    0x0580
#define COD_HID_COMBO                       0x05C0
#define COD_HID_MAJOR                       0x0500
#define COD_AV_HEADSETS                     0x0404
#define COD_AV_HANDSFREE                    0x0408
#define COD_AV_HEADPHONES                   0x0418
#define COD_AV_PORTABLE_AUDIO               0x041C
#define COD_AV_HIFI_AUDIO                   0x0428


#endif