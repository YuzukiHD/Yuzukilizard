#ifndef __RPBUF_INTERNAL_H__
#define __RPBUF_INTERNAL_H__

#include <stdint.h>
#include <aw_list.h>
#include <rpbuf.h>

#include "rpbuf_common.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * The max number of buffers that rpbuf can manage.
 * If necessary, enlarge or reduce this number.
 */
#define RPBUF_BUFFERS_NUM_MAX 32

#define RPBUF_SERVICE_MESSAGE_LENGTH_MAX 128

#define RPBUF_SERVICE_MESSAGE_PREAMBLE	0xA
#define RPBUF_SERVICE_MESSAGE_POSTAMBLE	0x5

struct rpbuf_service;
struct rpbuf_service_ops;

struct rpbuf_link {
	/*
	 * 'token' is a symbol to link service and controller. Only the service
	 * and controller with the same 'token' could be linked together.
	 */
	void *token;

	struct rpbuf_service *service;
	struct rpbuf_controller *controller;

	/*
	 * service_lock/controller_lock is used to avoid concurrent access to
	 * service/controller, including their creation and destruction.
	 * So these locks would keep alive in the whole live cycle of link.
	 *
	 * To avoid deadlock, these two locks cannot be used within each other.
	 * For example, we should NOT do:
	 *	{
	 *		lock(service_lock);
	 *		lock(controller_lock);
	 *		unlock(controller_lock);
	 *		unlock(service_lock);
	 *	}
	 * Instead, we should do:
	 *	{
	 *		lock(service_lock);
	 *		unlock(service_lock);
	 *		lock(controller_lock);
	 *		unlock(controller_lock);
	 *	}
	 */
	rpbuf_mutex_t service_lock;	/* protect service */
	rpbuf_mutex_t controller_lock;	/* protect controller */

	struct list_head list;
	rpbuf_spinlock_t lock;	/* protect link itself */
	int ref_cnt;
};

struct rpbuf_service {
	int id;
	struct list_head list;

	struct rpbuf_link *link;

	const struct rpbuf_service_ops *ops;
	void *priv;
};

struct rpbuf_controller {
	int id;
	struct list_head list;

	enum rpbuf_role role;

	struct rpbuf_link *link;

	/*
	 * A rpbuf_buffer cannot be completely used until both remote and local
	 * allocate it manually. The 'xxx_dummy_buffers' are used to store the
	 * incomplete buffer information. Once the information is complete, the
	 * buffer will be moved from 'xxx_dummy_buffers' to 'buffers'.
	 */
	/* To store buffer information received from remote */
	struct list_head remote_dummy_buffers;
	/* To store buffer information created by loacl */
	struct list_head local_dummy_buffers;
	/* To store buffers whose information is complete */
	struct rpbuf_buffer *buffers[RPBUF_BUFFERS_NUM_MAX];
	/* To store buffers whose information is alloc in local */
	uint8_t local_buffers[RPBUF_BUFFERS_NUM_MAX];

	const struct rpbuf_controller_ops *ops;
	void *priv;
};

struct rpbuf_buffer {
	char name[RPBUF_NAME_SIZE];
	int id;
	/*
	 * va: virtual address that program can directly use.
	 * pa: physical address.
	 * da: device address. This address is set by MASTER, and transfer to
	 *     SLAVE to use. What it means is defined by the SLAVE
	 *     translate_to_va() implementation.
	 */
	void *va;
	uintptr_t pa;
	uint64_t da;
	int len;

	const struct rpbuf_buffer_ops *ops;
	const struct rpbuf_buffer_cbs *cbs;
	void *priv;

	struct rpbuf_controller *controller;

	struct list_head dummy_list;
};

/*
 * The layout of a rpbuf service message is like this:
 *
 * struct rpbuf_service_message
 * {
 *	// Header
 *	u8 preamble;	// is always 0xA.
 *	u8 command;
 *	u16 content_len;
 *
 *	// The actual content of the rpbuf service message.
 *	// Its size is 'content_len', which will change with the 'command'.
 *	u8 content[];
 *
 *	// Trailer
 *	u8 postamble;	// is always 0x5.
 * };
 */

struct rpbuf_service_message_header {
	uint8_t preamble;
	uint8_t command;
	uint16_t content_len;
} __attribute__((packed));

struct rpbuf_service_message_trailer {
	uint8_t postamble;
} __attribute__((packed));

/*
 * NOTE:
 *   This enum is used as index in some arrays, such as:
 *	rpbuf_service_message_content_len[]
 *	rpbuf_service_command_handlers[]
 *   After modifying this enum, remember to update these arrays.
 */
enum rpbuf_service_command {
	RPBUF_SERVICE_CMD_UNKNOWN = 0,
	RPBUF_SERVICE_CMD_BUFFER_CREATED,
	RPBUF_SERVICE_CMD_BUFFER_DESTROYED,
	RPBUF_SERVICE_CMD_BUFFER_TRANSMITTED,
	RPBUF_SERVICE_CMD_MAX,
};

struct rpbuf_service_content_buffer_created {
	uint8_t name[RPBUF_NAME_SIZE];
	uint32_t id;
	uint32_t len;
	uint64_t pa;
	uint64_t da;
	/*
	 * Indicates that whether the local buffer is available before sending
	 * BUFFER_CREATED message to remote.
	 */
	uint8_t is_available;
} __attribute__((packed));

struct rpbuf_service_content_buffer_destroyed {
	uint32_t id;
} __attribute__((packed));

struct rpbuf_service_content_buffer_transmitted {
	uint32_t id;
	uint32_t offset;
	uint32_t data_len;
} __attribute__((packed));

struct rpbuf_service_ops {
	int (*notify)(void *msg, int msg_len, void *priv);
};

struct rpbuf_service *rpbuf_create_service(int id,
					   const struct rpbuf_service_ops *ops,
					   void *priv);
void rpbuf_destroy_service(struct rpbuf_service *service);

int rpbuf_register_service(struct rpbuf_service *service, void *token);
void rpbuf_unregister_service(struct rpbuf_service *service);

struct rpbuf_controller *rpbuf_create_controller(int id,
						 const struct rpbuf_controller_ops *ops,
						 void *priv);
void rpbuf_destroy_controller(struct rpbuf_controller* controller);

int rpbuf_register_controller(struct rpbuf_controller *controller,
			      void *token, enum rpbuf_role role);
void rpbuf_unregister_controller(struct rpbuf_controller *controller);

/**
 * rpbuf_service_get_notification - notice local that remote has notified us
 * @service: rpbuf service
 * @msg: message sent from remote
 * @msg_len: message length
 *
 * Return 0 on success, or a negative number or failure.
 */
int rpbuf_service_get_notification(struct rpbuf_service *service, void *msg, int msg_len);

static inline enum rpbuf_role rpbuf_role_toggle(enum rpbuf_role role)
{
	switch (role) {
	case RPBUF_ROLE_MASTER:
		return RPBUF_ROLE_SLAVE;
	case RPBUF_ROLE_SLAVE:
		return RPBUF_ROLE_MASTER;
	default:
		rpbuf_err("(%s:%d) invalid role\n", __func__, __LINE__);
		return RPBUF_ROLE_UNKNOWN;
	}
}
#ifdef __cplusplus
}
#endif

#endif /* ifndef __RPBUF_INTERNAL_H__ */

