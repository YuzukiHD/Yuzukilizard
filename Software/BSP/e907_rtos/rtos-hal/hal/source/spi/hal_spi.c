/*
 * Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
 *
 * Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
 * the the People's Republic of China and other countries.
 * All Allwinner Technology Co.,Ltd. trademarks are used with permission.
 *
 * DISCLAIMER
 * THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
 * IF YOU NEED TO INTEGRATE THIRD PARTY¡¯S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR
 * MPEGLA, ETC.)
 * IN ALLWINNERS¡¯SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
 * ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
 * ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT
 * TO MATTERS
 * COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
 * YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY¡¯S TECHNOLOGY.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY ALLWINNER"AS IS" AND TO THE MAXIMUM EXTENT
 * PERMITTED BY LAW, ALLWINNER EXPRESSLY DISCLAIMS ALL WARRANTIES OF ANY KIND,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING WITHOUT LIMITATION REGARDING
 * THE TITLE, NON-INFRINGEMENT, ACCURACY, CONDITION, COMPLETENESS, PERFORMANCE
 * OR MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * IN NO EVENT SHALL ALLWINNER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS, OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <interrupt.h>
#include <sunxi_hal_spi.h>
#include <hal_cache.h>
#include <hal_mem.h>
#include <hal_osal.h>
#include <hal_log.h>
#ifdef CONFIG_OS_MELIS
#include <hal_cfg.h>
#include <script.h>
#endif

#include <sunxi_hal_common.h>

#define SPI_ALIGN(x, a) ALIGN_UP(x, a)

#ifndef CONFIG_DRIVERS_DMA
#define CONFIG_SUNXI_SPI_CPU_XFER_ONLY
#endif

#if (0)
#define SPI_INFO_LEVEL
#endif

#if (0)
#define SPI_DATA_LEVEL
#endif

#if (0)
#define SPI_DUMPREG_LEVEL
#endif


#ifdef SPI_INFO_LEVEL
#define SPI_INFO(fmt, arg...) printf("%s()%d " fmt, __func__, __LINE__, ##arg)
#else
#define SPI_INFO(fmt, arg...)                                                  \
    do {                                                                   \
    } while (0);
#endif

#define SPI_ERR(fmt, arg...) printf("%s()%d " fmt, __func__, __LINE__, ##arg)
#define SPI_INIT(fmt, arg...) printf("%s()%d " fmt, __func__, __LINE__, ##arg)

static sunxi_spi_t g_sunxi_spi[HAL_SPI_MASTER_MAX];
struct sunxi_spi_params_t g_sunxi_spi_params[] = {
#ifdef SPI0_PARAMS
    SPI0_PARAMS,
#endif
#ifdef SPI1_PARAMS
    SPI1_PARAMS,
#endif
#ifdef SPI2_PARAMS
    SPI2_PARAMS,
#endif
#ifdef SPI3_PARAMS
    SPI3_PARAMS,
#endif
};


struct sunxi_spi_params_t g_sunxi_spi_params_sip[] = {
#ifdef SIP_SPI0_PARAMS
    SIP_SPI0_PARAMS,
#endif
};

void spi_dump_reg(sunxi_spi_t *sspi, uint32_t offset, uint32_t len)
{
    uint32_t i;
    uint8_t buf[64], cnt = 0;

    for (i = 0; i < len; i = i + REG_INTERVAL)
    {
        if (i % HEXADECIMAL == 0)
            cnt += sprintf((char *)(buf + cnt), "0x%08lx: ",
                           (unsigned long)(sspi->base + offset + i));

        cnt += sprintf((char *)(buf + cnt), "%08lx ",
			(unsigned long)hal_readl(sspi->base + offset + i));

        if (i % HEXADECIMAL == REG_CL)
        {
            printf("%s\n", buf);
            cnt = 0;
        }
    }
}

/* config chip select */
static spi_master_status_t spi_set_cs(hal_spi_master_slave_port_t chipselect, sunxi_spi_t *sspi)
{
    spi_master_status_t ret;
    uint32_t reg_val = hal_readl(sspi->base + SPI_TC_REG);

    if (chipselect < 4)
    {
        reg_val &= ~SPI_TC_SS_MASK; /* SS-chip select, clear two bits */
        reg_val |= chipselect
                   << SPI_TC_SS_BIT_POS; /* set chip select */
        hal_writel(reg_val, sspi->base + SPI_TC_REG);
        ret = SPI_MASTER_OK;
    }
    else
    {
        SPI_ERR("[spi%d] Chip Select set fail! cs = %d\n", sspi->port,
                chipselect);
        ret = SPI_MASTER_INVALID_PARAMETER;
    }

    return ret;
}

/* config spi */
static void spi_config_tc(uint32_t config, sunxi_spi_t *sspi)
{
    uint32_t reg_val = hal_readl(sspi->base + SPI_TC_REG);

    /*1. POL */
    if (config & SPI_POL_ACTIVE_)
    {
        reg_val |= SPI_TC_POL;    /*default POL = 1 */
    }
    else
    {
        reg_val &= ~SPI_TC_POL;
    }

    /*2. PHA */
    if (config & SPI_PHA_ACTIVE_)
    {
        reg_val |= SPI_TC_PHA;    /*default PHA = 1 */
    }
    else
    {
        reg_val &= ~SPI_TC_PHA;
    }

    /*3. SSPOL,chip select signal polarity */
    if (config & SPI_CS_HIGH_ACTIVE_)
    {
        reg_val &= ~SPI_TC_SPOL;
    }
    else
    {
        reg_val |= SPI_TC_SPOL;    /*default SSPOL = 1,Low level effect */
    }

    /*4. LMTF--LSB/MSB transfer first select */
    if (config & SPI_LSB_FIRST_ACTIVE_)
    {
        reg_val |= SPI_TC_FBS;
    }
    else
    {
        reg_val &= ~SPI_TC_FBS;    /*default LMTF =0, MSB first */
    }

    /* set DDB,DHB,SMC,SSCTL */
    /*5. dummy burst type */
    if (config & SPI_DUMMY_ONE_ACTIVE_)
    {
        reg_val |= SPI_TC_DDB;
    }
    else
    {
        reg_val &= ~SPI_TC_DDB;    /*default DDB =0, ZERO */
    }

    /*6.discard hash burst-DHB */
    if (config & SPI_RECEIVE_ALL_ACTIVE_)
    {
        reg_val &= ~SPI_TC_DHB;
    }
    else
    {
        reg_val |= SPI_TC_DHB;    /*default DHB =1, discard unused burst */
    }

    /*7. set SMC = 1 , SSCTL = 0 ,TPE = 1 */
    reg_val &= ~SPI_TC_SSCTL;

    hal_writel(reg_val, sspi->base + SPI_TC_REG);
}

/* delay internal read sample point*/
static void spi_sample_delay(uint32_t sdm, uint32_t sdc, sunxi_spi_t *sspi)
{
    uint32_t reg_val = hal_readl(sspi->base + SPI_TC_REG);
    uint32_t org_val = reg_val;

    if (sdm)
    {
        reg_val |= SPI_TC_SDM;
    }
    else
    {
        reg_val &= ~SPI_TC_SDM;
    }

    if (sdc)
    {
        reg_val |= SPI_TC_SDC;
    }
    else
    {
        reg_val &= ~SPI_TC_SDC;
    }

    if (reg_val != org_val)
    {
        hal_writel(reg_val, sspi->base + SPI_TC_REG);
    }
}

/* start spi transfer */
static void spi_start_xfer(sunxi_spi_t *sspi)
{
    uint32_t reg_val = hal_readl(sspi->base + SPI_TC_REG);

    reg_val |= SPI_TC_XCH;
    hal_writel(reg_val, sspi->base + SPI_TC_REG);
}

/* enable spi bus */
static void spi_enable_bus(sunxi_spi_t *sspi)
{
    uint32_t reg_val = hal_readl(sspi->base + SPI_GC_REG);

    reg_val |= SPI_GC_EN;
    hal_writel(reg_val, sspi->base + SPI_GC_REG);
}

/* disbale spi bus */
static void spi_disable_bus(sunxi_spi_t *sspi)
{
    uint32_t reg_val = hal_readl(sspi->base + SPI_GC_REG);

    reg_val &= ~SPI_GC_EN;
    hal_writel(reg_val, sspi->base + SPI_GC_REG);
}

/* set master mode */
static void spi_set_master(sunxi_spi_t *sspi)
{
    uint32_t reg_val = hal_readl(sspi->base + SPI_GC_REG);

    reg_val |= SPI_GC_MODE;
    hal_writel(reg_val, sspi->base + SPI_GC_REG);
}

/* soft reset spi controller */
static void spi_soft_reset(sunxi_spi_t *sspi)
{
    uint32_t reg_val = hal_readl(sspi->base + SPI_GC_REG);

    reg_val |= SPI_GC_SRST;
    hal_writel(reg_val, sspi->base + SPI_GC_REG);
}

/* enable transmit pause */
static void spi_enable_tp(sunxi_spi_t *sspi)
{
    uint32_t reg_val = hal_readl(sspi->base + SPI_GC_REG);

    reg_val |= SPI_GC_TP_EN;
    hal_writel(reg_val, sspi->base + SPI_GC_REG);
}

/* set ss control */
static void spi_ss_owner(sunxi_spi_t *sspi, uint32_t on_off)
{
    u32 reg_val = hal_readl(sspi->base + SPI_TC_REG);

    on_off &= 0x1;
    if (on_off)
    {
        reg_val |= SPI_TC_SS_OWNER;
    }
    else
    {
        reg_val &= ~SPI_TC_SS_OWNER;
    }
    hal_writel(reg_val, sspi->base + SPI_TC_REG);
}

/* enable irq type */
static void spi_enable_irq(uint32_t bitmap, sunxi_spi_t *sspi)
{
    uint32_t reg_val = hal_readl(sspi->base + SPI_INT_CTL_REG);

    bitmap &= SPI_INTEN_MASK;
    reg_val |= bitmap;
    hal_writel(reg_val, sspi->base + SPI_INT_CTL_REG);
}

/* disable irq type */
static void spi_disable_irq(uint32_t bitmap, sunxi_spi_t *sspi)
{
    uint32_t reg_val = hal_readl(sspi->base + SPI_INT_CTL_REG);

    bitmap &= SPI_INTEN_MASK;
    reg_val &= ~bitmap;
    hal_writel(reg_val, sspi->base + SPI_INT_CTL_REG);
}

