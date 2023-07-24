#include <stdio.h>
#include <inttypes.h>
#include <hal_mem.h>
#include <hal_gpio.h>
#include <openamp/sunxi_helper/openamp.h>
#include <openamp/sunxi_helper/openamp_log.h>
#include <openamp/sunxi_helper/mem_map.h>
#include <openamp/openamp_share_irq.h>

#define READY_TAG		(('R' << 24) | ('E' << 16) | ('D' << 8) | 'Y')
#define USED_TAG		(('U' << 24) | ('S' << 16) | ('E' << 8) | 'D')

/*
 * Table Format：
 *     The first 16 byte is reserved for _table_head
 *     The last CPU_BANK_NUM word is resource for gpio mask
 *
 *     ----------------
 *     | _table_head  |
 *     ----------------
 *     | _table_entry |
 *     ----------------
 *     |     ...      |
 *     ----------------
 *     |  GPIOA Mask  |
 *     ----------------
 *     |     ...      |
 *     ---------------
 */
struct _table_head {
	uint32_t tag;
	uint32_t len;
	uint32_t banks_off;
	uint32_t banks_num;
} __attribute__((packed));

struct _table_entry {
	uint32_t status;
	uint32_t type;
	uint32_t flags;
	uint32_t arch_irq;
} __attribute__((packed));

static uint32_t *share_irq_table = NULL;
static uint32_t *share_gpio_banks = NULL;
static uint32_t *share_gpio_irqs = NULL;
static uint32_t share_bank_num;
static int init_stage;

/* [0] = addr, [1] = len */
static uint32_t early_buf_info[2] __attribute__((__section__(".share_irq_table")));

static void init_share_gpio_irqs_default(uint32_t *tab, int nitems)
{
#ifdef SUNXI_IRQ_GPIOA
	if (nitems > ('A' - 'A'))
		tab['A' - 'A'] = SUNXI_IRQ_GPIOA;
#endif
#ifdef SUNXI_IRQ_GPIOB
	if (nitems > ('B' - 'A'))
		tab['B' - 'A'] = SUNXI_IRQ_GPIOB;
#endif
#ifdef SUNXI_IRQ_GPIOC
	if (nitems > ('C' - 'A'))
		tab['C' - 'A'] = SUNXI_IRQ_GPIOC;
#endif
#ifdef SUNXI_IRQ_GPIOD
	if (nitems > ('D' - 'A'))
		tab['D' - 'A'] = SUNXI_IRQ_GPIOD;
#endif
#ifdef SUNXI_IRQ_GPIOE
	if (nitems > ('E' - 'A'))
		tab['E' - 'A'] = SUNXI_IRQ_GPIOE;
#endif
#ifdef SUNXI_IRQ_GPIOF
	if (nitems > ('F' - 'A'))
		tab['F' - 'A'] = SUNXI_IRQ_GPIOF;
#endif
#ifdef SUNXI_IRQ_GPIOG
	if (nitems > ('G' - 'A'))
		tab['G' - 'A'] = SUNXI_IRQ_GPIOG;
#endif
#ifdef SUNXI_IRQ_GPIOH
	if (nitems > ('H' - 'A'))
		tab['H' - 'A'] = SUNXI_IRQ_GPIOH;
#endif
#ifdef SUNXI_IRQ_GPIOI
	if (nitems > ('I' - 'A'))
		tab['I' - 'A'] = SUNXI_IRQ_GPIOI;
#endif
#ifdef SUNXI_IRQ_GPIOJ
	if (nitems > ('J' - 'A'))
		tab['J' - 'A'] = SUNXI_IRQ_GPIOJ;
#endif
#ifdef SUNXI_IRQ_GPIOK
	if (nitems > ('K' - 'A'))
		tab['K' - 'A'] = SUNXI_IRQ_GPIOK;
#endif
#ifdef SUNXI_IRQ_GPIOL
	if (nitems > ('L' - 'A'))
		tab['L' - 'A'] = SUNXI_IRQ_GPIOL;
#endif
#ifdef SUNXI_IRQ_GPIOM
	if (nitems > ('M' - 'A'))
		tab['M' - 'A'] = SUNXI_IRQ_GPIOM;
#endif
#ifdef SUNXI_IRQ_GPION
	if (nitems > ('N' - 'A'))
		tab['N' - 'A'] = SUNXI_IRQ_GPION;
#endif
#ifdef SUNXI_IRQ_GPIOO
	if (nitems > ('O' - 'A'))
		tab['O' - 'A'] = SUNXI_IRQ_GPIOO;
#endif
#ifdef SUNXI_IRQ_GPIOP
	if (nitems > ('P' - 'A'))
		tab['P' - 'A'] = SUNXI_IRQ_GPIOP;
#endif

	return;
}

