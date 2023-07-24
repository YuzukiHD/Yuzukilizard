/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the people's Republic of China and other countries.
* All Allwinner Technology Co.,Ltd. trademarks are used with permission.
*
* DISCLAIMER
* THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
* IF YOU NEED TO INTEGRATE THIRD PARTY'S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
* IN ALLWINNERS'SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
* ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
* ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
* COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
* YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY'S TECHNOLOGY.
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
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include <hal_log.h>
#include <hal_cmd.h>
#include <hal_mem.h>
#include <hal_timer.h>
#include <sunxi_hal_spi.h>

#define TEST_READ 0
#define TEST_WRITE 1

static int cmd_test_spi(int argc, const char **argv)
{
    hal_spi_master_port_t  port;
    hal_spi_master_config_t cfg;
    char tbuf[10]={0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20};
    char rbuf[10];
    char rw = 1;
    int c;

    if (argc < 3)
    {
        printf("Usage:\n");
        printf("\thal_spi <port> <-r|-w>\n");
        return -1;
    }

    printf("Run spi test\n");

    port = strtol(argv[1], NULL, 0);
    while ((c = getopt(argc, (char *const *)argv, "r:w")) != -1)
    {
        switch (c)
        {
            case 'r':
                rw = TEST_READ;
                break;
            case 'w':
                rw = TEST_WRITE;
                break;
        }
    }

    cfg.clock_frequency = 5000000;
    cfg.slave_port = HAL_SPI_MASTER_SLAVE_0;
    cfg.cpha = HAL_SPI_MASTER_CLOCK_PHASE0;
    cfg.cpol = HAL_SPI_MASTER_CLOCK_POLARITY0;
    cfg.sip = 0;
    cfg.flash = 0;
    hal_spi_init(port, &cfg);
    if (rw == TEST_READ)
    {
        hal_spi_read(port, rbuf, 10);
        printf("rbuf: %s\n", rbuf);
    }
    else if (rw == TEST_WRITE)
    {
        hal_spi_write(port, tbuf, 10);
    }

    printf("Spi test finish\n");

    hal_spi_deinit(port);

    return 0;
}

FINSH_FUNCTION_EXPORT_CMD(cmd_test_spi, hal_spi, spi hal APIs tests)

static int cmd_test_spi_quad(int argc, const char **argv)
{
    hal_spi_master_port_t  port;
    hal_spi_master_config_t cfg;
    char tbuf[10]={0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20};
    char rbuf[10];

    if (argc < 2)
    {
        printf("Usage:\n");
        printf("\thal_spi_quad <port> \n");
        return -1;
    }

    printf("Run spi quad test\n");

    port = strtol(argv[1], NULL, 0);

    cfg.clock_frequency = 5000000;
    cfg.slave_port = HAL_SPI_MASTER_SLAVE_0;
    cfg.cpha = HAL_SPI_MASTER_CLOCK_PHASE0;
    cfg.cpol = HAL_SPI_MASTER_CLOCK_POLARITY0;
    cfg.sip = 0;
    cfg.flash = 0;
    hal_spi_init(port, &cfg);
    hal_spi_master_transfer_t tr;
    tr.tx_buf = (uint8_t *)tbuf;
    tr.tx_len = 10;
    tr.rx_buf = (uint8_t *)rbuf;
    tr.rx_len = 0;
    tr.tx_nbits = SPI_NBITS_QUAD;
    tr.tx_single_len = 10;
    tr.dummy_byte = 0;
    hal_spi_xfer(port, &tr);

    printf("Spi test finish\n");

    hal_spi_deinit(port);

    return 0;
}

FINSH_FUNCTION_EXPORT_CMD(cmd_test_spi_quad, hal_spi_quad, spi hal APIs tests)

static int cmd_test_spi_loop(int argc, const char **argv)
{
    hal_spi_master_port_t  port;
    hal_spi_master_config_t cfg;
    hal_spi_master_transfer_t tr;
    char *tbuf = NULL;
    char *rbuf = NULL;
    int size = 0;
    int i, j, loop;
    struct timeval start, end;
    int sucess = 0, failed = 0;
    unsigned long time = 0;
	double tr_speed = 0.0f;

    if (argc < 2)
    {
        printf("Usage:\n");
        printf("\t%s init <port> <freq>\n", argv[0]);
        printf("\t%s deinit <port>\n", argv[0]);
        printf("\t%s loop_test <port> <size> <loop_times>\n", argv[0]);
        return -1;
    }

    printf("Run spi loop test\n");

    port = strtol(argv[2], NULL, 0);

    if (!strcmp(argv[1], "init"))
    {
        cfg.clock_frequency = strtol(argv[3], NULL, 0);
        cfg.slave_port = HAL_SPI_MASTER_SLAVE_0;
        cfg.cpha = HAL_SPI_MASTER_CLOCK_PHASE0;
        cfg.cpol = HAL_SPI_MASTER_CLOCK_POLARITY0;
        cfg.sip = 0;
        cfg.flash = 0;
        hal_spi_init(port, &cfg);
    }
    else if (!strcmp(argv[1], "deinit"))
    {
        hal_spi_deinit(port);
    }
    else if (!strcmp(argv[1], "loop_test"))
    {
        size = strtol(argv[3], NULL, 0);
        loop = strtol(argv[4], NULL, 0);
        tbuf = hal_malloc(size);
        rbuf = hal_malloc(size);
        if (!(tbuf && rbuf))
        {
            printf("Request buffer size %d failed\n", size);
            return 0;
        }

        /* Init buffer data */
        for (i = 0; i < size; i++)
        {
            tbuf[i] = i & 0xFF;
        }

        tr.tx_buf = (uint8_t *)tbuf;
        tr.tx_len = size;
        tr.tx_nbits = SPI_NBITS_SINGLE;
        tr.tx_single_len = size;
        tr.rx_buf = (uint8_t *)rbuf;
        tr.rx_len = size;
        tr.rx_nbits = SPI_NBITS_SINGLE;

        for (i = 0; i < loop; i++)
        {
            printf("loop test round %d\n", i);

            if (tr.tx_len <= 32)
            {
                printf("tbuf: ");
                for (j = 0; j < tr.tx_len; j++)
                {
                    printf("%02X ", tr.tx_buf[j]);
                }
                printf("\n");
            }

            memset(tr.rx_buf, 0, tr.rx_len);
            gettimeofday(&start, NULL);
            hal_spi_xfer(port, &tr);
            gettimeofday(&end, NULL);

            if (tr.rx_len <= 32)
            {
                printf("rbuf: ");
                for (j = 0; j < tr.rx_len; j++)
                {
                    printf("%02X ", tr.rx_buf[j]);
                }
                printf("\n");
            }

            if (!memcmp(tr.tx_buf, tr.rx_buf, size))
            {
                sucess++;
                time += (1000000 * (end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec);
                printf("compare with tbuf rbuf %d : pass\n", size);
            }
            else
            {
                failed++;
                printf("compare with tbuf rbuf %d : failed\n", size);
            }

            hal_msleep(5);
        }

        hal_free((void *)tbuf);
        hal_free((void *)rbuf);

        printf("compare buffer total %d : sucess %d, failed %d\n", loop, sucess, failed);
        tr_speed = ((double)(size*sucess) / 1024.0f / 1024.0f) / ((double)time / 1000.0f / 1000.0f);
        printf("Transfer %d take %ld us (%lf MB/s)\n", size*sucess, time, tr_speed);
    }

    printf("Spi test finish\n");

    return 0;
}

FINSH_FUNCTION_EXPORT_CMD(cmd_test_spi_loop, hal_spi_loop, spi hal APIs tests)
