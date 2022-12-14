#ifndef __RPBUF_H__
#define __RPBUF_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define RPBUF_NAME_SIZE	32	/* include trailing '\0' */

struct rpbuf_controller;
struct rpbuf_buffer;

enum rpbuf_role {
	RPBUF_ROLE_UNKNOWN = 0,
	RPBUF_ROLE_MASTER,
	RPBUF_ROLE_SLAVE,
};

/*
 * The callback function type when buffer is available.
 * @buffer: rpbuf buffer
 * @priv: private data for user
 */
typedef void (*rpbuf_available_cb_t)(struct rpbuf_buffer *buffer, void *priv);

/*
 * The callback function type for receiving rpbuf buffer.
 * @buffer: rpbuf buffer
 * @data: the virtual address of buffer payload data
 * @data_len: the valid length of data (should be <= total length of buffer)
 * @priv: private data for user
 */
typedef int (*rpbuf_rx_cb_t)(struct rpbuf_buffer *buffer, void *data, int data_len, void *priv);

/*
 * The callback function type when remote destroys buffer.
 * @buffer: rpbuf buffer
 * @priv: private data for user
 */
typedef int (*rpbuf_destroyed_cb_t)(struct rpbuf_buffer *buffer, void *priv);

struct rpbuf_buffer_cbs {
	rpbuf_available_cb_t available_cb;
	rpbuf_rx_cb_t rx_cb;
	rpbuf_destroyed_cb_t destroyed_cb;
};

/**
 * struct rpbuf_buffer_ops - user-specific rpbuf buffer operations
 * @alloc_payload_memory: allocate memory for payload
 * @free_payload_memory: free memory for payload
 * @addr_remap: remap pa or da to va
 */
struct rpbuf_buffer_ops {
	int (*alloc_payload_memory)(struct rpbuf_buffer *buffer, void *priv);
	void (*free_payload_memory)(struct rpbuf_buffer *buffer, void *priv);
	void *(*addr_remap)(struct rpbuf_buffer *buffer,
			    uintptr_t pa, uint64_t da, int len, void *priv);
};

/**
 * struct rpbuf_buffer_ops - user-specific rpbuf controller operations
 * @alloc_payload_memory: allocate memory for payload
 * @free_payload_memory: free memory for payload
 * @addr_remap: remap pa or da to va
 */
struct rpbuf_controller_ops {
	int (*alloc_payload_memory)(struct rpbuf_controller *controller,
				    struct rpbuf_buffer *buffer, void *priv);
	void (*free_payload_memory)(struct rpbuf_controller *controller,
				    struct rpbuf_buffer *buffer, void *priv);
	void *(*addr_remap)(struct rpbuf_controller *controller,
			    uintptr_t pa, uint64_t da, int len, void *priv);
};

/**
 * rpbuf_init_service - Init service
 * @id: service id
 * @token: used to link with controller
 *
 * What @id and @token means is defined by the backend implementation of
 * rpbuf_init_service.
 *
 * Return pointer to rpbuf service on success, or NULL on failure.
 */
struct rpbuf_service *rpbuf_init_service(int id, void *token);

/**
 * rpbuf_deinit_service - The opposite operation of rpbuf_init_service
 * @service: rpbuf service
 */
void rpbuf_deinit_service(struct rpbuf_service *service);

/**
 * rpbuf_get_service_by_id - Get rpbuf service by id
 * @id: service id
 *
 * We rarely use this function to get rpbuf service. Because we usually operate
 * rpbuf controller to use service rather than use it directly.
 *
 * Return pointer to rpbuf service on success, or NULL on failure.
 */
struct rpbuf_service *rpbuf_get_service_by_id(int id);

/**
 * rpbuf_init_controller - Init controller
 * @id: controller id
 * @token: used to link with service
 * @role: MASTER or SLAVE
 * @ops: user-specific rpbuf controller operations
 * @priv: user-specific private data
 *
 * What @id and @token means is defined by the backend implementation of
 * rpbuf_init_controller
 *
 * Return pointer to rpbuf controller on success, or NULL on failure.
 */
