#include <hal_osal.h>
#include <init.h>

#include <openamp/sunxi_helper/openamp_platform.h>
#include <openamp/sunxi_helper/openamp_log.h>
#include <openamp/sunxi_helper/sunxi_rproc.h>
#include <openamp/sunxi_helper/mem_map.h>

#include "sunxi_rsc_tab.h"

static METAL_DECLARE_LIST(_sunxi_rprocs);
static METAL_DECLARE_LIST(_sunxi_rvdevs);
static hal_mutex_t _sunxi_rprocs_mutex;
static hal_mutex_t _sunxi_rvdevs_mutex;

struct openamp_sunxi_rproc {
	int id;
	struct remoteproc inst;
	struct metal_list node;
};

struct openamp_sunxi_rvdev {
	int id;
	struct rpmsg_virtio_device inst;
	struct metal_list node;
};

static int openamp_sunxi_mutex_create(void)
{
	_sunxi_rvdevs_mutex = hal_mutex_create();
	_sunxi_rprocs_mutex = hal_mutex_create();

	if ((_sunxi_rprocs_mutex == NULL) ||
			(_sunxi_rvdevs_mutex == NULL)) {
		openamp_err("remoteproc_init failed\n");

		return -1;
	}

	return 0;
}
core_initcall(openamp_sunxi_mutex_create)


static void sunxi_rprocs_add_rproc(struct openamp_sunxi_rproc *rproc)
{
	hal_mutex_lock(_sunxi_rprocs_mutex);
	metal_list_add_tail(&_sunxi_rprocs, &rproc->node);
	hal_mutex_unlock(_sunxi_rprocs_mutex);
}

static void sunxi_vdevs_add_vdev(struct openamp_sunxi_rvdev *rvdev)
{
	hal_mutex_lock(_sunxi_rvdevs_mutex);
	metal_list_add_tail(&_sunxi_rvdevs, &rvdev->node);
	hal_mutex_unlock(_sunxi_rvdevs_mutex);
}

static void sunxi_rprocs_del_rproc(struct openamp_sunxi_rproc *rproc)
{
	hal_mutex_lock(_sunxi_rprocs_mutex);
	metal_list_del(&rproc->node);
	hal_mutex_unlock(_sunxi_rprocs_mutex);
}

static void sunxi_vdevs_del_vdev(struct openamp_sunxi_rvdev *rvdev)
{
	hal_mutex_lock(_sunxi_rvdevs_mutex);
	metal_list_del(&rvdev->node);
	hal_mutex_unlock(_sunxi_rvdevs_mutex);
}

static struct openamp_sunxi_rproc *openamp_sunxi_rproc_init(int proc_id)
{
	int impls_size = 0;
	struct openamp_sunxi_rproc *rproc;
	struct rproc_global_impls *impl;
	struct remoteproc *ret = NULL;

	/* Create rproc and init it */
	rproc = metal_allocate_memory(sizeof(*rproc));
	if(!rproc) {
		openamp_err("Failed to allocate memory for platform_rproc\r\n");
		goto out;
	}

	if(proc_id >= sunxi_rproc_impls_size) {
		openamp_err("remoteproc_init failed\r\n");
		goto free_rproc;
	}
	impl = &(sunxi_rproc_impls[proc_id]);
	ret = remoteproc_init(&rproc->inst, impl->ops, impl->priv);
	if(ret == NULL) {
		openamp_err("remoteproc_init failed\n");
		goto free_rproc;
	}

	return rproc;
free_rproc:
	metal_free_memory(rproc);
out:
	return NULL;
}

static void resource_table_dump(void *rsc, int size)
{
	int i, j;
	struct resource_table *res = rsc;
	char *res_start;
	openamp_dbg("Resource table: entries: %d \r\n",res->num);

	for (i = 0; i < res->num; i++) {

		res_start = rsc;
		res_start += res->offset[i];
		uint32_t type = *(uint32_t *)res_start;
		if (type == RSC_CARVEOUT) {
			struct fw_rsc_carveout *shm = 
				(struct fw_rsc_carveout *)res_start;
			openamp_dbg("entry%d-%s-shm: pa:0x%08x da:%08x len:0x%x\r\n",
					i, shm->name, shm->pa, shm->da, shm->len);
		} 
		else if (type == RSC_VDEV) {
			struct fw_rsc_vdev *vdev =
				(struct fw_rsc_vdev *)res_start;

			openamp_dbg("entry%d-vdev%d: notifyid:%d cfg_len:%d\r\n",
					i, vdev->id, vdev->notifyid, vdev->config_len);
			for (j = 0; j < vdev->num_of_vrings; j++) {
				struct fw_rsc_vdev_vring *vring = &vdev->vring[j];
				openamp_dbg("entry%d-vring%d: notifyid:%d da:0x%08x\r\n",
						i, j, vring->notifyid, vring->da);
			}
		}
	}
}