/* enable dma irq */
#ifndef CONFIG_SUNXI_SPI_CPU_XFER_ONLY
static void spi_enable_dma_irq(uint32_t bitmap, sunxi_spi_t *sspi)
{
    uint32_t reg_val = hal_readl(sspi->base + SPI_FIFO_CTL_REG);

    bitmap &= SPI_FIFO_CTL_DRQEN_MASK;
    reg_val |= bitmap;
    hal_writel(reg_val, sspi->base + SPI_FIFO_CTL_REG);
}
#endif

/* disable dma irq */
static void spi_disable_dma_irq(uint32_t bitmap, sunxi_spi_t *sspi)
{
    uint32_t reg_val = hal_readl(sspi->base + SPI_FIFO_CTL_REG);

    bitmap &= SPI_FIFO_CTL_DRQEN_MASK;
    reg_val &= ~bitmap;
    hal_writel(reg_val, sspi->base + SPI_FIFO_CTL_REG);
}

/* query irq enable */
static uint32_t spi_qry_irq_enable(sunxi_spi_t *sspi)
{
    return (SPI_INTEN_MASK & hal_readl(sspi->base + SPI_INT_CTL_REG));
}

/* query irq pending */
static uint32_t spi_qry_irq_pending(sunxi_spi_t *sspi)
{
    return (SPI_INT_STA_MASK & hal_readl(sspi->base + SPI_INT_STA_REG));
}

/* clear irq pending */
static void spi_clr_irq_pending(uint32_t pending_bit, sunxi_spi_t *sspi)
{
    pending_bit &= SPI_INT_STA_MASK;
    hal_writel(pending_bit, sspi->base + SPI_INT_STA_REG);
}

/* query txfifo bytes */
static uint32_t spi_query_txfifo(sunxi_spi_t *sspi)
{
    uint32_t reg_val =
        (SPI_FIFO_STA_TX_CNT & hal_readl(sspi->base + SPI_FIFO_STA_REG));

    reg_val >>= SPI_TXCNT_BIT_POS;
    return reg_val;
}

/* query rxfifo bytes */
static uint32_t spi_query_rxfifo(sunxi_spi_t *sspi)
{
    uint32_t reg_val =
        (SPI_FIFO_STA_RX_CNT & hal_readl(sspi->base + SPI_FIFO_STA_REG));

    reg_val >>= SPI_RXCNT_BIT_POS;
    return reg_val;
}

/* reset fifo */
static void spi_reset_fifo(sunxi_spi_t *sspi)
{
    uint32_t reg_val = hal_readl(sspi->base + SPI_FIFO_CTL_REG);

    reg_val |= (SPI_FIFO_CTL_RX_RST | SPI_FIFO_CTL_TX_RST);
    /* Set the trigger level of RxFIFO/TxFIFO. */
    reg_val &= ~(SPI_FIFO_CTL_RX_LEVEL | SPI_FIFO_CTL_TX_LEVEL);
    reg_val |= (0x20 << 16) | 0x20;
    hal_writel(reg_val, sspi->base + SPI_FIFO_CTL_REG);
}

#ifndef CONFIG_SUNXI_SPI_CPU_XFER_ONLY
static void spi_set_rx_trig(uint32_t val, sunxi_spi_t *sspi)
{
    uint32_t reg_val = hal_readl(sspi->base + SPI_FIFO_CTL_REG);

    reg_val &= ~SPI_FIFO_CTL_RX_LEVEL;
    reg_val |= val & SPI_FIFO_CTL_RX_LEVEL;
    hal_writel(reg_val, sspi->base + SPI_FIFO_CTL_REG);
}

static void spi_set_tx_trig(uint32_t val, sunxi_spi_t *sspi)
{
    uint32_t reg_val = hal_readl(sspi->base + SPI_FIFO_CTL_REG);

    reg_val &= ~SPI_FIFO_CTL_TX_LEVEL;
    reg_val |= (val << 16) & SPI_FIFO_CTL_TX_LEVEL;
    hal_writel(reg_val, sspi->base + SPI_FIFO_CTL_REG);
}
#endif

/* set transfer total length BC, transfer length TC and single transmit length
 * STC */
static void spi_set_bc_tc_stc(uint32_t tx_len, uint32_t rx_len,
                              uint32_t stc_len, uint32_t dummy_cnt,
                              sunxi_spi_t *sspi)
{
    uint32_t reg_val;

    /* set MBC(0x30) = tx_len + rx_len + dummy_cnt */
    reg_val = hal_readl(sspi->base + SPI_BURST_CNT_REG);
    reg_val &= ~SPI_BC_CNT_MASK;
    reg_val |= (SPI_BC_CNT_MASK & (tx_len + rx_len + dummy_cnt));
    hal_writel(reg_val, sspi->base + SPI_BURST_CNT_REG);

    /* set MTC(0x34) = tx_len */
    reg_val = hal_readl(sspi->base + SPI_TRANSMIT_CNT_REG);
    reg_val &= ~SPI_TC_CNT_MASK;
    reg_val |= (SPI_TC_CNT_MASK & tx_len);
    hal_writel(reg_val, sspi->base + SPI_TRANSMIT_CNT_REG);

    /* set BBC(0x38) = dummy cnt & single mode transmit counter */
    reg_val = hal_readl(sspi->base + SPI_BCC_REG);
    reg_val &= ~SPI_BCC_STC_MASK;
    reg_val |= (SPI_BCC_STC_MASK & stc_len);
    reg_val &= ~(0xf << 24);
    reg_val |= (dummy_cnt << 24);
    hal_writel(reg_val, sspi->base + SPI_BCC_REG);
}

#if 0
/* set ss control */
static void spi_ss_ctrl(sunxi_spi_t *sspi, uint8_t on_off)
{
    uint32_t reg_val = hal_readl(sspi->base + SPI_TC_REG);

    on_off &= 0x1;
    if (on_off)
    {
        reg_val |= SPI_TC_SS_LEVEL;
    }
    else
    {
        reg_val &= ~SPI_TC_SS_LEVEL;
    }
    hal_writel(reg_val, sspi->base + SPI_TC_REG);
}
#endif

/* set dhb, 1: discard unused spi burst; 0: receiving all spi burst */
static void spi_set_all_burst_received(sunxi_spi_t *sspi)
{
	uint32_t reg_val = hal_readl(sspi->base + SPI_TC_REG);

	reg_val &= ~SPI_TC_DHB;
	writel(reg_val, sspi->base + SPI_TC_REG);
}

static void spi_disable_dual(sunxi_spi_t *sspi)
{
    uint32_t reg_val = hal_readl(sspi->base + SPI_BCC_REG);
    reg_val &= ~SPI_BCC_DUAL_MODE;
    hal_writel(reg_val, sspi->base + SPI_BCC_REG);
}

static void spi_enable_dual(sunxi_spi_t *sspi)
{
    uint32_t reg_val = hal_readl(sspi->base + SPI_BCC_REG);
    reg_val &= ~SPI_BCC_QUAD_MODE;
    reg_val |= SPI_BCC_DUAL_MODE;
    hal_writel(reg_val, sspi->base + SPI_BCC_REG);
}

static void spi_disable_quad(sunxi_spi_t *sspi)
{
    uint32_t reg_val = hal_readl(sspi->base + SPI_BCC_REG);

    reg_val &= ~SPI_BCC_QUAD_MODE;
    hal_writel(reg_val, sspi->base + SPI_BCC_REG);
}

static void spi_enable_quad(sunxi_spi_t *sspi)
{
    uint32_t reg_val = hal_readl(sspi->base + SPI_BCC_REG);

    reg_val |= SPI_BCC_QUAD_MODE;
    hal_writel(reg_val, sspi->base + SPI_BCC_REG);
}

static spi_master_status_t spi_mode_check(sunxi_spi_t *sspi)
{
    if (sspi->mode_type != MODE_TYPE_NULL)
    {
        SPI_ERR("[spi%d] invalid parameter\n", sspi->port);
        return SPI_MASTER_INVALID_PARAMETER;
    }

    /* full duplex */
    if (sspi->transfer->tx_buf && sspi->transfer->rx_buf)
    {
        /* fake full duplex for flash read/write */
        if (sspi->config.flash)
        {
            spi_set_bc_tc_stc(sspi->transfer->tx_len,
                            sspi->transfer->rx_len,
                            sspi->transfer->tx_single_len,
                            sspi->transfer->dummy_byte, sspi);
            sspi->mode_type = FULL_DUPLEX_TX_RX;

            if (sspi->transfer->rx_nbits == SPI_NBITS_QUAD)
            {
                spi_disable_dual(sspi);
                spi_enable_quad(sspi);
                SPI_INFO("[spi%d] Quad mode fake Full duplex tx rx\n",
                        sspi->port);
            }
            else if (sspi->transfer->rx_nbits == SPI_NBITS_DUAL)
            {
                spi_disable_quad(sspi);
                spi_enable_dual(sspi);
                SPI_INFO("[spi%d] Dual mode fake Full duplex tx rx\n",
                        sspi->port);
            }
            else
            {
                spi_disable_quad(sspi);
                spi_disable_dual(sspi);
                SPI_INFO("[spi%d] Single mode fake Full duplex tx rx\n",
                        sspi->port);
            }
        } /* real full duplex transmit by single mode */
        else
        {
            spi_disable_quad(sspi);
            spi_disable_dual(sspi);
            spi_set_all_burst_received(sspi);
            spi_set_bc_tc_stc(sspi->transfer->tx_len, 0,
                            sspi->transfer->tx_single_len,
                            0, sspi);
            sspi->mode_type = SINGLE_FULL_DUPLEX_RX_TX;
            SPI_INFO("[spi%d] Single mode real Full duplex tx rx\n",
                        sspi->port);
        }
    } /* half duplex transmit */
    else if (sspi->transfer->tx_buf)
    {
        if (sspi->transfer->tx_nbits == SPI_NBITS_QUAD)
        {
            spi_disable_dual(sspi);
            spi_enable_quad(sspi);
            spi_set_bc_tc_stc(sspi->transfer->tx_len, 0,
                              sspi->transfer->tx_single_len,
                              sspi->transfer->dummy_byte, sspi);
            sspi->mode_type = QUAD_HALF_DUPLEX_TX;
            SPI_INFO("[spi%d] Quad mode Half duplex tx\n",
                     sspi->port);
        }
        else if (sspi->transfer->tx_nbits == SPI_NBITS_DUAL)
        {
            spi_disable_quad(sspi);
            spi_enable_dual(sspi);
            spi_set_bc_tc_stc(sspi->transfer->tx_len, 0,
                              sspi->transfer->tx_single_len,
                              sspi->transfer->dummy_byte, sspi);
            sspi->mode_type = DUAL_HALF_DUPLEX_TX;
            SPI_INFO("[spi%d] Dual mode Half duplex tx\n",
                     sspi->port);
        }
        else
        {
            spi_disable_quad(sspi);
            spi_disable_dual(sspi);
            spi_set_bc_tc_stc(sspi->transfer->tx_len, 0,
                              sspi->transfer->tx_len,
                              sspi->transfer->dummy_byte, sspi);
            sspi->mode_type = SGLE_HALF_DUPLEX_TX;
            SPI_INFO("[spi%d] Single mode Half duplex tx\n",
                     sspi->port);
        }
    } /* half duplex receive */
    else if (sspi->transfer->rx_buf)
    {
        if (sspi->transfer->rx_nbits == SPI_NBITS_QUAD)
        {
            spi_disable_dual(sspi);
            spi_enable_quad(sspi);
            sspi->mode_type = QUAD_HALF_DUPLEX_RX;
            SPI_INFO("[spi%d] Quad mode Half duplex rx\n",
                     sspi->port);
        }
        else if (sspi->transfer->rx_nbits == SPI_NBITS_DUAL)
        {
            spi_disable_quad(sspi);
            spi_enable_dual(sspi);
            sspi->mode_type = DUAL_HALF_DUPLEX_RX;
            SPI_INFO("[spi%d] Dual mode Half duplex rx\n",
                     sspi->port);
        }
        else
        {
            spi_disable_quad(sspi);
            spi_disable_dual(sspi);
            sspi->mode_type = SGLE_HALF_DUPLEX_RX;
            SPI_INFO("[spi%d] Single mode Half duplex rx\n",
                     sspi->port);
        }
        spi_set_bc_tc_stc(0, sspi->transfer->rx_len, 0,
                          sspi->transfer->dummy_byte, sspi);
    }

    return SPI_MASTER_OK;
}

