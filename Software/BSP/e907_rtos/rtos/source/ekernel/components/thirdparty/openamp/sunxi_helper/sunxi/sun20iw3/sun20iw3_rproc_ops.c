#include <openamp/open_amp.h>
#include <openamp/sunxi_helper/openamp_log.h>
#include <openamp/sunxi_helper/shmem_ops.h>
#include <openamp/sunxi_helper/msgbox_ipi.h>
#include <openamp/sunxi_helper/mem_map.h>
#include <openamp/sunxi_helper/sunxi_rproc.h>

#include <hal_msgbox.h>
#include <backtrace.h>

#include "sun20iw3_rsc_tab.h"

#define MSGBOX_RISCV 1
#define MSGBOX_ARM   0


struct rproc_mem_list_entry {
	struct remoteproc_mem mem;
	struct metal_list node;
};

struct rproc_io_list_entry {
	struct metal_io_region io;
	struct metal_list node;
};

struct rproc_private_data {
	struct msgbox_ipi *vring_ipi;

	/* rproc may have multiple io/mem instances */
	struct metal_list mem_list;
	struct metal_list io_list;
};

static struct rproc_private_data private_data;

static struct msgbox_ipi_info vring_ipi_info = {
	.name = "vring-ipi",
	.remote_id = MSGBOX_ARM,
	.read_ch = CONFIG_MBOX_CHANNEL,
	.write_ch = CONFIG_MBOX_CHANNEL,
	.queue_size = CONFIG_MBOX_QUEUE_LENGTH,
};

static void vring_ipi_recv_call_back(void *priv, uint32_t data)
{
	struct remoteproc *rproc = (struct remoteproc *)priv;

	openamp_dbg("recv kick notify=%d.\r\n", data);
	/* tell rproc recv a message */
	remoteproc_get_notification(rproc, data);
}

struct remoteproc* rv_to_a7_rproc_init(struct remoteproc *rproc,
				   struct remoteproc_ops *ops, void *arg)
{
	struct rproc_private_data *priv = (struct rproc_private_data *)arg;
	struct msgbox_ipi *vring_ipi;

	if(!rproc || !ops || !arg) {
		openamp_err("Invalid argumemts\r\n");
		goto out;
	}

	vring_ipi = msgbox_ipi_create(&vring_ipi_info, vring_ipi_recv_call_back, (void *)rproc);
	if(!vring_ipi) {
		openamp_err("Failed to create ving0_ipi\r\n");
		goto out;
	}

	priv->vring_ipi = vring_ipi;
	openamp_dbg("Create ving0_ipi(addr:0x%p)\r\n", priv->vring_ipi);

	metal_list_init(&priv->mem_list);
	metal_list_init(&priv->io_list);

	rproc->priv = priv;
	rproc->ops = ops;

	openamp_info("Init proc ok.\r\n");

	return rproc;

free_vring0_ipi:
	msgbox_ipi_release(vring_ipi);
out:
	return NULL;
}
void rv_to_a7_rproc_remove(struct remoteproc *rproc)
{
	struct rproc_private_data *priv = (struct rproc_private_data *)(rproc->priv);
	struct metal_list *node;
	struct rproc_mem_list_entry *mem_entry;
	struct rproc_io_list_entry *io_entry;
	int ret;

	openamp_info("remove rproc...\r\n");

	metal_list_for_each(&priv->mem_list, node) {
		mem_entry = metal_container_of(node, struct rproc_mem_list_entry,
					    node);
		metal_free_memory(mem_entry);
	}
	metal_list_for_each(&priv->io_list, node) {
		io_entry = metal_container_of(node, struct rproc_io_list_entry,
					    node);
		metal_free_memory(io_entry);
	}

	openamp_dbg("release %s(addr:0x%p)...\r\n", priv->vring_ipi->info.name,
					priv->vring_ipi);

	msgbox_ipi_release(priv->vring_ipi);
}

