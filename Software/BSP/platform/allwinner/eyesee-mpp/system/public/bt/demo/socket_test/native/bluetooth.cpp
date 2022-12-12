#define LOG_TAG "Bluetooth"

#include "bt.h"
#include "callback.h"
#include "bt_util.h"

#include <hardware/bt_sock.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <errno.h>

static const bt_interface_t *sBluetoothInterface = NULL;
static const btsock_interface_t *sBluetoothSocketInterface = NULL;

const bt_interface_t* get_bluetooth_interface()
{
    return sBluetoothInterface;
}


//////////////////////////////////////bluedroid callback///////////////////////////////////////////


static void adapter_state_changed_callb(bt_state_t status)
{
    bdt_log("enter %s()", __FUNCTION__);
    adapter_state_change_callback(status);
}

static void adapter_properties_callb(bt_status_t status, int num_properties,
                                        bt_property_t *properties)
{

    bdt_log("%s: Status is: %d, Properties: %d", __FUNCTION__, status, num_properties);

    if (status != BT_STATUS_SUCCESS)
    {
        bdt_log("%s: Status %d is incorrect", __FUNCTION__, status);
        return;
    }
    adapter_properties_change_callback(num_properties, properties);
}

static void remote_device_properties_callb(bt_status_t status, bt_bdaddr_t *bd_addr,
                                              int num_properties, bt_property_t *properties)
{
    bdt_log("%s: Status is: %d, Properties: %d", __FUNCTION__, status, num_properties);

    if (status != BT_STATUS_SUCCESS)
    {
        bdt_log("%s: Status %d is incorrect", __FUNCTION__, status);
        return;
    }
    device_property_changed_callback(bd_addr, num_properties, properties);
}

static void device_found_callb(int num_properties, bt_property_t *properties)
{
    int addr_index = 0;
    int i = 0;
    bt_bdaddr_t *bd = NULL;

    for (i = 0; i < num_properties; i++)
    {
        if (properties[i].type == BT_PROPERTY_BDADDR)
        {
            bd = (bt_bdaddr_t *)(properties[i].val);
            addr_index = i;
        }
    }

    bdt_log("%s: Properties: %d, Address: %s", __FUNCTION__, num_properties,
        (const char *)properties[addr_index].val);

    remote_device_properties_callb(BT_STATUS_SUCCESS, (bt_bdaddr_t *)properties[addr_index].val,
                                      num_properties, properties);

    remote_device_found_callback(bd);
}

static void discovery_state_changed_callb(bt_discovery_state_t state)
{
    bdt_log("%s: DiscoveryState:%d ", __FUNCTION__, state);
    adapter_discovery_state_change_callback(state);
}

static void pin_request_callb(bt_bdaddr_t *bd_addr, bt_bdname_t *bdname, uint32_t cod)
{
	bdt_log("%s() cod: %d", __FUNCTION__, cod);
}

static void ssp_request_callb(bt_bdaddr_t *bd_addr, bt_bdname_t *bdname, uint32_t cod,
                                 bt_ssp_variant_t pairing_variant, uint32_t pass_key)
{
	bdt_log("%s() cod: %d, ssp_variant: %d, pass_key: %d", __FUNCTION__, cod, pairing_variant, pass_key);
	__ssp_request_callb(bd_addr, bdname, cod, pairing_variant, pass_key);
}

static void bond_state_changed_callb(bt_status_t status, bt_bdaddr_t *bd_addr,
                                        bt_bond_state_t state)
{
    if (!bd_addr)
    {
        bdt_log("Address is null in %s", __FUNCTION__);
        return;
    }
    bond_state_change_call_back(status, bd_addr, state);
}

static void acl_state_changed_callb(bt_status_t status, bt_bdaddr_t *bd_addr,
                                       bt_acl_state_t state)
{
    bdt_log("%s(), state: %d", __FUNCTION__, state);
    acl_state_changed_call_back(status, bd_addr, state);
}

static void thread_event_callb(bt_cb_thread_evt event)
{

}

static void dut_mode_recv_callb (uint16_t opcode, uint8_t *buf, uint8_t len)
{

}



static bt_callbacks_t sBluetoothCallbacks = {
    sizeof(bt_callbacks_t),
    adapter_state_changed_callb,
    adapter_properties_callb, /*adapter_properties_cb */
    remote_device_properties_callb, /* remote_device_properties_cb */
    device_found_callb, /* device_found_cb */
    discovery_state_changed_callb, /* discovery_state_changed_cb */
    pin_request_callb, /* pin_request_cb  */
    ssp_request_callb, /* ssp_request_cb  */
    bond_state_changed_callb, /*bond_state_changed_cb */
    acl_state_changed_callb, /* acl_state_changed_cb */
    thread_event_callb, /* thread_evt_cb */
    dut_mode_recv_callb, /*dut_mode_recv_cb */
};


//////////////////////////////////////////Native API//////////////////////////////////////////


