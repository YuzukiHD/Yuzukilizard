/*
 * thread pool
 * author: clarkyy
 * date: 20161118
 */

#include "thread_pool.h"
#include "log_handle.h"

typedef enum work_thread_flag_e {
	WORK_THREAD_STOP_NOT     = 0x01,
	WORK_THREAD_STOP_YES     = 0x02,
	WORK_THREAD_WORK_NOT     = 0x03,
	WORK_THREAD_WORK_YES     = 0x04,
	WORK_THREAD_EXIT_NOT     = 0x05,
	WORK_THREAD_EXIT_YES     = 0x06,
} work_thread_flag;

typedef struct work_thread_params_s {
	pthread_t                thread_id;        // thread id
	int                      stop_flag;        // stop flag: WORK_THREAD_STOP_NOT/WORK_THREAD_STOP_YES
	int                      work_flag;        // work flag: WORK_THREAD_WORK_NOT/WORK_THREAD_WORK_YES
	int                      status;           // exit flag: WORK_THREAD_EXIT_NOT/WORK_THREAD_EXIT_YES
	pthread_mutex_t          lock;             // lock for read/write
	thread_work_func         thread_handle;    // real handle function
	void                     *handle_params;   // params for handle function
} work_thread_params;

typedef struct work_thread_list_node_s {
	work_thread_params       node;
	struct work_thread_list_node_s *next;
} work_thread_list_node;

static work_thread_list_node  *g_work_thread_list_head = NULL;
static int                   g_work_thread_list_count = 0;
static pthread_mutex_t       g_global_lock;

static void set_zero_work_thread_params(work_thread_params *work_th_pa)
{
	work_th_pa->thread_id       = 0;
	work_th_pa->stop_flag       = WORK_THREAD_STOP_NOT;
	work_th_pa->work_flag       = WORK_THREAD_WORK_NOT;
	work_th_pa->status          = WORK_THREAD_EXIT_YES;
	work_th_pa->thread_handle   = NULL;
	work_th_pa->handle_params   = NULL;
	//work_th_pa->lock not destoryed until exit_server calls
}

#define WORK_THREAD_LOCK_READ(lock, key, val) \
	pthread_mutex_lock(&(lock));              \
	(key) = (val);                            \
	pthread_mutex_unlock(&(lock));

#define WORK_THREAD_LOCK_WRITE(lock, key, val)\
	pthread_mutex_lock(&(lock));              \
	(key) = (val);                            \
	pthread_mutex_unlock(&(lock));


static void *do_work_thread(void *params)
{
	work_thread_params *work_th_pa = (work_thread_params *)params;
	int stop_flag = 0, work_flag = 0;
	unsigned long work_thread_id = (unsigned long)work_th_pa->thread_id;

	LOG("%s: %lu starts\n", __FUNCTION__, work_thread_id);

	WORK_THREAD_LOCK_READ(work_th_pa->lock, stop_flag, work_th_pa->stop_flag);
	while (WORK_THREAD_STOP_NOT == stop_flag) {
		WORK_THREAD_LOCK_READ(work_th_pa->lock, work_flag, work_th_pa->work_flag);
		if ( WORK_THREAD_WORK_YES == work_flag) {
			if (work_th_pa->thread_handle) {
				work_th_pa->thread_handle(work_th_pa->handle_params);
			}

			WORK_THREAD_LOCK_WRITE(work_th_pa->lock, work_th_pa->work_flag, WORK_THREAD_WORK_NOT);
		}
		msleep(1);

		WORK_THREAD_LOCK_READ(work_th_pa->lock, stop_flag, work_th_pa->stop_flag);
	}

	pthread_mutex_lock(&work_th_pa->lock);
	set_zero_work_thread_params(work_th_pa);
	pthread_mutex_unlock(&work_th_pa->lock);
	LOG("%s: %lu exits\n", __FUNCTION__, work_thread_id);
	return 0;
}

/*
 * init thread pool
 * returns 0 if OK, <0 if something went wrong
 */
int init_thread_pool()
{
	int i = 0;
	work_thread_list_node *list_node = NULL;

	pthread_mutex_init(&g_global_lock, NULL);
	pthread_mutex_lock(&g_global_lock);
	g_work_thread_list_count = 6;
	// !!!head is not used
	g_work_thread_list_head = (work_thread_list_node *)malloc(sizeof(work_thread_list_node));
	for (i = 0, list_node = g_work_thread_list_head; i < g_work_thread_list_count; i++) {
		list_node->next = (work_thread_list_node *)malloc(sizeof(work_thread_list_node));
		list_node = list_node->next;
		list_node->next = NULL;
		set_zero_work_thread_params(&list_node->node);
		pthread_mutex_init(&list_node->node.lock, NULL);
	}
	pthread_mutex_unlock(&g_global_lock);

	LOG("%s: done\n", __FUNCTION__);
	return 0;
}


/*
 * add a work to thread queue
 * some work thread will execute the work function with work function params
 * work_func: real work function
 * work_func_params: params for real work function
 * returns 0 if OK, <0 if something went wrong
 */
