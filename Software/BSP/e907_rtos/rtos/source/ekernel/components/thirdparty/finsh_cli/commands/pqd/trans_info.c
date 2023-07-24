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
#include <unistd.h>
#include "lwip/sockets.h"
#include "mmap/mman.h"
#include "trans_header.h"
#include "sunxi_display2.h"
#include <sys_device.h>
#include <dfs_posix.h>
#include <sys_device.h>
#include <semaphore.h>

#if CONFIG_COMMAND_PQD_TEST
	#define DISP_CONFIG "/mnt/E/sunxi_pq.cfg"
	#define DISP_FIRMWARE "/mnt/E/disp_firmware"
#else
	#define DISP_CONFIG "/mnt/D/res/sunxi_pq.cfg"
	#define DISP_FIRMWARE "/mnt/D/res/disp_firmware"
#endif

pthread_mutex_t notfiy_mutex;
pthread_cond_t notfiy_mCond;
pthread_mutex_t mlist_mutex; 
sem_t sem_flush;

int disp_fd;
FILE *disp_firm_fd;
extern int can_close;
extern int de_version;
extern int ic;
extern int de;
struct list trans_list;

static int ic_de_talbe[3][2] = {
	{1855,201},
    {1859,201},
	{0,0}
};

extern int trans_to_dexx(int disp_fd, void *data, int cmd);
extern int download_firmware_generic(int disp_fd, int id, int de_d, char* buffer, trans_firm_data_t func);
extern int init_generic(void);
extern char* updata_firm_generic(struct PQ_package *packge, data_to_firm_t func, int* length);

static void flush_data(void)
{
	int fd = 0;
	printf("flush data ....\n");
	fd = open("/dev/udisk_flush", O_RDONLY);
	ioctl(fd, DEV_IOC_USR_FLUSH_CACHE, 0);
	close(fd);
	rt_thread_delay(5);
}

long* trans_firm_data(char *buf, int *need)
{
	int i = 0;
	int num = 1;
	int *nu_re = NULL;
	long *reg = NULL;

	while (buf[i] != '}') {
		if (buf[i] == ',') {
			num++;
		}
#if (PQ_DEBUG_LEVEL>2)
		printf("%c", buf[i]);
		if (i%60==0) {
			printf("\n");
		}
#endif
		i++;
	}
	if (*need > 0 && num < *need) {
		PQ_Printf("a err num%d.",num);
		return NULL;
	}
	nu_re = (int *)malloc(num * sizeof(int)); 
	if (nu_re == NULL) {
		PQ_Printf("line[%d] malloc err.", __LINE__);
		return NULL;
	}
	i = 0;
	num = 0;
	nu_re[num++] = 1;
	while(buf[i] != '}') {
		if (buf[i] == ',') {
			nu_re[num++] = i+1;
		}
		i++;
	}
	reg = (long *)malloc(num * sizeof(long));
	if (reg == NULL) {
		PQ_Printf("line[%d] malloc err.", __LINE__);
		free(nu_re);
		return NULL;
	}
#if (PQ_DEBUG_LEVEL > 1)
		printf("data:\n");
#endif
	i = 0;
	while(i < num) {
		reg[i] = (long)atoi(&buf[(int)nu_re[i]]);
#if (PQ_DEBUG_LEVEL > 1)
		printf("%ld, ", reg[i]);
#endif		
		i++;
	}
#if (PQ_DEBUG_LEVEL > 1)
	printf("\n");
#endif
	free(nu_re);
	*need = num;

	return reg;
}

static int data_to_firm(char *firm, int length, int next, long value)
{
	int i = 0, tmp = next;
	char d[20];

	if (firm == NULL) {
		PQ_Printf("[%d]: give a NULL buf:", __LINE__);
		return 0;
	}
	if (next >= length) {
		PQ_Printf("[%d]:please give us a big buf:", __LINE__);
		return next;

	}
	if (next == 0) {
		firm[next++] = '{';
	}
	d[0] = '0';
	while ((value/10)&& i < 20) {
		d[i++] = value%10 + '0';
		value =  value/10;
	}
	if (i == 0)
		i++;
	while (i > 0 && (next) < length) {
		firm[next++] = d[--i];
	}
	if (next >= length){
		PQ_Printf("[%d]:please give us a big buf%d:", __LINE__, i);
		return tmp;
	}
	firm[next++] = ',';
	return next;
}

