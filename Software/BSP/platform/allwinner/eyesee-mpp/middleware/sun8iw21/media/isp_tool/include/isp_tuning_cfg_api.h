/*
 ******************************************************************************
 * 
 * isp_tuning_cfg_api.h
 * 
 * Hawkview ISP - isp_tuning_cfg_api.h module
 * 
 * Copyright (c) 2016 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 * 
 * Version		  Author         Date		    Description
 * 
 *   1.0		  clarkyy	2016/05/31	VIDEO INPUT
 * 
 *****************************************************************************
 */

#ifndef _ISP_TUNING_CFG_API_H_
#define _ISP_TUNING_CFG_API_H_

#include "isp_tuning_cfg.h"

#if 1
/* some essential structs and functions for test */

struct isp_lib_context {
	struct isp_param_config isp_ini_cfg;
};

struct isp_tuning {
	struct hw_isp_device *isp;
	struct isp_lib_context *ctx;
	struct isp_param_config params;
	unsigned int frame_count;
	unsigned int pix_max;
	//pthread_mutex_t mutex;
};

struct hw_isp_device {
	unsigned int id;
	/*struct isp_sensorint sensor;
	struct isp_subdev isp_sd;
	struct isp_stats stats_sd;
	enum isp_state state;
	const struct isp_dev_operations *ops;
	void *priv;*/
	void *ctx;
	void *tuning;
	
};

void *isp_dev_get_tuning(struct hw_isp_device *isp);
#endif



/*
 * get isp cfg
 * @isp: hw isp device
 * @group_id: group id
 * @cfg_ids: cfg ids, supports one or more ids combination by '|'(bit operation)
 * @cfg_data: cfg data to get, data sorted by @cfg_ids
 * @returns: cfg data length, negative if something went wrong
 */
HW_S32 hw_get_isp_cfg(struct hw_isp_device *isp, HW_U8 group_id, HW_U32 cfg_ids, void *cfg_data);

/*
 * set isp cfg
 * @isp: hw isp device
 * @group_id: group id
 * @cfg_id: cfg ids, supports one or more ids combination by '|'(bit operation)
 * @cfg_data: cfg data to set, data sorted by @cfg_ids
 * @returns: cfg data length, negative if something went wrong
 */
HW_S32 hw_set_isp_cfg(struct hw_isp_device *isp, HW_U8 group_id, HW_U32 cfg_ids, void *cfg_data);


#endif /* _ISP_TUNING_CFG_API_H_ */
