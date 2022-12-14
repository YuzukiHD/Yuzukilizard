/*
*********************************************************************************************************
*                                                   eBase
*                                       the Abstract of Hardware
*                                              the OAL of CSP
*
*                               (c) Copyright 2006-2010, AW China
*                                           All Rights Reserved
*
* File      :   csp.c
* Date      :   2011-01-20
* By        :   holigun
* Version   :   V1.00
* History     :
*   aw1619的中断表，中断源
*********************************************************************************************************
*/
#ifndef _CSP_INTC_PARA_H_
#define _CSP_INTC_PARA_H_

#define CSP_IRQ_SOUCE_COUNT     (128)
#define CSP_INT_PHY_ADDR_BASE   0x01c20400
#define CSP_INT_PHY_ADDR_SIZE   0x400

typedef struct _intc_private_regs
{
    volatile __u32  vctr;           //0x00
    volatile __u32  vtblbaddr;      //0x04
    volatile __u32  reserved0;      //
    volatile __u32  cfg;            //0x0c
    volatile __u32  pendclr0;       //0x10
    volatile __u32  pendclr1;       //0x14
    volatile __u32  reserved1[2];   //0x18
    volatile __u32  enable0;        //0x20
    volatile __u32  enable1;        //0x24
    volatile __u32  reserved2[2];   //0x28
    volatile __u32  mask0;          //0x30
    volatile __u32  mask1;          //0x34
    volatile __u32  reserved3[2];   //0x38
    volatile __u32  response0;      //0x40
    volatile __u32  response1;      //0x44
    volatile __u32  reserved4[2];   //0x48
    volatile __u32  ff0;            //0x50
    volatile __u32  ff1;            //0x54
    volatile __u32  reserved5[2];   //0x58
    volatile __u32  prio0;          //0x60
    volatile __u32  prio1;          //0x64
    volatile __u32  prio2;          //0x68
    volatile __u32  prio3;          //0x6C
} _intc_private_regs_t;

#ifdef CONFIG_SOC_SUN3IW2            //SUN3IW2 platform.
#define INTC_IRQNO_FIQ           0
#define INTC_IRQNO_UART0         1
#define INTC_IRQNO_UART1         2
#define INTC_IRQNO_UART2         3
#define INTC_IRQNO_IR            6
#define INTC_IRQNO_TWI0          7
#define INTC_IRQNO_TWI1          8
#define INTC_IRQNO_TWI2          9
#define INTC_IRQNO_SPI0          10
#define INTC_IRQNO_SPI1          11
#define INTC_IRQNO_TIMER0        13
#define INTC_IRQNO_TIMER1        14
#define INTC_IRQNO_HSTIMER       15
#define INTC_IRQNO_WATCHDOG      16
#define INTC_IRQNO_PWM           17
#define INTC_IRQNO_DMA           18
#define INTC_IRQNO_CE            19
#define INTC_IRQNO_RTP           20
#define INTC_IRQNO_ADDA          21
#define INTC_IRQNO_LRADC         22
#define INTC_IRQNO_SDMMC0        23
#define INTC_IRQNO_SDMMC1        24
#define INTC_IRQNO_TSC           25
#define INTC_IRQNO_USB0          26
#define INTC_IRQNO_TVDEC         27
#define INTC_IRQNO_TVENC         28
#define INTC_IRQNO_TCON_LCD0     29
#define INTC_IRQNO_DE            30
#define INTC_IRQNO_ALARM0        31
#define INTC_IRQNO_ALARM1        32
#define INTC_IRQNO_DEINTERLACE   33
#define INTC_IRQNO_VE            34
#define INTC_IRQNO_DAUDIO        35
#define INTC_IRQNO_DRAM_MDFS     36
#define INTC_IRQNO_ROTATE        37
#define INTC_IRQNO_PIOB          38
#define INTC_IRQNO_PIOF          40
#define INTC_IRQNO_PIOG          41
#define INTC_IRQNO_TCON_TV0      42
#define INTC_IRQNO_MIPI_DSI      43
#define INTC_IRQNO_USB_ECHIO     44
#define INTC_IRQNO_USB_OHCIO     45
#define INTC_IRQNO_SCR           46
#define INTC_IRQNO_DTMB          47