static spi_master_status_t spi_cpu_write(sunxi_spi_t *sspi)
{
    uint32_t len = sspi->transfer->tx_len;
    const uint8_t *buf = sspi->transfer->tx_buf;
    uint32_t poll_time = 0x7ffffff;
#ifdef SPI_DATA_LEVEL
    uint32_t i, j;
    uint8_t dbuf[64], cnt = 0;
#endif

    if (NULL == buf)
    {
        SPI_ERR("[spi%d] invalid parameter\n", sspi->port);
        return SPI_MASTER_INVALID_PARAMETER;
    }

#ifdef SPI_DUMPREG_LEVEL
    SPI_INFO("[spi%d] dump reg:\n", sspi->port);
    spi_dump_reg(sspi, 0, 0x60);
#endif

#ifdef SPI_DATA_LEVEL
    SPI_INFO("tx_len = %d\n", len);
    for (i = 0; i < len; i += 16)
    {
        cnt = 0;
        cnt += sprintf(dbuf + cnt, "%04x: ", i);
        for (j = 0; ((i + j) < len) && (j < 16); j++)
            cnt += sprintf(dbuf + cnt, "%02x ",
                           ((uint8_t *)(buf))[i + j]);
        printf("%s\n", dbuf);
    }
#endif

    for (; len > 0; --len)
    {
        hal_writeb(*buf++, sspi->base + SPI_TXDATA_REG);
        while ((spi_query_txfifo(sspi) >= MAX_FIFU) && poll_time--)
           ;
        if (poll_time == 0)
        {
            SPI_ERR("[spi%d] cpu transfer data time out!\n",
                    sspi->port);
            SPI_INFO("[spi%d] dump reg:\n", sspi->port);
            spi_dump_reg(sspi, 0, 0x60);
            return SPI_MASTER_ERROR_TIMEOUT;
        }
    }
    return SPI_MASTER_OK;
}

static spi_master_status_t spi_cpu_read(sunxi_spi_t *sspi)
{
    uint32_t len = sspi->transfer->rx_len;
    uint8_t *buf = sspi->transfer->rx_buf;
    uint32_t poll_time = 0x7ffffff;
    uint32_t n;

#ifdef SPI_DATA_LEVEL
    uint32_t i, j;
    uint8_t dbuf[64], cnt = 0;
#endif

    if (NULL == buf)
    {
        SPI_ERR("[spi%d] invalid parameter\n", sspi->port);
        return SPI_MASTER_INVALID_PARAMETER;
    }

#ifdef SPI_DUMPREG_LEVEL
    SPI_INFO("[spi%d] dump reg:\n", sspi->port);
    spi_dump_reg(sspi, 0, 0x60);
#endif

    for (n = 0; n < len; n++)
    {
        while (!spi_query_rxfifo(sspi) && poll_time--)
            ;
        if (poll_time <= 0)
        {
            SPI_ERR("[spi%d] cpu receive data time out!\n",
                    sspi->port);
            SPI_INFO("[spi%d] dump reg:\n", sspi->port);
            spi_dump_reg(sspi, 0, 0x60);
            return SPI_MASTER_ERROR_TIMEOUT;
        }

        *(buf + n) = hal_readb(sspi->base + SPI_RXDATA_REG);
    }

#ifdef SPI_DATA_LEVEL
    SPI_INFO("rx_len = %d\n", len);
    for (i = 0; i < len; i += 16)
    {
        cnt = 0;
        cnt += sprintf(dbuf + cnt, "%04x: ", i);
        for (j = 0; ((i + j) < len) && (j < 16); j++)
            cnt += sprintf(dbuf + cnt, "%02x ",
                           ((uint8_t *)(buf))[i + j]);
        printf("%s\n", dbuf);
    }
#endif

    return SPI_MASTER_OK;
}

static spi_master_status_t spi_cpu_write_read(sunxi_spi_t *sspi)
{
    uint32_t len = sspi->transfer->tx_len;
    uint8_t *tx_buf = sspi->transfer->tx_buf;
    uint8_t *rx_buf = sspi->transfer->rx_buf;
    uint32_t align_loop, left_loop;
    int i;
    int ret = SPI_MASTER_OK;

    align_loop = len / MAX_FIFU;
    left_loop  = len % MAX_FIFU;

    SPI_INFO("[spi%d] data align loop %d\n", sspi->port, align_loop);
    SPI_INFO("[spi%d] data left loop\n", sspi->port, left_loop);

    /* len over FIFO size, need slipt the buffer */
	if (align_loop > 0) {
		for (i = 0; i < align_loop; i++) {
			sspi->transfer->tx_len = MAX_FIFU;
            sspi->transfer->rx_len = MAX_FIFU;
			ret = spi_cpu_write(sspi);
            if (ret != SPI_MASTER_OK) {
                goto err0;
            }
			ret = spi_cpu_read(sspi);
            if (ret != SPI_MASTER_OK) {
                goto err0;
            }
			sspi->transfer->tx_buf += MAX_FIFU;
			sspi->transfer->rx_buf += MAX_FIFU;
		}
	}

	/* Left Bytes */
    sspi->transfer->tx_len = left_loop;
    sspi->transfer->rx_len = left_loop;
	ret = spi_cpu_write(sspi);
    if (ret != SPI_MASTER_OK) {
        goto err0;
    }
	ret = spi_cpu_read(sspi);
    if (ret != SPI_MASTER_OK) {
        goto err0;
    }

err0:
    sspi->transfer->tx_len = len;
    sspi->transfer->rx_len = len;
	sspi->transfer->tx_buf = tx_buf;
	sspi->transfer->rx_buf = rx_buf;

    return ret;
}

#ifndef CONFIG_SUNXI_SPI_CPU_XFER_ONLY
static spi_master_status_t spi_dma_tx_config(struct sunxi_spi *sspi)
{
    hal_dma_chan_status_t ret;
    uint32_t flags = 0;
    uint32_t len = sspi->transfer->tx_len;
    uint8_t const *buf = sspi->transfer->tx_buf;
    struct dma_slave_config *config = &sspi->dma_tx.config;

#ifdef SPI_DATA_LEVEL
    unsigned int i, j;
    u8 dbuf[64], cnt = 0;
#endif

    if (NULL == buf)
    {
        SPI_ERR("[spi%d] invalid parameter\n", sspi->port);
        return SPI_MASTER_INVALID_PARAMETER;
    }

    if (len <= ALIGN_DMA_BUF_SIZE)
    {
        memcpy(sspi->align_dma_buf, buf, len);
        buf = sspi->align_dma_buf;
    }
    else
    {
        SPI_ERR("[spi%d] tx size over dma align buf size\n",
                sspi->port);
        /* buffer on DMA must align to cache line */
        if ((unsigned long)buf & (64 - 1) || len & (64 - 1))
        {
            SPI_ERR("[spi%d] tx buf or len not align to 64\n",
                    sspi->port);
            return SPI_MASTER_INVALID_PARAMETER;
        }
    }
    hal_dcache_clean((unsigned long)buf, SPI_ALIGN(len, 64));

#ifdef SPI_DATA_LEVEL
    SPI_INFO("tx_len = %d\n", len);
    for (i = 0; i < len; i += 16)
    {
        cnt = 0;
        cnt += sprintf(dbuf + cnt, "%03x: ", i);
        for (j = 0; ((i + j) < len) && (j < 16); j++)
            cnt += sprintf(dbuf + cnt, "%02x ",
                           ((uint8_t *)(buf))[i + j]);
        printf("%s\n", dbuf);
    }
#endif

    ret = hal_dma_chan_request(&sspi->dma_tx.chan);
    if (ret == HAL_DMA_CHAN_STATUS_BUSY)
    {
        SPI_ERR("[spi%d] request dma_rx failed\n", sspi->port);
        return SPI_MASTER_ERROR;
    }

    spi_set_tx_trig(32, sspi);
    spi_set_rx_trig(32, sspi);
    spi_enable_dma_irq(SPI_FIFO_CTL_TX_DRQEN, sspi);

#ifdef SPI_DUMPREG_LEVEL
    SPI_INFO("[spi%d] dump reg:\n", sspi->port);
    spi_dump_reg(sspi, 0, 0x60);
#endif

    config->direction = DMA_MEM_TO_DEV;
    config->dst_addr = sspi->base + SPI_TXDATA_REG;
    config->src_addr = (unsigned long)buf;

    if (len % DMA_SLAVE_BUSWIDTH_4_BYTES)
    {
        config->src_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
        config->dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
        config->dst_maxburst = DMA_SLAVE_BURST_16;
        config->src_maxburst = DMA_SLAVE_BURST_16;
    }
    else
    {
        config->src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
        config->dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
        config->dst_maxburst = DMA_SLAVE_BURST_8;
        config->src_maxburst = DMA_SLAVE_BURST_8;
    }

    config->slave_id =
        sunxi_slave_id(SUNXI_SPI_DRQ_TX(sspi->port), DRQSRC_SDRAM);

    SPI_INFO("[spi%d] config:\n"
             "  direction: %d\n"
             "  src_addr: 0x%08lx,"
             "  dst_addr: 0x%08lx\n"
             "  src_addr_width: %d,"
             "  dst_addr_width: %d\n"
             "  src_maxburst: %lu,"
             "  dst_maxburst: %lu\n"
             "  slave_id: 0x%08lx\n",
             sspi->port, config->direction, config->src_addr,
             config->dst_addr, config->src_addr_width,
             config->dst_addr_width, config->src_maxburst,
             config->dst_maxburst, config->slave_id);

    return SPI_MASTER_OK;
}

