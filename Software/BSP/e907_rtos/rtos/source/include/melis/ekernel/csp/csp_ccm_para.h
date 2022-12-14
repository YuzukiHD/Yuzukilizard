/*
**********************************************************************************************************************
*                                                   Melis
*                          the Easy Portable/Player Operation System
*
*                                (c) Copyright 2010-2030
*                               All Rights Reserved
*
* By      : WangGang
* Version : v1.00
* Data    : 2016.1.21
* Note    : CCMU function  for SUN3IW2
**********************************************************************************************************************
*/
#ifndef _CSP_CCM_PARA_H_
#define _CSP_CCM_PARA_H_
#include <typedef.h>
#include <kconfig.h>

#if defined CONFIG_DRIVERS_SUNXI_CLK
typedef enum
{
    CSP_CCM_SYS_CLK_TYPE_ROOT     = -1,
    CSP_CCM_SYS_CLK_TYPE_FIXED_SRC,
    CSP_CCM_SYS_CLK_TYPE_FIXED_FACTOR,
    CSP_CCM_SYS_CLK_TYPE_FACTOR,
    CSP_CCM_SYS_CLK_TYPE_PERIPH,
} sys_clk_type_t;

#define  CSP_CCM_SYS_CLK_GET_RATE_NOCACHE       (ture)
#define  CSP_CCM_SYS_CLK_GET_RATE_CACHE         (false)
#define  CSP_CCM_SYS_CLK_RATE_UNINITIALIZED     (0)
#define  CSP_CCM_SYS_CLK_RATE_ROOT              (0)

/************************************************************************************************
* Enum hal_clk_id_t
* @Description: This enum defines the Clock-id that CCMU HAL support
* # Clock-id defines as Bitmap 32bits
* #|PERIPH CLOCK   |FACTOR PLL CLOCK  |FIXED FACTOR CLOCK     |FIXED SOURCE CLOCK  |
* #|768~1023       |512~767           |256~511                |0~255               |
*************************************************************************************************/
#define CSP_CCM_SYS_CLK_SECTION                 (256)
#define CSP_CCM_SYS_CLK_FIXED_SRC_OFFSET        (CSP_CCM_SYS_CLK_TYPE_FIXED_SRC*CSP_CCM_SYS_CLK_SECTION)
#define CSP_CCM_SYS_CLK_FIXED_FACTOR_OFFSET     (CSP_CCM_SYS_CLK_TYPE_FIXED_FACTOR*CSP_CCM_SYS_CLK_SECTION)
#define CSP_CCM_SYS_CLK_FACTOR_OFFSET           (CSP_CCM_SYS_CLK_TYPE_FACTOR*CSP_CCM_SYS_CLK_SECTION)
#define CSP_CCM_SYS_CLK_PERIPH_OFFSET           (CSP_CCM_SYS_CLK_TYPE_PERIPH*CSP_CCM_SYS_CLK_SECTION)
#define CSP_CCM_SYS_CLK_GET_TYPE(clk_id)        (clk_id/CSP_CCM_SYS_CLK_SECTION)

