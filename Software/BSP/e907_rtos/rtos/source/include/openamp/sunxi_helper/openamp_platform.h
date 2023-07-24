#ifndef __OPENAMP_SUNXI_PLARFORM_H_
#define __OPENAMP_SUNXI_PLARFORM_H_

#include <openamp/open_amp.h>
#include <openamp/sunxi_helper/rsc_table.h>

#ifdef __cplusplus
extern "C" {
#endif

struct remoteproc *openamp_sunxi_create_rproc(int proc_id, int rsc_id);
void openamp_sunxi_release_rproc(struct remoteproc *rproc);
struct remoteproc *openamp_sunxi_get_rproc(int id);

struct rpmsg_device *openamp_sunxi_create_rpmsg_vdev(
		struct remoteproc *rproc,
		int vdev_id, int role,
		void (*rst_cb)(struct virtio_device *vdev),
		rpmsg_ns_bind_cb ns_bind_cb);
void openamp_sunxi_release_rpmsg_vdev(struct remoteproc *rproc,
		struct rpmsg_device *rpmsg_dev);
struct rpmsg_device *openamp_sunxi_get_rpmsg_vdev(int id);

#ifdef __cplusplus
}
#endif

#endif /* __OPENAMP_SUNXI_PLARFORM_H_ */
