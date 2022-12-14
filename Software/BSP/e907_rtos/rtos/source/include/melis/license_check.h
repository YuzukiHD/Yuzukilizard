#ifndef _CHECK_H_
#define _CHECK_H_

#include <typedef.h>
#include <kapi.h>
#include <mod_defs.h>

int app_license_check_init(void *hdle, uint8_t *code1, uint8_t *code2);
int app_ioctrl_check(__mp *mp, __u32 cmd,  __s32 aux, void *pbuffer);

int kernel_license_check_init(uint8_t *code);
int kernel_ioctrl_check(void *hdle, int cmd, void *arg);

int check_plugin(void *hdle, uint8_t *code);
int plugin_ioctrl_check(__mp *mp, __u32 cmd,  __s32 aux, void *pbuffer);

#endif
