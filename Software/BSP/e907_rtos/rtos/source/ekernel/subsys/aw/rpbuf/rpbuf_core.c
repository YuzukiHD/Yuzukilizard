#include <errno.h>
#include <string.h>
#include <sunxi_hal_common.h>
#include <init.h>
#include "rpbuf_internal.h"

typedef int (*rpbuf_service_command_handler_t)(struct rpbuf_service *service,
					       enum rpbuf_service_command cmd,
					       void *content);

LIST_HEAD(__rpbuf_links);
static rpbuf_mutex_t __rpbuf_links_lock;

static int rpbuf_links_lock_create(void)
{
	__rpbuf_links_lock = rpbuf_mutex_create();
	return 0;
}
core_initcall(rpbuf_links_lock_create);

/* will nerver execute */
static void rpbuf_links_lock_delete(void)
{
	rpbuf_mutex_delete(__rpbuf_links_lock);
}

/* Remember to use __rpbuf_links_lock outside to protect this function */
static inline struct rpbuf_link *rpbuf_find_link(void *token)
{
	struct rpbuf_link *link;

	list_for_each_entry(link, &__rpbuf_links, list) {
		if (link->token == token)
			return link;
	}
	return NULL;
}

static inline struct rpbuf_buffer *rpbuf_find_dummy_buffer_by_name(
		struct list_head *dummy_buffers, const char *name)
{
	struct rpbuf_buffer *buffer;

	list_for_each_entry(buffer, dummy_buffers, dummy_list) {
		if (0 == strncmp(buffer->name, name, RPBUF_NAME_SIZE))
			return buffer;
	}
	return NULL;
}

static inline struct rpbuf_buffer *rpbuf_find_dummy_buffer_by_id(
		struct list_head *dummy_buffers, int id)
{
	struct rpbuf_buffer *buffer;
	list_for_each_entry(buffer, dummy_buffers, dummy_list) {
		if (buffer->id == id)
			return buffer;
	}
	return NULL;
}

static inline int rpbuf_get_free_local_buffers_id(struct rpbuf_controller *controller)
{
	int i;

	for (i = 0; i < RPBUF_BUFFERS_NUM_MAX; i++) {
		if (!controller->local_buffers[i]) {
			controller->local_buffers[i] = 1;
			return i;
		}
	}
	return -ENOENT;
}

static inline int rpbuf_put_free_local_buffers_id(struct rpbuf_controller *controller, int id)
{
	if (id >= RPBUF_BUFFERS_NUM_MAX) {
		rpbuf_err("invalid local_buffer id %d, its length should be "
			  "in range [0, %d)\n", id, RPBUF_NAME_SIZE);
		return -ENOENT;
	}

	controller->local_buffers[id] = 0;
	return id;
}

static struct rpbuf_buffer *rpbuf_alloc_buffer_instance(const char *name, int len)
{
	struct rpbuf_buffer *buffer;
	size_t name_len = strlen(name);

	buffer = rpbuf_zalloc(sizeof(struct rpbuf_buffer));
	if (!buffer) {
		rpbuf_err("zalloc for rpbuf_buffer failed\n");
		goto err_out;
	}

	if (name_len == 0 || name_len >= RPBUF_NAME_SIZE) {
		rpbuf_err("invalid buffer name \"%s\", its length should be "
			  "in range [1, %d)\n", name, RPBUF_NAME_SIZE);
		goto err_free_rpbuf_buffer;
	}
	strncpy(buffer->name, name, RPBUF_NAME_SIZE);

	buffer->len = len;

	return buffer;

err_free_rpbuf_buffer:
	rpbuf_free(buffer);
err_out:
	return NULL;
}

static void rpbuf_free_buffer_instance(struct rpbuf_buffer *buffer)
{
	rpbuf_free(buffer);
}

static int rpbuf_alloc_buffer_payload_memory_default(struct rpbuf_controller *controller,
						     struct rpbuf_buffer *buffer)
{
	int ret;
	void *addr;

	addr = dma_alloc_coherent(buffer->len);
	if (!addr) {
		rpbuf_err("dma_alloc_coherent for len %d failed\n", buffer->len);
		ret = -ENOMEM;
		goto err_out;
	}

	rpbuf_dbg("allocate payload memory: addr %p, len %d\n", addr, buffer->len);

	/* On RTOS, va and pa are the same */
	buffer->va = addr;
	buffer->pa = (uintptr_t)addr;
	buffer->da = (uint64_t)((uintptr_t)addr);

	return 0;

err_out:
	return ret;
}

static int rpbuf_alloc_buffer_payload_memory(struct rpbuf_controller *controller,
					     struct rpbuf_buffer *buffer)
{
	if (buffer->ops && buffer->ops->alloc_payload_memory)
		return buffer->ops->alloc_payload_memory(buffer, buffer->priv);

	if (controller->ops && controller->ops->alloc_payload_memory)
		return controller->ops->alloc_payload_memory(controller, buffer,
							     controller->priv);

	return rpbuf_alloc_buffer_payload_memory_default(controller, buffer);
}

static void rpbuf_free_buffer_payload_memory_default(struct rpbuf_controller *controller,
						     struct rpbuf_buffer *buffer)
{
	void *addr = buffer->va;

	rpbuf_dbg("free payload memory: addr %p, len %d\n", addr, buffer->len);
	dma_free_coherent(addr);
}

static void rpbuf_free_buffer_payload_memory(struct rpbuf_controller *controller,
					     struct rpbuf_buffer *buffer)
{
	if (buffer->ops && buffer->ops->free_payload_memory)
		return buffer->ops->free_payload_memory(buffer, buffer->priv);

	if (controller->ops && controller->ops->free_payload_memory)
		return controller->ops->free_payload_memory(controller, buffer,
							    controller->priv);

	rpbuf_free_buffer_payload_memory_default(controller, buffer);
}

static void *rpbuf_addr_remap_default(struct rpbuf_controller *controller,
				      struct rpbuf_buffer *buffer,
				      uintptr_t pa, uint64_t da, int len)
{
	/* On RTOS, va and pa are the same */
	return (void *)((uintptr_t)da);
}

