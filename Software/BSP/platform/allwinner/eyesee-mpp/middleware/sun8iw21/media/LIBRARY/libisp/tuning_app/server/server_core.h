#ifndef _SERVER_CORE_H_V100_
#define _SERVER_CORE_H_V100_

#include "capture_image.h"
struct ToolsIniTuning_cfg {
	int enable;
	char base_path[50];
	sensor_input sensor_cfg;
	struct isp_param_config *params;
};
void SetIniTuningEn(struct ToolsIniTuning_cfg cfg);
struct ToolsIniTuning_cfg GetIniTuningEn(void);

/*
 * socket handle threads for heart jump, preview, capture, tuning, statistics, shell script and so on
 * params: client socket fd
 */
void *sock_handle_heart_jump_thread(void *sock_th_params);
void *sock_handle_preview_thread(void *sock_th_params);
void *sock_handle_capture_thread(void *sock_th_params);
void *sock_handle_blockinfo_thread(void *sock_th_params);
void *sock_handle_tuning_thread(void *sock_th_params);
void *sock_handle_log_thread(void *params);
void *sock_handle_statistics_thread(void *sock_th_params);
void *sock_handle_script_thread(void *sock_th_params);
void *sock_handle_register_thread(void *sock_th_params);
void *sock_handle_aelv_thread(void *sock_th_params);
void *sock_handle_set_input_thread(void *sock_th_params);
void *sock_handle_raw_flow_thread(void *sock_th_params);
void *sock_handle_isp_version_thread(void *sock_th_params);
void *sock_handle_preview_vencode_thread(void *params);
/*
 * check all threads status
 * one thread uses a bit, 0 - not run/exit, 1 - running
 * returns all threads status
 *    bit[0] - heart jump
 *    bit[1] - preview
 *    ......
 *    bit[24] - 1 to quit server
 *    others reserved
 */
typedef enum _thread_status_flag_e {
	TH_STATUS_HEART_JUMP               = 0x00000001,
	TH_STATUS_PREVIEW                  = 0x00000002,
	TH_STATUS_CAPTURE                  = 0x00000004,
	TH_STATUS_TUNING                   = 0x00000008,
	TH_STATUS_STATISTICS               = 0x00000010,
	TH_STATUS_SCRIPT                   = 0x00000020,
	TH_STATUS_REGISTER                 = 0x00000040,
	TH_STATUS_AELV                     = 0x00000080,
	TH_STATUS_SET_INPUT                = 0x00000100,
	TH_STATUS_RAW_FLOW                 = 0x00000200,
	TH_STATUS_VENC_TUNING              = 0x00000400,
	TH_STATUS_PREVIEW_VENCODE          = 0x00000800,
	TH_STATUS_BLOCK_INFO               = 0x00001000,
	TH_STATUS_LOG                      = 0x00002000,
	// others
	//...
	TH_STATUS_SERVER_QUIT              = 0x01000000,
} eThreadStatusFlag;

int CheckThreadsStatus();

#endif /* _SERVER_CORE_H_V100_ */