typedef enum _CSP_CCM_SYS_CLK
{
    CSP_CCM_SYS_CLK_UNINITIALIZED       = -1,
    CSP_CCM_SYS_CLK_ROOT,
    /*
    *   FIXED SOURCE CLOCK 0~255
    */
    CSP_CCM_SYS_CLK_HOSC24M     = CSP_CCM_SYS_CLK_FIXED_SRC_OFFSET,
    CSP_CCM_SYS_CLK_HOSC24MD2,
    CSP_CCM_SYS_CLK_IOSC16M,
    CSP_CCM_SYS_CLK_OSC48M,
    CSP_CCM_SYS_CLK_OSC48MD4,
    CSP_CCM_SYS_CLK_LOSC,
    CSP_CCM_SYS_CLK_RC16M,
    CSP_CCM_SYS_CLK_PERI0DIV25M_PLL,
    /*
    *   FIXED FACTOR CLOCK 256~511
    */
    CSP_CCM_SYS_CLK_PERI0X2_PLL         = CSP_CCM_SYS_CLK_FIXED_FACTOR_OFFSET,
    CSP_CCM_SYS_CLK_PERI1X2_PLL,
    CSP_CCM_SYS_CLK_AUDIOX2_PLL,
    CSP_CCM_SYS_CLK_AUDIOX4_PLL,
    CSP_CCM_SYS_CLK_VIDEOX4_PLL,
    CSP_CCM_SYS_CLK_DDRDIV4_PLL,
    /*
    *   FACTOR CLOCK 512~1023
    */
    CSP_CCM_SYS_CLK_CPUX_C0_PLL     =  CSP_CCM_SYS_CLK_FACTOR_OFFSET,
    CSP_CCM_SYS_CLK_CPUX_C1_PLL,
    CSP_CCM_SYS_CLK_DDR0_PLL,
    CSP_CCM_SYS_CLK_DDR1_PLL,
    CSP_CCM_SYS_CLK_32K_PLL,
    CSP_CCM_SYS_CLK_PERI0_PLL,
    CSP_CCM_SYS_CLK_PERI1_PLL,
    CSP_CCM_SYS_CLK_GPU0_PLL,
    CSP_CCM_SYS_CLK_GPU1_PLL,
    CSP_CCM_SYS_CLK_VIDEO0_PLL,
    CSP_CCM_SYS_CLK_VIDEO1_PLL,
    CSP_CCM_SYS_CLK_VIDEO2_PLL,
    CSP_CCM_SYS_CLK_VE_PLL,
    CSP_CCM_SYS_CLK_DE_PLL,
    CSP_CCM_SYS_CLK_ISP_PLL,
    CSP_CCM_SYS_CLK_HSIC_PLL,
    CSP_CCM_SYS_CLK_AUDIO_PLL,
    CSP_CCM_SYS_CLK_VIDEO_PLL,
    CSP_CCM_SYS_CLK_MIPI_PLL,
    CSP_CCM_SYS_CLK_HDMI_PLL,
    CSP_CCM_SYS_CLK_USB_PLL,
    CSP_CCM_SYS_CLK_EDP_PLL,
    CSP_CCM_SYS_CLK_SATA_PLL,
    CSP_CCM_SYS_CLK_ADC_PLL,
    CSP_CCM_SYS_CLK_DTMB_PLL,
    CSP_CCM_SYS_CLK_24M_PLL,
    CSP_CCM_SYS_CLK_EVE_PLL,
    CSP_CCM_SYS_CLK_CVE_PLL,
    CSP_CCM_SYS_CLK_ISE_PLL,
    CSP_CCM_SYS_CLK_CSI_PLL,

    /*
        PERIPH MODULE CLOCK 1024~2047
    */
    CSP_CCM_MOD_CLK_BUS_C0_CPU  = CSP_CCM_SYS_CLK_PERIPH_OFFSET,
    CSP_CCM_MOD_CLK_BUS_C1_CPU,
    CSP_CCM_MOD_CLK_BUS_C0_AXI,
    CSP_CCM_MOD_CLK_BUS_C1_AXI,
    CSP_CCM_MOD_CLK_BUS_CPUAPB,
    CSP_CCM_MOD_CLK_BUS_PSI,
    CSP_CCM_MOD_CLK_BUS_AHB1,
    CSP_CCM_MOD_CLK_BUS_AHB2,
    CSP_CCM_MOD_CLK_BUS_AHB3,
    CSP_CCM_MOD_CLK_BUS_APB1,
    CSP_CCM_MOD_CLK_BUS_APB2,
    CSP_CCM_MOD_CLK_BUS_CCI400,
    CSP_CCM_MOD_CLK_BUS_MBUS,
    CSP_CCM_MOD_CLK_DMA,
    CSP_CCM_MOD_CLK_DE,
    CSP_CCM_MOD_CLK_EE,
    CSP_CCM_MOD_CLK_DI,
    CSP_CCM_MOD_CLK_G2D,
    CSP_CCM_MOD_CLK_EDMA,
    CSP_CCM_MOD_CLK_EVE,
    CSP_CCM_MOD_CLK_CVE,
    CSP_CCM_MOD_CLK_GPU,
    CSP_CCM_MOD_CLK_CE,
    CSP_CCM_MOD_CLK_VE,
    CSP_CCM_MOD_CLK_EISE,
    CSP_CCM_MOD_CLK_NNA,
    CSP_CCM_MOD_CLK_NNA_RST,
    CSP_CCM_MOD_CLK_MSGBOX,
    CSP_CCM_MOD_CLK_SPINLOCK,
    CSP_CCM_MOD_CLK_HSTIMER,
    CSP_CCM_MOD_CLK_AVS,
    CSP_CCM_MOD_CLK_DBGSYS,
    CSP_CCM_MOD_CLK_PWM,
    CSP_CCM_MOD_CLK_IOMMU,
    CSP_CCM_MOD_CLK_GPIO,
    CSP_CCM_MOD_CLK_DRAM,
    CSP_CCM_MOD_CLK_NAND0,
    CSP_CCM_MOD_CLK_NAND1,
    CSP_CCM_MOD_CLK_SDMMC0_MOD,
    CSP_CCM_MOD_CLK_SDMMC0_RST,
    CSP_CCM_MOD_CLK_SDMMC0_BUS,
    CSP_CCM_MOD_CLK_SDMMC1_MOD,
    CSP_CCM_MOD_CLK_SDMMC1_RST,
    CSP_CCM_MOD_CLK_SDMMC2_BUS,
    CSP_CCM_MOD_CLK_SDMMC2_MOD,
    CSP_CCM_MOD_CLK_SDMMC2_RST,
    CSP_CCM_MOD_CLK_SDMMC1_BUS,
    CSP_CCM_MOD_CLK_SMHC3,
    CSP_CCM_MOD_CLK_SMHC4,
    CSP_CCM_MOD_CLK_SMHC5,
    CSP_CCM_MOD_CLK_UART0,
    CSP_CCM_MOD_CLK_UART1,
    CSP_CCM_MOD_CLK_UART2,
    CSP_CCM_MOD_CLK_UART3,
    CSP_CCM_MOD_CLK_UART4,
    CSP_CCM_MOD_CLK_UART5,
    CSP_CCM_MOD_CLK_UART6,
    CSP_CCM_MOD_CLK_UART7,
    CSP_CCM_MOD_CLK_TWI0,
    CSP_CCM_MOD_CLK_TWI1,
    CSP_CCM_MOD_CLK_TWI2,
    CSP_CCM_MOD_CLK_TWI3,
    CSP_CCM_MOD_CLK_TWI4,
    CSP_CCM_MOD_CLK_CAN0,
    CSP_CCM_MOD_CLK_CAN1,
    CSP_CCM_MOD_CLK_CAN2,
    CSP_CCM_MOD_CLK_SCR0,
    CSP_CCM_MOD_CLK_SCR1,
    CSP_CCM_MOD_CLK_SCR2,
    CSP_CCM_MOD_CLK_SCR3,
    CSP_CCM_MOD_CLK_SPI0,
    CSP_CCM_MOD_CLK_SPI1,
    CSP_CCM_MOD_CLK_SPI2,
    CSP_CCM_MOD_CLK_SPI3,
    CSP_CCM_MOD_CLK_SPI4,
    CSP_CCM_MOD_CLK_SPI5,
    CSP_CCM_MOD_CLK_SPI6,
    CSP_CCM_MOD_CLK_SPI7,
    CSP_CCM_MOD_CLK_THS,
    CSP_CCM_MOD_CLK_GMAC,
    CSP_CCM_MOD_CLK_EPHY,
    CSP_CCM_MOD_CLK_EMAC,
    CSP_CCM_MOD_CLK_SATA,
    CSP_CCM_MOD_CLK_TS0,
    CSP_CCM_MOD_CLK_TS1,
    CSP_CCM_MOD_CLK_IRTX,
    CSP_CCM_MOD_CLK_KEYPAD,
    CSP_CCM_MOD_CLK_GPADC,
    CSP_CCM_MOD_CLK_LEDC,
    CSP_CCM_MOD_CLK_PIO,
    CSP_CCM_MOD_CLK_MAD,
    CSP_CCM_MOD_CLK_LPSD,
    CSP_CCM_MOD_CLK_DTMB,
    CSP_CCM_MOD_CLK_I2S0,
    CSP_CCM_MOD_CLK_I2S1,
    CSP_CCM_MOD_CLK_I2S2,
    CSP_CCM_MOD_CLK_SPDIF,
    CSP_CCM_MOD_CLK_DSD,
    CSP_CCM_MOD_CLK_DMIC,
    CSP_CCM_MOD_CLK_AUDIOCODEC_1X,
    CSP_CCM_MOD_CLK_AUDIOCODEC_4X,
    CSP_CCM_MOD_CLK_WLAN,
    CSP_CCM_MOD_CLK_USB0,
    CSP_CCM_MOD_CLK_USB1,
    CSP_CCM_MOD_CLK_USB2,
    CSP_CCM_MOD_CLK_USB3,
    CSP_CCM_MOD_CLK_USBOHCI0,
    CSP_CCM_MOD_CLK_USBOHCI0_12M,
    CSP_CCM_MOD_CLK_USBOHCI1,
    CSP_CCM_MOD_CLK_USBOHCI1_12M,
    CSP_CCM_MOD_CLK_USBEHCI0,
    CSP_CCM_MOD_CLK_USBEHCI1,
    CSP_CCM_MOD_CLK_USBOTG,
    CSP_CCM_MOD_CLK_HDMI0,
    CSP_CCM_MOD_CLK_HDMI1,
    CSP_CCM_MOD_CLK_HDMI2,
    CSP_CCM_MOD_CLK_HDMI3,
    CSP_CCM_MOD_CLK_MIPI_DSI0,
    CSP_CCM_MOD_CLK_MIPI_DPHY0,
    CSP_CCM_MOD_CLK_MIPI_HOST0,
    CSP_CCM_MOD_CLK_MIPI_DSI1,
    CSP_CCM_MOD_CLK_MIPI_HOST1,
    CSP_CCM_MOD_CLK_MIPI_DSI2,
    CSP_CCM_MOD_CLK_MIPI_HOST2,
    CSP_CCM_MOD_CLK_MIPI_DSI3,
    CSP_CCM_MOD_CLK_MIPI_HOST3,
    CSP_CCM_MOD_CLK_MIPI_DSC,
    CSP_CCM_MOD_CLK_DISPLAY_TOP,
    CSP_CCM_MOD_CLK_TCON_LCD0,
    CSP_CCM_MOD_CLK_TCON_LCD1,
    CSP_CCM_MOD_CLK_TCON_LCD2,
    CSP_CCM_MOD_CLK_TCON_LCD3,
    CSP_CCM_MOD_CLK_TCON_TV0,
    CSP_CCM_MOD_CLK_TCON_TV1,
    CSP_CCM_MOD_CLK_TCON_TV2,
    CSP_CCM_MOD_CLK_TCON_TV3,
    CSP_CCM_MOD_CLK_TVE0,
    CSP_CCM_MOD_CLK_TVE1,
    CSP_CCM_MOD_CLK_LVDS,
    CSP_CCM_MOD_CLK_TVD0,
    CSP_CCM_MOD_CLK_TVD1,
    CSP_CCM_MOD_CLK_TVD2,
    CSP_CCM_MOD_CLK_TVD3,
    CSP_CCM_MOD_CLK_TVD4,
    CSP_CCM_MOD_CLK_TVD5,
    CSP_CCM_MOD_CLK_EDP,
    CSP_CCM_MOD_CLK_CSI0,
    CSP_CCM_MOD_CLK_CSI1,
    CSP_CCM_MOD_CLK_MIPI_CSI,
    CSP_CCM_MOD_CLK_SUB_LVDS,
    CSP_CCM_MOD_CLK_HISP,
    CSP_CCM_MOD_CLK_CSI_TOP,
    CSP_CCM_MOD_CLK_CSI_MASTER0,
    CSP_CCM_MOD_CLK_CSI_MASTER1,
    CSP_CCM_MOD_CLK_ISP,
    CSP_CCM_MOD_CLK_DSPO,
} CSP_CCM_sysClkNo_t, sys_clk_id_t, *sys_clk_id_pt;