static void *rpbuf_addr_remap(struct rpbuf_controller *controller,
			      struct rpbuf_buffer *buffer,
			      uintptr_t pa, uint64_t da, int len)
{
	if (buffer->ops && buffer->ops->addr_remap)
		return buffer->ops->addr_remap(buffer, pa, da, len, buffer->priv);

	if (controller->ops && controller->ops->addr_remap)
		return controller->ops->addr_remap(controller, pa, da, len,
						   controller->priv);

	return rpbuf_addr_remap_default(controller, buffer, pa, da, len);
}

static struct rpbuf_link *rpbuf_create_link(void *token)
{
	struct rpbuf_link *link;

	link = rpbuf_zalloc(sizeof(struct rpbuf_link));
	if (!link) {
		rpbuf_err("zalloc for rpbuf_link failed\n");
		link = NULL;
		goto out;
	}
	link->service_lock = rpbuf_mutex_create();
	link->controller_lock = rpbuf_mutex_create();
	link->token = token;
	link->ref_cnt = 0;
out:
	return link;
}

static void rpbuf_destroy_link(struct rpbuf_link *link)
{
	rpbuf_mutex_delete(link->service_lock);
	rpbuf_mutex_delete(link->controller_lock);
	rpbuf_free(link);
}

struct rpbuf_service *rpbuf_create_service(int id,
					   const struct rpbuf_service_ops *ops,
					   void *priv)
{
	struct rpbuf_service *service;

	service = rpbuf_zalloc(sizeof(struct rpbuf_service));
	if (!service) {
		rpbuf_err("zalloc for rpbuf_service failed\n");
		service = NULL;
		goto out;
	}

	service->id = id;

	service->ops = ops;
	service->priv = priv;

out:
	return service;
}

void rpbuf_destroy_service(struct rpbuf_service *service)
{
	if (!service) {
		rpbuf_err("invalid service\n");
		return;
	}

	service->ops = NULL;
	service->priv = NULL;

	rpbuf_free(service);
}

int rpbuf_register_service(struct rpbuf_service *service, void *token)
{
	struct rpbuf_link *link;
	uint32_t flags;
	int ret;

	rpbuf_mutex_lock(__rpbuf_links_lock);

	if (!service || !token) {
		ret = -EINVAL;
		goto out;
	}

	rpbuf_dbg("register service %p (token: %p)\n", service, token);

	link = rpbuf_find_link(token);
	if (!link) {
		link = rpbuf_create_link(token);
		if (!link) {
			rpbuf_err("rpbuf_create_link failed\n");
			ret = -EINVAL;
			goto out;
		}
		rpbuf_dbg("create new link %p (token: %p)\n", link, token);
		list_add_tail(&link->list, &__rpbuf_links);
	}

	rpbuf_mutex_lock(link->service_lock);
	flags = rpbuf_spin_lock_irqsave(&link->lock);

	if (link->service && link->service != service) {
		rpbuf_spin_unlock_irqrestore(&link->lock, flags);
		rpbuf_err("another service in the same token has been registered\n");
		ret = -EINVAL;
		goto unlock_service_lock;
	}

	link->service = service;
	service->link = link;
	link->ref_cnt++;

	rpbuf_dbg("link: service: %p, controller: %p, ref_cnt: %d\n",
		  link->service, link->controller, link->ref_cnt);

	rpbuf_spin_unlock_irqrestore(&link->lock, flags);

	ret = 0;
unlock_service_lock:
	rpbuf_mutex_unlock(link->service_lock);
out:
	rpbuf_mutex_unlock(__rpbuf_links_lock);
	return ret;
}

void rpbuf_unregister_service(struct rpbuf_service *service)
{
	struct rpbuf_link *link = service->link;
	uint32_t flags;

	rpbuf_mutex_lock(__rpbuf_links_lock);

	rpbuf_mutex_lock(link->service_lock);
	flags = rpbuf_spin_lock_irqsave(&link->lock);

	rpbuf_dbg("unregister service %p\n", service);

	link->service = NULL;
	service->link = NULL;
	link->ref_cnt--;

	if (link->ref_cnt > 0) {
		rpbuf_spin_unlock_irqrestore(&link->lock, flags);
		rpbuf_mutex_unlock(link->service_lock);
		rpbuf_mutex_unlock(__rpbuf_links_lock);
		return;
	}

	list_del(&link->list);

	rpbuf_spin_unlock_irqrestore(&link->lock, flags);
	rpbuf_mutex_unlock(link->service_lock);

	rpbuf_dbg("destroy link %p\n", link);
	rpbuf_destroy_link(link);

	rpbuf_mutex_unlock(__rpbuf_links_lock);
}

struct rpbuf_controller *rpbuf_create_controller(int id,
						 const struct rpbuf_controller_ops *ops,
						 void *priv)
{
	struct rpbuf_controller *controller;

	controller = rpbuf_zalloc(sizeof(struct rpbuf_controller));
	if (!controller) {
		rpbuf_err("zalloc for rpbuf_controller failed\n");
		controller = NULL;
		goto out;
	}

	controller->id = id;
	controller->ops = ops;
	controller->priv= priv;

	INIT_LIST_HEAD(&controller->remote_dummy_buffers);
	INIT_LIST_HEAD(&controller->local_dummy_buffers);
	memset(controller->buffers, 0,
			sizeof(struct rpbuf_buffer *) * RPBUF_BUFFERS_NUM_MAX);
	memset(controller->local_buffers, 0, RPBUF_BUFFERS_NUM_MAX);
out:
	return controller;
}

void rpbuf_destroy_controller(struct rpbuf_controller* controller)
{
	struct rpbuf_buffer *buffer;
	struct rpbuf_buffer *tmp;
	int i;

	if (!controller) {
		rpbuf_err("invalid controller\n");
		return;
	}

	list_for_each_entry_safe(buffer, tmp, &controller->remote_dummy_buffers, dummy_list) {
		list_del(&buffer->dummy_list);
		rpbuf_free_buffer_instance(buffer);
	}
	list_for_each_entry_safe(buffer, tmp, &controller->local_dummy_buffers, dummy_list) {
		list_del(&buffer->dummy_list);
		rpbuf_free_buffer_instance(buffer);
		rpbuf_put_free_local_buffers_id(controller, buffer->id);
	}
	for (i = 0; i < RPBUF_BUFFERS_NUM_MAX; i++) {
		buffer = controller->buffers[i];
		if (buffer) {
			rpbuf_free_buffer_instance(buffer);
		}
	}

	controller->priv = NULL;

	rpbuf_free(controller);
}

