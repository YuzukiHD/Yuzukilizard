/* *******************************************************************************
 * Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 * *******************************************************************************/
/**
 * @file remote_connector.h
 * @brief 第三方SDK连接适配器接口类
 * @author id:826
 * @version v0.3
 * @date 2016-08-31
 */

#pragma once

#include <string>

/** Connector Controller Type */
typedef enum {
    CTRL_TYPE_TUTK   = 0x01,
    CTRL_TYPE_ONVIF  = 0x02,
} ControllerType;

class DeviceAdapter;

class RemoteConnector {
    public:
        struct InitParam {
            std::string arg1;
            std::string arg2;
            void *reversed;
        };

        virtual ~RemoteConnector() {}
        virtual void SetAdapter(DeviceAdapter *adapter) {};
        virtual int Init(const InitParam &param) { return 0; }
        virtual int Start() { return 0;}
        virtual int Stop() {return 0;}
        virtual int SendVideoData(const char *data, int size, void *data_info, int info_size) {return 0;}
};