#ifdef SPI_INFO_LEVEL
static void spi_dma_tx_callback(void *para)
{
    sunxi_spi_t *sspi = (sunxi_spi_t *)para;
    SPI_INFO("[spi%d] DMA TX callback function\n", sspi->port);
}
#endif

static spi_master_status_t spi_dma_tx_submit(struct sunxi_spi *sspi)
{
    hal_dma_status_t ret;
    uint32_t len = sspi->transfer->tx_len;
    struct dma_slave_config *config = &sspi->dma_tx.config;
    struct sunxi_dma_chan *chan = sspi->dma_tx.chan;

    ret = hal_dma_slave_config(chan, config);
    if (ret)
    {
        SPI_ERR("[spi%d] dma slave config failed! return %d\n",
                sspi->port, ret);
        return SPI_MASTER_ERROR;
    }

    ret = hal_dma_prep_device(chan, config->dst_addr, config->src_addr, len,
                              config->direction);
    if (ret)
    {
        SPI_ERR("[spi%d] dma prep device failed! return %d\n",
                sspi->port, ret);
        return SPI_MASTER_ERROR;
    }

#ifdef SPI_INFO_LEVEL
    chan->callback = spi_dma_tx_callback;
    chan->callback_param = sspi;
#endif

    ret = hal_dma_start(chan);
    if (ret)
    {
        SPI_ERR("[spi%d] dma start error! return %d\n", sspi->port,
                ret);
        return SPI_MASTER_ERROR;
    }

    return SPI_MASTER_OK;
}

static void spi_dma_rx_callback(void *para)
{
    sunxi_spi_t *sspi = (sunxi_spi_t *)para;

    if (hal_sem_post(sspi->xSemaphore_rx))
    {
        SPI_ERR("[spi%d] xSemaphoreGive failed.\n", sspi->port);
    }
}

static spi_master_status_t spi_dma_rx_config(struct sunxi_spi *sspi)
{
    hal_dma_chan_status_t ret;
    uint32_t flags = 0;
    uint32_t len = sspi->transfer->rx_len, size = 0;
    uint8_t *buf = sspi->transfer->rx_buf;
    struct dma_slave_config *config = &sspi->dma_rx.config;

    if (NULL == buf)
    {
        SPI_ERR("[spi%d] invalid parameter\n", sspi->port);
        return SPI_MASTER_INVALID_PARAMETER;
    }

    if (len <= ALIGN_DMA_BUF_SIZE)
    {
        buf = sspi->align_dma_buf;
        memset(buf, 0, len);
    }
    else
    {
        SPI_ERR("[spi%d] rx size over dma align buf size\n",
                sspi->port);
        /* buffer on DMA must align to cache line */
        if ((unsigned long)buf & (64 - 1) || len & (64 - 1))
        {
            SPI_ERR("[spi%d] rx buf or len not align to 64\n",
                    sspi->port);
            return SPI_MASTER_INVALID_PARAMETER;
        }
    }
    hal_dcache_invalidate((unsigned long)buf, SPI_ALIGN(len, 64));
    spi_enable_dma_irq(SPI_FIFO_CTL_RX_DRQEN, sspi);

#ifdef SPI_DUMPREG_LEVEL
    SPI_INFO("[spi%d] dump reg:\n", sspi->port);
    spi_dump_reg(sspi, 0, 0x60);
#endif

    ret = hal_dma_chan_request(&sspi->dma_rx.chan);
    if (ret == HAL_DMA_CHAN_STATUS_BUSY)
    {
        SPI_ERR("[spi%d] request dma_rx failed\n", sspi->port);
        return SPI_MASTER_ERROR;
    }

    config->direction = DMA_DEV_TO_MEM;
    config->dst_addr = (unsigned long)buf;
    config->src_addr = sspi->base + SPI_RXDATA_REG;

    if (len % DMA_SLAVE_BUSWIDTH_4_BYTES)
    {
        config->src_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
        config->dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
        config->dst_maxburst = DMA_SLAVE_BURST_16;
        config->src_maxburst = DMA_SLAVE_BURST_16;
    }
    else
    {
        config->src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
        config->dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
        config->dst_maxburst = DMA_SLAVE_BURST_8;
        config->src_maxburst = DMA_SLAVE_BURST_8;
    }

    config->slave_id =
        sunxi_slave_id(DRQDST_SDRAM, SUNXI_SPI_DRQ_RX(sspi->port));

    SPI_INFO("[spi%d] config:\n"
             "  direction: %d\n"
             "  src_addr: 0x%08lx,"
             "  dst_addr: 0x%08lx\n"
             "  src_addr_width: %d,"
             "  dst_addr_width: %d\n"
             "  src_maxburst: %lu,"
             "  dst_maxburst: %lu\n"
             "  slave_id: 0x%08lx\n",
             sspi->port, config->direction, config->src_addr,
             config->dst_addr, config->src_addr_width,
             config->dst_addr_width, config->src_maxburst,
             config->dst_maxburst, config->slave_id);

    return SPI_MASTER_OK;
}

static spi_master_status_t spi_dma_rx_submit(struct sunxi_spi *sspi)
{
    hal_dma_status_t ret;
    uint32_t flags = 0;
    uint32_t len = sspi->transfer->rx_len, size = 0;
    struct sunxi_dma_chan *chan = sspi->dma_rx.chan;
    struct dma_slave_config *config = &sspi->dma_rx.config;

    ret = hal_dma_slave_config(chan, config);
    if (ret)
    {
        SPI_ERR("[spi%d] dma slave config failed! return %d\n",
                sspi->port, ret);
        return SPI_MASTER_ERROR;
    }

    ret = hal_dma_prep_device(chan, config->dst_addr, config->src_addr, len,
                              config->direction);
    if (ret)
    {
        SPI_ERR("[spi%d] dma prep device failed! return %d\n",
                sspi->port, ret);
        return SPI_MASTER_ERROR;
    }

    chan->callback = spi_dma_rx_callback;
    chan->callback_param = sspi;

    ret = hal_dma_start(chan);
    if (ret)
    {
        SPI_ERR("[spi%d] dma start error! return %d\n", sspi->port,
                ret);
        return SPI_MASTER_ERROR;
    }

    return SPI_MASTER_OK;
}

#endif

static spi_master_status_t spi_transfer(sunxi_spi_t *sspi)
{
    switch (sspi->mode_type)
    {
        case SGLE_HALF_DUPLEX_RX:
        case DUAL_HALF_DUPLEX_RX:
        case QUAD_HALF_DUPLEX_RX:
        {
#ifndef CONFIG_SUNXI_SPI_CPU_XFER_ONLY
            /* >64 use DMA transfer, or use cpu */
            if (sspi->transfer->rx_len > BULK_DATA_BOUNDARY)
            {
                SPI_INFO("[spi%d] rx by dma\n", sspi->port);
                /* For Rx mode, the DMA end(not TC flag) is real end. */
                spi_disable_irq(SPI_INTEN_TC, sspi);
                if (spi_dma_rx_config(sspi))
                {
                    return SPI_MASTER_ERROR;
                }
                if (spi_dma_rx_submit(sspi))
                {
                    return SPI_MASTER_ERROR;
                }
                spi_start_xfer(sspi);
            }
            else
            {
#endif
                SPI_INFO("[spi%d] rx by cpu\n", sspi->port);
                spi_clr_irq_pending(SPI_INT_STA_MASK, sspi);
                spi_start_xfer(sspi);
                spi_cpu_read(sspi);
#ifndef CONFIG_SUNXI_SPI_CPU_XFER_ONLY
            }
#endif
            break;
        }
        case SGLE_HALF_DUPLEX_TX:
        case DUAL_HALF_DUPLEX_TX:
        case QUAD_HALF_DUPLEX_TX:
        {
#ifndef CONFIG_SUNXI_SPI_CPU_XFER_ONLY
            /* >64 use DMA transfer, or use cpu */
            if (sspi->transfer->tx_len > BULK_DATA_BOUNDARY)
            {
                SPI_INFO("[spi%d] tx by dma\n", sspi->port);
                sspi->sem = 1;
                spi_start_xfer(sspi);
                if (spi_dma_tx_config(sspi))
                {
                    return SPI_MASTER_ERROR;
                }
                if (spi_dma_tx_submit(sspi))
                {
                    return SPI_MASTER_ERROR;
                }
            }
            else
            {
#endif
                SPI_INFO("[spi%d] tx by cpu\n", sspi->port);
                spi_start_xfer(sspi);
                spi_cpu_write(sspi);
#ifndef CONFIG_SUNXI_SPI_CPU_XFER_ONLY
            }
#endif
            break;
        }
        case SINGLE_FULL_DUPLEX_RX_TX:
        {
#ifndef CONFIG_SUNXI_SPI_CPU_XFER_ONLY
            /* >64 use DMA transfer, or use cpu */
            if (sspi->transfer->rx_len > BULK_DATA_BOUNDARY && sspi->transfer->tx_len > BULK_DATA_BOUNDARY)
            {
                SPI_INFO("[spi%d] tx and rx by dma\n", sspi->port);
                /* For Rx mode, the DMA end(not TC flag) is real end. */
                // spi_disable_irq(SPI_INTEN_TC, sspi);
                spi_start_xfer(sspi);
                sspi->sem = 1;
                if (spi_dma_rx_config(sspi))
                {
                    return SPI_MASTER_ERROR;
                }
                if (spi_dma_rx_submit(sspi))
                {
                    return SPI_MASTER_ERROR;
                }
                if (spi_dma_tx_config(sspi))
                {
                    return SPI_MASTER_ERROR;
                }
                if (spi_dma_tx_submit(sspi))
                {
                    return SPI_MASTER_ERROR;
                }
            }
            else
            {
#endif
                SPI_INFO("[spi%d] tx and rx by cpu\n", sspi->port);
                spi_clr_irq_pending(SPI_INT_STA_MASK, sspi);
                spi_start_xfer(sspi);
                spi_cpu_write_read(sspi);
#ifndef CONFIG_SUNXI_SPI_CPU_XFER_ONLY
            }
#endif
            break;
        }
        case FULL_DUPLEX_TX_RX:
        {
#ifndef CONFIG_SUNXI_SPI_CPU_XFER_ONLY
            /* >64 use DMA transfer, or use cpu */
            if (sspi->transfer->rx_len > BULK_DATA_BOUNDARY)
            {
                SPI_INFO("[spi%d] tx and rx by dma\n", sspi->port);
                /* For Rx mode, the DMA end(not TC flag) is real end. */
                spi_disable_irq(SPI_INTEN_TC, sspi);
                spi_start_xfer(sspi);
                spi_cpu_write(sspi);
                if (spi_dma_rx_config(sspi))
                {
                    return SPI_MASTER_ERROR;
                }
                if (spi_dma_rx_submit(sspi))
                {
                    return SPI_MASTER_ERROR;
                }
            }
            else
            {
#endif
                SPI_INFO("[spi%d] tx and rx by cpu\n", sspi->port);
                spi_start_xfer(sspi);
                spi_cpu_write(sspi);
                spi_cpu_read(sspi);
#ifndef CONFIG_SUNXI_SPI_CPU_XFER_ONLY
            }
#endif
            break;
        }
        default:
        {
            SPI_ERR("[spi%d] invalid parameter\n", sspi->port);
            return SPI_MASTER_INVALID_PARAMETER;
        }
    }

    return SPI_MASTER_OK;
}

