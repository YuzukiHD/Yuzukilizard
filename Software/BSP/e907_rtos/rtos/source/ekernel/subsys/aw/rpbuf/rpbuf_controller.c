#include <errno.h>
#include <aw_list.h>
#include <init.h>
#include "rpbuf_internal.h"

LIST_HEAD(__rpbuf_controllers);
static rpbuf_mutex_t __rpbuf_controllers_lock;

static int rpbuf_controllers_lock_create(void)
{
	__rpbuf_controllers_lock = rpbuf_mutex_create();
	return 0;
}
core_initcall(rpbuf_controllers_lock_create);

/* will never execute */
static void rpbuf_controllers_lock_delete(void)
{
	rpbuf_mutex_delete(__rpbuf_controllers_lock);
}

struct rpbuf_controller *rpbuf_init_controller(int id, void *token, enum rpbuf_role role,
					       const struct rpbuf_controller_ops *ops,
					       void *priv)
{
	int ret;
	struct rpbuf_controller *controller;

	if (role != RPBUF_ROLE_MASTER && role != RPBUF_ROLE_SLAVE) {
		rpbuf_err("invalid rpbuf role\n");
		goto err_out;
	}

	controller = rpbuf_create_controller(id, ops, NULL);
	if (!controller) {
		rpbuf_err("rpbuf_create_controller failed\n");
		goto err_out;
	}

	ret = rpbuf_register_controller(controller, token, role);
	if (ret < 0) {
		rpbuf_err("rpbuf_register_controller failed\n");
		goto err_destroy_controller;
	}

	rpbuf_mutex_lock(__rpbuf_controllers_lock);
	list_add_tail(&controller->list, &__rpbuf_controllers);
	rpbuf_mutex_unlock(__rpbuf_controllers_lock);

	return controller;

err_destroy_controller:
	rpbuf_destroy_controller(controller);
err_out:
	return NULL;
}

void rpbuf_deinit_controller(struct rpbuf_controller* controller)
{
	if (!controller) {
		rpbuf_err("invalid rpbuf_controller ptr\n");
		return;
	}

	rpbuf_mutex_lock(__rpbuf_controllers_lock);
	list_del(&controller->list);
	rpbuf_mutex_unlock(__rpbuf_controllers_lock);

	rpbuf_unregister_controller(controller);
	rpbuf_destroy_controller(controller);
}

struct rpbuf_controller *rpbuf_get_controller_by_id(int id)
{
	struct rpbuf_controller *controller;

	rpbuf_mutex_lock(__rpbuf_controllers_lock);
	list_for_each_entry(controller, &__rpbuf_controllers, list) {
		if (controller->id == id) {
			rpbuf_mutex_unlock(__rpbuf_controllers_lock);
			return controller;
		}
	}
	rpbuf_mutex_unlock(__rpbuf_controllers_lock);

	return NULL;
}