struct remoteproc *openamp_sunxi_create_rproc(int proc_id, int rsc_id)
{
	int ret;

	void *rsc_table = NULL;
	int rsc_table_size;
	metal_phys_addr_t table_pa;
	uint32_t attribute;

	int impls_size = 0;
	struct openamp_sunxi_rproc *rproc;
	struct openamp_sunxi_rvdev *rvdev;
	struct rproc_global_impls *impl;

	/* Get resource table by id */
	resource_table_init(rsc_id, &rsc_table, &rsc_table_size);

	openamp_dbg("resource table: va: 0x%08x, size: 0x%x\n",
			(uint32_t)rsc_table, rsc_table_size);

	rproc = openamp_sunxi_rproc_init(proc_id);
	if(rproc == NULL)
		goto out;

	/*  Mmap resource table, so that remoteproc can use it. */
	openamp_dbg("start map resource table\r\n");
	ret = mem_va_to_pa((uint32_t)rsc_table, &table_pa, &attribute);
	if(ret != 0) {
		openamp_dbg("Failed to translate rsc_table address %p to pa\n", rsc_table);
		goto remove_rproc;
	}

	rsc_table = remoteproc_mmap(&rproc->inst, &table_pa, NULL, rsc_table_size, attribute,
					&rproc->inst.rsc_io);

#ifdef CONFIG_SLAVE_EARLY_BOOT
	openamp_info("Wait master update resource_table\r\n");
	rproc_virtio_early_wait_remote_ready(resource_table_get_vdev(0),
					rproc->inst.rsc_io, VIRTIO_DEV_SLAVE);
	openamp_info("resource_table ready\r\n");
	hal_dcache_invalidate((unsigned long)rsc_table, rsc_table_size);
#endif

	/* Parse resource table to remoteproc */
	ret = remoteproc_set_rsc_table(&rproc->inst, (struct resource_table *)rsc_table,
					rsc_table_size);
	if(ret != 0) {
		openamp_err("Failed to translate rsc_table address %p to pa\n", rsc_table);
		goto remove_rproc;
	}

	rproc->id = proc_id;
	sunxi_rprocs_add_rproc(rproc);

	return &rproc->inst;
remove_rproc:
	remoteproc_remove(&rproc->inst);
free_rproc:
	metal_free_memory(rproc);
out:
	return NULL;
}

void openamp_sunxi_release_rproc(struct remoteproc *inst)
{
	struct openamp_sunxi_rproc *rproc = metal_container_of(
					inst, struct openamp_sunxi_rproc, inst);

	sunxi_rprocs_del_rproc(rproc);

	remoteproc_remove(inst);
	metal_free_memory(rproc);
}

struct remoteproc *openamp_sunxi_get_rproc(int id)
{
	struct openamp_sunxi_rproc *rproc = NULL;
	struct remoteproc *inst = NULL;
	struct metal_list *node;

	hal_mutex_lock(_sunxi_rprocs_mutex);
	metal_list_for_each(&_sunxi_rprocs, node) {
		rproc = metal_container_of(node, struct openamp_sunxi_rproc, node);
		if(rproc->id == id) {
			inst = &rproc->inst;
			break;
		}
	}
	hal_mutex_unlock(_sunxi_rprocs_mutex);
	return inst;
}

