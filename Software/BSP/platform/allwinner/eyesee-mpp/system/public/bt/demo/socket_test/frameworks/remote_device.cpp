#define LOG_TAG "BluetoothRemoteDevice"
#include "bt_util.h"
#include <hardware/bluetooth.h>
#include "bt.h"
#include <string.h>
#include <stdio.h>

//#define MAX_DEVICE   20


bt_device_t bt_devices[MAX_DEVICE];
int bt_devices_found_num = 0;


void remote_device_found_callback(bt_bdaddr_t *bd)
{

    const uint8_t *addr = bd->address;

    printf("remote device found: %02x:%02x:%02x:%02x:%02x:%02x",
                addr[0], addr[1], addr[2],
                addr[3], addr[4], addr[5]);
}


bt_device_t *get_bt_device(bt_bdaddr_t *bd)
{
    int i = 0;

    for (i = 0; i < MAX_DEVICE; i++)
    {
        if (!bdaddr_compare(bd, &bt_devices[i].addr))
            return &(bt_devices[i]);
    }
    return NULL;
}

int find_empty_device_index()
{
    int i = 0;

    for (i = 0; i < MAX_DEVICE; i++)
    {
        if (bt_devices[i].is_found == 0)
            return i;
    }

    return 0;
}

void device_property_changed_callback(bt_bdaddr_t *bd_addr, int num_properties, bt_property_t *properties)
{

    int i = 0;
    bt_device_t *device;
    int index = 0;
    bt_property_type_t type;
    char *val;

    device = get_bt_device(bd_addr);
    if (device == NULL)
    {
        index = find_empty_device_index();
        device = &bt_devices[index];
        device->is_found = 1;

        for (i = 0; i < num_properties; i++)
        {
            type = properties[i].type;
            val = (char *)(properties[i].val);

            if (type == BT_PROPERTY_BDNAME)
            {
                //ALOGI("remote device name is %s", val);
                memset(device->name, 0, sizeof(device->name));
                strcpy(device->name, val);
            }
            else if (type == BT_PROPERTY_BDADDR)
            {
                bt_bdaddr_t *bd_addr = (bt_bdaddr_t*)(properties[i].val);
                const uint8_t *addr = bd_addr->address;
                //ALOGI("remote device mac = %02x:%02x:%02x:%02x:%02x:%02x",
                //    addr[0], addr[1], addr[2],
                //    addr[3], addr[4], addr[5]);
                int j = 0;
                for (j = 0; j < 6; j++)
                    (device->addr).address[j] = addr[j];
            }
            else if (type == BT_PROPERTY_CLASS_OF_DEVICE)
            {
                device->cod = (*(uint32_t *)(properties[i].val));
				//ALOGI("remote device cod: %d", device->cod);
            }
            else if (type == BT_PROPERTY_TYPE_OF_DEVICE)
            {
                device->tod = (*(uint8_t *)(properties[i].val));
            }
            else if (type == BT_PROPERTY_UUIDS)
            {
                int num = (properties[i].len) / (sizeof(bt_uuid_t));
                bt_uuid_t *p_uuid = (bt_uuid_t *)(properties[i].val);
                int m = 0;
                for (m = 0; m < MAX_UUID; m++)
                    memset((device->uuid[m]).uu, 0, sizeof(bt_uuid_t));
                for (m = 0; m < num; m++)
                {
                    memcpy((device->uuid[m]).uu, p_uuid, sizeof(bt_uuid_t));
                    p_uuid++;
                }
            }
            else if (type == BT_PROPERTY_REMOTE_RSSI)
            {
                device->rssi = (*(int8_t *)(properties[i].val));
            }
        }

    }

}

