#ifndef __OPENAMP_H_
#define __OPENAMP_H_

#include <openamp/rpmsg.h>
#include <openamp/sunxi_helper/openamp_log.h>
#include <openamp/sunxi_helper/openamp_platform.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RPMSG_MAX_LEN			(512 - 16)

/**
 *	openamp_init
 *	this function will init openamp framework
 *	@return 0 success
 * */
int openamp_init(void);
void openamp_deinit(void);

/**
 *	openamp_ept_open
 *	this function will creat a endpoint used for transmit
 *	@name:	endpoint name
 *	@rpmsg_id:	rpmsg device id.define in rssource table
 *	@src_addr:	source addr
 *	@dst_addr:	target addr
 *	@priv:		ept private data
 *	@cd:		it will be called when endpoint recive data
 *	@unbind_cb:	it will be called when unbind
 *
 *	@return endpoint pointer
 * */
struct rpmsg_endpoint *openamp_ept_open(const char *name, int rpmsg_id,
				uint32_t src_addr, uint32_t dst_addr,void *priv,
				rpmsg_ept_cb cb, rpmsg_ns_unbind_cb unbind_cb);

void openamp_ept_close(struct rpmsg_endpoint *ept);

int openamp_rpmsg_send(struct rpmsg_endpoint *ept, void *data, uint32_t len);

#ifdef CONFIG_RPMSG_CLIENT

#define RPMSG_MAX_NAME_LEN		32

struct rpmsg_ept_client {
	uint32_t id;		/* unique id for every client */
	char name[RPMSG_MAX_NAME_LEN];
	struct rpmsg_endpoint *ept; /* ept->priv can used by user */
	void *priv;			/* user used */
};

typedef int (*rpmsg_func_cb)(struct rpmsg_ept_client *client);

void rpmsg_ctrldev_init_thread(void *arg);
int rpmsg_ctrldev_create(void);
void rpmsg_ctrldev_release(void);

int rpmsg_client_bind(const char *name, rpmsg_ept_cb cb, rpmsg_func_cb bind,
				rpmsg_func_cb unbind, uint32_t cnt, void *priv);
void rpmsg_client_unbind(const char *name);
void rpmsg_client_clear(const char *name);
void rpmsg_client_reset(void);

void rpmsg_eptldev_close(struct rpmsg_ept_client *client);

#endif

#ifdef CONFIG_RPMSG_NOTIFY
int rpmsg_notify(char *name, void *data, int len);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __OPENAMP_H_ */