/* wake up the sleep thread, and give the result code */
static hal_irqreturn_t  spi_irq_handler(void *ptr)
{
    uint32_t status = 0, enable = 0;
    sunxi_spi_t *sspi = (sunxi_spi_t *)ptr;

    enable = spi_qry_irq_enable(sspi);
    status = spi_qry_irq_pending(sspi);
    spi_clr_irq_pending(status, sspi);

    sspi->result = SPI_XFER_OK;

    /* master mode, Transfer Complete Interrupt */
    if (status & SPI_INT_STA_TC)
    {
        SPI_INFO("[spi%d] SPI TC COME\n", sspi->port);
        spi_disable_irq(SPI_INT_STA_TC | SPI_INT_STA_ERR, sspi);
#ifndef CONFIG_SUNXI_SPI_CPU_XFER_ONLY
        if (sspi->sem)
        {
            sspi->sem = 0;
            if (hal_sem_post(sspi->xSemaphore_tx))
                SPI_ERR("[spi%d] xSemaphorePostFromISR failed.\n",
                             sspi->port);
        }
#endif
        return 0;
    }
    else if (status & SPI_INT_STA_ERR)     /* master mode:err */
    {
        SPI_ERR("[spi%d]  SPI ERR! status %"PRIx32"\n", sspi->port, status);
        /* __log("[spi%d] dump reg:\n", sspi->port); */
        /* spi_dump_reg(sspi, 0, 0x60); */
        spi_disable_irq(SPI_INT_STA_TC | SPI_INT_STA_ERR, sspi);
        sspi->result = SPI_XFER_FAILED;
#ifndef CONFIG_SUNXI_SPI_CPU_XFER_ONLY
        if (sspi->sem)
        {
            sspi->sem = 0;
            if (hal_sem_post(sspi->xSemaphore_tx))
                SPI_ERR("[spi%d] xSemaphorePostFromISR failed.\n",
                             sspi->port);
        }
#endif
        return 0;
    }
    return 0;
}

static spi_master_status_t spi_pinctrl_init(sunxi_spi_t *sspi, uint32_t sip)
{
#ifdef CONFIG_OS_MELIS
    user_gpio_set_t gpio_cfg[6] = {0};
    gpio_pin_t spi_pin[6];
    gpio_muxsel_t spi_muxsel[6];
    int count, i;
    char spi_name[16];
    int ret = SPI_MASTER_OK;

    if (sspi->port >= HAL_SPI_MASTER_MAX)
    {
        SPI_ERR("[spi%d] invalid port\n", sspi->port);
        return SPI_MASTER_INVALID_PARAMETER;
    }

    sprintf(spi_name, "spi%d", sspi->port);
    count = Hal_Cfg_GetGPIOSecKeyCount(spi_name);
    if (!count)
    {
        SPI_ERR("[spi%d] not support in sys_config\n", sspi->port);
        return SPI_MASTER_INVALID_PARAMETER;
    }
    Hal_Cfg_GetGPIOSecData(spi_name, gpio_cfg, count);

    for (i = 0; i < count; i++)
    {
        spi_pin[i] = (gpio_cfg[i].port - 1) * 32 + gpio_cfg[i].port_num;
        spi_muxsel[i] = gpio_cfg[i].mul_sel;
        ret = hal_gpio_pinmux_set_function(spi_pin[i], spi_muxsel[i]);
	    if (ret)
        {
            SPI_ERR("[spi%d] PIN%u set function failed! return %d\n",
                sspi->port, spi_pin[i], ret);
            return SPI_MASTER_ERROR;
        }
	    ret = hal_gpio_set_driving_level(spi_pin[i], gpio_cfg[i].drv_level);
        if (ret)
        {
            SPI_ERR("[spi%d] PIN%u set driving level failed! return %d\n",
                sspi->port, gpio_cfg[i].drv_level, ret);
            return SPI_MASTER_ERROR;
        }
        if (gpio_cfg[i].pull)
        {
            ret = hal_gpio_set_pull(spi_pin[i], gpio_cfg[i].pull);
        }
    }

    return ret;
#elif defined(CONFIG_ARCH_SUN20IW2)
//#elif 1
    uint8_t i;
    gpio_pin_t spi_pin[6];
    int ret = SPI_MASTER_OK;
    struct sunxi_spi_params_t *para;

    if (sspi->port >= HAL_SPI_MASTER_MAX)
    {
        SPI_ERR("[spi%d] invalid port\n", sspi->port);
        return SPI_MASTER_INVALID_PARAMETER;
    }
#ifdef SIP_SPI0_PARAMS
    if (sip) {
        para = &g_sunxi_spi_params_sip[sspi->port];
    } 
    else
#endif
    {
        para = &g_sunxi_spi_params[sspi->port];
    }

    spi_pin[0] = para->gpio_clk;
    spi_pin[1] = para->gpio_mosi;
    spi_pin[2] = para->gpio_cs0;
    spi_pin[3] = para->gpio_miso;
    spi_pin[4] = para->gpio_wp;
    spi_pin[5] = para->gpio_hold;

    for (i = 0; i < para->gpio_num; i++)
    {
        ret = hal_gpio_pinmux_set_function(spi_pin[i],
			para->mux);
        if (ret)
        {
            SPI_ERR("[spi%d] PIN%u set function failed! return %d\n",
                sspi->port, spi_pin[i], ret);
            ret = SPI_MASTER_ERROR;
	    goto err;
        }
        ret = hal_gpio_set_driving_level(spi_pin[i],
			para->driv_level);
        if (ret)
        {
            SPI_ERR("[spi%d] PIN%u set driving level failed! return %d\n",
                sspi->port, spi_pin[i], ret);
            ret = SPI_MASTER_ERROR;
	    goto err;
        }
        if (i == 2) //CS
        {
            ret = hal_gpio_set_pull(spi_pin[i], GPIO_PULL_UP);
        }
        else
        {
            ret = hal_gpio_set_pull(spi_pin[i], GPIO_PULL_DOWN_DISABLED);
        }
    }

err:
    return ret;
#else
    //SPI_ERR("[spi%d] not support sys_config format\n", sspi->port);
    uint8_t i;
    gpio_pin_t spi_pin[6];
    int ret = SPI_MASTER_OK;
    struct sunxi_spi_params_t *para;

    if (sspi->port >= HAL_SPI_MASTER_MAX)
    {
        SPI_ERR("[spi%d] invalid port\n", sspi->port);
        return SPI_MASTER_INVALID_PARAMETER;
    }
#ifdef SIP_SPI0_PARAMS
    if (sip) {
        para = &g_sunxi_spi_params_sip[sspi->port];
    } 
    else
#endif
    {
        para = &g_sunxi_spi_params[sspi->port];
    }

    spi_pin[0] = para->gpio_clk;
    spi_pin[1] = para->gpio_mosi;
    spi_pin[2] = para->gpio_cs0;
    spi_pin[3] = para->gpio_miso;
    spi_pin[4] = para->gpio_wp;
    spi_pin[5] = para->gpio_hold;

    for (i = 0; i < para->gpio_num; i++)
    {
        ret = hal_gpio_pinmux_set_function(spi_pin[i],
			para->mux);
        if (ret)
        {
            SPI_ERR("[spi%d] PIN%u set function failed! return %d\n",
                sspi->port, spi_pin[i], ret);
            ret = SPI_MASTER_ERROR;
	    goto err;
        }
        ret = hal_gpio_set_driving_level(spi_pin[i],
			para->driv_level);
        if (ret)
        {
            SPI_ERR("[spi%d] PIN%u set driving level failed! return %d\n",
                sspi->port, spi_pin[i], ret);
            ret = SPI_MASTER_ERROR;
	    goto err;
        }
        if (i == 2) //CS
        {
            ret = hal_gpio_set_pull(spi_pin[i], GPIO_PULL_UP);
        }
        else
        {
            ret = hal_gpio_set_pull(spi_pin[i], GPIO_PULL_DOWN_DISABLED);
        }
    }
err:
    return ret;


#endif
}

