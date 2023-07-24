/*
Copyright (c) 2020-2030 allwinnertech.com JetCui<cuiyuntao@allwinnertech.com>

*/

#define ALLIGN(x,y) ((x+y)/y*y)

#if !defined(PQ_DEBUG_LEVEL)
#define PQ_DEBUG_LEVEL 0
#endif

#if defined(RV_INUX)
typedef long __s64;
#endif

#ifdef ANDROID_PLT
#include <utils/Log.h>
#define PQ_Printf(...) ALOGD(__VA_ARGS__)
#else
#define PQ_Printf(...) printf("sunxi PQ ");\
	printf(__VA_ARGS__);\
	printf("\n");
#endif

typedef long* (*trans_firm_data_t)(char *buf, int *need);
typedef int (*data_to_firm_t)(char* firm, int e, int next, long value);

enum{
	READ_CMD = 0,
	WRITE_CMD
};

enum{
	BEGIN_FANG = 0, // [
	END_FANG,       // ]
	BEGIN_DA,       // {
	END_DA          // }
};


struct PQ_package {
	char head[4];
	char cmd;
	char type;
	int  size;
	char data[0];
}__attribute__ ((packed));

struct trans_header {
	int id;
	char data[0];
}__attribute__ ((packed));

struct trans_reply {
	int id;
	int ret;
}__attribute__ ((packed));

struct list{
	struct list* pre;
	struct list* next;
};

typedef struct list aw_list;

struct trans_data {
	struct list alist;
	int client_fd;
#if PQ_DEBUG_LEVEL
	int count;
#endif
	char data[0];
};

static inline void init_list(aw_list *list)
{
	list->next = list;
	list->pre = list;
}

static inline void add_list_tail(aw_list *head, aw_list *list_new)
{
	list_new->next = head;
	list_new->pre = head->pre;
	head->pre->next = list_new;
	head->pre = list_new;
}

static inline void del_list(aw_list *list_old)
{
	list_old->next->pre = list_old->pre;
	list_old->pre->next = list_old->next;
	init_list(list_old); 
}

static inline int list_empty(aw_list *list)
{
	return (list->pre == list) && (list->next == list);
}