#define CSP_CCM_MOD_RST_BUS_VE          0

#elif defined CONFIG_DRIVERS_SUNXI_CCU

#define  CSP_CCM_SYS_CLK_GET_RATE_NOCACHE       (ture)
#define  CSP_CCM_SYS_CLK_GET_RATE_CACHE         (false)
#define  CSP_CCM_SYS_CLK_RATE_UNINITIALIZED     (0)
#define  CSP_CCM_SYS_CLK_RATE_ROOT              (0)

/************************************************************************************************
* Enum hal_clk_type_t
* @Description: This enum defines the type of Clock
*************************************************************************************************/
typedef enum
{
    CSP_CCM_SUNXI_UNKOWN_CCU    = -1,
    CSP_CCM_SUNXI_FIXED_CCU     = 0,
    CSP_CCM_SUNXI_RTC_CCU       ,
    CSP_CCM_SUNXI_CCU           ,
    CSP_CCM_SUNXI_R_CCU         ,
    CSP_CCM_SUNXI_CCU_NUMBER    ,
} sys_clk_type_t;

typedef uint32_t CSP_CCM_sysClkNo_t;
typedef uint32_t sys_clk_id_t;

/*unify the FIXED_CCU type and CCU type with same set of encode*/
#define CSP_CCM_SYS_CLK_UNINITIALIZED           -1