int rpbuf_register_controller(struct rpbuf_controller *controller,
			      void *token, enum rpbuf_role role)
{
	struct rpbuf_link *link;
	uint32_t flags;
	int ret;

	rpbuf_mutex_lock(__rpbuf_links_lock);

	if (!controller || !token) {
		ret = -EINVAL;
		goto out;
	}

	rpbuf_dbg("register controller %p (token: %p)\n", controller, token);

	link = rpbuf_find_link(token);
	if (!link) {
		link = rpbuf_create_link(token);
		if (!link) {
			rpbuf_err("rpbuf_create_link failed\n");
			ret = -EINVAL;
			goto out;
		}
		rpbuf_dbg("create new link %p (token: %p)\n", link, token);
		list_add_tail(&link->list, &__rpbuf_links);
	}

	rpbuf_mutex_lock(link->controller_lock);
	flags = rpbuf_spin_lock_irqsave(&link->lock);

	if (link->controller && link->controller != controller) {
		rpbuf_spin_unlock_irqrestore(&link->lock, flags);
		rpbuf_err("another controller in the same token has been registered\n");
		ret = -EINVAL;
		goto unlock_controller_lock;
	}

	link->controller = controller;
	controller->link = link;
	link->ref_cnt++;

	rpbuf_dbg("link: service: %p, controller: %p, ref_cnt: %d\n",
		  link->service, link->controller, link->ref_cnt);

	rpbuf_spin_unlock_irqrestore(&link->lock, flags);

	controller->role = role;

	ret = 0;
unlock_controller_lock:
	rpbuf_mutex_unlock(link->controller_lock);
out:
	rpbuf_mutex_unlock(__rpbuf_links_lock);
	return ret;
}

void rpbuf_unregister_controller(struct rpbuf_controller *controller)
{
	struct rpbuf_link *link = controller->link;
	uint32_t flags;

	rpbuf_mutex_lock(__rpbuf_links_lock);

	rpbuf_mutex_lock(link->controller_lock);
	flags = rpbuf_spin_lock_irqsave(&link->lock);

	rpbuf_dbg("unregister controller %p\n", controller);

	link->controller = NULL;
	controller->link = NULL;
	link->ref_cnt--;

	if (link->ref_cnt > 0) {
		rpbuf_spin_unlock_irqrestore(&link->lock, flags);
		rpbuf_mutex_unlock(link->controller_lock);
		rpbuf_mutex_unlock(__rpbuf_links_lock);
		return;
	}

	list_del(&link->list);

	rpbuf_spin_unlock_irqrestore(&link->lock, flags);
	rpbuf_mutex_unlock(link->controller_lock);

	rpbuf_dbg("destroy link %p\n", link);
	rpbuf_destroy_link(link);

	rpbuf_mutex_unlock(__rpbuf_links_lock);
}

static const int rpbuf_service_message_content_len[RPBUF_SERVICE_CMD_MAX] = {
	0, /* RPBUF_SERVICE_CMD_UNKNOWN */
	sizeof(struct rpbuf_service_content_buffer_created),
	sizeof(struct rpbuf_service_content_buffer_destroyed),
	sizeof(struct rpbuf_service_content_buffer_transmitted),
};

/*
 * Return size of the rpbuf service message on success, otherwise a negative
 * number on failure.
 */
static int rpbuf_compose_service_message(uint8_t *msg, enum rpbuf_service_command cmd,
					 void *content)
{
	struct rpbuf_service_message_header header;
	struct rpbuf_service_message_trailer trailer;
	uint8_t *p;

	if (cmd == RPBUF_SERVICE_CMD_UNKNOWN || cmd >= RPBUF_SERVICE_CMD_MAX) {
		rpbuf_err("invalid rpbuf service command\n");
		return -EINVAL;
	}

	header.preamble = RPBUF_SERVICE_MESSAGE_PREAMBLE;
	header.command = cmd;

	header.content_len = rpbuf_service_message_content_len[cmd];
	if (header.content_len > RPBUF_SERVICE_MESSAGE_LENGTH_MAX
			- sizeof(struct rpbuf_service_message_header)
			- sizeof(struct rpbuf_service_message_trailer)) {
		rpbuf_err("rpbuf service message content length too long\n");
		return -EINVAL;
	}

	trailer.postamble = RPBUF_SERVICE_MESSAGE_POSTAMBLE;

	p = msg;
	memcpy(p, &header, sizeof(header));
	p += sizeof(header);
	memcpy(p, content, header.content_len);
	p += header.content_len;
	memcpy(p, &trailer, sizeof(trailer));

	return sizeof(header) + header.content_len + sizeof(trailer);
}

static int rpbuf_notify_by_service(struct rpbuf_service *service,
				   enum rpbuf_service_command cmd, void *content)
{
	int ret;
	uint8_t msg[RPBUF_SERVICE_MESSAGE_LENGTH_MAX];
	int msg_len;

	ret = rpbuf_compose_service_message(msg, cmd, content);
	if (ret < 0) {
		rpbuf_err("failed to generate rpbuf service message\n");
		return ret;
	}
	msg_len = ret;

	if (!service->ops || !service->ops->notify) {
		rpbuf_err("service has not valid notify operation\n");
		return -EINVAL;
	}
	ret = service->ops->notify(msg, msg_len, service->priv);
	if (ret < 0) {
		rpbuf_err("notify error: %d\n", ret);
		return ret;
	}

	return 0;
}

/*
 * NOTE:
 *   To avoid deadlock, this function CANNOT be run within 'link->controller_lock'.
 */
static int rpbuf_notify_by_link(struct rpbuf_link *link,
				enum rpbuf_service_command cmd, void *content)
{
	int ret;
	uint32_t flags;
	struct rpbuf_service *service;

	rpbuf_mutex_lock(link->service_lock);

	flags = rpbuf_spin_lock_irqsave(&link->lock);
	service = link->service;
	if (!service) {
		rpbuf_err("link %p has no service\n", link);
		rpbuf_spin_unlock_irqrestore(&link->lock, flags);
		ret = -ENOENT;
		goto out;
	}
	rpbuf_spin_unlock_irqrestore(&link->lock, flags);