int download_firmware(void)
{
	int ret, off_set = 0, file_off = 0, id = 0, de = 0;
	int size, i = 0, go_end = 0;

	char *t_buff = NULL;
	int begin_id = 0, next = BEGIN_FANG;
	char find;
	ret = fseek(disp_firm_fd, 0, SEEK_END);
		if (0 != ret) {
		ret = -3;
		printf("error SEEK_END");
		goto err;
	}
	size = ftell(disp_firm_fd);
	if ( size<5 ) {
		printf("file error");
		goto err;
	}
	fseek(disp_firm_fd, 0, SEEK_SET);
	if (0 !=fseek(disp_firm_fd, 0, SEEK_CUR)) {
		ret = -3;
        printf("error SEEK_SET");
		goto err;
	}
	find = '\0';
	t_buff = (char *)malloc(size);
	fread(t_buff, size, 1, disp_firm_fd);

find_str:
	switch (next) {
		case BEGIN_FANG:
			find = '[';
		break;
		case END_FANG:
			find = ']';
		break;
		case BEGIN_DA:
			find = '{';
		break;
		case END_DA:
			find = '}';
		break;
		default:
		PQ_Printf("%s not a canonical write", DISP_FIRMWARE);
	}
	off_set = i;
	while ((t_buff[i]) != find) {
		i++;
		if (i == size) {
			goto err;
		}
	}
	off_set = i;
	if (find == '[') {
		i++;
		while (t_buff[i++] < '0');
		if (t_buff[i-1] > '9') {
			PQ_Printf("give us a err cfg[%c].", t_buff[i-1]);
			goto err;
		}
		id = atoi(&t_buff[i-1]);
		i--;
		while(i < size) {
			if (t_buff[i] == ',') {
				while (t_buff[i++] < '0');
				if (t_buff[i-1] > '9') {
					PQ_Printf("give us a err ,[%c].", t_buff[i-1]);
					goto err;
				}
				de = atoi(&t_buff[i-1]);
			}
			if(t_buff[i++] == ']')
				break;
		}
		if (i >= size) {
			PQ_Printf("give us a err [%c].", t_buff[i-1]);
			goto err;
		}
		next = BEGIN_DA;
		goto find_str;
    }
	if (find == '{') {
		begin_id = i;
		next = END_DA;
		goto find_str;
	}
	if (find == '}') {
		download_firmware_generic(disp_fd, id, de, &t_buff[begin_id], trans_firm_data);
	}
	next = BEGIN_FANG;
	if (size - i > 10) {
		printf("size -i is %d\n", size-i);
		goto find_str;
	}

	free(t_buff);

	PQ_Printf("config display firmware completed...");
	return 0;
err:
	PQ_Printf("config display firmware err %d.", ret);
	free(t_buff);
	return ret;

}

static void xchang_data(char* buff, int length, int f_id)
{
	int off_set = 0, id = -1, de_need = 0, i = 0, j = 0;
	off_t off_end, find_off, begin_id = 0;
	int size = 0, find_size = 0;
	int index = 0;
	char *t_buff = NULL, *tt_buff = NULL;

	char find_flag = '[';

	printf("xchang data \n");
	disp_firm_fd = fopen(DISP_FIRMWARE, "r+");
	if (disp_firm_fd == NULL) {
		PQ_Printf("not found the %s file", DISP_FIRMWARE);
		return;
	}
	if (0 != fseek(disp_firm_fd, 0, SEEK_END)) {
		printf("seek end error\n");
		goto err;
	}
	size = ftell(disp_firm_fd);

	fseek(disp_firm_fd, 0, SEEK_SET);
	if (0 != fseek(disp_firm_fd, 0, SEEK_CUR)) {
		printf("seek start error\n");
		goto err;
	}

	t_buff = (char *)malloc(size);
	fread(t_buff, size, 1, disp_firm_fd);

find_part:

	while (i <= size &&(t_buff[i]) != find_flag ) {
		if (i++ >= size) {
			goto delete_part;
		}
	}

	off_set = i;
	if (find_flag == '[') {
		begin_id = i;
		find_off = off_set;
		i++;
		while (t_buff[i++] < '0');
		if (t_buff[i-1] > '9') {
			printf("give us a err cfg[%c].", t_buff[i-1]);
			goto err;
		}
		id = atoi(&t_buff[i-1]);
		if (id != f_id) {
			goto find_part;
		}
		i--;
		while (i < size) {
			if (t_buff[i] == ',') {
				while (t_buff[i++] < '0'){
					if (t_buff[i-1] > '9') {
						printf("give us a err ,[%c].", t_buff[i-1]);
						goto err;
					}
				}
				de_need = atoi(&t_buff[i-1]);
			}
			if(t_buff[i++] == ']') {
				break;
			}
        }
        if (de_need == de) {
			while(i <= size){
				if (t_buff[i++] == '['){
					find_size = i - begin_id;
					goto delete_part;
				}
			}
			find_size = i - begin_id;
			goto delete_part;
        }
        if (i >= size) {
            printf("%d:map little.", __LINE__);
            goto err;
        }
		goto find_part;
    }

delete_part:
	printf("delete part\n");
	tt_buff = (char *)malloc(4096);
	memset(tt_buff, 0, 4096);
	if (tt_buff == NULL) {
		printf("%d:malloc err.", __LINE__);
		goto err;
	}
	if (find_size != 0 && length) {
		fseek(disp_firm_fd, find_off, SEEK_SET);
		begin_id = ftell(disp_firm_fd);
		//printf("begin_id is %d, find_off is %d\n", begin_id, find_off);
		if (find_off != begin_id) {
			printf("%d:fseek err.", __LINE__);
			goto err;
		}
		while(index <= size-find_size){
			if(index <= begin_id){
				tt_buff[index] = t_buff[index];
			}else{
				tt_buff[index] = t_buff[index+find_size-1];
			}
			index++;
		}
	} else {
		while(index < size){
			tt_buff[index] = t_buff[index];
			index++;
		}
	}

write_part:
	printf("write part\n");
	int fn = fileno(disp_firm_fd);
	if (ftruncate(fn, 0) != 0) {
		printf("can`t ftruncate file!\n");
		goto err;
	}
	strcat(tt_buff, buff);
	fseek(disp_firm_fd, 0, SEEK_SET);
	if (ftell(disp_firm_fd) != 0) {
		printf("can`t return file header!\n");
		goto err;
	}
	int buf_size = index + length;
	fwrite(tt_buff, buf_size, 1, disp_firm_fd);
	//fflush(disp_firm_fd);
	printf("update firmware completed...\n");
	fclose(disp_firm_fd);
	return;
err:
	fclose(disp_firm_fd);
	return;
	}

