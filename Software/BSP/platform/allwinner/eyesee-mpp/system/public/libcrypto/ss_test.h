/*
 *
 * Copyright (C) 2013 Allwinner.
 *
 * Mintow <duanmintao@allwinnertech.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef _SUNXI_SECURITY_SYSTEM_H_
#define _SUNXI_SECURITY_SYSTEM_H_

#define SS_TEST_BUF_SIZE    (8*1024)

#define DBG(string, args...) \
    do { \
        printf("%s, %u - ", __FILE__, __LINE__); \
        printf(string, ##args); \
    } while (0)
#define ENGINE_load_builtin_engines ENGINE_load_af_alg
#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif

extern void ENGINE_load_af_alg(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif /* end of _SUNXI_SECURITY_SYSTEM_H_ */

