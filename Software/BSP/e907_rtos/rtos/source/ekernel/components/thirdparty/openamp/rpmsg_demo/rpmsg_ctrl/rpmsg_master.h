#ifndef _RPMSG_MASTER_H_
#define _RPMSG_MASTER_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <aw_list.h>
#include <hal_osal.h>
#include <hal_queue.h>
#include <openamp/sunxi_helper/openamp.h>

#define RPMSG_ACK_OK                0x13131411

#define RPMSG_ACK_FAILED            0x13131412
#define RPMSG_ACK_NOLISTEN          0x13131413
#define RPMSG_ACK_BUSY              0x13131414
#define RPMSG_ACK_NOMEM             0x13131415
#define RPMSG_ACK_NOENT             0x13131416

#define RPMSG_CREATE_CLIENT         0x13141413
#define RPMSG_CLOSE_CLIENT          0x13141414 /* host release */
#define RPMSG_RELEASE_CLIENT        0x13141415 /* client release */
#define RPMSG_RESET_GRP_CLIENT      0x12131516
#define RPMSG_RESET_ALL_CLIENT      0x14151617
#define RPMSG_RELEASE               0xffffffff

#define __pack						__attribute__((__packed__))

struct rpmsg_ctrl_msg {
	char name[RPMSG_MAX_NAME_LEN];
	uint32_t id;
	uint32_t ctrl_id;
	uint32_t cmd;
} __pack;

struct rpmsg_ctrl_msg_ack {
	uint32_t id;
	uint32_t ack;
} __pack;

void rpmsg_ctrldev_close(void);

int rpmsg_ctrldev_create(void);

struct rpmsg_endpoint *
rpmsg_eptdev_create_client(struct rpmsg_ctrl_msg *msg, rpmsg_ept_cb cb,
				rpmsg_ns_unbind_cb ubind, void *priv);

void rpmsg_eptdev_release_client(struct rpmsg_endpoint *ept);
void rpmsg_client_unbind(const char *name);

#endif