int save_cfg_firmware(struct trans_data* data)
{
	int length = 0;
	char *buf;
	struct PQ_package *PQ_head = (struct PQ_package *)data->data;
	struct trans_header *id_head = (struct trans_header *)PQ_head->data;
	printf("save cfg firmware!\n");
	buf = updata_firm_generic(PQ_head, data_to_firm, &length);
	if (buf) {
		if(length > 0) {
			buf[length-1] = '}';
			buf[length] = 0xD;
			xchang_data(buf, length+1, id_head->id);
		}
		free(buf);
	}
	return 0;
}

void send_to_translate(struct trans_data* data)
{
#if PQ_DEBUG_LEVEL
	PQ_Printf("receive %d.", data->count);
#endif
	init_list(&data->alist);
	pthread_mutex_lock(&mlist_mutex);
	add_list_tail(&trans_list, &data->alist);
	pthread_mutex_unlock(&mlist_mutex);
	pthread_cond_signal(&notfiy_mCond);
}

static inline void trans_send(struct trans_data* data, int read, int reply)
{
	struct PQ_package *PQ_head =  NULL;
	PQ_head = (struct PQ_package *)data->data;
	struct trans_reply *replay_d = NULL;
#if PQ_DEBUG_LEVEL
	PQ_Printf("send %d cmd:%d.", data->count, PQ_head->cmd);
#endif
	if ((read && reply != 0) || (!read)) {
		replay_d = (struct trans_reply *)PQ_head->data;
		replay_d->id = 0;
		replay_d->ret = reply;
		PQ_head->size =  sizeof(struct PQ_package) + sizeof(struct trans_reply);
	}
	send(data->client_fd, PQ_head, PQ_head->size, 0);
	free(data);
    data = NULL;
}

void* translate_thread(void *arg)
{
	int ret;
	struct trans_data* data;
	struct list *alist = NULL;
	struct PQ_package *PQ_head =  NULL;
	PQ_head = (struct PQ_package *)arg;


	PQ_Printf("Sunxi PQ entry tanslate thread.");

	while(1) {

        pthread_mutex_lock(&mlist_mutex);
		if (list_empty(&trans_list)) {
            PQ_Printf("list empty");
			pthread_cond_wait(&notfiy_mCond, &mlist_mutex);
		}
		alist = trans_list.next;
		del_list(alist);
		pthread_mutex_unlock(&mlist_mutex);
		data = (struct trans_data*) alist;
		PQ_head = (struct PQ_package *)data->data;

		switch (PQ_head->cmd) {
		case 0:
            PQ_Printf("alist point %p", alist);
			trans_send(data, 1, 0);
            PQ_Printf("case 0 \n");
		break;
		case 1:
			/* read */
			ret = trans_to_dexx(disp_fd, PQ_head->data, READ_CMD);
			trans_send(data, 1, ret);
            PQ_Printf("case 1\n");
		break;
		case 3:
			/* save */
			ret = trans_to_dexx(disp_fd, PQ_head->data, WRITE_CMD);
			if (disp_firm_fd != NULL && !ret) {
				 ret = save_cfg_firmware(data);
			}
			trans_send(data, 0, ret);
                    	sem_post(&sem_flush);
            PQ_Printf("case 3\n");
		break;
		case 2:
			/* set */
			ret = trans_to_dexx(disp_fd, PQ_head->data, WRITE_CMD);
			trans_send(data, 0, ret);
            PQ_Printf("case 2\n");
		break;
		case 4:
			/* upload cfg */
			ret = (long)fopen(DISP_CONFIG, "r");
			/*TODO*/
			trans_send(data, 1, ret);
            PQ_Printf("case 4\n");
		break;
		default:
			PQ_Printf("give us a wronge cmd:%d ...",PQ_head->cmd);
			trans_send(data, 0, -1);
		}
	}
}