#define CSP_CCM_SYS_CLK_HOSC24M                 0
#define CSP_CCM_SYS_CLK_LOSC                    1
#define CSP_CCM_SYS_CLK_MAX                     2

#define CSP_CCM_SYS_CLK_OSC12M                  (0   + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_SYS_CLK_PLL_CPUX                (1   + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_SYS_CLK_PLL_DDR0                (2   + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_SYS_CLK_PLL_PERIPH0_PARENT      (3   + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_SYS_CLK_PLL_PERIPH0             (4   + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_SYS_CLK_PLL_PERIPH0_2x          (5   + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_SYS_CLK_PLL_PERIPH0_800M        (6   + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_SYS_CLK_PLL_PERIPH0_DIV3        (7   + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_SYS_CLK_PLL_VIDEO0              (8   + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_SYS_CLK_PLL_VIDEO0_2X           (9   + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_SYS_CLK_PLL_VIDEO0_4X           (10  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_SYS_CLK_PLL_VIDEO1              (11  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_SYS_CLK_PLL_VIDEO1_2X           (12  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_SYS_CLK_PLL_VIDEO1_4X           (13  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_SYS_CLK_PLL_VE                  (14  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_SYS_CLK_PLL_AUDIO0              (15  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_SYS_CLK_PLL_AUDIO0_2X           (16  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_SYS_CLK_PLL_AUDIO0_4X           (17  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_SYS_CLK_PLL_AUDIO1              (18  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_SYS_CLK_PLL_AUDIO1_DIV2         (19  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_SYS_CLK_PLL_AUDIO1_DIV5         (20  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_SYS_CLK_PLL_CPUX_DIV            (21  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_CPUX                    (22  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_AXI                     (23  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_APB                     (24  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_PSI_AHB                 (25  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_APB0                    (26  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_APB1                    (27  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_MBUS                    (28  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_DE0                     (29  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_DE0                 (30  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_DI                      (31  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_DI                  (32  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_G2D                     (33  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_G2D                 (34  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_CE                      (35  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_CE                  (36  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_VE                      (37  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_VE                  (38  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_DMA                 (39  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_MSGBOX0             (40  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_MSGBOX1             (41  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_MSGBOX2             (42  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_SPINLOCK            (43  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_HSTIMER             (44  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_AVS                     (45  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_DBG                 (46  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_PWM                 (47  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_IOMMU               (48  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_DRAM                    (49  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_MBUS_DMA                (50  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_MBUS_VE                 (51  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_MBUS_CE                 (52  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_MBUS_TVIN               (53  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_MBUS_CSI                (54  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_MBUS_G2D                (55  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_DRAM                (56  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_MMC0                    (57  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_MMC1                    (58  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_MMC2                    (59  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_MMC0                (60  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_MMC1                (61  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_MMC2                (62  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_UART0               (63  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_UART1               (64  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_UART2               (65  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_UART3               (66  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_UART4               (67  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_UART5               (68  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_I2C0                (69  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_I2C1                (70  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_I2C2                (71  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_I2C3                (72  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_CAN0                (73  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_CAN1                (74  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_SPI0                    (75  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_SPI1                    (76  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_SPI0                (77  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_SPI1                (78  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_EMAC0_25M               (79  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_EMAC0               (80  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_IR_TX                   (81  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_IR_TX               (82  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_GPADC               (83  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_THS                 (84  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_I2S0                    (85  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_I2S1                    (86  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_I2S2                    (87  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_I2S2_ASRC               (88  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_I2S0                (89  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_I2S1                (90  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_I2S2                (91  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_SPDIF_TX                (92  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_SPDIF_RX                (93  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_SPDIF               (94  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_DMIC                    (95  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_DMIC                (96  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_AUDIO_DAC               (97  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_AUDIO_ADC               (98  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_AUDIO_CODEC         (99  + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_USB_OHCI0               (100 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_USB_OHCI1               (101 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_OHCI0               (102 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_OHCI1               (103 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_EHCI0               (104 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_EHCI1               (105 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_OTG                 (106 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_LRADC               (107 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_DPSS_TOP0           (108 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_HDMI_24M                (109 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_HDMI_CEC                (110 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_HDMI_CEC_32K            (111 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_HDMI                (112 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_MIPI_DSI                (113 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_MIPI_DSI            (114 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_TCON_LCD0               (115 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_TCON_LCD0           (116 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_TCON_TV                 (117 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_TCON_TV             (118 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_TVE                     (119 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_TVE                 (120 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_TVE_TOP             (121 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_TVD                     (122 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_TVD                 (123 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_TVD_TOP             (124 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_LEDC                    (125 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_LEDC                (126 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_CSI_TOP                 (127 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_CSI0_MCLK               (128 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_CSI                 (129 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_TPADC                   (130 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_TPADC               (131 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_TZMA                (132 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_DSP                     (133 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_DSP_CFG             (134 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_RISCV                   (135 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_RISCV_AXI               (136 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_BUS_RISCV_CFG           (137 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_FANOUT_24M              (138 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_FANOUT_12M              (139 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_FANOUT_16M              (140 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_FANOUT_25M              (141 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_FANOUT_32K              (142 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_FANOUT_27M              (143 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_FANOUT_PCLK             (144 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_FANOUT0_OUT             (145 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_FANOUT1_OUT             (146 + CSP_CCM_SYS_CLK_MAX)
#define CSP_CCM_MOD_CLK_FANOUT2_OUT             (147 + CSP_CCM_SYS_CLK_MAX)