struct rpmsg_device *openamp_sunxi_create_rpmsg_vdev(struct remoteproc *rproc,
		int vdev_id, int role,void (*rst_cb)(struct virtio_device *vdev),
		rpmsg_ns_bind_cb ns_bind_cb)
{
	int ret;
	struct openamp_sunxi_rvdev *rvdev;
	struct virtio_device *vdev;
	struct virtio_device *inst;
	struct fw_rsc_carveout *rvdev_shm;
	struct metal_io_region *shm_io;
	void *shm_buf;
	struct rpmsg_virtio_shm_pool *shm_pool = NULL;

	/* Allocate vdev device memory */
	rvdev = metal_allocate_memory(sizeof(*rvdev));
	if(!rvdev) {
		openamp_err("Failed to allocate memory for sunxi_rvdev\r\n");
		goto out;
	}

	/* Get shared memory resource */
	rvdev_shm = resource_table_get_rvdev_shm_entry(rproc->rsc_table, vdev_id);
	if(!rvdev_shm) {
		openamp_err("Failed to get shared memory resource\r\n");
		goto free_rvdev;
	}
	openamp_dbg("rvdev_shm pa:0x%08x da:0x%08x\r\n", rvdev_shm->pa, rvdev_shm->da);

	shm_io = remoteproc_get_io_with_pa(rproc, rvdev_shm->pa);
	if(!shm_io) {
		openamp_err("Failed to get shared memory I/O region \r\n");
		goto free_rvdev;
	}
	openamp_dbg("shm_io virt:0x%08x physmap:0x%08x\r\n", shm_io->virt, *(shm_io->physmap));

	shm_buf = metal_io_phys_to_virt(shm_io, rvdev_shm->pa);
	if(!shm_buf) {
		openamp_err("Failed to get shared memory virt address\r\n");
		goto free_rvdev;
	}
	openamp_dbg("rvdev_shm virt address:0x%p\r\n", shm_buf);

	/* Create vdev. It will no return until master ready */
	openamp_dbg("Create virtio device\r\n");
	vdev = remoteproc_create_virtio(rproc, vdev_id, role, rst_cb);

	/* Only RPMsg virtio master needs to initialize the shm_pool */
	if (role == VIRTIO_DEV_MASTER) {
		shm_pool = metal_allocate_memory(sizeof(struct rpmsg_virtio_shm_pool));
		if (!shm_pool) {
			openamp_err("Failed to allocate memory for shm_pool\n");
			goto remove_virtio;
		}
		rpmsg_virtio_init_shm_pool(shm_pool, shm_buf, rvdev_shm->len);
	}
	/*
	 * NOTE:
	 *   For RPMsg virtio slave, the shm_pool argument is NULL.
	 */
	openamp_info("Wait connected to remote master\r\n");
	ret = rpmsg_init_vdev(&rvdev->inst, vdev, ns_bind_cb, shm_io, shm_pool);
	if(ret != 0) {
		openamp_err("rpmsg_init_vdev failed\n");
		goto free_shm_pool;
	}
	openamp_info("Connecte to remote master Successed\r\n");
	rvdev->id = vdev_id;
	sunxi_vdevs_add_vdev(rvdev);

	openamp_dbg("notifyid list:\r\n");
	openamp_dbg("vring0 notifyid=%d\r\n", vdev->vrings_info[0].notifyid);
	openamp_dbg("vring1 notifyid=%d\r\n", vdev->vrings_info[1].notifyid);

	return rpmsg_virtio_get_rpmsg_device(&rvdev->inst);

free_shm_pool:
	if(shm_pool != NULL)
		metal_free_memory(shm_pool);
remove_virtio:
	remoteproc_remove_virtio(rproc, vdev);
free_rvdev:
	metal_free_memory(rvdev);
out:
	return NULL;

}
void openamp_sunxi_release_rpmsg_vdev(struct remoteproc *rproc,
		struct rpmsg_device *rpmsg_dev)
{
	struct rpmsg_virtio_device *inst = metal_container_of(rpmsg_dev,
			struct rpmsg_virtio_device, rdev);
	struct openamp_sunxi_rvdev *rvdev = metal_container_of(inst,
			struct openamp_sunxi_rvdev, inst);
	struct rpmsg_virtio_shm_pool *shm_pool = inst->shpool;

	sunxi_vdevs_del_vdev(rvdev);

	rpmsg_deinit_vdev(inst);
	if (shm_pool) {
		metal_free_memory(shm_pool);
	}
	remoteproc_remove_virtio(rproc, inst->vdev);
	metal_free_memory(rvdev);

}
struct rpmsg_device *openamp_sunxi_get_rpmsg_vdev(int id)
{
	struct openamp_sunxi_rvdev *rvdev;
	struct rpmsg_device *rdev = NULL;
	struct metal_list *node;

	hal_mutex_lock(_sunxi_rvdevs_mutex);
	metal_list_for_each(&_sunxi_rvdevs, node) {
		rvdev = metal_container_of(node,
				struct openamp_sunxi_rvdev, node);

		if (rvdev->id == id) {
			rdev = rpmsg_virtio_get_rpmsg_device(&rvdev->inst);
			break;
		}
	}
	hal_mutex_unlock(_sunxi_rvdevs_mutex);
	return rdev;
}




