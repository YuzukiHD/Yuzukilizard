#ifndef _BLUETOOTH_CALLBACK_H
#define _BLUETOOTH_CALLBACK_H

#include <hardware/bluetooth.h>

extern void adapter_state_change_callback(int status);

extern void adapter_properties_change_callback(int num_properties, bt_property_t *properties);

extern void remote_device_found_callback(bt_bdaddr_t *bd);

extern void adapter_discovery_state_change_callback(bt_discovery_state_t state);

extern void device_property_changed_callback(bt_bdaddr_t *bd_addr, int num_properties, bt_property_t *properties);

extern void bond_state_change_call_back(bt_status_t status, bt_bdaddr_t *bd_addr, bt_bond_state_t state);

extern void __ssp_request_callb(bt_bdaddr_t *bd_addr, bt_bdname_t *bdname, uint32_t cod,
                                 bt_ssp_variant_t pairing_variant, uint32_t pass_key);

extern void acl_state_changed_call_back(bt_status_t status, bt_bdaddr_t *bd_addr, bt_acl_state_t state);

#endif