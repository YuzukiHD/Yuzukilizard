#ifndef OPENAMP_SUN20IW3_RSC_TAB_H_
#define OPENAMP_SUN20IW3_RSC_TAB_H_

#include <stddef.h>
#include <openamp/open_amp.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NUM_RESOURCE_ENTRIES	2
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
	/* rpmsg vdev entry */
	struct fw_rsc_vdev vdev;
	struct fw_rsc_vdev_vring vring0;
	struct fw_rsc_vdev_vring vring1;
}__attribute__((packed));

void resource_table_init(int rsc_id, void **table_ptr, int *length);
struct fw_rsc_carveout *resource_table_get_rvdev_shm_entry(void *rsc_table, int rvdev_id);
unsigned int resource_table_vdev_notifyid(void);
struct fw_rsc_vdev *resource_table_get_vdev(int index);


#ifdef __cplusplus
}
#endif

#endif /* OPENAMP_SUN20IW3_RSC_TAB_H_ */