struct rpbuf_controller *rpbuf_init_controller(int id, void *token, enum rpbuf_role role,
					       const struct rpbuf_controller_ops *ops,
					       void *priv);

/*
 * rpbuf_deinit_controller - The opposite operation of rpbuf_init_controller
 * @controller: rpbuf controller
 */
void rpbuf_deinit_controller(struct rpbuf_controller* controller);

/**
 * rpbuf_get_controller_by_id - Get rpbuf controller by id
 * @id: controller id
 *
 * Return pointer to rpbuf controller on success, or NULL on failure.
 */
struct rpbuf_controller *rpbuf_get_controller_by_id(int id);

/**
 * rpbuf_alloc_buffer - allocate a rpbuf buffer
 * @controller: rpbuf controller
 * @name: buffer name
 * @len: buffer length
 * @ops: user-specific rpbuf buffer operations (set NULL if not used)
 * @cbs: rpbuf buffer callbacks (set NULL if not used)
 * @priv: private data for user (set NULL if not used)
 *
 * Return pointer to rpbuf buffer on success, or NULL on failure.
 */
struct rpbuf_buffer *rpbuf_alloc_buffer(struct rpbuf_controller *controller,
					const char *name, int len,
					const struct rpbuf_buffer_ops *ops,
					const struct rpbuf_buffer_cbs *cbs,
					void *priv);

/**
 * rpbuf_free_buffer - free a rpbuf buffer
 * @buffer: rpbuf buffer
 *
 * This function will trigger the buffer destroyed_cb on remote.
 *
 * Return 0 on success, or a negative number on failure.
 */
int rpbuf_free_buffer(struct rpbuf_buffer *buffer);

/**
 * rpbuf_buffer_is_available - check whether a rpbuf buffer is available for local
 * @buffer: rpbuf buffer
 *
 * Return 1 when available, 0 when not available.
 */
int rpbuf_buffer_is_available(struct rpbuf_buffer *buffer);

/**
 * rpbuf_transmit_buffer - transmit a rpbuf buffer to remote
 * @buffer: rpbuf buffer
 * @offset: the address offset to store data
 * @data_len: the valid length of data
 *
 * This function will trigger the buffer rx_cb on remote.
 *
 * Return 0 on success, or a negative number on failure.
 */
int rpbuf_transmit_buffer(struct rpbuf_buffer *buffer,
			  unsigned int offset, unsigned int data_len);

/*
 * Get the name of rpbuf buffer.
 */
const char *rpbuf_buffer_name(struct rpbuf_buffer *buffer);
/*
 * Get the id of rpbuf buffer.
 */
int rpbuf_buffer_id(struct rpbuf_buffer *buffer);
/*
 * Get the virtual address of rpbuf buffer.
 */
void *rpbuf_buffer_va(struct rpbuf_buffer *buffer);
/*
 * Get the physical address of rpbuf buffer.
 */
uintptr_t rpbuf_buffer_pa(struct rpbuf_buffer *buffer);
/*
 * Get the device address of rpbuf buffer.
 */
uint64_t rpbuf_buffer_da(struct rpbuf_buffer *buffer);
/*
 * Get the length of rpbuf buffer.
 */
int rpbuf_buffer_len(struct rpbuf_buffer *buffer);
/*
 * Get the user private data of rpbuf buffer.
 */
void *rpbuf_buffer_priv(struct rpbuf_buffer *buffer);

/*
 * Set the virtual address of rpbuf buffer.
 */
void rpbuf_buffer_set_va(struct rpbuf_buffer *buffer, void *va);
/*
 * Set the physical address of rpbuf buffer.
 */
void rpbuf_buffer_set_pa(struct rpbuf_buffer *buffer, uintptr_t pa);
/*
 * Set the device address of rpbuf buffer.
 */
void rpbuf_buffer_set_da(struct rpbuf_buffer *buffer, uint64_t da);

#ifdef __cplusplus
}
#endif

#endif /* ifndef __RPBUF_H__ */