	ret = rpbuf_notify_by_service(service, cmd, content);
	if (ret < 0) {
		rpbuf_err("notify BUFFER_CREATED error\n");
		goto out;
	}

	ret = 0;
out:
	rpbuf_mutex_unlock(link->service_lock);
	return ret;
}

static int rpbuf_service_command_buffer_created_handler(struct rpbuf_service *service,
							enum rpbuf_service_command cmd,
							void *content)
{
	int ret;
	struct rpbuf_link *link = service->link;
	struct rpbuf_controller *controller;
	uint32_t flags;
	enum rpbuf_role role;
	struct rpbuf_service_content_buffer_created *cont = content;
	struct rpbuf_service_content_buffer_created content_back;
	struct rpbuf_buffer *buffer;
	int i;
	int id;
	int is_available = 0;

	rpbuf_mutex_lock(link->controller_lock);

	flags = rpbuf_spin_lock_irqsave(&link->lock);
	controller = link->controller;
	if (!controller) {
		rpbuf_err("service %p not linked with controller\n", service);
		rpbuf_spin_unlock_irqrestore(&link->lock, flags);
		ret = -ENOENT;
		goto unlock_controller_lock;
	}
	rpbuf_spin_unlock_irqrestore(&link->lock, flags);

	role = controller->role;
	if (role != RPBUF_ROLE_MASTER && role != RPBUF_ROLE_SLAVE) {
		rpbuf_err("unknown rpbuf role\n");
		ret = -EINVAL;
		goto unlock_controller_lock;
	}

	if (cont->id >= RPBUF_BUFFERS_NUM_MAX) {
		rpbuf_err("invalid buffer id %d. It cannot be larger than %d\n",
			  (int)cont->id, RPBUF_BUFFERS_NUM_MAX);
		ret = -EINVAL;
		goto unlock_controller_lock;
	}

	/*
	 * Check whether buffer with the same name already exists in 'buffers'.
	 * It should not exist normally. But if exists, it means that remote
	 * had some issues before and is re-allocating the buffer now. At this
	 * time if remote buffer is not available, We need to send BUFFER_CREATED
	 * command back to remote in order to complete its buffer creation.
	 */
	for (i = 0; i < RPBUF_BUFFERS_NUM_MAX; i++) {
		buffer = controller->buffers[i];
		if (!buffer)
			continue;
		if (0 == strncmp(buffer->name, (char *)cont->name, RPBUF_NAME_SIZE)) {
			rpbuf_info("(unexpected) remote re-allocating buffer \"%s\" "
				   "(available: %d)\n",
				   buffer->name, cont->is_available);

			if (cont->is_available) {
				ret = 0;
				goto unlock_controller_lock;
			}

			strncpy((char *)content_back.name, buffer->name, RPBUF_NAME_SIZE);
			content_back.id = buffer->id;
			content_back.len = buffer->len;
			content_back.pa = buffer->pa;
			content_back.da = buffer->da;
			content_back.is_available = 1;

			rpbuf_mutex_unlock(link->controller_lock);

			ret = rpbuf_notify_by_link(link,
						   RPBUF_SERVICE_CMD_BUFFER_CREATED,
						   (void *)&content_back);
			if (ret < 0) {
				rpbuf_err("rpbuf_notify_by_link BUFFER_CREATED "
					  "failed: %d\n", ret);
				goto out;
			}
			ret = 0;
			goto out;
		}
	}

	buffer = rpbuf_find_dummy_buffer_by_name(&controller->local_dummy_buffers,
						 (char *)cont->name);
	if (buffer) {
		/*
		 * A buffer with the same name is found in 'local_dummy_buffers',
		 * meaning that it has been allocated by local before. Now we
		 * update its information and move it to 'buffers'.
		 */

		if (buffer->len != cont->len) {
			rpbuf_err("buffer length not match with remote "
				"(local: %d, remote: %lu)\n", buffer->len, cont->len);
			ret = -EINVAL;
			goto unlock_controller_lock;
		}

		if (role == RPBUF_ROLE_SLAVE) {
			buffer->pa = cont->pa;
			buffer->da = cont->da;
			buffer->va = rpbuf_addr_remap(controller, buffer,
						      buffer->pa, buffer->da,
						      buffer->len);
		}

		if (role == RPBUF_ROLE_SLAVE)
			id = buffer->id;
		else
			id = cont->id;

		if (controller->buffers[id]) {
			rpbuf_err("buffer with id %d already exists\n", id);
			ret = -EINVAL;
			goto unlock_controller_lock;
		}

		controller->buffers[id] = buffer;
		buffer->id = id;

		list_del(&buffer->dummy_list);

		rpbuf_dbg("buffer \"%s\" (id:%d): local_dummy_buffers -> buffers\n",
			  buffer->name, buffer->id);
		is_available = 1;
	} else {
		/*
		 * If not found, create a new buffer to save this information
		 * and add it to 'remote_dummy_buffers', waiting for local to
		 * allocate it manually later.
		 *
		 * In this situation, if a buffer with the same name already
		 * exists in 'remote_dummy_buffers', it is an unexpected state.
		 * Maybe remote had some issues before and is re-allocating the
		 * buffer now. Anyway, we overwrite the previous information.
		 */
		buffer = rpbuf_find_dummy_buffer_by_name(&controller->remote_dummy_buffers,
							 (char *)cont->name);
		if (buffer) {
			rpbuf_info("buffer \"%s\": (unexpected) "
				   "overwrite information in 'remote_dummy_buffers'\n",
				   buffer->name);
			strncpy(buffer->name, (char *)cont->name, RPBUF_NAME_SIZE);
			buffer->len = cont->len;
			buffer->id = cont->id;
			buffer->pa = cont->pa;
			buffer->da = cont->da;
		} else {
			buffer = rpbuf_alloc_buffer_instance((char *)cont->name,
							     cont->len);
			if (!buffer) {
				rpbuf_err("rpbuf_alloc_buffer_instance failed\n");
				ret = -ENOMEM;
				goto unlock_controller_lock;
			}
			buffer->id = cont->id;
			buffer->pa = cont->pa;
			buffer->da = cont->da;
			list_add_tail(&buffer->dummy_list,
				      &controller->remote_dummy_buffers);

			rpbuf_dbg("buffer \"%s\": NULL -> remote_dummy_buffers\n",
				  buffer->name);
		}
	}

	ret = 0;

	if (is_available && buffer->cbs && buffer->cbs->available_cb) {
		rpbuf_mutex_unlock(link->controller_lock);
		buffer->cbs->available_cb(buffer, buffer->priv);
		goto out;
	}

unlock_controller_lock:
	rpbuf_mutex_unlock(link->controller_lock);
out:
	return ret;
}

