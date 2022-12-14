/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the People's Republic of China and other countries.
* All Allwinner Technology Co.,Ltd. trademarks are used with permission.
*
* DISCLAIMER
* THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
* IF YOU NEED TO INTEGRATE THIRD PARTY’S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
* IN ALLWINNERS’SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
* ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
* ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
* COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
* YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY’S TECHNOLOGY.
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
#ifndef _SOC_H_
#define _SOC_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef IHS_VALUE
#define  IHS_VALUE    (24000000)
#endif

#ifndef EHS_VALUE
#define  EHS_VALUE    (24000000)
#endif


/* -------------------------  Interrupt Number Definition  ------------------------ */
typedef enum IRQn
{
    NMI_EXPn                        =   -2,      /* NMI Exception */
    /* ----------------------  SmartL Specific Interrupt Numbers  --------------------- */
    Machine_Software_IRQn           =   3,      /* Machine software interrupt */
    User_Timer_IRQn                 =   4,      /* User timer interrupt */
    Supervisor_Timer_IRQn           =   5,      /* Supervisor timer interrupt */
    CORET_IRQn                      =   7,      /* core Timer Interrupt */
    Machine_External_IRQn           =   11,     /* Machine external interrupt */
    CPUX_MBOX_CPUX_IRQn         =   16,     /* CPUX MSGBOX READ IRQ FOR CPUX */
    CPUX_MBOX_E907_IRQn         =   17,     /* CPUX MSGBOX READ IRQ FOR E907 */
    UART0_IRQn                      =   18,     /* uart0 Interrupt */
    UART1_IRQn                      =   19,     /* uart1 Interrupt */
    UART2_IRQn                      =   20,     /* uart2 Interrupt */
    UART3_IRQn                      =   21,     /* uart3 Interrupt */
    TWI0_IRQn                       =   25,     /* twi0 Interrupt */
    TWI1_IRQn                       =   26,     /* twi1 Interrupt */
    TWI2_IRQn                       =   27,     /* twi2 Interrupt */
    TWI3_IRQn                       =   28,     /* twi3 Interrupt */
    TWI4_IRQn                       =   29,     /* twi4 Interrupt */
    SPI0_IRQn                       =   31,     /* spi0 Interrupt */
    SPI1_IRQn                       =   32,     /* spi1 Interrupt */
    SPI2_IRQn                       =   33,     /* spi2 Interrupt */
    PWM_IRQn                        =   34,     /* pwm Interrupt */
    SPI_FLASH_IRQn                  =   35,     /* spi flash Interrupt */
    WIEGAND_IRQn                    =   36,     /* wiegand Interrupt */
    SPI3_IRQn                       =   37,     /* spi3 Interrupt */
    DMIC_IRQn                       =   40,     /* dmic Interrupt */
    AUDIO_CODEC_IRQn                =   41,     /* audio codec Interrupt */
    I2S_PCM0_IRQn                   =   42,     /* i2s pcm0 Interrupt */
    I2S_PCM1_IRQn                   =   43,     /* i2s pcm1 Interrupt */
    USB_OTG_IRQn                    =   45,     /* usb otg Interrupt */
    USB_EHCI_IRQn                   =   46,     /* usb ehci Interrupt */
    USB_OHCI_IRQn                   =   47,     /* usb ohci Interrupt */
    SMHC0_IRQn                  =   56,     /* smhc0 Interrupt */
    SMHC1_IRQn                  =   57,     /* smhc1 Interrupt */
    SMHC2_IRQn                  =   58,     /* smhc2 Interrupt */
    MSI_IRQn                    =   59,     /* msi Interrupt */
    SMC_IRQn                    =   60,     /* smc Interrupt */
    GMAC_IRQn                   =   62,     /* gmac Interrupt */
    CCU_FERR_IRQn           =   64,     /* ccu ferr Interrupt */
    AHB_HREADY_TIME_OUT_IRQn        =   65,     /* ahb hready time out Interrupt */
    DMA_CPUX_NS_IRQn                =   66,     /* dma cpux ns Interrupt */
    DMA_CPUX_S_IRQn             =   67,     /* dma cpux s Interrupt */
    CE_NS_IRQn                      =   68,     /* ce ns Interrupt */
    CE_S_IRQn                       =   69,     /* ce s Interrupt */
    SPINLOCK_IRQn               =   70,     /* spinlock Interrupt */
    HSTIMER0_IRQn               =   71,     /* hstimer0 Interrupt */
    HSTIMER1_IRQn               =   72,     /* hstimer1 Interrupt */
    GPADC_IRQn                      =   73,     /* gpadc Interrupt */
    THS_IRQn                        =   74,     /* ths Interrupt */
    TIMER0_IRQn                     =   75,     /* timer0 Interrupt */
    TIMER1_IRQn                     =   76,     /* timer1 Interrupt */
    TIMER2_IRQn                     =   77,     /* timer2 Interrupt */
    TIMER3_IRQn                     =   78,     /* timer3 Interrupt */
    WDG_IRQn                        =   79,     /* wdg Interrupt */
    IOMMU_IRQn                      =   80,     /* iommu Interrupt */
    NPU_IRQn                        =   81,     /* npu Interrupt */
    VE_IRQn                         =   82,     /* ve Interrupt */
    GPIOA_NS_IRQn                   =   83,     /* gpioa ns Interrupt */
    GPIOA_S_IRQn                    =   84,     /* gpioa s Interrupt */
    GPIOC_NS_IRQn                   =   87,     /* gpioc ns Interrupt */
    GPIOC_S_IRQn                    =   88,     /* gpioc s Interrupt */
    GPIOD_NS_IRQn                   =   89,     /* gpiod ns Interrupt */
    GPIOD_S_IRQn                    =   90,     /* gpiod s Interrupt */
    GPIOE_NS_IRQn                   =   91,     /* gpioe ns Interrupt */
    GPIOE_S_IRQn                    =   92,     /* gpioe s Interrupt */
    GPIOF_NS_IRQn                   =   93,     /* gpiof ns Interrupt */
    GPIOF_S_IRQn                    =   94,     /* gpiof s Interrupt */
    GPIOG_NS_IRQn                   =   95,     /* gpiog ns Interrupt */
    GPIOG_S_IRQn                    =   96,     /* gpiog s Interrupt */
    GPIOH_NS_IRQn                   =   97,     /* gpioh ns Interrupt */
    GPIOH_S_IRQn                    =   98,     /* gpioh s Interrupt */
    GPIOI_NS_IRQn                   =   99,     /* gpioi ns Interrupt */
    GPIOI_S_IRQn                    =   100,     /* gpioi s Interrupt */
    DMAC_E907_NS_IRQn               =   101,     /* dmac e907 ns Interrupt */
    DMAC_E907_S_IRQn                =   102,     /* dmac e907 s Interrupt */
    DE_IRQn                         =   103,     /* de Interrupt */
    G2D_IRQn                        =   105,     /* g2d Interrupt */
    LCD_IRQn                        =   106,     /* lcd Interrupt */
    DSI_IRQn                        =   108,     /* dsi Interrupt */
    CSI_DMA0_IRQn                   =   111,     /* csi dma0 Interrupt */
    CSI_DMA1_IRQn                   =   112,     /* csi dma1 Interrupt */
    CSI_DMA2_IRQn                   =   113,     /* csi dma2 Interrupt */
    CSI_DMA3_IRQn                   =   114,     /* csi dma3 Interrupt */
    CSI_PARSER0_IRQn                =   116,     /* csi parser0 Interrupt */
    CSI_PARSER1_IRQn                =   117,     /* csi parser1 Interrupt */
    CSI_PARSER2_IRQn                =   118,     /* csi parser2 Interrupt */
    CSI_CMB_IRQn                    =   120,     /* csi cmb Interrupt */
    CSI_TDM_IRQn                    =   121,     /* csi tdm Interrupt */
    CSI_TOP_PKT_IRQn                =   122,     /* csi top pkt Interrupt */
    CSI_ISP0_IRQn                   =   124,     /* csi isp0 Interrupt */
    CSI_ISP1_IRQn                   =   125,     /* csi isp1 Interrupt */
    CSI_ISP2_IRQn                   =   126,     /* csi isp2 Interrupt */
    CSI_ISP3_IRQn                   =   127,     /* csi isp3 Interrupt */
    VIPP0_IRQn                      =   128,     /* vipp0 Interrupt */
    VIPP1_IRQn                      =   129,     /* vipp1 Interrupt */
    VIPP2_IRQn                      =   130,     /* vipp2 Interrupt */
    VIPP3_IRQn                      =   131,     /* vipp3 Interrupt */
    E907_MBOX_E907_IRQn             =   144,     /* e907 msgbox read irq for e907 Interrupt */
    E907_MBOX_CPUX_IRQn             =   145,     /* e907 msgbox write irq for cpux Interrupt */
    E907_WDG_IRQn                   =   146,     /* e907 wdg Interrupt */
    E907_TIMER0_IRQn                =   147,     /* e907 timer0 Interrupt */
    E907_TIMER1_IRQn                =   148,     /* e907 timer1 Interrupt */
    E907_TIMER2_IRQn                =   149,     /* e907 timer2 Interrupt */
    E907_TIMER3_IRQn                =   150,     /* e907 timer3 Interrupt */
    NMI_IRQn                        =   152,     /* nmi Interrupt */
    PPU_IRQn                        =   153,     /* ppu Interrupt */
    ALARM_IRQn                      =   154,     /* alarm Interrupt */
    AHBS_HREADY_TIME_OUT_IRQn       =   155,     /* cpus ahb ready time out irq Interrupt */
    PMC_IRQn                        =   156,     /* pmc Interrupt */
    GIC_C0_IRQn                     =   157,     /* gic c0 Interrupt */
    TWD_IRQn                        =   158,     /* twd Interrupt */
}
IRQn_Type;

#ifdef __cplusplus
}
#endif

#endif  /* _SOC_H_ */
