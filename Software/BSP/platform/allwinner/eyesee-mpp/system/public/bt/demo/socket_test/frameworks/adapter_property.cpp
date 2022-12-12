#define LOG_TAG "BluetoothProperty"

#include <hardware/bluetooth.h>
#include "bt.h"
#include "bt_util.h"
#include <string.h>

#define BT_DISCOVERY_STOPPED  0x00
#define BT_DISCOVERY_STARTED  0x01

//#define MAX_DEVICE   5


static int bt_discovering = 0;


typedef struct {
    bt_bdaddr_t addr;
    char name[100];
    int scan_mode;
} bt_adapter_t;

static bt_adapter_t bt_adapter;


extern bt_device_t bt_devices[MAX_DEVICE];
int bt_devices_bonded_num = 0;


static bt_device_t bonded_device;

// get the first bonded device for HFP
bt_device_t *get_last_bonded_device()
{
    return &bonded_device;
}


static void add_bonded_device(bt_bdaddr_t *addr)
{
    bt_device_t *p = NULL;
    int i = 0;

    // we always record the last bonded device.
    for (i = 0; i < 6; i++)
        bonded_device.addr.address[i] = addr->address[i];
    bonded_device.is_bond = 1;

    //ALOGI("enter %s()", __FUNCTION__);

    for (i = 0; i < MAX_DEVICE; i++)
    {
        p = &bt_devices[i];
        if (bdaddr_compare(&(p->addr), addr) == 0)
            break;
    }

    if (i == MAX_DEVICE)
        i = find_empty_device_index();

    p = &bt_devices[i];
    p->is_found = 1;
    p->is_bond = 1;
}



void adapter_properties_change_callback(int num_properties, bt_property_t *properties)
{
    int i = 0;
    bt_property_type_t type;
    char *val;

    bdt_log("%s(), num pro: %d", __FUNCTION__, num_properties);
    for (i = 0; i < num_properties; i++)
    {
        type = properties[i].type;
        val = (char *)(properties[i].val);
        //ALOGI("type: %d", type);

        if (type == BT_PROPERTY_BDNAME)
        {
            //ALOGI("adapter name is %s", val);
            memset(bt_adapter.name, 0, sizeof(bt_adapter.name));
            strcpy(bt_adapter.name, val);
        }
        else if (type == BT_PROPERTY_BDADDR)
        {
            bt_bdaddr_t *bd_addr = (bt_bdaddr_t*)(properties[i].val);
            const uint8_t *addr = bd_addr->address;
            //ALOGI("adapter mac = %02x:%02x:%02x:%02x:%02x:%02x",
            //    addr[0], addr[1], addr[2],
            //    addr[3], addr[4], addr[5]);
            int j = 0;
            for (j = 0; j < 6; j++)
                bt_adapter.addr.address[j] = addr[j];
        }
        else if (type == BT_PROPERTY_CLASS_OF_DEVICE)
        {

        }
        else if (type == BT_PROPERTY_ADAPTER_SCAN_MODE)
        {
            bt_adapter.scan_mode = *((char *)(properties[i].val));
        }
        else if (type == BT_PROPERTY_UUIDS)
        {

        }
        else if (type == BT_PROPERTY_ADAPTER_BONDED_DEVICES)
        {
            int num, k;
            bt_bdaddr_t *p = (bt_bdaddr_t *)(properties[i].val);
            //ALOGI("bonded device len: %d", properties[i].len);
            // properties[i].len == 512, mean there is no bond device.
            if (properties[i].len != 512)
            {
                num = (properties[i].len) / (sizeof(bt_bdaddr_t));
                //ALOGI("num bond devices: %d", num);
                for (k = 0; k < num; k++)
                {
                    add_bonded_device(p);
                    p++;
                }
            }
        }
        else if (type == BT_PROPERTY_ADAPTER_DISCOVERY_TIMEOUT)
        {

        }
    }

}


void adapter_discovery_state_change_callback(bt_discovery_state_t state)
{
    if (state == BT_DISCOVERY_STOPPED)
    {
        bt_discovering = 0;
    }
    else if (state == BT_DISCOVERY_STARTED)
    {
        bt_discovering = 1;
    }
}

void adapter_get_adapter_name(char name[])
{
    if (name != NULL)
        strcpy(name, bt_adapter.name);
}

void adapter_get_adapter_addr(bt_bdaddr_t *addr)
{
    if (addr != NULL)
    {
        int j = 0;
        for (j = 0; j < 6; j++)
            (addr->address)[j] = bt_adapter.addr.address[j];
    }
}

int adapter_is_discovering()
{
    return bt_discovering;
}

extern int bluetooth_set_adapter_property(int type, char value[], int len);
extern int bluetooth_set_adapter_scan_mode(bt_scan_mode_t *mode);
void adapter_set_scan_mode(bt_scan_mode_t mode)
{
    bluetooth_set_adapter_scan_mode(&mode);
}

void adapter_get_property(int type)
{
    bluetooth_get_adapter_property(type);
}

int adapter_get_scan_mode()
{
    return bt_adapter.scan_mode;
}

void adapter_set_name(char name[], int len)
{
    bluetooth_set_adapter_property(BT_PROPERTY_BDNAME, name, len);
}

void make_adapter_can_be_found(bool found)
{
    bt_scan_mode_t mode = BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE;

    if (found)
    {
        mode = BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE;
        adapter_set_scan_mode(mode);
    }
    else
    {
        mode = BT_SCAN_MODE_CONNECTABLE;
        adapter_set_scan_mode(mode);
    }
}