static int rpbuf_service_command_buffer_destroyed_handler(struct rpbuf_service *service,
							  enum rpbuf_service_command cmd,
							  void *content)
{
	int ret;
	struct rpbuf_link *link = service->link;
	struct rpbuf_controller *controller;
	uint32_t flags;
	enum rpbuf_role role;
	struct rpbuf_service_content_buffer_destroyed *cont = content;
	struct rpbuf_buffer *buffer;

	rpbuf_mutex_lock(link->controller_lock);

	flags = rpbuf_spin_lock_irqsave(&link->lock);
	controller = link->controller;
	if (!controller) {
		rpbuf_err("service %p not linked with controller\n", service);
		rpbuf_spin_unlock_irqrestore(&link->lock, flags);
		ret = -ENOENT;
		goto err_out;
	}
	rpbuf_spin_unlock_irqrestore(&link->lock, flags);

	role = controller->role;
	if (role != RPBUF_ROLE_MASTER && role != RPBUF_ROLE_SLAVE) {
		rpbuf_err("unknown rpbuf role\n");
		ret = -EINVAL;
		goto err_out;
	}

	if (cont->id >= RPBUF_BUFFERS_NUM_MAX) {
		rpbuf_err("invalid buffer id %d. It cannot be larger than %d\n",
			  (int)cont->id, RPBUF_BUFFERS_NUM_MAX);
		ret = -EINVAL;
		goto err_out;
	}

	buffer = rpbuf_find_dummy_buffer_by_id(&controller->remote_dummy_buffers, cont->id);
	if (buffer) {
		/*
		 * A buffer with the same name is found in 'remote_dummy_buffers',
		 * meaning that local doesn't allocate it or has freed it.
		 * Now we delete it from the list.
		 */
		list_del(&buffer->dummy_list);

		rpbuf_dbg("buffer \"%s\": remote_dummy_buffers -> NULL\n",
			  buffer->name);

		/*
		 * Remember to free the instance after deleting a entry from
		 * 'remote_dummy_buffers'.
		 */
		rpbuf_free_buffer_instance(buffer);
		buffer = NULL;
	} else {
		/*
		 * If not found, meaning that local has allocated it. Now it
		 * should be stored in 'buffers'. We just move it to
		 * 'local_dummy_buffers' and wait for local to free it manually
		 * later.
		 */
		buffer = controller->buffers[cont->id];
		if (buffer) {
			controller->buffers[cont->id] = NULL;
			list_add_tail(&buffer->dummy_list, &controller->local_dummy_buffers);
			rpbuf_dbg("buffer \"%s\" (id:%d): buffers -> local_dummy_buffers\n",
				  buffer->name, (int)cont->id);
		} else {
			rpbuf_err("buffer not found, nothing handled with BUFFER_DESTROYED\n");
			ret = -EINVAL;
			goto err_out;
		}
	}

	rpbuf_mutex_unlock(link->controller_lock);

	if (buffer && buffer->cbs && buffer->cbs->destroyed_cb)
		return buffer->cbs->destroyed_cb(buffer, buffer->priv);

	return 0;

err_out:
	rpbuf_mutex_unlock(link->controller_lock);
	return ret;
}

static int rpbuf_service_command_buffer_transmitted_handler(struct rpbuf_service *service,
							    enum rpbuf_service_command cmd,
							    void *content)
{
	int ret;
	struct rpbuf_link *link = service->link;
	struct rpbuf_controller *controller;
	uint32_t flags;
	struct rpbuf_service_content_buffer_transmitted *cont = content;
	struct rpbuf_buffer *buffer;

	rpbuf_mutex_lock(link->controller_lock);

	flags = rpbuf_spin_lock_irqsave(&link->lock);
	controller = link->controller;
	if (!controller) {
		rpbuf_err("service %p not linked with controller\n", service);
		rpbuf_spin_unlock_irqrestore(&link->lock, flags);
		rpbuf_mutex_unlock(link->controller_lock);
		ret = -ENOENT;
		goto err_out;
	}
	rpbuf_spin_unlock_irqrestore(&link->lock, flags);

	if (cont->id >= RPBUF_BUFFERS_NUM_MAX) {
		rpbuf_err("invalid buffer id %d. It cannot be larger than %d\n",
			  (int)cont->id, RPBUF_BUFFERS_NUM_MAX);
		rpbuf_mutex_unlock(link->controller_lock);
		ret = -EINVAL;
		goto err_out;
	}

	buffer = controller->buffers[cont->id];
	if (!buffer) {
		rpbuf_info("no buffer with id %d in local\n", (int)cont->id);
		rpbuf_mutex_unlock(link->controller_lock);
		ret = -ENOENT;
		goto err_out;
	}

	rpbuf_mutex_unlock(link->controller_lock);

	rpbuf_dbg("buffer \"%s\" (id:%d) received from remote\n", buffer->name, buffer->id);

	rpbuf_dcache_invalidate((uint8_t *)buffer->va + cont->offset, buffer->len);

	if (buffer->cbs && buffer->cbs->rx_cb)
		return buffer->cbs->rx_cb(buffer, buffer->va + cont->offset,
					  cont->data_len, buffer->priv);

	return 0;

err_out:
	return ret;
}

static const rpbuf_service_command_handler_t
rpbuf_service_command_handlers[RPBUF_SERVICE_CMD_MAX] = {
	NULL, /* RPBUF_SERVICE_CMD_UNKNOWN */
	rpbuf_service_command_buffer_created_handler,
	rpbuf_service_command_buffer_destroyed_handler,
	rpbuf_service_command_buffer_transmitted_handler,
};