static spi_master_status_t spi_clk_init(sunxi_spi_t *sspi, u32 mode_clk, struct sunxi_spi_params_t *para)
{
    unsigned long rate;
    int rate_pll = 0, rate_hosc = 0;
    hal_clk_status_t ret;
    hal_clk_t pclk_pll, pclk_hosc;

    sspi->reset = hal_reset_control_get(para->reset_type, para->reset_id);
    hal_reset_control_deassert(sspi->reset);

    sspi->mclk = hal_clock_get(para->mclk_type, para->mclk_id);
    sspi->bus_clk = hal_clock_get(para->bus_type, para->bus_id);
    pclk_pll = hal_clock_get(para->pclk_pll_type, para->pclk_pll_id);
    pclk_hosc = hal_clock_get(para->pclk_hosc_type, para->pclk_hosc_id);

    SPI_INFO("[spi%d] get pclk_pll %d\n", sspi->port, hal_clk_get_rate(pclk_pll));
    SPI_INFO("[spi%d] get pclk_hosc %d\n", sspi->port, hal_clk_get_rate(pclk_hosc));

    if (hal_clk_get_rate(pclk_pll) >= mode_clk)
    {
        if (!hal_clk_set_parent(sspi->mclk, pclk_pll))
            rate_pll = hal_clk_round_rate(sspi->mclk, mode_clk);
    }
    if (hal_clk_get_rate(pclk_hosc) >= mode_clk)
    {
         if (!hal_clk_set_parent(sspi->mclk, pclk_hosc))
            rate_hosc = hal_clk_round_rate(sspi->mclk, mode_clk);
    }

    SPI_INFO("[spi%d] get pll rate %d\n", sspi->port, rate_pll);
    SPI_INFO("[spi%d] get hosc rate %d\n", sspi->port, rate_hosc);
    SPI_INFO("[spi%d] get mode rate %d\n", sspi->port, mode_clk);

    if (abs(rate_pll - mode_clk) < abs(rate_hosc - mode_clk))
        sspi->pclk = pclk_pll;
    else
        sspi->pclk = pclk_hosc;

    ret = hal_clk_set_parent(sspi->mclk, sspi->pclk);
    if (ret)
    {
        SPI_ERR("[spi%d] clk set parent failed! return %d\n",
                sspi->port, ret);
        return SPI_MASTER_ERROR;
    }

    ret = hal_clk_set_rate(sspi->mclk, mode_clk);
    if (ret)
    {
        SPI_ERR("[spi%d] clk set rate failed! return %d\n", sspi->port,
                ret);
        return SPI_MASTER_ERROR;
    }

    rate = hal_clk_get_rate(sspi->mclk);
    if (!rate)
    {
        SPI_ERR("[spi%d] clk get rate failed! return %ld\n", sspi->port,
                rate);
        return SPI_MASTER_ERROR;
    } 
    else if (rate != mode_clk)
    {
        SPI_ERR("[spi%d] clk rate auto adjust to %ld\n",
                sspi->port, rate);
    }

    ret = hal_clock_enable(sspi->bus_clk);
    if (ret)
    {
        SPI_ERR("[spi%d] couldn't enable bus_clk! return %d\n", sspi->port,
                ret);
        return SPI_MASTER_ERROR;
    }

    ret = hal_clock_enable(sspi->mclk);
    if (ret)
    {
        SPI_ERR("[spi%d] couldn't enable mclk! return %d\n", sspi->port,
                ret);
        return SPI_MASTER_ERROR;
    }

    return SPI_MASTER_OK;
}

static spi_master_status_t spi_clk_exit(sunxi_spi_t *sspi)
{
    hal_clk_status_t ret;

    ret = hal_clock_disable(sspi->mclk);
    if (ret)
    {
        SPI_ERR("[spi%d] couldn't disable mclk! return %d\n",
                sspi->port, ret);
        return SPI_MASTER_ERROR;
    }
    ret = hal_clock_disable(sspi->bus_clk);
#ifdef CONFIG_DRIVERS_SUNXI_CLK
    if (ret && ret != HAL_CLK_STATUS_ERROR_CLK_FACTOR_REFUSED)
#else
    if (ret)
#endif
    {
        SPI_ERR("[spi%d] couldn't disable bus_clk! return %d\n",
                sspi->port, ret);
        return SPI_MASTER_ERROR;
    }

    hal_clock_put(sspi->mclk);
    hal_clock_put(sspi->bus_clk);

    hal_reset_control_assert(sspi->reset);
    hal_reset_control_put(sspi->reset);

    return SPI_MASTER_OK;
}

static spi_master_status_t spi_cpu_complete(sunxi_spi_t *sspi)
{
    uint32_t timeout = 0xffff;

    SPI_INFO("[spi%d] :wait complete\n", sspi->port);
    while (!sspi->result && timeout--)
        ;
    if (timeout <= 0)
    {
        SPI_ERR("[spi%d] xfer timeout\n", sspi->port);
        SPI_INFO("[spi%d] dump reg:\n", sspi->port);
        spi_dump_reg(sspi, 0, 0x60);
        return SPI_MASTER_ERROR_TIMEOUT;
    }
    else if (SPI_XFER_FAILED == sspi->result)
    {
        SPI_ERR("[spi%d] xfer failed...\n", sspi->port);
        SPI_INFO("[spi%d] dump reg:\n", sspi->port);
        spi_dump_reg(sspi, 0, 0x60);
        return SPI_MASTER_ERROR;
    }

    return SPI_MASTER_OK;
}

#ifndef CONFIG_SUNXI_SPI_CPU_XFER_ONLY
static spi_master_status_t spi_dma_tx_complete(sunxi_spi_t *sspi)
{
    uint32_t flags = 0;
    uint32_t timeout = 0x7fffffff;
    struct sunxi_dma_chan *chan = sspi->dma_tx.chan;
    hal_dma_status_t dma_ret;
    spi_master_status_t ret = 0;
    int xResult;

    xResult = hal_sem_timedwait(sspi->xSemaphore_tx, 100);//100*10ms
    if (xResult == 0)
    {
        if (SPI_XFER_OK == sspi->result)
        {
            SPI_INFO("ok\n");
            ret = SPI_MASTER_OK;
        }
        else if (SPI_XFER_FAILED == sspi->result)
        {
            SPI_ERR("[spi%d] xfer failed...\n", sspi->port);
            SPI_INFO("[spi%d] dump reg:\n", sspi->port);
            spi_dump_reg(sspi, 0, 0x60);
            ret = SPI_MASTER_ERROR;
        }
    }
    else
    {
        SPI_ERR("[spi%d] dma xfer timeout\n", sspi->port);
        SPI_INFO("[spi%d] dump reg:\n", sspi->port);
        spi_dump_reg(sspi, 0, 0x60);

        sspi->result = SPI_XFER_FAILED;

        if (hal_sem_post(sspi->xSemaphore_tx))
        {
            SPI_ERR("[spi%d] xSemaphoreGive failed.\n", sspi->port);
        }

        ret = SPI_MASTER_ERROR_TIMEOUT;
    }

end:

    dma_ret = hal_dma_stop(chan);
    if (dma_ret)
    {
        SPI_ERR("[spi%d] dma stop error! ret %d\n", sspi->port,
                dma_ret);
        return SPI_MASTER_ERROR;
    }

    dma_ret = hal_dma_chan_free(chan);
    if (dma_ret)
    {
        SPI_ERR("[spi%d] free dma_tx failed, ret %d\n", sspi->port,
                dma_ret);
        return SPI_MASTER_ERROR;
    }

    return ret;
}

static spi_master_status_t spi_dma_rx_complete(sunxi_spi_t *sspi)
{
    uint32_t flags = 0;
    uint32_t len = sspi->transfer->rx_len, size = 0;
    uint8_t *buf = sspi->transfer->rx_buf;
    struct sunxi_dma_chan *chan = sspi->dma_rx.chan;
    hal_dma_status_t dma_ret;
    spi_master_status_t ret;
    int  xResult;

#ifdef SPI_DATA_LEVEL
    unsigned int i, j;
    u8 dbuf[64], cnt = 0;
#endif

    xResult = hal_sem_timedwait(sspi->xSemaphore_rx, 100);//100*10ms
    if (xResult != 0)
    {
        SPI_ERR("[spi%d] dma xfer timeout\n", sspi->port);
        SPI_INFO("[spi%d] dump reg:\n", sspi->port);
        spi_dump_reg(sspi, 0, 0x40);

        sspi->result = SPI_XFER_FAILED;

        ret = SPI_MASTER_ERROR_TIMEOUT;
        goto end;
    }

    hal_dcache_invalidate((unsigned long)sspi->align_dma_buf, SPI_ALIGN(len, 64));
    sspi->result = SPI_XFER_OK;
    if (len <= ALIGN_DMA_BUF_SIZE)
    {
        memcpy(buf, sspi->align_dma_buf, len);
    }
    ret = SPI_MASTER_OK;
    SPI_INFO("ok\n");

#ifdef SPI_DATA_LEVEL
    SPI_INFO("rx_len = %d\n", len);
    for (i = 0; i < len; i += 16)
    {
        cnt = 0;
        cnt += sprintf(dbuf + cnt, "%03x: ", i);
        for (j = 0; ((i + j) < len) && (j < 16); j++)
            cnt += sprintf(dbuf + cnt, "%02x ",
                           ((uint8_t *)(buf))[i + j]);
        printf("%s\n", dbuf);
    }
#endif

end:
    spi_disable_irq(SPI_INT_STA_TC | SPI_INT_STA_ERR, sspi);

    dma_ret = hal_dma_stop(chan);
    if (dma_ret)
    {
        SPI_ERR("[spi%d] dma stop error! ret %d\n", sspi->port,
                dma_ret);
        ret = SPI_MASTER_ERROR;
    }

    dma_ret = hal_dma_chan_free(chan);
    if (dma_ret)
    {
        SPI_ERR("[spi%d] free dma_rx failed, ret %d\n", sspi->port,
                dma_ret);
        return SPI_MASTER_ERROR;
    }

    return ret;
}
#endif

/*
 * < 64 : cpu ;  >= 64 : dma
 * wait for done completion in this function, wakup in the irq hanlder
 */
