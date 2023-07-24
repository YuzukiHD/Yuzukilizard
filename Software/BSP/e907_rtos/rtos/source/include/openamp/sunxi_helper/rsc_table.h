#ifndef OPENAMP_RSC_TABLE_H_
#define OPENAMP_RSC_TABLE_H_

#define RPROC_CPU_ID			(0)
#define RPROC_IRQ_TAB			(1)

/* for resource table function */
void resource_table_init(int rsc_id, void **table_ptr, int *length);
struct fw_rsc_carveout *resource_table_get_rvdev_shm_entry(void *rsc_table, int rvdev_id);
unsigned int resource_table_vdev_notifyid(void);
struct fw_rsc_vdev *resource_table_get_vdev(int index);

#endif /* OPENAMP_RSC_TABLE_H_ */