#define CSP_CCM_MOD_CLK_MAX                     (CSP_CCM_MOD_CLK_FANOUT2_OUT + 1)

#define CSP_CCM_MOD_CLK_MAX_NO                  CSP_CCM_MOD_CLK_FANOUT2_OUT
#define CSP_CCM_MOD_CLK_NUMBER                  (CSP_CCM_MOD_CLK_MAX_NO + 1)

#define CSP_CCM_MOD_RST_MBUS                    0
#define CSP_CCM_MOD_RST_BUS_DE0                 1
#define CSP_CCM_MOD_RST_BUS_DI                  2
#define CSP_CCM_MOD_RST_BUS_G2D                 3
#define CSP_CCM_MOD_RST_BUS_CE                  4
#define CSP_CCM_MOD_RST_BUS_VE                  5
#define CSP_CCM_MOD_RST_BUS_DMA                 6
#define CSP_CCM_MOD_RST_BUS_MSGBOX0             7
#define CSP_CCM_MOD_RST_BUS_MSGBOX1             8
#define CSP_CCM_MOD_RST_BUS_MSGBOX2             9
#define CSP_CCM_MOD_RST_BUS_SPINLOCK            10
#define CSP_CCM_MOD_RST_BUS_HSTIMER             11
#define CSP_CCM_MOD_RST_BUS_DBG                 12
#define CSP_CCM_MOD_RST_BUS_PWM                 13
#define CSP_CCM_MOD_RST_BUS_DRAM                14
#define CSP_CCM_MOD_RST_BUS_MMC0                15
#define CSP_CCM_MOD_RST_BUS_MMC1                16
#define CSP_CCM_MOD_RST_BUS_MMC2                17
#define CSP_CCM_MOD_RST_BUS_UART0               18
#define CSP_CCM_MOD_RST_BUS_UART1               19
#define CSP_CCM_MOD_RST_BUS_UART2               20
#define CSP_CCM_MOD_RST_BUS_UART3               21
#define CSP_CCM_MOD_RST_BUS_UART4               22
#define CSP_CCM_MOD_RST_BUS_UART5               23
#define CSP_CCM_MOD_RST_BUS_I2C0                24
#define CSP_CCM_MOD_RST_BUS_I2C1                25
#define CSP_CCM_MOD_RST_BUS_I2C2                26
#define CSP_CCM_MOD_RST_BUS_I2C3                27
#define CSP_CCM_MOD_RST_BUS_CAN0                28
#define CSP_CCM_MOD_RST_BUS_CAN1                29
#define CSP_CCM_MOD_RST_BUS_SPI0                30
#define CSP_CCM_MOD_RST_BUS_SPI1                31
#define CSP_CCM_MOD_RST_BUS_EMAC0               32
#define CSP_CCM_MOD_RST_BUS_IR_TX               33
#define CSP_CCM_MOD_RST_BUS_GPADC               34
#define CSP_CCM_MOD_RST_BUS_THS                 35
#define CSP_CCM_MOD_RST_BUS_I2S0                36
#define CSP_CCM_MOD_RST_BUS_I2S1                37
#define CSP_CCM_MOD_RST_BUS_I2S2                38
#define CSP_CCM_MOD_RST_BUS_SPDIF               39
#define CSP_CCM_MOD_RST_BUS_DMIC                40
#define CSP_CCM_MOD_RST_BUS_AUDIO_CODEC         41
#define CSP_CCM_MOD_RST_USB_PHY0                42
#define CSP_CCM_MOD_RST_USB_PHY1                43
#define CSP_CCM_MOD_RST_BUS_OHCI0               44
#define CSP_CCM_MOD_RST_BUS_OHCI1               45
#define CSP_CCM_MOD_RST_BUS_EHCI0               46
#define CSP_CCM_MOD_RST_BUS_EHCI1               47
#define CSP_CCM_MOD_RST_BUS_OTG                 48
#define CSP_CCM_MOD_RST_BUS_LRADC               49
#define CSP_CCM_MOD_RST_BUS_DPSS_TOP0           50
#define CSP_CCM_MOD_RST_BUS_HDMI_SUB            51
#define CSP_CCM_MOD_RST_BUS_HDMI_MAIN           52
#define CSP_CCM_MOD_RST_BUS_MIPI_DSI            53
#define CSP_CCM_MOD_RST_BUS_TCON_LCD0           54
#define CSP_CCM_MOD_RST_BUS_TCON_TV             55
#define CSP_CCM_MOD_RST_BUS_LVDS0               56
#define CSP_CCM_MOD_RST_BUS_TVE                 57
#define CSP_CCM_MOD_RST_BUS_TVE_TOP             58
#define CSP_CCM_MOD_RST_BUS_TVD                 59
#define CSP_CCM_MOD_RST_BUS_TVD_TOP             60
#define CSP_CCM_MOD_RST_BUS_LEDC                61
#define CSP_CCM_MOD_RST_BUS_CSI                 62
#define CSP_CCM_MOD_RST_BUS_TPADC               63
#define CSP_CCM_MOD_RST_BUS_DSP                 64
#define CSP_CCM_MOD_RST_BUS_DSP_CFG             65
#define CSP_CCM_MOD_RST_BUS_DSP_DBG             66
#define CSP_CCM_MOD_RST_BUS_RISCV_CFG           67
#define CSP_CCM_MOD_RST_BUS_RISCV_SOFT          69
#define CSP_CCM_MOD_RST_BUS_RISCV_CPU_SOFT      70

