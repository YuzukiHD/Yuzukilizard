#ifndef __CSP_DMAC_PARA_H__
#define __CSP_DMAC_PARA_H__

#include <typedef.h>
#include <kconfig.h>
#if defined(CONFIG_SOC_SUN8IW19) || defined(CONFIG_SOC_SUN20IW1) || defined(CONFIG_SOC_SUN20IW3)

typedef enum _csp_dma_status {
	CSP_DMA_INVALID_PARAMETER = -2,
	CSP_DMA_ERROR = -1,
	CSP_DMA_COMPLETE,
	CSP_DMA_IN_PROGRESS,
	CSP_DMA_PAUSED,
} csp_dma_status;

typedef enum _csp_dma_transfer_direction {
	CSP_DMA_MEM_TO_MEM = 0,
	CSP_DMA_MEM_TO_DEV = 1,
	CSP_DMA_DEV_TO_MEM = 2,
	CSP_DMA_DEV_TO_DEV = 3,
	CSP_DMA_TRANS_NONE,
} csp_dma_transfer_direction;

typedef enum _csp_dma_slave_buswidth {
	CSP_DMA_SLAVE_BUSWIDTH_UNDEFINED    = 0,
	CSP_DMA_SLAVE_BUSWIDTH_1_BYTE       = 1,
	CSP_DMA_SLAVE_BUSWIDTH_2_BYTES      = 2,
	CSP_DMA_SLAVE_BUSWIDTH_3_BYTES      = 3,
	CSP_DMA_SLAVE_BUSWIDTH_4_BYTES      = 4,
	CSP_DMA_SLAVE_BUSWIDTH_8_BYTES      = 8,
	CSP_DMA_SLAVE_BUSWIDTH_16_BYTES     = 16,
	CSP_DMA_SLAVE_BUSWIDTH_32_BYTES     = 32,
	CSP_DMA_SLAVE_BUSWIDTH_64_BYTES     = 64,
} csp_dma_slave_buswidth;

typedef enum _csp_dma_slave_burst {
	CSP_DMA_SLAVE_BURST_1   = 1,
	CSP_DMA_SLAVE_BURST_4   = 4,
	CSP_DMA_SLAVE_BURST_8   = 8,
	CSP_DMA_SLAVE_BURST_16  = 16,
}csp_dma_slave_burst;

typedef struct _csp_dma_slave_config {
	csp_dma_transfer_direction direction;
	unsigned long src_addr;
	unsigned long dst_addr;
	csp_dma_slave_buswidth src_addr_width;
	csp_dma_slave_buswidth dst_addr_width;
	uint32_t src_maxburst;
	uint32_t dst_maxburst;
	uint32_t slave_id;
	unsigned long cyclic;
	unsigned long total_bytes;
	unsigned long period_bytes;
} csp_dma_slave_config;

typedef struct _csp_dma_chan {
	uint8_t used:1;
	uint8_t chan_count:4;
	uint8_t	cyclic:1;
	csp_dma_slave_config  cfg;
	uint32_t periods_pos;
	uint32_t buf_len;
	void /*struct sunxi_dma_lli*/ *desc;
	uint32_t irq_type;
	void *callback;
	void *callback_param;
	/* volatile kspinlock_t lock; */
	volatile int lock;
} csp_dma_chan;

#define CSP_DMA_HDLER_TYPE_CNT                      2
#define CSP_DMA_TRANSFER_PKG_HALF                   0
#define CSP_DMA_TRANSFER_PKG_END                    1
#define CSP_DMA_TRANSFER_QUENE_END                  2

#define CSP_DMAC_CFG_CONTINUOUS_ENABLE              (0x01)
#define CSP_DMAC_CFG_CONTINUOUS_DISABLE             (0x00)

#define CSP_DMAC_CFG_WAIT_1_DMA_CLOCK               (0x00)
#define CSP_DMAC_CFG_WAIT_2_DMA_CLOCK               (0x01)
#define CSP_DMAC_CFG_WAIT_3_DMA_CLOCK               (0x02)
#define CSP_DMAC_CFG_WAIT_4_DMA_CLOCK               (0x03)
#define CSP_DMAC_CFG_WAIT_5_DMA_CLOCK               (0x04)
#define CSP_DMAC_CFG_WAIT_6_DMA_CLOCK               (0x05)
#define CSP_DMAC_CFG_WAIT_7_DMA_CLOCK               (0x06)
#define CSP_DMAC_CFG_WAIT_8_DMA_CLOCK               (0x07)

#define CSP_DMAC_CFG_ADDR_TYPE_LINEAR               (0x00)
#define CSP_DMAC_CFG_ADDR_TYPE_IO                   (0x01)

#define dma_slave_id(d, s)  (((d)<<16) | (s))

/*define DMA device identity depending on spec*/
#define DRQSRC_SRAM_RXTX            0
#define DRQSRC_SDRAM_RXTX           1
#define DRQSRC_DAUDIO_0_RXTX        3
#define DRQSRC_DAUDIO_1_RXTX        4
#define DRQSRC_GPADC_RX             12
#define DRQSRC_UART0_RXTX           14
#define DRQSRC_UART1_RXTX           15
#define DRQSRC_UART2_RXTX           16
#define DRQSRC_UART3_RXTX           17
#define DRQSRC_SPI0_RXTX            22
#define DRQSRC_SPI1_RXTX            23
#define DRQSRC_OTG_EP1_RXTX         30
#define DRQSRC_OTG_EP2_RXTX         31
#define DRQSRC_OTG_EP3_RXTX         32
#define DRQSRC_OTG_EP4_RXTX         33
#define DRQSRC_OTG_EP5_RXTX         34
#define DRQSRC_TWI0_RXTX            43
#define DRQSRC_TWI1_RXTX            44
#define DRQSRC_TWI2_RXTX            45
#define DRQSRC_TWI3_RXTX            46

#if defined CONFIG_SOC_SUN20IW1 
#define DRQSRC_SPDIF_RXTX           2
#define DRQSRC_DAUDIO_2_RXTX        5
#define DRQSRC_AUDIO_CODEC_RXTX     7
#define DRQSRC_DMIC_RX              8
#define DRQSRC_TPADC_RX             13 
#define DRQDST_IR_TX                13 
#define DRQSRC_UART4_RXTX           18
#define DRQSRC_UART5_RXTX           19
#define DRQSRC_LEDC_TX              42
#endif

#if defined CONFIG_SOC_SUN8IW19 
#define DRQSRC_AUDIO_CODEC_RXTX     6
#define DRQSRC_SPI2_RXTX            24
#define DRQSRC_R_TWI0_RXTX          48
#endif

#endif//defined CONFIG_SOC_SUN8IW19 || defined CONFIG_SOC_SUN20IW1

#endif  //__CSP_DMAC_PARA_H__