void *rv_to_a7_rproc_mmap(struct remoteproc *rproc,
		      metal_phys_addr_t *pa, metal_phys_addr_t *da,
		      size_t size, unsigned int attribute,
		      struct metal_io_region **io)
{
	struct rproc_private_data *priv = (struct rproc_private_data *)(rproc->priv);
	metal_phys_addr_t	lpa;
	metal_phys_addr_t	lda;
	metal_phys_addr_t	lva;
	struct rproc_mem_list_entry *mem_entry;
	struct rproc_io_list_entry *io_entry;

	int ret;

	lpa = *pa;
	lda = *da;

	/* *
	 * pa is physical address on the rv side
	 * */
	if (lpa == METAL_BAD_PHYS && lda == METAL_BAD_PHYS) {
		openamp_err("Invalid phy addr and dev addr\r\n");
		goto out;
	} else if (lpa == METAL_BAD_PHYS) {
		ret = mem_va_to_pa(lda, &lpa, NULL);
		if(ret != 0) {
			openamp_err("Failed to translate device address(0x%08lx) to va\r\n", lda);
			goto out;
		}
	}else if (lda == METAL_BAD_PHYS) {
		ret = mem_pa_to_va(lpa, &lda, NULL);
		if (ret != 0) {
			openamp_err("Failed to translate physical address(0x%08lx) to va\r\n", lpa);
			goto out;
		}
	}

	mem_entry = metal_allocate_memory(sizeof(*mem_entry));
	if (!mem_entry) {
		openamp_err("Failed to allocate memory for remoteproc mem\r\n");
		goto out;
	}

	io_entry = metal_allocate_memory(sizeof(*io_entry));
	if (!io_entry) {
		openamp_err("Failed to allocate memory for remoteproc io\r\n");
		goto free_mem;
	}

	remoteproc_init_mem(&mem_entry->mem, NULL, lpa, lda, size, &io_entry->io);

	ret = mem_pa_to_va(lpa, &lva, NULL);
	if (ret < 0) {
		openamp_err("Failed to translate pa 0x%lx to va\n", lpa);
		goto free_io;
	}

	metal_io_init(&io_entry->io, (void *)lva, &mem_entry->mem.pa, size, (uint32_t)(-1),
					attribute, &shmem_sunxi_io_ops);

	remoteproc_add_mem(rproc, &mem_entry->mem);

	*pa = lpa;
	*da = lda;
	if(io)
		*io = &(io_entry->io);

	metal_list_add_tail(&priv->mem_list, &mem_entry->node);
	metal_list_add_tail(&priv->io_list, &io_entry->node);

	openamp_info("map pa(0x%08x) to va(0x%08x)\r\n", *pa, lva);

	return metal_io_phys_to_virt(&(io_entry->io), mem_entry->mem.pa);
free_io:
	metal_free_memory(io_entry);
free_mem:
	metal_free_memory(mem_entry);
out:
	return NULL;
}
int rv_to_a7_rproc_notify(struct remoteproc *rproc, uint32_t id)
{
	int ret;
	struct rproc_private_data *priv = rproc->priv;

	ret = msgbox_ipi_notify(priv->vring_ipi, id);
	if (ret < 0) {
		openamp_err("Vring0 notify failed\n");
		goto out;
	}

	return 0;
out:
	return ret;
}


static struct remoteproc_ops sun20iw3_rproc_ops = {
	.init = rv_to_a7_rproc_init,
	.remove = rv_to_a7_rproc_remove,
	.mmap = rv_to_a7_rproc_mmap,
	.handle_rsc = NULL,
	.config = NULL,
	.start = NULL,
	.stop = NULL,
	.shutdown = NULL,
	.notify = rv_to_a7_rproc_notify,
	.get_mem = NULL,
};


struct rproc_global_impls sunxi_rproc_impls[] = {
	{ .ops = &sun20iw3_rproc_ops, .priv = &private_data },
};

const size_t sunxi_rproc_impls_size = GET_RPROC_GLOBAL_IMPLS_ITEMS(sunxi_rproc_impls);