int bluetooth_start_discovery()
{
    //ALOGV("%s:",__FUNCTION__);

    int result = -1;
    if (!sBluetoothInterface) return result;

    result = sBluetoothInterface->start_discovery();

    return result;
}


int bluetooth_cancel_discovery()
{
    //ALOGV("%s:",__FUNCTION__);

    int result = -1;
    if (!sBluetoothInterface) return result;

    result = sBluetoothInterface->cancel_discovery();

    return result;
}


int bluetooth_create_bond(bt_bdaddr_t *address)
{
    //ALOGV("%s:",__FUNCTION__);

    int result = -1;

    if (!sBluetoothInterface) return result;

    result = sBluetoothInterface->create_bond((bt_bdaddr_t *)address);

    return result;
}


int bluetooth_remove_bond(bt_bdaddr_t *address)
{
    //ALOGV("%s:",__FUNCTION__);

    int result;
    if (!sBluetoothInterface) return -1;

    result = sBluetoothInterface->remove_bond((bt_bdaddr_t *)address);

    return result;
}


int bluetooth_cancel_bond(bt_bdaddr_t *address)
{
    //ALOGV("%s:",__FUNCTION__);

    int result;
    if (!sBluetoothInterface) return -1;

    result = sBluetoothInterface->cancel_bond((bt_bdaddr_t *)address);

    return result;
}


int bluetooth_pin_reply(bt_bdaddr_t *address, int accept, int len, char pinArray[])
{
    //ALOGV("%s() accept=%d len=%d pin=%s", __FUNCTION__, accept, len, pinArray);

    int result = -1;
    if (!sBluetoothInterface) return result;

    result = sBluetoothInterface->pin_reply((bt_bdaddr_t*)address, accept, len,
                                              (bt_pin_code_t *) pinArray);

    return result;
}


int bluetooth_ssp_reply_native(bt_bdaddr_t *address, int type, int accept, int passkey)
{
    //ALOGV("%s() type=%d accept=%d passkey=%d", __FUNCTION__, type, accept, passkey);

    int result = -1;
    if (!sBluetoothInterface) return result;

    result = sBluetoothInterface->ssp_reply((bt_bdaddr_t *)address,
         (bt_ssp_variant_t) type, accept, passkey);

    return result;
}


int bluetooth_set_adapter_property(int type, char value[], int len)
{
    bdt_log("%s:",__FUNCTION__);

    int result = -1;
    if (!sBluetoothInterface) return result;

    bt_property_t prop;
    prop.type = (bt_property_type_t)type;
    prop.len = len;
    prop.val = value;

    result = sBluetoothInterface->set_adapter_property(&prop);

    return result;
}

int bluetooth_set_adapter_scan_mode(bt_scan_mode_t *mode)
{
    bdt_log("%s:",__FUNCTION__);

    int result = -1;
    if (!sBluetoothInterface) return result;

    bt_property_t prop;
    prop.type = (bt_property_type_t)BT_PROPERTY_ADAPTER_SCAN_MODE;
    prop.len = 1;
    prop.val = (void *)mode;

    result = sBluetoothInterface->set_adapter_property(&prop);

    return result;
}

int bluetooth_get_adapter_properties()
{
    bdt_log("%s:",__FUNCTION__);

    int result = -1;
    if (!sBluetoothInterface) return result;

    result = sBluetoothInterface->get_adapter_properties();

    return result;
}


int bluetooth_get_adapter_property(int type)
{
    bdt_log("%s:",__FUNCTION__);

    int result = -1;
    if (!sBluetoothInterface) return result;

    result = sBluetoothInterface->get_adapter_property((bt_property_type_t)type);

    return result;
}


int bluetooth_get_device_property(bt_bdaddr_t *address, int type)
{
    bdt_log("%s:",__FUNCTION__);

    int result = -1;
    if (!sBluetoothInterface) return result;

    result = sBluetoothInterface->get_remote_device_property((bt_bdaddr_t *)address,
                                                              (bt_property_type_t)type);

    return result;
}


int bluetooth_set_device_property(bt_bdaddr_t *address, int type, char value[], int len)
{
    bdt_log("%s:",__FUNCTION__);

    int result = -1;
    if (!sBluetoothInterface) return result;

    bt_property_t prop;
    prop.type = (bt_property_type_t)type;
    prop.len = len;
    prop.val = value;

    result = sBluetoothInterface->set_remote_device_property((bt_bdaddr_t *)address, &prop);

    return result;
}


int bluetooth_connect_socket_native(bt_bdaddr_t *addr, int type, char uuid[], int channel, int flag)
{
    int socket_fd;
    bt_status_t status;

    if (!sBluetoothSocketInterface)
    {
        bdt_log("sBluetoothSocketInterface is NULL.");
        return -1;
    }

    if ( (status = sBluetoothSocketInterface->connect((bt_bdaddr_t *) addr, (btsock_type_t) type,
                       (const uint8_t*) uuid, channel, &socket_fd, flag)) != BT_STATUS_SUCCESS)
    {
        bdt_log("Socket connection failed: %d", status);
        goto Fail;
    }

    if (socket_fd < 0)
    {
        bdt_log("Fail to create file descriptor on socket fd");
        goto Fail;
    }

    return socket_fd;

Fail:
    return -1;
}