#elif defined CONFIG_SOC_SUN3IW1   //legacy platform

#define INTC_IRQNO_FIQ           0
#define INTC_IRQNO_UART0         1
#define INTC_IRQNO_UART1         2
#define INTC_IRQNO_UART2         3
#define INTC_IRQNO_SPDIF         5
#define INTC_IRQNO_IR            6
#define INTC_IRQNO_TWI0          7
#define INTC_IRQNO_TWI1          8
#define INTC_IRQNO_TWI2          9
#define INTC_IRQNO_SPI0          10
#define INTC_IRQNO_SPI1          11
#define INTC_IRQNO_TIMER0        13
#define INTC_IRQNO_TIMER1        14
#define INTC_IRQNO_TIMER2        15
#define INTC_IRQNO_TIMER2345     15
#define INTC_IRQNO_WATCHDOG      16
#define INTC_IRQNO_RSB           17
#define INTC_IRQNO_DMA           18
#define INTC_IRQNO_TP            20
#define INTC_IRQNO_ADDA          21
#define INTC_IRQNO_LRADC         22
#define INTC_IRQNO_SDMMC0        23
#define INTC_IRQNO_SDMMC1        24
#define INTC_IRQNO_USB0          26
#define INTC_IRQNO_TVENC         28
#define INTC_IRQNO_TVDEC         27
#define INTC_IRQNO_TCON0         29
#define INTC_IRQNO_DE_SCALE0     30
#define INTC_IRQNO_DE_IMAGE0     31
#define INTC_IRQNO_CSI           32
#define INTC_IRQNO_DEINTERLACE   33
#define INTC_IRQNO_VE            34
#define INTC_IRQNO_DAUDIO        35
#define INTC_IRQNO_PIOD          38
#define INTC_IRQNO_PIOE          39
#define INTC_IRQNO_PIOF          40
#define INTC_IRQNO_SOFTIRQ0      52

#elif defined CONFIG_SOC_SUN8IW19  //legacy platform

