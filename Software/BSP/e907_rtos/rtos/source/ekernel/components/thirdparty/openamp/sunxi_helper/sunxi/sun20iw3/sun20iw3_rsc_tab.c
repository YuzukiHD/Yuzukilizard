#include <openamp/sunxi_helper/openamp.h>
#include <openamp/sunxi_helper/shmem_ops.h>
#ifdef CONFIG_AMP_SHARE_IRQ
#include <openamp/openamp_share_irq.h>
#endif
#include "sun20iw3_rsc_tab.h"

#define RPMSG_IPU_C0_FEATURES			1
//#define VRING_TX_ADDRESS				0x48440000
//#define VRING_RX_ADDRESS 				0x48460000

#define VRING_TX_ADDRESS				FW_RSC_U32_ADDR_ANY
#define VRING_RX_ADDRESS 				FW_RSC_U32_ADDR_ANY

#define VRING_NUM_BUFFS					128
#define VRING_ALIGN						0x1000		/* 4K */

//#define SHMEM_PA			0x48400000
//#define SHMEM_DA			0x48400000

#define SHMEM_PA			FW_RSC_U32_ADDR_ANY
#define SHMEM_DA			FW_RSC_U32_ADDR_ANY
#define SHMEM_SIZE			0x40000			/* 256K */

#define IRQ_TAB_PA			FW_RSC_U32_ADDR_ANY
#define IRQ_TAB_DA			FW_RSC_U32_ADDR_ANY
#define IRQ_TAB_SIZE		0x1000			/* 4K */

#define __section_t(s)			__attribute((__section__(#s)))
#define __resource				__section_t(.resource_table)

#ifdef CONFIG_AMP_TRACE_SUPPORT
extern char amp_log_buffer[];
#endif

/**
 *	Note: vdev da and vring da need to same with linux dts reserved-memory
 *	otherwise linux will alloc new memory
 * */
static struct sunxi_resource_table __resource resource_table = {
	.version = 1,
	.num = NUM_RESOURCE_ENTRIES,
	.reserved = {0, 0},
	.offset = {
		offsetof(struct sunxi_resource_table, rvdev_shm),
#ifdef CONFIG_AMP_SHARE_IRQ
		offsetof(struct sunxi_resource_table, share_irq),
#endif
#ifdef CONFIG_AMP_TRACE_SUPPORT
		offsetof(struct sunxi_resource_table, trace),
#endif
		offsetof(struct sunxi_resource_table, vdev),
	},
	.rvdev_shm = {
		.type = RSC_CARVEOUT,
		.da = SHMEM_DA,
		.pa = SHMEM_PA,
		.len = SHMEM_SIZE,
		.flags = MEM_CACHEABLE,
		.reserved = 0,
		.name = "vdev0buffer",
	},
#ifdef CONFIG_AMP_SHARE_IRQ
	.share_irq = {
		.type = RSC_CARVEOUT,
		.da = IRQ_TAB_DA,
		.pa = IRQ_TAB_PA,
		.len = IRQ_TAB_SIZE,
		.flags = MEM_NONCACHEABLE,
		.reserved = 0,	/* must be zero */
		.name = "share_irq_table",
	},
#endif
#ifdef CONFIG_AMP_TRACE_SUPPORT
	.trace = {
		.type = RSC_TRACE,
		.da = (uint32_t)amp_log_buffer,
		.len = CONFIG_AMP_TRACE_BUF_SIZE,
		.reserved = 0,
		.name = "e907_log",
	},
#endif

	.vdev = {
		.type = RSC_VDEV,
		.id = VIRTIO_ID_RPMSG,
		.notifyid = 0,
		.dfeatures = RPMSG_IPU_C0_FEATURES,
		.gfeatures = 0,
		.config_len = 0,
		.status = 0,
		.num_of_vrings = NUM_VRINGS,
		.reserved = {0, 0},
	},

	.vring0 = {
		.da = VRING_TX_ADDRESS,
		.align = VRING_ALIGN,
		.num = VRING_NUM_BUFFS,
		.notifyid = VRING0_ID,
		.reserved = 0,
	},

	.vring1 = {
		.da = VRING_RX_ADDRESS,
		.align = VRING_ALIGN,
		.num = VRING_NUM_BUFFS,
		.notifyid = VRING1_ID,
		.reserved = 0,
	},

};

void resource_table_init(int rsc_id, void **table_ptr, int *length)
{
	*length = sizeof(resource_table);
	*table_ptr = (void *)&resource_table;
}

struct fw_rsc_carveout *resource_table_get_rvdev_shm_entry(void *rsc_table, int rvdev_id)
{
	switch (rvdev_id) {
	case RPROC_CPU_ID:
		return &resource_table.rvdev_shm;
#ifdef CONFIG_AMP_SHARE_IRQ
	case RPROC_IRQ_TAB:
		return &resource_table.share_irq;
#endif
	default:
		openamp_err("Invalid rsc_id\n");
		return NULL;
	}
}

unsigned int resource_table_vdev_notifyid(void)
{
	return resource_table.vdev.notifyid;
}

struct fw_rsc_vdev *resource_table_get_vdev(int index)
{
	return &resource_table.vdev;
}