int rpbuf_service_get_notification(struct rpbuf_service *service, void *msg, int msg_len)
{
	int ret;
	struct rpbuf_service_message_header *header = msg;
	struct rpbuf_service_message_trailer *trailer
		= msg + sizeof(*header) + header->content_len;
	enum rpbuf_service_command cmd;

	if (sizeof(*header) + header->content_len + sizeof(*trailer) != msg_len) {
		rpbuf_err("incorrecti rpbuf service message length\n");
		ret = -EINVAL;
		goto err_out;
	}
	if (header->preamble != RPBUF_SERVICE_MESSAGE_PREAMBLE
			|| trailer->postamble != RPBUF_SERVICE_MESSAGE_POSTAMBLE) {
		rpbuf_err("invalid received rpbuf service message\n");
		ret = -EINVAL;
		goto err_out;
	}
	if (header->command == RPBUF_SERVICE_CMD_UNKNOWN
			|| header->command >= RPBUF_SERVICE_CMD_MAX) {
		rpbuf_err("invalid rpbuf service command\n");
		return -EINVAL;
	}
	cmd = header->command;
	if (header->content_len != rpbuf_service_message_content_len[cmd]) {
		rpbuf_err("rpbuf service message content length not match "
			  "(command: %d, request: %d, actual: %d)\n",
			  cmd, rpbuf_service_message_content_len[cmd], header->content_len);
		ret = -EINVAL;
		goto err_out;
	}

	ret = rpbuf_service_command_handlers[cmd](service, cmd, msg + sizeof(*header));
	if (ret < 0) {
		rpbuf_err("rpbuf_service_command_handlers[%d] error (%d)\n",
			  cmd, ret);
		goto err_out;
	}

	return 0;
err_out:
	return ret;
}

static int rpbuf_free_buffer_internal(struct rpbuf_buffer *buffer, int do_notify)
{
	int ret;
	struct rpbuf_controller *controller;
	struct rpbuf_link *link;
	enum rpbuf_role role;
	struct rpbuf_buffer *dummy_buffer;
	struct rpbuf_service_content_buffer_destroyed content;

	if (!buffer || !buffer->controller || !buffer->controller->link) {
		rpbuf_err("invalid arguments\n");
		ret = -EINVAL;
		goto err_out;
	}
	controller = buffer->controller;
	link = controller->link;
	role = controller->role;

	if (role != RPBUF_ROLE_MASTER && role != RPBUF_ROLE_SLAVE) {
		rpbuf_err("unknown rpbuf role\n");
		ret = -EINVAL;
		goto err_out;
	}

	content.id = buffer->id;

	/*
	 * Notify remote first. If this notification failed, remote doesn't know
	 * the buffer is freed, we cannot go ahead to release the buffer resource.
	 *
	 * NOTE:
	 * If later operation failed, when next time we call rpbuf_free_buffer(),
	 * BUFFER_DESTROYED message will be sent repeatly to remote. We should
	 * handle this situation in BUFFER_DESTROYED handler.
	 */
	if (do_notify) {
		ret = rpbuf_notify_by_link(link,
					   RPBUF_SERVICE_CMD_BUFFER_DESTROYED,
					   (void *)&content);
		if (ret < 0) {
			rpbuf_err("rpbuf_notify_by_link BUFFER_DESTROYED failed: %d\n", ret);
			goto err_out;
		}
	}

	rpbuf_mutex_lock(link->controller_lock);

	dummy_buffer = rpbuf_find_dummy_buffer_by_name(
			&controller->local_dummy_buffers, buffer->name);
	if (dummy_buffer) {
		/*
		 * A buffer with the same name is found in 'local_dummy_buffers',
		 * meaning that remote doesn't allocate it or has freed it.
		 * Here the 'dummy_buffer' and 'buffer' should be the same.
		 */

		if (dummy_buffer != buffer) {
			rpbuf_err("unexpected buffer ptr %p, it should be %p\n",
				  buffer, dummy_buffer);
			rpbuf_mutex_unlock(link->controller_lock);
			ret = -EINVAL;
			goto err_out;
		}

		list_del(&buffer->dummy_list);
		controller->buffers[buffer->id] = NULL;

		rpbuf_dbg("buffer \"%s\": local_dummy_buffers -> NULL\n",
			  buffer->name);
	} else {
		/*
		 * If not found, meaning that remote has allocated it. Now it
		 * should be stored in 'buffers' and we should move it
		 * to 'remote_dummy_buffers'.
		 */

		dummy_buffer = controller->buffers[buffer->id];
		if (!dummy_buffer) {
			rpbuf_err("buffer not found, maybe it is not allocated?\n");
			rpbuf_mutex_unlock(link->controller_lock);
			ret = -EINVAL;
			goto err_out;
		} else if (dummy_buffer != buffer) {
			rpbuf_err("unexpected buffer ptr %p, it should be %p\n",
				  buffer, dummy_buffer);
			rpbuf_mutex_unlock(link->controller_lock);
			ret = -EINVAL;
			goto err_out;
		}
		controller->buffers[buffer->id] = NULL;

		dummy_buffer = rpbuf_find_dummy_buffer_by_name(
				&controller->remote_dummy_buffers, buffer->name);
		if (dummy_buffer) {
			/*
			 * In this situation, if a buffer with the same name
			 * already exists in 'remote_dummy_buffers', it is an
			 * unexpected state. Anyway, we overwrite the previous
			 * information.
			 */
			rpbuf_info("buffer \"%s\": (unexpected) "
				"overwrite information in 'remote_dummy_buffers'\n",
				buffer->name);
			strncpy(dummy_buffer->name, buffer->name, RPBUF_NAME_SIZE);
			dummy_buffer->len = buffer->len;
			dummy_buffer->id = buffer->id;
			dummy_buffer->pa = buffer->pa;
			dummy_buffer->da = buffer->da;
		} else {
			/*
			 * The instance of 'buffer' will be freed later, so we
			 * cannot directly add it to 'remote_dummy_buffers'.
			 * Instead, we allocate a new instance and copy 'buffer'
			 * information to it.
			 */
			dummy_buffer = rpbuf_alloc_buffer_instance(buffer->name,
								   buffer->len);
			if (!dummy_buffer) {
				rpbuf_err("rpbuf_alloc_buffer_instance failed\n");
				rpbuf_mutex_unlock(link->controller_lock);
				ret = -ENOMEM;
				goto err_out;
			}
			dummy_buffer->id = buffer->id;
			dummy_buffer->pa = buffer->pa;
			dummy_buffer->da = buffer->da;
			list_add_tail(&dummy_buffer->dummy_list,
				      &controller->remote_dummy_buffers);

			rpbuf_dbg("buffer \"%s\" (id:%d): buffers -> remote_dummy_buffers\n",
				  buffer->name, buffer->id);
		}
	}

	/*
	 * Anyway MASTER frees payload memory here. Therefore users should ensure
	 * that SLAVE calls rpbuf_free_buffer() before MASTER.
	 */
	if (role == RPBUF_ROLE_MASTER)
		rpbuf_free_buffer_payload_memory(controller, buffer);
	else
		rpbuf_put_free_local_buffers_id(controller, buffer->id);

	rpbuf_mutex_unlock(link->controller_lock);

	rpbuf_free_buffer_instance(buffer);

	return 0;
err_out:
	return ret;
}

