/*
Copyright (c) 2020-2030 allwinnertech.com JetCui<cuiyuntao@allwinnertech.com>

*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <memory.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <dfs_posix.h>
#include "mmap/mman.h"
#include "../trans_header.h"
#include "../sunxi_display2.h"
#include "de_generic.h"

#ifndef DEXX_SO_SP
extern int trans_to_spec(int disp_fd, int de, void *data, int cmd);
extern int download_firmware_spec(int disp_fd, int id, int de, char* buffer, trans_firm_data_t func);
extern int updata_firm_spec(char* buf, int length, struct trans_base *data, data_to_firm_t func);

#else
typedef int (*trans_to_spec_t)(int disp_fd, int de, void *data, int cmd);
typedef int (*download_firmware_spec_t)(int disp_fd, int id, int de, char* buffer, trans_firm_data_t func);
typedef int (*updata_firm_spec_t)(char* buf, int length, struct trans_base *data, data_to_firm_t func);

trans_to_spec_t trans2spec = NULL;
download_firmware_spec_t download_firm_spec = NULL;
updata_firm_spec_t updata_firm_spec = NULL;

#endif
int de = 0;
int ic = 100;
int de_version = -1;

int trans_to_dexx(int disp_fd, void *data, int cmd)
{
	int ret = -1;
	struct trans_base *base_data = (struct trans_base *)(data);
	unsigned long arg[4] = {0};
	switch (base_data->id) {
	case 1:
	{
		struct ic_base * ic_de = (struct ic_base *)(data);
		/* get the de and ic version */
		if (cmd == WRITE_CMD) {
			de = ic_de->de;
		}
		if (cmd == READ_CMD) {
			ic_de->de = de_version;
			ic_de->ic = ic;
		}
#if PQ_DEBUG_LEVEL
		PQ_Printf("CMD:%d, %d  %d", cmd, ic_de->de, ic_de->ic);
#endif

	}
	break;
	case 2:
	{
		arg[1] = (unsigned long)de;
		arg[2]= (unsigned long)&base_data->xxx;
		arg[3]= (unsigned long)1;
#if PQ_DEBUG_LEVEL
		struct Regiser_pair * set = (struct Regiser_pair *)&base_data->xxx;
		PQ_Printf("%p read:%08x=%08x", set,set->offset, set->value);
#endif
		if (cmd == WRITE_CMD) {
			arg[0]= PQ_SET_REG;
		}else {
			arg[0]= PQ_GET_REG;
		}
		/* get display type to  verify the DISP_TV_SET_GAMMA_TABLE */
		ret = ioctl(disp_fd, DISP_PQ_PROC, (unsigned long)arg);
		if (ret) {
			PQ_Printf("err: DISP_PQ_PROC single %d failed: %d", cmd, __LINE__);
			return -1;
		}
#if PQ_DEBUG_LEVEL
		PQ_Printf("get:%08x=%08x\n", set->offset, set->value);
#endif
	}
	break;
	case 3:
	{
		struct Regiser_set *reg_set = (struct Regiser_set *)(data);
		arg[1] = (unsigned long)de;
		arg[2]= (unsigned long)&reg_set->set[0];
		arg[3]=(unsigned long)reg_set->count;

		if (cmd == WRITE_CMD) {
			arg[0]= PQ_SET_REG;
		}else {
			arg[0]= PQ_GET_REG;
		}
		/* get display type to  verify the DISP_TV_SET_GAMMA_TABLE */
		ret = ioctl(disp_fd, DISP_PQ_PROC, (unsigned long)arg);
		if (ret) {
			PQ_Printf("err: DISP_PQ_PROC set %d failed: %d", cmd, __LINE__);
			return -1;
		}
	}
	break;
	case 4:

	break;
	case 5:

	break;
	default:
#ifndef DEXX_SO_SP
		ret = trans_to_spec(disp_fd, de, base_data, cmd);
#else
		if (trans2spec != NULL) {
			ret = trans2spec(disp_fd, de, base_data, cmd);
		} else {
			PQ_Printf("trans2spec is NULL...");
		}
#endif
		if (ret < 0) {
			PQ_Printf("tran to spec ic err...");
		}
	}

	return 0;
}