spi_master_status_t hal_spi_xfer(hal_spi_master_port_t port,
        hal_spi_master_transfer_t *transfer)
{
    spi_master_status_t ret = 0;
    sunxi_spi_t *sspi = &g_sunxi_spi[port];

    if (NULL == transfer)
    {
        SPI_ERR("[spi%d] invalid parameter\n", sspi->port);
        ret = SPI_MASTER_INVALID_PARAMETER;
        goto end;
    }

    SPI_INFO("[spi%d] tl=%lu rl=%lu, tsl=%lu\n", sspi->port, transfer->tx_len,
             transfer->rx_len, transfer->tx_single_len);

    if ((!transfer->tx_buf && !transfer->rx_buf) ||
        (!transfer->tx_len && !transfer->rx_buf))
    {
        SPI_ERR("[spi%d] invalid parameter\n", sspi->port);
        ret = SPI_MASTER_INVALID_PARAMETER;
        goto end;
    }

#ifdef CONFIG_COMPONENTS_PM
    pm_wakelocks_acquire(&sspi->wl, PM_WL_TYPE_WAIT_INC, OS_WAIT_FOREVER);
#endif

    sspi->result = SPI_XFER_READY;
    sspi->transfer = transfer;

    if (spi_mode_check(sspi))
    {
        SPI_ERR("[spi%d] invalid parameter\n", sspi->port);
        ret = SPI_MASTER_INVALID_PARAMETER;
        goto end;
    }

    spi_clr_irq_pending(SPI_INT_STA_MASK, sspi);
    spi_disable_dma_irq(SPI_FIFO_CTL_DRQEN_MASK, sspi);
    spi_reset_fifo(sspi);
    spi_enable_irq(SPI_INTEN_TC | SPI_INTEN_ERR, sspi);

    if (spi_transfer(sspi))
    {
        ret = SPI_MASTER_ERROR;
        goto end;
    }

    switch (sspi->mode_type)
    {
        case SGLE_HALF_DUPLEX_RX:
        case DUAL_HALF_DUPLEX_RX:
        case QUAD_HALF_DUPLEX_RX:
        case FULL_DUPLEX_TX_RX:
        {
#ifndef CONFIG_SUNXI_SPI_CPU_XFER_ONLY
            /* >64 use DMA transfer, or use cpu */
            if (sspi->transfer->rx_len > BULK_DATA_BOUNDARY)
            {
                if (spi_dma_rx_complete(sspi))
                {
                    ret = SPI_MASTER_ERROR;
                    goto end;
                }
            }
            else
            {
#endif
                if (spi_cpu_complete(sspi))
                {
                    ret = SPI_MASTER_ERROR;
                    goto end;
                }
                else
                {
                    ret = SPI_MASTER_OK;
                }
#ifndef CONFIG_SUNXI_SPI_CPU_XFER_ONLY
            }
#endif
            break;
        }
        case SGLE_HALF_DUPLEX_TX:
        case DUAL_HALF_DUPLEX_TX:
        case QUAD_HALF_DUPLEX_TX:
        {
#ifndef CONFIG_SUNXI_SPI_CPU_XFER_ONLY
            /* >64 use DMA transfer, or use cpu */
            if (sspi->transfer->tx_len > BULK_DATA_BOUNDARY)
            {
                if (spi_dma_tx_complete(sspi))
                {
                    ret = SPI_MASTER_ERROR;
                    goto end;
                }
            }
            else
            {
#endif
                if (spi_cpu_complete(sspi))
                {
                    ret = SPI_MASTER_ERROR;
                    goto end;
                }
                else
                {
                    ret = SPI_MASTER_OK;
                }
#ifndef CONFIG_SUNXI_SPI_CPU_XFER_ONLY
            }
#endif
            break;
        }
        case SINGLE_FULL_DUPLEX_RX_TX:
        {
#ifndef CONFIG_SUNXI_SPI_CPU_XFER_ONLY
            /* >64 use DMA transfer, or use cpu */
            if (sspi->transfer->rx_len > BULK_DATA_BOUNDARY && sspi->transfer->tx_len > BULK_DATA_BOUNDARY)
            {
                if (spi_dma_tx_complete(sspi))
                {
                    ret = SPI_MASTER_ERROR;
                    goto end;
                }
                if (spi_dma_rx_complete(sspi))
                {
                    ret = SPI_MASTER_ERROR;
                    goto end;
                }
            }
            else
            {
#endif
                if (spi_cpu_complete(sspi))
                {
                    ret = SPI_MASTER_ERROR;
                    goto end;
                }
                else
                {
                    ret = SPI_MASTER_OK;
                }
#ifndef CONFIG_SUNXI_SPI_CPU_XFER_ONLY
            }
#endif
            break;
        }
        default:
        {
            SPI_ERR("[spi%d] invalid parameter\n", sspi->port);
            ret = SPI_MASTER_INVALID_PARAMETER;
        }
    }

end:
    if (sspi->mode_type != MODE_TYPE_NULL)
    {
        sspi->mode_type = MODE_TYPE_NULL;
    }

#ifdef CONFIG_COMPONENTS_PM
    pm_wakelocks_release(&sspi->wl);
#endif

    return ret;
}

#if 0
static int sunxi_get_spic_clk(sunxi_spi_t *sspi)
{
    unsigned long rate;

    rate = hal_clk_get_rate(sspi->mclk);
    if (!rate)
        SPI_ERR("[spi%d] clk get rate failed! return %ld\n", sspi->port, rate);
    return rate;
}
#endif

spi_master_status_t hal_spi_hw_config(hal_spi_master_port_t port, hal_spi_master_config_t *spi_config)
{
    unsigned long clock_frequency;
    sunxi_spi_t *sspi = &g_sunxi_spi[port];
    struct sunxi_spi_params_t *para;

    if (NULL == spi_config)
    {
        SPI_ERR("[spi%d] invalid parameter\n", port);
        return SPI_MASTER_INVALID_PARAMETER;
    }
#ifdef SIP_SPI0_PARAMS
    if (spi_config->sip) {
        para = &g_sunxi_spi_params_sip[port];
    } else
#endif
    {
        para = &g_sunxi_spi_params[port];
    }

    sspi->base = para->reg_base;
    sspi->port = port;
    sspi->mode_type = MODE_TYPE_NULL;

    if (spi_config->clock_frequency)
        clock_frequency = spi_config->clock_frequency;
    else
        clock_frequency = SPI_MOD_CLK;

    if (clock_frequency > SPI_MAX_FREQUENCY)
    {
        clock_frequency = SPI_MAX_FREQUENCY;
        SPI_ERR("[spi%d] invalid parameter! max_frequency is 100MHZ\n",
                sspi->port);
    }
    else
    {
        SPI_INIT("[spi%d] clock_frequency = %ldHZ\n", sspi->port,
                 clock_frequency);
    }

    if (clock_frequency >= SPI_HIGH_FREQUENCY)
    {
        spi_sample_delay(0, 1, sspi);
    }
    else if (clock_frequency <= SPI_LOW_FREQUENCY)
    {
        spi_sample_delay(1, 0, sspi);
    }
    else
    {
        spi_sample_delay(0, 0, sspi);
    }

    if (spi_clk_init(sspi, clock_frequency, para))
    {
        SPI_ERR("[spi%d] init clk %ld failed, use default clk\n", sspi->port, clock_frequency);
        spi_clk_init(sspi, SPI_MAX_FREQUENCY, para);
    }

    if (spi_config->slave_port)
    {
        SPI_ERR("[spi%d] software control cs isn't support \n",
                sspi->port);
        return SPI_MASTER_INVALID_PARAMETER;
    }
    else
    {
        spi_set_cs(spi_config->slave_port, sspi);
    }

    if (spi_config->bit_order)
    {
        spi_config_tc(SPI_LSB_FIRST_ACTIVE_, sspi);
    }

    spi_config_tc((spi_config->cpol) | (spi_config->cpha), sspi);

    spi_enable_bus(sspi);
    spi_set_master(sspi);
    spi_enable_tp(sspi);
    /*spi controller sends ss signal automatically*/
    spi_ss_owner(sspi, 0);
    /* reset fifo */
    spi_reset_fifo(sspi);

    return SPI_MASTER_OK;

}

#ifdef CONFIG_COMPONENTS_PM
static int spi_dev_suspend(struct pm_device *dev, suspend_mode_t mode)
{
    sunxi_spi_t *sspi = (sunxi_spi_t *)dev->data;
    SPI_INFO("spi_dev_suspend port %d\n", sspi->port);

    spi_disable_bus(sspi);

    hal_disable_irq(sspi->irqnum);

    if (spi_clk_exit(sspi))
    {
        SPI_ERR("[spi%d] exit clk error\n", sspi->port);
        return -1;
    }

	return 0;
}

static int spi_dev_resume(struct pm_device *dev, suspend_mode_t mode)
{
    sunxi_spi_t *sspi = (sunxi_spi_t *)dev->data;
    struct sunxi_spi_params_t *para;
    SPI_INFO("spi_dev_resume port %d\n", sspi->port);

#ifdef SIP_SPI0_PARAMS
    if (sspi->config.sip) {
        para = &g_sunxi_spi_params_sip[sspi->port];
    } else
#endif
    {
        para = &g_sunxi_spi_params[sspi->port];
    }

    hal_enable_irq(sspi->irqnum);

    spi_soft_reset(sspi);

    hal_spi_hw_config(sspi->port, &sspi->config);

	return 0;
}

static struct pm_devops spi_dev_devops = {
	.suspend = spi_dev_suspend,
	.resume = spi_dev_resume,
};
#endif

