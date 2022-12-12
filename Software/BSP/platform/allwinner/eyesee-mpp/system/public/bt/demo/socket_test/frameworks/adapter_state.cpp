#define LOG_TAG "BluetoothState"

#include <stdio.h>
#include "bt_util.h"
#include "bt.h"

static int bt_enabled = -1;
static state_changed_callback state_callback = NULL;

static int bt_conn = -1;
static con_state_changed_callback con_callback = NULL;

void adapter_state_change_callback(int status)
{
    if (status == BT_STATE_OFF)
    {
        //ALOGI("bluetooth is off...");
        bt_enabled = 0;
    }
    else if (status == BT_STATE_ON)
    {
        //ALOGI("bluetooth is on...");
        bt_enabled = 1;
    }
    else
    {
        bdt_log("Incorrect status in stateChangeCallback");
    }

    if (state_callback != NULL)
        state_callback(status);
}

void bluetooth_register_adapter_state_changed_callback(state_changed_callback cb)
{
    state_callback = cb;
}


int adapter_get_adapter_state()
{
    return bt_enabled;
}

void acl_state_changed_call_back(bt_status_t status, bt_bdaddr_t *bd_addr, bt_acl_state_t state)
{
    bt_conn = state;

    if (con_callback != NULL)
        con_callback(status, bd_addr, state);
}

void bluetooth_register_conn_state_changed_callback(con_state_changed_callback cb)
{
    con_callback = cb;
}

int adapter_get_adapter_conn_state()
{
    return bt_conn;
}
