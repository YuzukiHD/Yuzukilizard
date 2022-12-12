#define LOG_TAG "BluetoothBondState"

#include <hardware/bluetooth.h>
#include "bt.h"
#include <stdio.h>
#include <stdlib.h>


static bond_changed_callback callback = NULL;


void bond_state_change_call_back(bt_status_t status, bt_bdaddr_t *bd_addr, bt_bond_state_t state)
{
    if (callback != NULL)
        callback(status, bd_addr, state);
}

void bluetooth_register_bond_state_changed_callback(bond_changed_callback cb)
{
    callback = cb;
}


void __ssp_request_callb(bt_bdaddr_t *bd_addr, bt_bdname_t *bdname, uint32_t cod,
                                 bt_ssp_variant_t pairing_variant, uint32_t pass_key)
{
	// accept automatically
	int accept = 1;
	bluetooth_ssp_reply_native(bd_addr, pairing_variant, accept, 0);
}