struct rpbuf_buffer *rpbuf_alloc_buffer(struct rpbuf_controller *controller,
					const char *name, int len,
					const struct rpbuf_buffer_ops *ops,
					const struct rpbuf_buffer_cbs *cbs,
					void *priv)
{
	int ret;
	struct rpbuf_link *link;
	enum rpbuf_role role;
	struct rpbuf_buffer *buffer;
	struct rpbuf_buffer *dummy_buffer;
	int i;
	struct rpbuf_service_content_buffer_created content;
	int is_available = 0;

	if (!controller || !controller->link) {
		rpbuf_err("invalid arguments\n");
		goto err_out;
	}
	link = controller->link;
	role = controller->role;

	if (role != RPBUF_ROLE_MASTER && role != RPBUF_ROLE_SLAVE) {
		rpbuf_err("unknown rpbuf role\n");
		goto err_out;
	}

	buffer = rpbuf_alloc_buffer_instance(name, len);
	if (!buffer) {
		rpbuf_err("rpbuf_alloc_buffer_instance failed\n");
		goto err_out;
	}
	buffer->controller = controller;
	buffer->ops = ops;
	buffer->cbs = cbs;
	buffer->priv = priv;

	rpbuf_mutex_lock(link->controller_lock);

	if (role == RPBUF_ROLE_MASTER) {
		ret = rpbuf_alloc_buffer_payload_memory(controller, buffer);
		if (ret < 0) {
			rpbuf_err("rpbuf_alloc_buffer_payload_memory failed\n");
			rpbuf_mutex_unlock(link->controller_lock);
			goto err_free_instance;
		}
	}

	dummy_buffer = rpbuf_find_dummy_buffer_by_name(
			&controller->local_dummy_buffers, name);
	if (dummy_buffer) {
		rpbuf_err("buffer \"%s\" already exists in 'local_dummy_buffers'\n", name);
		rpbuf_mutex_unlock(link->controller_lock);
		goto err_free_payload_memory;
	}
	for (i = 0; i < RPBUF_BUFFERS_NUM_MAX; i++) {
		dummy_buffer = controller->buffers[i];
		if (!dummy_buffer) {
			continue;
		}
		if (0 == strncmp(dummy_buffer->name, name, RPBUF_NAME_SIZE)) {
			rpbuf_err("buffer \"%s\" already exists in 'buffers'\n", name);
			rpbuf_mutex_unlock(link->controller_lock);
			goto err_free_payload_memory;
		}
	}

	dummy_buffer = rpbuf_find_dummy_buffer_by_name(
			&controller->remote_dummy_buffers, name);
	if (dummy_buffer) {
		/*
		 * A buffer with the same name is found in 'remote_dummy_buffers',
		 * meaning that remote has allocated it and notified local.
		 * Now we move it to 'buffers'.
		 */

		if (buffer->len != dummy_buffer->len) {
			rpbuf_err("buffer length not match with remote "
				  "(local: %d, remote: %d)\n",
				  buffer->len, dummy_buffer->len);
			rpbuf_mutex_unlock(link->controller_lock);
			goto err_free_payload_memory;
		}

		if (role == RPBUF_ROLE_SLAVE) {
			buffer->pa = dummy_buffer->pa;
			buffer->da = dummy_buffer->da;
			buffer->va = rpbuf_addr_remap(controller, buffer,
						      buffer->pa, buffer->da,
						      buffer->len);

			/* find a free buffer id */
			buffer->id = rpbuf_get_free_local_buffers_id(controller);
			if (buffer->id < 0) {
				rpbuf_err("buffers has no free id, "
					  "Maybe we should enlarge RPBUF_BUFFERS_NUM_MAX(%d)\n",
					  RPBUF_BUFFERS_NUM_MAX);
				rpbuf_mutex_unlock(link->controller_lock);
				ret = -EACCES;
				goto err_free_payload_memory;
			}
			rpbuf_dbg("buffer \"%s\"(id:%d): NULL -> local_dummy_buffers\n",
				  buffer->name, buffer->id);

			if (controller->buffers[buffer->id]) {
				rpbuf_err("buffer already exists(id:%d:%s).\n",
					  buffer->id, controller->buffers[buffer->id]->name);
				rpbuf_mutex_unlock(link->controller_lock);
				ret = -EACCES;
				goto err_free_payload_memory;
			}
			controller->buffers[buffer->id] = buffer;
		} else {
			buffer->id = dummy_buffer->id;
			controller->buffers[buffer->id] = buffer;
			rpbuf_dbg("buffer \"%s\": NULL -> local_dummy_buffers\n", buffer->name);
		}

		/*
		 * The instances of entries in 'remote_dummy_buffers' are not
		 * allocated/freed by rpbuf_alloc_buffer()/rpbuf_free_buffer().
		 * After deleting from the list, we should free the instance.
		 */
		list_del(&dummy_buffer->dummy_list);
		rpbuf_free_buffer_instance(dummy_buffer);

		rpbuf_dbg("buffer \"%s\" (id:%d): remote_dummy_buffers -> buffers\n",
			  buffer->name, buffer->id);
		is_available = 1;
	} else {
		if (role == RPBUF_ROLE_SLAVE) {
			/* find a free buffer id */
			buffer->id = rpbuf_get_free_local_buffers_id(controller);
			if (buffer->id < 0) {
				rpbuf_err("buffers cannot be more than %d. "
					  "Maybe we should enlarge RPBUF_BUFFERS_NUM_MAX\n",
					  RPBUF_BUFFERS_NUM_MAX);
				rpbuf_mutex_unlock(link->controller_lock);
				ret = -EACCES;
				goto err_free_payload_memory;
			}
		}
		/*
		 * If not found, add this buffer to 'local_dummy_buffers',
		 * waiting for remote message to update it.
		 */
		list_add_tail(&buffer->dummy_list, &controller->local_dummy_buffers);
		if (role == RPBUF_ROLE_SLAVE) {
			rpbuf_dbg("buffer \"%s\"(id:%d): NULL -> local_dummy_buffers\n",
				  buffer->name, buffer->id);
		} else {
			rpbuf_dbg("buffer \"%s\": NULL -> local_dummy_buffers\n",
				  buffer->name);
		}
	}

	rpbuf_mutex_unlock(link->controller_lock);

	/* Copy buffer information to rpbuf message content */
	strncpy((char *)content.name, buffer->name, RPBUF_NAME_SIZE);
	content.id = buffer->id;
	content.len = buffer->len;
	content.pa = buffer->pa;
	content.da = buffer->da;
	content.is_available = is_available;

	/* Notify remote */
	ret = rpbuf_notify_by_link(link,
				   RPBUF_SERVICE_CMD_BUFFER_CREATED,
				   (void *)&content);
	if (ret < 0) {
		rpbuf_err("rpbuf_notify_by_link BUFFER_CREATED failed: %d\n", ret);
		/* Free allocated resource, without sending BUFFER_DESTROYED to remote */
		rpbuf_free_buffer_internal(buffer, 0);
		goto err_out;
	}

	if (is_available && buffer->cbs && buffer->cbs->available_cb)
		buffer->cbs->available_cb(buffer, buffer->priv);

	return buffer;

err_free_payload_memory:
	if (role == RPBUF_ROLE_MASTER) {
		rpbuf_mutex_lock(link->controller_lock);
		rpbuf_free_buffer_payload_memory(controller, buffer);
		rpbuf_mutex_unlock(link->controller_lock);
	}
err_free_instance:
	rpbuf_free_buffer_instance(buffer);
err_out:
	return NULL;
}