int add_work(thread_work_func work_func, void *work_func_params)
{
	int i = 0, ret = -1;
	work_thread_list_node *list_node = NULL;

	pthread_mutex_lock(&g_global_lock);
	for (i = 0, list_node = g_work_thread_list_head; i < g_work_thread_list_count; i++) {
		list_node = list_node->next;
		pthread_mutex_lock(&list_node->node.lock);
		if (WORK_THREAD_EXIT_YES == list_node->node.status) {  // not created or exits
			set_zero_work_thread_params(&list_node->node);
			ret = pthread_create(&list_node->node.thread_id, NULL, do_work_thread, (void *)&list_node->node);
			if (0 == ret) {
				//pthread_join(list_node->node.thread_id, NULL);  // its blocked
				list_node->node.status = WORK_THREAD_EXIT_NOT;
				list_node->node.thread_handle = work_func;
				list_node->node.handle_params = work_func_params;
				list_node->node.work_flag = WORK_THREAD_WORK_YES;
				LOG("%s: create and work at %lu\n", __FUNCTION__, (unsigned long)list_node->node.thread_id);
			} else {
				ret = -1;
				LOG("%s: failed to create thread(%d, %s)\n", __FUNCTION__, errno, strerror(errno));
			}
			pthread_mutex_unlock(&list_node->node.lock);
			break;
 		} else if (WORK_THREAD_WORK_NOT == list_node->node.work_flag) { // not work
			list_node->node.thread_handle = work_func;
			list_node->node.handle_params = work_func_params;
			list_node->node.work_flag = WORK_THREAD_WORK_YES;
			pthread_mutex_unlock(&list_node->node.lock);

			ret = 0;
			LOG("%s: work at %lu\n", __FUNCTION__, (unsigned long)list_node->node.thread_id);
			break;
		}
		pthread_mutex_unlock(&list_node->node.lock);
	}

	if (i >= g_work_thread_list_count) {
		list_node->next = (work_thread_list_node *)malloc(sizeof(work_thread_list_node));
		list_node = list_node->next;
		list_node->next = NULL;
		set_zero_work_thread_params(&list_node->node);
		pthread_mutex_init(&list_node->node.lock, NULL);
		g_work_thread_list_count++;
		LOG("%s: list increased - %d\n", __FUNCTION__, g_work_thread_list_count);

		pthread_mutex_lock(&list_node->node.lock);
		ret = pthread_create(&list_node->node.thread_id, NULL, do_work_thread, (void *)&list_node->node);
		if (0 == ret) {
			//pthread_join(list_node->node.thread_id, NULL);  // its blocked
			list_node->node.status = WORK_THREAD_EXIT_NOT;
			list_node->node.thread_handle = work_func;
			list_node->node.handle_params = work_func_params;
			list_node->node.work_flag = WORK_THREAD_WORK_YES;
			LOG("%s: create and work at %lu\n", __FUNCTION__, (unsigned long)list_node->node.thread_id);
		} else {
			ret = -1;
			LOG("%s: failed to create thread(%d, %s)\n", __FUNCTION__, errno, strerror(errno));
		}
		pthread_mutex_unlock(&list_node->node.lock);
	}

	pthread_mutex_unlock(&g_global_lock);

	return ret;
}

/*
 * exit thread pool
 * returns 0 if OK, <0 if something went wrong
 */
int exit_thread_pool()
{
	int i = 0, status = 0;
	work_thread_list_node *list_node = NULL, *list_node0 = NULL;
	
	LOG("%s: ready to exit\n", __FUNCTION__);

	pthread_mutex_lock(&g_global_lock);
	LOG("%s: ready to stop threads(%d)\n", __FUNCTION__, g_work_thread_list_count);
	for (i = 0, list_node = g_work_thread_list_head; i < g_work_thread_list_count; i++) {
		list_node = list_node->next;
		WORK_THREAD_LOCK_WRITE(list_node->node.lock, list_node->node.stop_flag, WORK_THREAD_STOP_YES);
	}

	msleep(32);
	for (i = 0, list_node = g_work_thread_list_head; i < g_work_thread_list_count; i++) {
		list_node = list_node->next;
		WORK_THREAD_LOCK_READ(list_node->node.lock, status, list_node->node.status);
		while (WORK_THREAD_EXIT_NOT == status) {
			msleep(32);
			WORK_THREAD_LOCK_READ(list_node->node.lock, status, list_node->node.status);
		}
		LOG("%s: %d exits\n", __FUNCTION__, i);
	}

	for (i = 0, list_node = g_work_thread_list_head->next; i < g_work_thread_list_count; i++) {
		list_node0 = list_node;
		list_node = list_node->next;
		pthread_mutex_destroy(&list_node0->node.lock);
		free(list_node0);
	}
	free(g_work_thread_list_head);
	g_work_thread_list_head = NULL;
	g_work_thread_list_count = 0;

	pthread_mutex_unlock(&g_global_lock);
	pthread_mutex_destroy(&g_global_lock);

	LOG("%s: exits\n", __FUNCTION__);
	return 0;
}