#endif

typedef enum {
	CSP_CCM_SUNXI_RESET = 0,
	CSP_CCM_SUNXI_R_RESET,
	CSP_CCM_SUNXI_RESET_NUMBER,
} sys_reset_type_t;

enum CCM_CONST
{
    MHz                    = (1000 * 1000),
    FREQ_0                 = 0,
    FREQ_HOSC              = (24 * MHz),
    FREQ_LOSC              = (32768),
    FREQ_AUDIO_PLL_HIGH    =  24576000,
    FREQ_AUDIO_PLL_LOW     = 22579200,
};

typedef enum
{
    CSP_CCM_ERR_NONE,
    CSP_CCM_ERR_PARA_NULL,
    CSP_CCM_ERR_OSC_FREQ_CANNOT_BE_SET,  //频率不能设置
    CSP_CCM_ERR_PLL_FREQ_LOW,
    CSP_CCM_ERR_PLL_FREQ_HIGH,
    CSP_CCM_ERR_FREQ_NOT_STANDARD,
    CSP_CCM_ERR_CLK_NUM_NOT_SUPPORTED,
    CSP_CCM_ERR_DIVIDE_RATIO,
    CSP_CCM_ERR_CLK_IS_OFF,
    CSP_CCM_ERR_SRC_CLK_NOT_AVAILABLE,
    CSP_CCM_ERR_GET_CLK_FREQ,
    CSP_CCM_ERR_CLK_NO_INVALID,

    CSP_CCM_ERR_RESET_CONTROL_DENIED,
    CSP_CCM_ERR_NULL_PARA,
    CSP_CCM_ERR_PARA_VALUE,
} CSP_CCM_err_t;

#endif //#ifndef _CSP_CCM_PARA_H_

