#define LOG_TAG "BlueCar"

#include "bt_util.h"
#include <hardware/bluetooth.h>
#include "bt.h"


#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>


bt_bdaddr_t target_device;


static void bond_state_change(bt_status_t status, bt_bdaddr_t *bd_addr, bt_bond_state_t state)
{
	char addr[20];
	bd2str(bd_addr, addr);

    if (state == BT_BOND_STATE_BONDED)
    {
        bdt_log("remote device %s bonded...", addr);
    }
    else if (state == BT_BOND_STATE_BONDING)
    {
        bdt_log("remote device %s bonding...", addr);
    }
    else if (state == BT_BOND_STATE_NONE)
    {
        bdt_log("remote device %s bond none...", addr);
    }
}


extern int sock_init();

int main(int argc, char *argv[])
{
    
    bt_device_t *dev;
    bt_bdaddr_t addr;
    int flag = 0;

	// init bt.
    bluetooth_init();
	// enable bt.
    bluetooth_enable();

	// wait until bt adapter on.
    while(adapter_get_adapter_state() != 1)
    {
        sleep(1);
    }


	// we do not need to discovery any device.
    if (adapter_is_discovering() == 1)
        bluetooth_cancel_discovery();

    sock_init();
    sleep(1);

    // make the device visible.
	make_adapter_can_be_found(true);

/*
    dev = get_last_bonded_device();
    // if any device had been bonded.
    if (dev->is_bond == 1) {
        char addr[20];
        bd2str(&(dev->addr), addr);
        ALOGI("Bonded device: %s\n", addr);
        a2dp_sink_connect_native(&(dev->addr));
        hfpclient_connect_native(&(dev->addr));
        sleep(2);
    }
*/


	// dead loop, in order to keep this process alive.
    while(1)
    {
        sleep(5);
    }

    return 0;
}