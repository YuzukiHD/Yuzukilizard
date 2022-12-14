#ifndef __OPENAMP_SUNXI_HELPER_OPENAMP_LOG_H__
#define __OPENAMP_SUNXI_HELPER_OPENAMP_LOG_H__

#include <stdio.h>
#include <hal_log.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OPENAMP_DBG

#ifdef OPENAMP_DBG
#define openamp_dbg(fmt, args...) \
	hal_log_debug("[AMP_DBG]" fmt, ##args)
#define openamp_dbgFromISR(fmt, args...) \
	hal_log_debug("[AMP_DBG]" fmt, ##args)
#else
#define openamp_dbg(fmt, args...)
#define openamp_dbgFromISR(fmt, args...)
#endif /* OPENAMP_DBG */

#define openamp_info(fmt, args...) \
	hal_log_info("[AMP_INFO]" fmt, ##args)
#define openamp_infoFromISR(fmt, args...) \
	hal_log_info("[AMP_INFO]" fmt, ##args)

#define openamp_err(fmt, args...) \
	hal_log_err("[AMP_ERR]" fmt, ##args)
#define openamp_errFromISR(fmt, args...) \
	hal_log_err("[AMP_ERR]" fmt, ##args)

#ifdef __cplusplus
}
#endif

#endif /* ifndef __OPENAMP_SUNXI_HELPER_OPENAMP_LOG_H__ */