int bluetooth_create_socket_channel_native(int type, char *service_name, char uuid[], int channel, int flag)
{
    int socket_fd;
    bt_status_t status;

    if (!sBluetoothSocketInterface)
    {
        bdt_log("sBluetoothSocketInterface is NULL.");
        return -1;
    }

    if ((status = sBluetoothSocketInterface->listen((btsock_type_t) type, service_name,
                       (const uint8_t*) uuid, channel, &socket_fd, flag)) != BT_STATUS_SUCCESS)
    {
        bdt_log("Socket listen failed: %d", status);
        goto Fail;
    }

    if (socket_fd < 0)
    {
        bdt_log("Fail to create file descriptor on socket fd");
        goto Fail;
    }

    return socket_fd;

Fail:
    return -1;
}

int bluetooth_enable()
{
    //ALOGV("%s:",__FUNCTION__);

    int ret = -1;
    if (!sBluetoothInterface) return ret;

    ret = sBluetoothInterface->enable();

    return ret;
}

int bluetooth_disable()
{
    //ALOGV("%s:",__FUNCTION__);

    int ret = -1;
    if (!sBluetoothInterface) return ret;

    ret = sBluetoothInterface->disable();

    return ret;
}


static int load(const char *id,
        const char *path,
        const struct hw_module_t **pHmi)
{
    int status;
    void *handle;
    struct hw_module_t *hmi;

    const char *sym = HAL_MODULE_INFO_SYM_AS_STR;
    /*
     * load the symbols resolving undefined symbols before
     * dlopen returns. Since RTLD_GLOBAL is not or'd in with
     * RTLD_NOW the external symbols will not be global
     */
    handle = dlopen(path, RTLD_NOW);
    if (handle == NULL)
    {
        char const *err_str = dlerror();
        bdt_log("load: module=%s\n%s", path, err_str?err_str:"unknown");
        status = -EINVAL;
        goto done;
    }

    /* Get the address of the struct hal_module_info. */
    hmi = (struct hw_module_t *)dlsym(handle, sym);
    if (hmi == NULL)
    {
        bdt_log("load: couldn't find symbol %s", sym);
        status = -EINVAL;
        goto done;
    }

    /* Check that the id matches */
    if (strcmp(id, hmi->id) != 0)
    {
        bdt_log("load: id=%s != hmi->id=%s", id, hmi->id);
        status = -EINVAL;
        goto done;
    }

    hmi->dso = handle;

    /* success */
    status = 0;

    /* Check that the id matches */
    if (strcmp(id, hmi->id) != 0)
    {
        bdt_log("load: id=%s != hmi->id=%s", id, hmi->id);
        status = -EINVAL;
        goto done;
    }

    hmi->dso = handle;

    /* success */
    status = 0;

done:
    if (status != 0)
    {
        hmi = NULL;
        if (handle != NULL)
        {
            dlclose(handle);
            handle = NULL;
        }
    }
    else
    {
        //bdt_log("loaded HAL id=%s path=%s hmi=%p handle=%p",
        //       id, path, *pHmi, handle);
        bdt_log("load HAL lib succ...");
    }

    *pHmi = hmi;

    return status;
}

static int my_hw_get_module(const char *id, const struct hw_module_t **module) {
    int status;

    status = load(id, "bluetooth.default.so", module);
    return status;
}


int bluetooth_init()
{
    int err;
    int ret;
    hw_module_t* module;

    const char *id = BT_STACK_MODULE_ID;

    err = my_hw_get_module(id, (hw_module_t const**)&module);
    if (err == 0)
    {
        hw_device_t* abstraction;
        err = module->methods->open(module, id, &abstraction);
        if (err == 0)
        {
            bluetooth_module_t* btStack = (bluetooth_module_t *)abstraction;
            sBluetoothInterface = btStack->get_bluetooth_interface();
        }
        else
        {
           bdt_log("Error while opening Bluetooth library");
        }
    }
    else
    {
        bdt_log("No Bluetooth Library found");
    }

    if (sBluetoothInterface)
    {
        ret = sBluetoothInterface->init(&sBluetoothCallbacks);
        if (ret != BT_STATUS_SUCCESS)
        {
            bdt_log("Error while setting the callbacks: %d\n", ret);
            sBluetoothInterface = NULL;
            return -1;
        }

        if ((sBluetoothSocketInterface = (btsock_interface_t *)
                  sBluetoothInterface->get_profile_interface(BT_PROFILE_SOCKETS_ID)) == NULL)
        {
            bdt_log("Error getting socket interface");
        }
    }

    return err;
}

int bluetooth_cleanup()
{
    bdt_log("%s:",__FUNCTION__);

    int ret = -1;
    if (!sBluetoothInterface) return ret;

    sBluetoothInterface->cleanup();
    bdt_log("%s: return from cleanup", __FUNCTION__);

    return 0;
}