int rpbuf_free_buffer(struct rpbuf_buffer *buffer)
{
	return rpbuf_free_buffer_internal(buffer, 1);
}

int rpbuf_buffer_is_available(struct rpbuf_buffer *buffer)
{
	struct rpbuf_controller *controller;
	struct rpbuf_link *link;
	struct rpbuf_buffer *tmp_buffer;

	if (!buffer || !buffer->controller || !buffer->controller->link) {
		rpbuf_err("invalid arguments\n");
		return 0;
	}
	controller = buffer->controller;
	link = controller->link;

	if (buffer->id >= RPBUF_BUFFERS_NUM_MAX) {
		rpbuf_err("invalid buffer id %d. It cannot be larger than %d\n",
			  buffer->id, RPBUF_BUFFERS_NUM_MAX);
		return 0;
	}

	rpbuf_mutex_lock(link->controller_lock);

	tmp_buffer = controller->buffers[buffer->id];
	if (!tmp_buffer) {
		rpbuf_mutex_unlock(link->controller_lock);
		return 0;
	} else if (tmp_buffer != buffer) {
		rpbuf_err("unexpected buffer ptr %p, it should be %p\n",
			  buffer, tmp_buffer);
		rpbuf_mutex_unlock(link->controller_lock);
		return 0;
	}

	rpbuf_mutex_unlock(link->controller_lock);

	return 1;
}

int rpbuf_transmit_buffer(struct rpbuf_buffer *buffer,
			  unsigned int offset, unsigned int data_len)
{
	int ret;
	struct rpbuf_controller *controller;
	struct rpbuf_link *link;
	struct rpbuf_service_content_buffer_transmitted content;

	if (!buffer || !buffer->controller || !buffer->controller->link) {
		rpbuf_err("invalid arguments\n");
		return -EINVAL;
	}
	controller = buffer->controller;
	link = controller->link;

	if (offset + data_len > buffer->len) {
		rpbuf_err("data (offset + len: %u + %u) over the buffer range (len: %d)\n",
			  offset, data_len, buffer->len);
		return -EINVAL;
	}

	content.id = buffer->id;
	content.offset = offset;
	content.data_len = data_len;

	if (!rpbuf_buffer_is_available(buffer)) {
		rpbuf_err("buffer not available\n");
		return -EACCES;
	}

	rpbuf_dcache_clean((uint8_t *)buffer->va + offset, buffer->len);

	ret = rpbuf_notify_by_link(link, RPBUF_SERVICE_CMD_BUFFER_TRANSMITTED,
				   (void *)&content);
	if (ret < 0) {
		rpbuf_err("rpbuf_notify_by_link BUFFER_TRANSMITTED failed: %d\n", ret);
		return ret;
	}

	return 0;
}

const char *rpbuf_buffer_name(struct rpbuf_buffer *buffer)
{
	return buffer->name;
}

int rpbuf_buffer_id(struct rpbuf_buffer *buffer)
{
	return buffer->id;
}

void *rpbuf_buffer_va(struct rpbuf_buffer *buffer)
{
	return buffer->va;
}

uintptr_t rpbuf_buffer_pa(struct rpbuf_buffer *buffer)
{
	return buffer->pa;
}

uint64_t rpbuf_buffer_da(struct rpbuf_buffer *buffer)
{
	return buffer->da;
}

int rpbuf_buffer_len(struct rpbuf_buffer *buffer)
{
	return buffer->len;
}

void *rpbuf_buffer_priv(struct rpbuf_buffer *buffer)
{
	return buffer->priv;
}

void rpbuf_buffer_set_va(struct rpbuf_buffer *buffer, void *va)
{
	buffer->va = va;
}

void rpbuf_buffer_set_pa(struct rpbuf_buffer *buffer, uintptr_t pa)
{
	buffer->pa = pa;
}

void rpbuf_buffer_set_da(struct rpbuf_buffer *buffer, uint64_t da)
{
	buffer->da = da;
}
