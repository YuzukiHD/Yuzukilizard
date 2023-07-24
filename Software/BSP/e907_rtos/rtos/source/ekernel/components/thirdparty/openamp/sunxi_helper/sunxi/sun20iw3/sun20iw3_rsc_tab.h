#ifndef OPENAMP_SUN20IW3_RSC_TAB_H_
#define OPENAMP_SUN20IW3_RSC_TAB_H_

#include <stddef.h>
#include <openamp/open_amp.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_AMP_TRACE_SUPPORT
#define AMP_TRACE_RESOURCE_ENTRIES			(1)
#else
#define AMP_TRACE_RESOURCE_ENTRIES			(0)
#endif

#ifdef CONFIG_AMP_SHARE_IRQ
#define AMP_SHARE_IRQ						(1)
#else
#define AMP_SHARE_IRQ						(0)
#endif

#define NUM_RESOURCE_ENTRIES	(2 + AMP_TRACE_RESOURCE_ENTRIES + AMP_SHARE_IRQ)
#define NUM_VRINGS				2

#define VRING0_ID				1
#define VRING1_ID				2

struct sunxi_resource_table {
	uint32_t version;
	uint32_t num;
	uint32_t reserved[2];
	uint32_t offset[NUM_RESOURCE_ENTRIES];

	/* shared memory entry for rpmsg virtIO device */
	struct fw_rsc_carveout rvdev_shm;
#ifdef CONFIG_AMP_SHARE_IRQ
	struct fw_rsc_carveout share_irq;
#endif
#ifdef CONFIG_AMP_TRACE_SUPPORT
	struct fw_rsc_trace trace;
#endif
	/* rpmsg vdev entry */
	struct fw_rsc_vdev vdev;
	struct fw_rsc_vdev_vring vring0;
	struct fw_rsc_vdev_vring vring1;
}__attribute__((packed));

#ifdef __cplusplus
}
#endif

#endif /* OPENAMP_SUN20IW3_RSC_TAB_H_ */