int openamp_share_irq_early_init(void)
{
	struct fw_rsc_carveout *carveout;
	void *share_buf;
	uint32_t buf, buf_len;
	int ret;

	init_stage = 0;

	carveout = resource_table_get_rvdev_shm_entry(NULL, RPROC_IRQ_TAB);
	if (!carveout) {
		//openamp_err("Can't Find shmem,id=%d\n", RPROC_IRQ_TAB);
		init_stage = 1;
		return -EINVAL;
	}

	if (early_buf_info[0] != 0) {
		buf = early_buf_info[0];
		buf_len = early_buf_info[1];
		goto _init;
	}

	if (carveout->pa == 0 || carveout->pa == FW_RSC_U32_ADDR_ANY) {
		//openamp_err("share_irq_table not ready.\n");
		init_stage = 2;
		return -ENODEV;
	}
	buf = carveout->pa;
	buf_len = carveout->len;

_init:
	ret = mem_pa_to_va(buf, (metal_phys_addr_t *)(&share_buf), NULL);
	if (ret < 0) {
		//openamp_err("openamp_share_irq_early_init Failed.\n");
		init_stage = 3;
		return -EFAULT;
	}

	openamp_share_irq_init(share_buf, buf_len);

	return 0;
}
int openamp_share_irq_init(void *share_buf, int buf_len)
{
	int len, i;
	struct _table_entry *ptable;
	struct _table_head *phead;

	share_irq_table = share_buf;

	phead = share_buf;
	ptable = share_buf + sizeof(*phead);

	if (phead->tag == USED_TAG) {
		//openamp_err("IRQ Table already init\n");
		init_stage = 4;
		return 0;
	}
	if (phead->tag != READY_TAG) {
		//openamp_err("IRQ Table no ready, val:0x%x.\n", share_irq_table[0]);
		init_stage = 5;
		return -EFAULT;
	}

	if (phead->len != buf_len) {
		//openamp_err("Different len, exception 0x%x, actually 0x%x.\n", phead->len, buf_len);
		init_stage = 6;
		phead->len = buf_len;
	}

	share_gpio_banks = (uint32_t *)((char *)share_irq_table + phead->banks_off);
	share_bank_num = phead->banks_num;

	/* record gpio hwirq */
	share_gpio_irqs = hal_malloc(sizeof(uint32_t) * share_bank_num);
	if (!share_gpio_irqs) {
		//openamp_err("Malloc share_gpio_irqs Failed.\n");
		share_irq_table = NULL;
		share_gpio_banks = NULL;
		share_bank_num = 0;
		init_stage = 7;
		return -ENOMEM;
	}

	len = (phead->banks_off - sizeof(*phead)) / sizeof(*ptable);
	memset(share_gpio_irqs, 0, sizeof(uint32_t) * share_bank_num);
	init_share_gpio_irqs_default(share_gpio_irqs, share_bank_num);
	for (i = 0; i < len; i++) {
		uint32_t type = ptable[i].type;
		if (type >= 1 && type <= share_bank_num && ptable[i].arch_irq != 0)
			share_gpio_irqs[type - 1] = ptable[i].arch_irq;
	}

	phead->tag = USED_TAG;

	return 0;
}

uint32_t sunxi_get_banks_mask(int hwirq)
{
	int banks, i;

	if (!share_gpio_banks)
		return 0;

	banks = -1;
	for (i = 0; i < share_bank_num; i++) {
		if (share_gpio_irqs[i] == hwirq) {
			banks = i;
			break;
		}
	}
	if (banks >= 0)
		return share_gpio_banks[banks];
	else
		return 0;
}

bool sunxi_is_arch_irq(int irq)
{
	if (!share_irq_table)
		return false;
	if (share_irq_table[irq])
		return true;
	return false;
}

static int cmd_dump_shirq(int argc, char ** argv)
{
	int i;

	if (init_stage != 0) {
		printf("Init IRQ Table Failed,ret=%d\r\n", init_stage);
		return 0;
	}

	if (!share_irq_table) {
		printf("Please Init Share Table.\n");
		return 0;
	}

	printf("GPIO Table:Addr=%p\n", share_gpio_banks);
	printf("\tAddr=%p\n", share_gpio_banks);
	printf("\tBankNum=%" PRId32 "\n", share_bank_num);

	printf("GPIO Info:\n");
	for (i = 0; i < share_bank_num; i++)
		printf("GPIO%c MASK:  0x%08" PRIx32 " IRQ:%" PRId32 "\n", 'A' + i, share_gpio_banks[i],
						share_gpio_irqs[i]);

    return 0;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_dump_shirq, dump_shirq, show share interrupt table);