spi_master_status_t hal_spi_init(hal_spi_master_port_t port,
        hal_spi_master_config_t *cfg)
{
    sunxi_spi_t *sspi = &g_sunxi_spi[port];
    struct sunxi_spi_params_t *para;
    char irqname[32];
#ifdef CONFIG_COMPONENTS_PM
    char *devicename;
    char *wlname;
#endif

    if (port >= HAL_SPI_MASTER_MAX)
    {
        SPI_ERR("[spi%d] invalid port\n", port);
        return SPI_MASTER_ERROR;
    }

    if (sspi->used != HAL_SPI_MASTER_IDLE)
    {
        SPI_ERR("[spi%d] re-init port is in used\n", port);
        return SPI_MASTER_ERROR_BUSY;
    }

    sspi->used = HAL_SPI_MASTER_BUSY;

#ifdef SIP_SPI0_PARAMS
    if (cfg->sip) {
        para = &g_sunxi_spi_params_sip[port];
    } else
#endif
    {
        para = &g_sunxi_spi_params[port];
    }

    sspi->base = para->reg_base;
    sspi->irqnum = para->irq_num;
    sspi->port = port;
    sspi->mode_type = MODE_TYPE_NULL;
    memcpy(&sspi->config, cfg, sizeof(sspi->config));

    SPI_INFO("spi[%d] init, reg base is %lx \n", port, sspi->base);

    snprintf(irqname, 32, "spi%d", port);
    if (hal_request_irq(sspi->irqnum, spi_irq_handler, irqname, sspi) < 0)
    {
        SPI_ERR("[spi%d] request irq error\n", sspi->port);
        return SPI_MASTER_ERROR;
    }
    hal_enable_irq(sspi->irqnum);
    if (spi_pinctrl_init(sspi, cfg->sip))
    {
        SPI_ERR("[spi%d] init pinctrl error\n", sspi->port);
        return SPI_MASTER_ERROR;
    }

    spi_soft_reset(sspi);

    hal_spi_hw_config(port, cfg);

#ifndef CONFIG_SUNXI_SPI_CPU_XFER_ONLY
    sspi->xSemaphore_tx = hal_sem_create(0);
    if (sspi->xSemaphore_tx == NULL)
    {
        SPI_ERR("[spi%d] creating semaphore_tx failed.\n", sspi->port);
        return SPI_MASTER_ERROR;
    }
    sspi->xSemaphore_rx = hal_sem_create(0);
    if (sspi->xSemaphore_rx == NULL)
    {
        SPI_ERR("[spi%d] creating semaphore_rx failed.\n", sspi->port);
        return SPI_MASTER_ERROR;
    }
    sspi->align_dma_buf = dma_alloc_coherent(ALIGN_DMA_BUF_SIZE);
    if (!sspi->align_dma_buf)
    {
        SPI_ERR("[spi%d] alloc memory failed\n", sspi->port);
        return SPI_MASTER_ERROR_NOMEM;
    }
    SPI_INIT("[spi%d] DMA xfer enable\n", sspi->port);
#else
    SPI_INIT("[spi%d] CPU xfer only\n", sspi->port);
#endif

#ifdef CONFIG_COMPONENTS_PM
    wlname = hal_malloc(32);
    snprintf(wlname, 32, "spi%d_wakelock", port);
    sspi->wl.name = wlname;
    sspi->wl.ref = 0;

    devicename = hal_malloc(32);
    snprintf(devicename, 32, "spi%d_dev", port);
    sspi->pm.name = devicename;
    sspi->pm.ops = &spi_dev_devops;
    sspi->pm.data = sspi;

    pm_devops_register(&sspi->pm);
#endif

    return SPI_MASTER_OK;
}

spi_master_status_t hal_spi_deinit(hal_spi_master_port_t port)
{
    sunxi_spi_t *sspi = &g_sunxi_spi[port];

    if (sspi->used != HAL_SPI_MASTER_BUSY)
    {
        SPI_ERR("[spi%d] un-used port cannot deinit\n", sspi->port);
        return SPI_MASTER_ERROR;
    }

    sspi->used = HAL_SPI_MASTER_IDLE;

#ifdef CONFIG_COMPONENTS_PM
    pm_devops_unregister(&sspi->pm);
    hal_free((void *)sspi->wl.name);
    hal_free((void *)sspi->pm.name);
#endif

    spi_disable_bus(sspi);

    hal_disable_irq(sspi->irqnum);

#ifndef CONFIG_SUNXI_SPI_CPU_XFER_ONLY
    dma_free_coherent(sspi->align_dma_buf);
    hal_sem_delete(sspi->xSemaphore_tx);
    hal_sem_delete(sspi->xSemaphore_rx);
#endif

    if (spi_clk_exit(sspi))
    {
        SPI_ERR("[spi%d] exit clk error\n", sspi->port);
        return SPI_MASTER_ERROR;
    }

    hal_free_irq(sspi->irqnum);

    return SPI_MASTER_OK;
}

spi_master_status_t hal_spi_write(hal_spi_master_port_t port,
        void *buf, uint32_t size)
{
    spi_master_status_t ret;
    hal_spi_master_transfer_t tr;

    tr.tx_buf = buf;
    tr.tx_len = size;
    tr.rx_buf = NULL;
    tr.rx_len = 0;
    tr.dummy_byte = 0;
    tr.tx_single_len = size;
    tr.tx_nbits = SPI_NBITS_SINGLE;

    SPI_INFO("spi[%d] write data,len is %ld \n", port, size);
    ret = hal_spi_xfer(port, &tr);
    return ret;
}

spi_master_status_t hal_spi_read(hal_spi_master_port_t port,
        void *buf, uint32_t size)
{
    spi_master_status_t ret;
    hal_spi_master_transfer_t tr;

    tr.rx_buf = buf;
    tr.rx_len = size;
    tr.tx_buf = NULL;
    tr.tx_len = 0;
    tr.dummy_byte = 0;
    tr.tx_single_len = size;
    tr.rx_nbits = SPI_NBITS_SINGLE;

    SPI_INFO("spi[%d] read data,len is %ld \n", port, size);
    ret = hal_spi_xfer(port, &tr);
    return ret;
}

#ifdef CONFIG_DRIVERS_SPI_PANIC_TRANSFER
static spi_master_status_t spi_panic_transfer(sunxi_spi_t *sspi)
{
    switch (sspi->mode_type)
    {
        case SGLE_HALF_DUPLEX_RX:
        case DUAL_HALF_DUPLEX_RX:
        case QUAD_HALF_DUPLEX_RX:
        {
            SPI_INFO("[spi%d] rx by cpu\n", sspi->port);
            spi_clr_irq_pending(SPI_INT_STA_MASK, sspi);
            spi_start_xfer(sspi);
            spi_cpu_read(sspi);
            break;
        }
        case SGLE_HALF_DUPLEX_TX:
        case DUAL_HALF_DUPLEX_TX:
        case QUAD_HALF_DUPLEX_TX:
        {
            SPI_INFO("[spi%d] tx by cpu\n", sspi->port);
            spi_start_xfer(sspi);
            spi_cpu_write(sspi);
            break;
        }
        case SINGLE_FULL_DUPLEX_RX_TX:
        {
            SPI_INFO("[spi%d] tx and rx by cpu\n", sspi->port);
            spi_clr_irq_pending(SPI_INT_STA_MASK, sspi);
            spi_start_xfer(sspi);
            spi_cpu_write_read(sspi);
            break;
        }
        case FULL_DUPLEX_TX_RX:
        {
            SPI_INFO("[spi%d] tx and rx by cpu\n", sspi->port);
            spi_start_xfer(sspi);
            spi_cpu_write(sspi);
            spi_cpu_read(sspi);
            break;
        }
        default:
        {
            SPI_ERR("[spi%d] invalid parameter\n", sspi->port);
            return SPI_MASTER_INVALID_PARAMETER;
        }
    }

    return SPI_MASTER_OK;
}

/*
 * < 64 : cpu ;  >= 64 : dma
 * wait for done completion in this function, wakup in the irq hanlder
 */
spi_master_status_t hal_spi_panic_xfer(hal_spi_master_port_t port,
        hal_spi_master_transfer_t *transfer)
{
    uint32_t flags = 0;
    spi_master_status_t ret = 0;
    sunxi_spi_t *sspi = &g_sunxi_spi[port];

    if (NULL == transfer)
    {
        SPI_ERR("[spi%d] invalid parameter\n", sspi->port);
        ret = SPI_MASTER_INVALID_PARAMETER;
        goto end;
    }

    SPI_INFO("[spi%d] tl=%lu rl=%lu, tsl=%lu\n", sspi->port, transfer->tx_len,
             transfer->rx_len, transfer->tx_single_len);

    if ((!transfer->tx_buf && !transfer->rx_buf) ||
        (!transfer->tx_len && !transfer->rx_buf))
    {
        SPI_ERR("[spi%d] invalid parameter\n", sspi->port);
        ret = SPI_MASTER_INVALID_PARAMETER;
        goto end;
    }

    sspi->result = SPI_XFER_READY;
    sspi->transfer = transfer;

    if (spi_mode_check(sspi))
    {
        SPI_ERR("[spi%d] invalid parameter\n", sspi->port);
        ret = SPI_MASTER_INVALID_PARAMETER;
        goto end;
    }

    spi_clr_irq_pending(SPI_INT_STA_MASK, sspi);
    spi_disable_dma_irq(SPI_FIFO_CTL_DRQEN_MASK, sspi);
    spi_reset_fifo(sspi);
    spi_enable_irq(SPI_INTEN_TC | SPI_INTEN_ERR, sspi);

    if (spi_panic_transfer(sspi))
    {
        ret = SPI_MASTER_ERROR;
        goto end;
    }

    switch (sspi->mode_type)
    {
        case SGLE_HALF_DUPLEX_RX:
        case DUAL_HALF_DUPLEX_RX:
        case QUAD_HALF_DUPLEX_RX:
        case FULL_DUPLEX_TX_RX:
        {
            if (spi_cpu_complete(sspi))
            {
                ret = SPI_MASTER_ERROR;
                goto end;
            }
            else
            {
                ret = SPI_MASTER_OK;
            }
            break;
        }
        case SGLE_HALF_DUPLEX_TX:
        case DUAL_HALF_DUPLEX_TX:
        case QUAD_HALF_DUPLEX_TX:
        {
            if (spi_cpu_complete(sspi))
            {
                ret = SPI_MASTER_ERROR;
                goto end;
            }
            else
            {
                ret = SPI_MASTER_OK;
            }
            break;
        }
        case SINGLE_FULL_DUPLEX_RX_TX:
        {
            if (spi_cpu_complete(sspi))
            {
                ret = SPI_MASTER_ERROR;
                goto end;
            }
            else
            {
                ret = SPI_MASTER_OK;
            }
            break;
        }
        default:
        {
            SPI_ERR("[spi%d] invalid parameter\n", sspi->port);
            ret = SPI_MASTER_INVALID_PARAMETER;
        }
    }
end:
    if (sspi->mode_type != MODE_TYPE_NULL)
    {
        sspi->mode_type = MODE_TYPE_NULL;
    }

    return ret;
}
#endif