static void flush_thread(void *arg)
{
       while(1)
	{
		sem_wait(&sem_flush);
		flush_data();
		rt_thread_delay(2);
	}
}

static void get_ic_de(void)
{

#if defined(CONFIG_SOC_SUN3IW2) 
    ic = 1707;
#elif  defined (CONFIG_SOC_SUN20IW1)
    ic = 1859;
#elif  defined (CONFIG_SOC_SUN8IW19)
    ic = 1817;
#elif  defined (CONFIG_SOC_SUN8IW20)
    ic = 1859;
#else
    ic = 0;
    PQ_Printf("chip:unkonw\n");
#endif
    int i = 0;
	PQ_Printf("IC num is %d", ic);
	while (ic_de_talbe[i][0] != 0) {
		if (ic_de_talbe[i][0] == ic) {
			de_version = ic_de_talbe[i][1];
			break;
		}
		i++;
	}
    PQ_Printf("ic_id=%d, de_version=%d \n", ic, de_version);
}

int init_pq(void)
{
    get_ic_de();
    disp_fd = open("/dev/disp", O_RDWR);
	if (disp_fd < 0) {
		PQ_Printf("open display err %d\n", disp_fd);
		return -1;
	}
    disp_firm_fd = fopen(DISP_FIRMWARE, "r");
    if (disp_firm_fd == NULL) 
    {
		PQ_Printf("not found the %s file", DISP_FIRMWARE);
        return -1;
    } else {
        download_firmware();
    }
    fclose(disp_firm_fd);
    PQ_Printf("init pq finsh\n");
    return 0;
}

int PQ_translate_init(void)
{
    PQ_Printf("PQ translate init\n");
    int stack_size = 20480;
	pthread_t pthread_handle;
	pthread_attr_t attr;
	pthread_t flush_handle;
	pthread_attr_t flush_attr;
	pthread_attr_init(&attr);
	pthread_attr_init(&flush_attr);
	pthread_attr_setstacksize(&attr, stack_size);	

	pthread_cond_init(&notfiy_mCond, NULL);
	pthread_mutex_init(&notfiy_mutex, NULL);
	pthread_mutex_init(&mlist_mutex, NULL);
        
	sem_init(&sem_flush, 0, 0);
  
	get_ic_de();

	init_generic();

	disp_fd = open("/dev/disp", O_RDWR);
	if (disp_fd < 0) {
		PQ_Printf("open display err %d\n", disp_fd);
		return -1;
	}
	init_list(&trans_list);

	/* download  the firmware  to  display */
	disp_firm_fd = fopen(DISP_FIRMWARE, "r+");
	if (disp_firm_fd == NULL) {
		PQ_Printf("There is not the %s, creat it.", DISP_FIRMWARE);
		disp_firm_fd = fopen(DISP_FIRMWARE, "w+");
		if (disp_firm_fd == NULL) {
			PQ_Printf("Can not creat %s.", DISP_FIRMWARE);
		} else {
			if (de_version > 0) {
				int file_size = 0;
				char file_head[10] = {0};
				fseek(disp_firm_fd, 0, SEEK_SET);
				if (0 != ftell(disp_firm_fd)) {
					printf("seek end error\n");
					return -1;
				}
				sprintf(file_head, "#DE%d\n", de_version);
				file_size = strlen(file_head);
				fwrite(file_head, file_size, 1, disp_firm_fd);
				//fflush(disp_firm_fd);
				fclose(disp_firm_fd);
			}
		}
	} else {
		PQ_Printf("-------init config-------\n");
		download_firmware();
		fclose(disp_firm_fd);
	}
	pthread_create(&pthread_handle, &attr, translate_thread, NULL);
	pthread_setname_np(pthread_handle,"pq_translate");
	pthread_create(&flush_handle, &flush_attr, (void *)flush_thread, NULL);
	pthread_setname_np(flush_handle,"pq_flush");
	return 0;
}