int download_firmware_generic(int disp_fd, int id, int de_d, char* buffer, trans_firm_data_t func)
{
	int i = 0, de_swp = de, ret = -1;
	long *data = NULL;
	de = de_d;

	switch (id) {
	case 1:
	{
		/* get the de and ic version */
	}
	break;
	case 2:
	{
		struct Regiser_single set;
		set.id = 2;
		i = 2;
		data = func(buffer, &i);
		if(data == NULL) {
			PQ_Printf("de_generic [%d] Trans err..", __LINE__);
			goto err;
		}
		set.reg.offset = data[0];
		set.reg.value = data[1];
		trans_to_dexx(disp_fd, &set, WRITE_CMD);
		free(data);
	}
	break;
	case 3:
	{
		struct Regiser_set *reg_set = NULL;
		int j = 0;
		i = 0;
		data = func(buffer, &i);
		if(data == NULL) {
			PQ_Printf("de_generic [%d] Trans err..", __LINE__);
			goto err;
		}
		reg_set = (struct Regiser_set *)malloc(sizeof(struct Regiser_set)
							+ sizeof(struct Regiser_pair)* i/2);
		reg_set->id = 3;
		reg_set->count = i;
		while (j < i) {
			reg_set->set[j].offset = data[2*j];
			reg_set->set[j].value = data[2*j + 1];
			j++;
		}
		trans_to_dexx(disp_fd, reg_set, WRITE_CMD);
		free(data);
		free(reg_set);
	}
	break;
	case 4:

	break;
	case 5:

	break;
	default:
#ifndef DEXX_SO_SP

		ret = download_firmware_spec(disp_fd, id, de_d, buffer, func);
#else
		if (download_firm_spec != NULL) {
			ret  = download_firm_spec(disp_fd, id, de_d, buffer, func);
		} else {
			PQ_Printf("download_firm_spec is NULL...");
		}
#endif
		if (ret < 0) {
			PQ_Printf("download_firmware_spec err...");
		}
	}

	de = de_swp;
	return 0;
err:
	de = de_swp;
	return -1;
}

char* updata_firm_generic(struct PQ_package *packge, data_to_firm_t func, int* length)
{
	int ret = -1;
	
	struct trans_base *data = (struct trans_base *)packge->data;

	char *buf = (char*)malloc(packge->size* 4 + 10);
	memset(buf, 0, (packge->size* 4 + 10));
	if (buf == NULL) {
		PQ_Printf("[%d] malloc err..", __LINE__);
	}
	switch (data->id) {
	case 1:
	{
		/* get the de and ic version */
	}
	break;
	default:
#ifndef DEXX_SO_SP

		ret = updata_firm_spec(buf, packge->size* 4 + 10, data, 0);
        printf("use DEXX_SO_SP\n");
#else
		if (updata_firm_spec != NULL) {
			ret  = updata_firm_spec(buf, packge->size* 4 + 10, data, 0);
		} else {
			PQ_Printf("updata_firm_spec is NULL...");
		}
        printf("no use DEXX_SO_SP\n");
#endif
		if (ret < 0) {
			PQ_Printf("updata_firm_spec err...");
			free(buf);
			buf = NULL;
		}
	}

	*length = ret;
	return buf;

}

int init_generic(void)
{
	int ret = 0; 
	void *handle_dexx = NULL;
#ifdef DEXX_SO_SP

#ifdef ANDROID_PLT
	char path[40] = "/vendor/lib/libde";
	ret = sprintf(path, "/vendor/lib/libde%d.so", de_version);
#else
	char path[40] = "/lib/libde";
	ret = sprintf(path, "/lib/libde%d.so", de_version);
#endif
	PQ_Printf("%s", path);
	handle_dexx = dlopen(path, RTLD_LAZY);
	if (handle_dexx == NULL) {
		PQ_Printf("dlopen %s err.\n", path);
		return -1;
	}

	trans2spec = dlsym(handle_dexx, "trans_to_spec");
	if (trans2spec == NULL) {
		PQ_Printf("get	trans2spec NULL err.\n");
	}
	download_firm_spec = dlsym(handle_dexx, "download_firmware_spec");
	if (download_firm_spec == NULL) {
		PQ_Printf("get	download_firm_spec NULL err.\n");
	}
	updata_firm_spec = dlsym(handle_dexx, "updata_firm_spec");
	if (updata_firm_spec == NULL) {
		PQ_Printf("get	updata_firm_spec NULL err.\n");
	}
#endif
	return ret;
}