#define	INTC_IRQNO_SGI0                  (  0)
#define	INTC_IRQNO_SGI1                  (  1)
#define	INTC_IRQNO_SGI2                  (  2)
#define	INTC_IRQNO_SGI3                  (  3)
#define	INTC_IRQNO_SGI4                  (  4)
#define	INTC_IRQNO_SGI5                  (  5)
#define	INTC_IRQNO_SGI6                  (  6)
#define	INTC_IRQNO_SGI7                  (  7)
#define	INTC_IRQNO_SGI8                  (  8)
#define	INTC_IRQNO_SGI9                  (  9)
#define	INTC_IRQNO_SGI10                 ( 10)
#define	INTC_IRQNO_SGI11                 ( 11)
#define	INTC_IRQNO_SGI12                 ( 12)
#define	INTC_IRQNO_SGI13                 ( 13)
#define	INTC_IRQNO_SGI14                 ( 14)
#define	INTC_IRQNO_SGI15                 ( 15)
#define	INTC_IRQNO_PPI0                  ( 16)
#define	INTC_IRQNO_PPI1                  ( 17)
#define	INTC_IRQNO_PPI2                  ( 18)
#define	INTC_IRQNO_PPI3                  ( 19)
#define	INTC_IRQNO_PPI4                  ( 20)
#define	INTC_IRQNO_PPI5                  ( 21)
#define	INTC_IRQNO_PPI6                  ( 22)
#define	INTC_IRQNO_PPI7                  ( 23)
#define	INTC_IRQNO_PPI8                  ( 24)
#define	INTC_IRQNO_PPI9                  ( 25)
#define	INTC_IRQNO_PPI10                 ( 26)
#define	INTC_IRQNO_PPI11                 ( 27)
#define	INTC_IRQNO_PPI12                 ( 28)
#define	INTC_IRQNO_PPI13                 ( 29)
#define	INTC_IRQNO_PPI14                 ( 30)
#define	INTC_IRQNO_PPI15                 ( 31)
#define	INTC_IRQNO_GPADC                 ( 32)
#define	INTC_IRQNO_THS                   ( 33)
//#define	INTC_IRQNO_RESERVED              ( 34)
//#define	INTC_IRQNO_RESERVED              ( 35)
//#define	INTC_IRQNO_RESERVED              ( 36)
//#define	INTC_IRQNO_RESERVED              ( 37)
//#define	INTC_IRQNO_RESERVED              ( 38)
//#define	INTC_IRQNO_RESERVED              ( 39)
//#define	INTC_IRQNO_RESERVED              ( 40)
#define	INTC_IRQNO_DRAM                  ( 41)
#define	INTC_IRQNO_DMA                   ( 42)
//#define	INTC_IRQNO_RESERVED              ( 43)
//#define	INTC_IRQNO_RESERVED              ( 44)
//#define	INTC_IRQNO_RESERVED              ( 45)
#define	INTC_IRQNO_WDOG                  ( 46)
#define	INTC_IRQNO_PWM                   ( 47)
//#define	INTC_IRQNO_RESERVED              ( 48)
#define	INTC_IRQNO_BUS_TIMEOUT           ( 49)
#define	INTC_IRQNO_SMC                   ( 50)
#define	INTC_IRQNO_PSI                   ( 51)
#define	INTC_IRQNO_NNA                   ( 52)
#define	INTC_IRQNO_G2D                   ( 53)
//#define	INTC_IRQNO_RESERVED              ( 54)
//#define	INTC_IRQNO_RESERVED              ( 55)
//#define	INTC_IRQNO_RESERVED              ( 56)
#define	INTC_IRQNO_VE                    ( 57)
#define	INTC_IRQNO_EISE                  ( 58)
#define	INTC_IRQNO_GMAC0                 ( 59)
#define	INTC_IRQNO_AUDIOCODEC            ( 60)
//#define	INTC_IRQNO_RESERVED              ( 61)
//#define	INTC_IRQNO_RESERVED              ( 62)
#define	INTC_IRQNO_DE                    ( 63)
//#define	INTC_IRQNO_RESERVED              ( 64)
#define	INTC_IRQNO_ISP0                  ( 65)
//#define	INTC_IRQNO_RESERVED              ( 66)
#define	INTC_IRQNO_CE_NS                 ( 67)
#define	INTC_IRQNO_CE_S                  ( 68)
#define	INTC_IRQNO_I2S_PCM0              ( 69)
#define	INTC_IRQNO_I2S_PCM1              ( 70)
//#define	INTC_IRQNO_RESERVED              ( 71)
//#define	INTC_IRQNO_RESERVED              ( 72)
#define	INTC_IRQNO_TWI0                  ( 73)
#define	INTC_IRQNO_TWI1                  ( 74)
#define	INTC_IRQNO_TWI2                  ( 75)
#define	INTC_IRQNO_TWI3                  ( 76)
#define	INTC_IRQNO_MIPI_DSI0             ( 77)
#define	INTC_IRQNO_SMHC0                 ( 78)
#define	INTC_IRQNO_SMHC1                 ( 79)
#define	INTC_IRQNO_SMHC2                 ( 80)
#define	INTC_IRQNO_UART0                 ( 81)
#define	INTC_IRQNO_UART1                 ( 82)
#define	INTC_IRQNO_UART2                 ( 83)
#define	INTC_IRQNO_UART3                 ( 84)
//#define	INTC_IRQNO_RESERVED              ( 85)
#define	INTC_IRQNO_SPI0                  ( 86)
#define	INTC_IRQNO_SPI1                  ( 87)
#define	INTC_IRQNO_SPI2                  ( 88)
//#define	INTC_IRQNO_RESERVED              ( 89)
#define	INTC_IRQNO_HSTIMER0              ( 90)
#define	INTC_IRQNO_HSTIMER1              ( 91)
#define	INTC_IRQNO_TMR0                  ( 92)
#define	INTC_IRQNO_TMR1                  ( 93)
#define	INTC_IRQNO_TCON_LCD0             ( 94)
//#define	INTC_IRQNO_RESERVED              ( 95)
#define	INTC_IRQNO_USB20_DRD_DEVICE      ( 96)
#define	INTC_IRQNO_USB20_DRD_EHCI        ( 97)
#define	INTC_IRQNO_USB20_DRD_OHCI        ( 98)
#define	INTC_IRQNO_GPIOC                 ( 99)
#define	INTC_IRQNO_GPIOD                 (100)
#define	INTC_IRQNO_GPIOE                 (101)
#define	INTC_IRQNO_GPIOF                 (102)
#define	INTC_IRQNO_GPIOG                 (103)
#define	INTC_IRQNO_GPIOH                 (104)
#define	INTC_IRQNO_GPIOI                 (105)
#define	INTC_IRQNO_CSI_DMA0              (106)
#define	INTC_IRQNO_CSI_DMA1              (107)
#define	INTC_IRQNO_CSI_DMA2              (108)
#define	INTC_IRQNO_CSI_DMA3              (109)
#define	INTC_IRQNO_CSI_PARSER0           (110)
#define	INTC_IRQNO_CSI_PARSER1           (111)
//#define	INTC_IRQNO_RESERVED              (112)
//#define	INTC_IRQNO_RESERVED              (113)
#define	INTC_IRQNO_CSI_MIPI0_RX          (114)
//#define	INTC_IRQNO_RESERVED              (115)
//#define	INTC_IRQNO_RESERVED              (116)
//#define	INTC_IRQNO_RESERVED              (117)
//#define	INTC_IRQNO_RESERVED              (118)
//#define	INTC_IRQNO_RESERVED              (119)
//#define	INTC_IRQNO_RESERVED              (120)
//#define	INTC_IRQNO_RESERVED              (121)
//#define	INTC_IRQNO_RESERVED              (122)
//#define	INTC_IRQNO_RESERVED              (123)
#define	INTC_IRQNO_CSI_TOP_PKT           (124)
#define	INTC_IRQNO_DSPO                  (125)
//#define	INTC_IRQNO_RESERVED              (126)
#define	INTC_IRQNO_IOMMU                 (127)
//#define	INTC_IRQNO_RESERVED              (128)
//#define	INTC_IRQNO_RESERVED              (129)
//#define	INTC_IRQNO_RESERVED              (130)
//#define	INTC_IRQNO_RESERVED              (131)
//#define	INTC_IRQNO_RESERVED              (132)
//#define	INTC_IRQNO_RESERVED              (133)
//#define	INTC_IRQNO_RESERVED              (134)
//#define	INTC_IRQNO_RESERVED              (135)
/*CPUS*/
#define	INTC_IRQNO_EXTERNALNMI           (136)
#define	INTC_IRQNO_R_ALARM0              (137)
#define	INTC_IRQNO_R_GPIOL               (138)
#define	INTC_IRQNO_R_TWI0                (139)
#define	INTC_IRQNO_R_RSB                 (142)
#define	INTC_IRQNO_R_OWC                 (146)
/*CPUX*/
#define	INTC_IRQNO_C0_CTI0               (160)
#define	INTC_IRQNO_C0_COMMTX0            (164)
#define	INTC_IRQNO_C0_COMMRX0            (168)
#define	INTC_IRQNO_C0_PMU0               (172)
#define	INTC_IRQNO_C0_AXI_ERROR          (176)
#define	INTC_IRQNO_AXI_WR_IRQ            (178)
#define	INTC_IRQNO_AXI_RD_IRQ            (179)


#endif
//=======================================================================================================

#define INTC_NMI_TRIGGER_MOD_LOW_LEVEL      0x00
#define INTC_NMI_TRIGGER_MOD_HIGH_LEVEL     0x01
#define INTC_NMI_TRIGGER_MOD_NEGATIVE_EDGE  0x02
#define INTC_NMI_TRIGGER_MOD_POSITIVE_EDGE  0x03
#define INTC_NMI_TRIGGER_MOD_MAX            0x03
//=======================================================================================================
#endif  //_CSP_INTC_PARA_H_
