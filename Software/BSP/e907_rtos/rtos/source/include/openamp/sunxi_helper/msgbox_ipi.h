#ifndef OPENAMP_SUNXI_MSGBOX_IPI_H
#define OPENAMP_SUNXI_MSGBOX_IPI_H

#include <stdint.h>
#include <hal_osal.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*msgbox_ipi_callback)(void *priv, uint32_t data);
#ifdef CONFIG_RPMSG_SPEEDTEST
typedef void (*msgbox_sta_callback)(void);
#endif
struct msgbox_ipi_info {
	const char *name;
	int remote_id;
	int read_ch;
	int write_ch;
	uint32_t queue_size;
};

struct msgbox_ipi {
	struct msgbox_ipi_info info;
	msgbox_ipi_callback callback;
	void *priv;
	struct msg_endpoint *ept;
	hal_queue_t recv_queue;
	void *msgbox_task_handler;
};

struct msgbox_ipi *msgbox_ipi_create(struct msgbox_ipi_info *info,
				msgbox_ipi_callback callback, void *priv);

void msgbox_ipi_release(struct msgbox_ipi *ipi);

int msgbox_ipi_notify(struct msgbox_ipi *ipi, uint32_t data);

#ifdef CONFIG_RPMSG_SPEEDTEST
void msgbox_set_full_callback(msgbox_sta_callback call);
void msgbox_set_empty_callback(msgbox_sta_callback call);
void msgbox_reset_full_callback(void);
void msgbox_reset_empty_callback(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
