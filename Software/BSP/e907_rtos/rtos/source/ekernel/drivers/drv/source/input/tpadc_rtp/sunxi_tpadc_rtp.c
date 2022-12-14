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
#include <stdio.h>
#include <stdlib.h>
#include <sunxi_input.h>
#include <log.h>
#include <debug.h>
#include <init.h>
#include <kapi.h>
#include "sunxi_hal_tpadc.h"
#include "sunxi_tpadc_rtp.h"
#include "mod_touchpanel.h"
#include "sunxi_drv_rtp.h"
#include <math.h>

#define TPCHANNEL0 0
#define TPCHANNEL1 1
#define TPCHANNEL2 2
#define TPCHANNEL3 3
#define TPREG_BASE 0x02009c00
#define SAMPLING_CNT 4//采样过滤次数


typedef struct __DRV_TPADC_RTP_T
{
    rt_sem_t data_sem;
    unsigned int thread_id;
    unsigned int data[2];//实时数据
    unsigned int avr_data[2];//滤波后的数据
    unsigned int data_flag[3];
    unsigned int lcd_posx;
    unsigned int lcd_posy;
}__rtp_data_t;


typedef struct __RTP_CALI_T
{
    bool flag;
    float adc_x[5];
    float adc_y[5];
    float lcd_x[5];
    float lcd_y[5];
    float kx;
    float ky;
    __krnl_event_t   *tp_sem_adret;       //用于通知，表示当前次校准已经完成
}__rtp_cali_t;

extern int32_t console_LTSDevEvent(uint32_t type, uint32_t code, int32_t value);

static __rtp_data_t *rtpdata;
static int init_flag = 0;;

__rtp_cali_t cali;
float xfac,yfac,xoff,yoff;


#if 1//bsp temp use
#define TP_DEBOUNCE_EN	9
#define TP_DEBOUNCE_EN_WIDTH	1

#define TP_DEBOUNCE_TIME	12
#define TP_DEBOUNCE_TIME_WIDTH	8

static void rtp_enable_irq(bool enable)
{
    if(enable)
    (*((volatile unsigned int *)(TPREG_BASE + 0x10))) |= 1 << 16;
    else
    (*((volatile unsigned int *)(TPREG_BASE + 0x10))) &= ~(1 << 16);
}

static void rtp_clean_fifo(void)
{
    (*((volatile unsigned int *)(TPREG_BASE + 0x10))) |= 1 << 4;

}

static void sunxi_set_acqiure_time(uint32_t val)
{
	uint32_t reg_val;

	reg_val = readl(TPREG_BASE + TP_CTRL0);
	reg_val  = SET_BITS(TACQ, TACQ_WIDTH, reg_val, val);

	writel(reg_val, TPREG_BASE + TP_CTRL0);
}

static void sunxi_set_frequency_divider(uint32_t val)
{
	uint32_t reg_val;

	reg_val = readl(TPREG_BASE + TP_CTRL0);
	reg_val  = SET_BITS(FS_DIV, FS_DIV_WIDTH, reg_val, val);

	writel(reg_val, TPREG_BASE + TP_CTRL0);
}

static void sunxi_set_filter_type(uint32_t val)
{
	uint32_t reg_val;

	reg_val = readl(TPREG_BASE + TP_CTRL3);
	reg_val  = SET_BITS(FILTER_TYPE, FILTER_TYPE_WIDTH, reg_val, val);

	writel(reg_val, TPREG_BASE + TP_CTRL3);
}

static void sunxi_set_sensitivity(uint32_t val)
{
	uint32_t reg_val;

	reg_val = readl(TPREG_BASE + TP_CTRL2);
	reg_val  = SET_BITS(TP_SENSITIVE, TP_SENSITIVE_WIDTH, reg_val, val);

	writel(reg_val, TPREG_BASE + TP_CTRL2);
}

static void sunxi_set_clk_divider(int32_t val)
{
	uint32_t reg_val;

	reg_val = readl(TPREG_BASE + TP_CTRL0);
	reg_val  = SET_BITS(ADC_CLK_DIVIDER, ADC_CLK_DIVIDER_WIDTH, reg_val, val);

	writel(reg_val, TPREG_BASE + TP_CTRL0);
}

static void sunxi_set_up_debou_time(uint32_t val)
{
	uint32_t reg_val;

	reg_val = readl(TPREG_BASE + TP_CTRL1);
	reg_val  = SET_BITS(TP_DEBOUNCE_TIME, TP_DEBOUNCE_TIME_WIDTH, reg_val, val);

	writel(reg_val, TPREG_BASE + TP_CTRL1);
}

static void sunxi_pressure_enable(uint32_t val)
{
	uint32_t reg_val;

	reg_val = readl(TPREG_BASE + TP_CTRL2);
	reg_val  = SET_BITS(PRE_MEA_EN, PRE_MEA_EN_WIDTH, reg_val, val);

	writel(reg_val, TPREG_BASE + TP_CTRL2);
}
#endif
__u32 upper_get_data(__u32 channel)
{
    return rtpdata->avr_data[channel];
}


int tp_adjust_func(__s32 cnt,void *pbuffer)
{
    __s32 ret = EPDK_FAIL;
    __u8  err;
    __ev_tp_pos_t  *tp_ad = (__ev_tp_pos_t *)pbuffer;

    switch (cnt)
    {
        case 0:
        {     
            cali.flag = 0;   
            cali.tp_sem_adret     = esKRNL_SemCreate(0);
            break;
        }

        case 1:
        {
            esKRNL_SemPend(cali.tp_sem_adret, 0, &err);            
            cali.adc_x[0] = upper_get_data(CHANNEL_X);
            cali.adc_y[0] = upper_get_data(CHANNEL_Y);
            cali.lcd_x[0] =  tp_ad->disp_xpos;
            cali.lcd_y[0] =  tp_ad->disp_ypos; 
            rt_thread_delay(20);           
            break;
        }

        case 2:
        {
            esKRNL_SemPend(cali.tp_sem_adret, 0, &err);            
            cali.adc_x[1] = upper_get_data(CHANNEL_X);
            cali.adc_y[1] = upper_get_data(CHANNEL_Y);
            cali.lcd_x[1] =  tp_ad->disp_xpos;
            cali.lcd_y[1] =  tp_ad->disp_ypos;
            rt_thread_delay(20);           
            break;
        }

        case 3:
        {
            esKRNL_SemPend(cali.tp_sem_adret, 0, &err);            
            cali.adc_x[2] = upper_get_data(CHANNEL_X);
            cali.adc_y[2] = upper_get_data(CHANNEL_Y);
            cali.lcd_x[2] =  tp_ad->disp_xpos;
            cali.lcd_y[2] =  tp_ad->disp_ypos;
            rt_thread_delay(20);           
            break;
        }

        case 4:
        {
            esKRNL_SemPend(cali.tp_sem_adret, 0, &err);            
            cali.adc_x[3] = upper_get_data(CHANNEL_X);
            cali.adc_y[3] = upper_get_data(CHANNEL_Y);
            cali.lcd_x[3] =  tp_ad->disp_xpos;
            cali.lcd_y[3] =  tp_ad->disp_ypos; 
            rt_thread_delay(20);           
            break;
        }

        case 5:
        {
            #if 0
            char kx[10];
            char ky[10];
            esKRNL_SemPend(cali.tp_sem_adret, 0, &err);            
            cali.adc_x[4] = upper_get_data(CHANNEL_X);
            cali.adc_y[4] = upper_get_data(CHANNEL_Y);
            cali.lcd_x[4] =  tp_ad->disp_xpos;
            cali.lcd_y[4] =  tp_ad->disp_ypos;
            cali.kx = (float)(abs(cali.lcd_x[1] - cali.lcd_x[0]) + abs(cali.lcd_x[2] - cali.lcd_x[3]))/(abs(cali.adc_x[1] - cali.adc_x[0]) + abs(cali.adc_x[3] - cali.adc_x[2]));
            cali.ky = (float)(abs(cali.lcd_y[2] - cali.lcd_y[1]) + abs(cali.lcd_y[3] - cali.lcd_y[0]))/(abs(cali.adc_y[2] - cali.adc_y[1]) + abs(cali.adc_y[3] - cali.adc_y[0]));
            cali.flag = 1;

            __hdle fh;
            esKRNL_SemDel(cali.tp_sem_adret, OS_DEL_ALWAYS, &err);
            sprintf(kx, "%f", cali.kx);
            sprintf(ky, "%f", cali.ky);

            //__log("kx = %s   ky = %s",kx,ky);
            fh = fopen("e:\\tp.cfg","wb");
            if(!fh)
            {
                __log("fh fail");
                return;
            }

            fwrite((void *)kx, 10, 1, fh);
            fwrite((void *)ky, 10, 1, fh);
            //fseek(fh,10,SEEK_SET);
            fwrite((void *)&cali.lcd_x[4], 4, 1, fh);
            fwrite((void *)&cali.lcd_y[4], 4, 1, fh);
            fwrite((void *)&cali.adc_x[4], 4, 1, fh);
            fwrite((void *)&cali.adc_y[4], 4, 1, fh);

            fclose(fh);

            #endif

            {
                u16 pos_temp[4][2] = {0};//坐标缓存值
                u8  cnt=0;	
                u16 d1,d2;
                u32 tem1,tem2;
                __hdle fh;
                double fac; 	

                esKRNL_SemPend(cali.tp_sem_adret, 0, &err);            
                cali.adc_x[4] = upper_get_data(CHANNEL_X);
                cali.adc_y[4] = upper_get_data(CHANNEL_Y);
                cali.lcd_x[4] =  tp_ad->disp_xpos;
                cali.lcd_y[4] =  tp_ad->disp_ypos;

                pos_temp[0][0] =  cali.adc_x[0];
                pos_temp[0][1] =  cali.adc_y[0];
                pos_temp[1][0] =  cali.adc_x[1];
                pos_temp[1][1] =  cali.adc_y[1];
                pos_temp[2][0] =  cali.adc_x[2];
                pos_temp[2][1] =  cali.adc_y[2];
                pos_temp[3][0] =  cali.adc_x[3];
                pos_temp[3][1] =  cali.adc_y[3];                                               
                
                tem1=abs(pos_temp[0][0]-pos_temp[1][0]);//x1-x2
                tem2=abs(pos_temp[0][1]-pos_temp[1][1]);//y1-y2
                tem1*=tem1;
                tem2*=tem2;
                d1=sqrt(tem1+tem2);//得到1,2的距离

                tem1=abs(pos_temp[2][0]-pos_temp[3][0]);//x3-x4
                tem2=abs(pos_temp[2][1]-pos_temp[3][1]);//y3-y4
                tem1*=tem1;
                tem2*=tem2;
                d2=sqrt(tem1+tem2);//得到3,4的距离
                fac=(float)d1/d2;

                if(fac<0.95||fac>1.05||d1==0||d2==0)//不合格
                {
                    __err("not right cali data");
                    return EPDK_FAIL;
                }

                tem1=abs(pos_temp[0][0]-pos_temp[2][0]);//x1-x3
                tem2=abs(pos_temp[0][1]-pos_temp[2][1]);//y1-y3
                tem1*=tem1;
                tem2*=tem2;
                d1=sqrt(tem1+tem2);//得到1,3的距离
                
                tem1=abs(pos_temp[1][0]-pos_temp[3][0]);//x2-x4
                tem2=abs(pos_temp[1][1]-pos_temp[3][1]);//y2-y4
                tem1*=tem1;
                tem2*=tem2;
                d2=sqrt(tem1+tem2);//得到2,4的距离
                fac=(float)d1/d2; 

                if(fac<0.95||fac>1.05)//不合格
                {
                    __err("not right cali data");
                    return EPDK_FAIL;
                }

                //对角线相等
                tem1=abs(pos_temp[1][0]-pos_temp[2][0]);//x1-x3
                tem2=abs(pos_temp[1][1]-pos_temp[2][1]);//y1-y3
                tem1*=tem1;
                tem2*=tem2;
                d1=sqrt(tem1+tem2);//得到1,4的距离

                tem1=abs(pos_temp[0][0]-pos_temp[3][0]);//x2-x4
                tem2=abs(pos_temp[0][1]-pos_temp[3][1]);//y2-y4
                tem1*=tem1;
                tem2*=tem2;
                d2=sqrt(tem1+tem2);//得到2,3的距离
                fac=(float)d1/d2;
                
                if(fac<0.95||fac>1.05)//不合格
                {
                    __err("not right cali data");
                    return EPDK_FAIL;
                }

                xfac=(float)( cali.lcd_x[1] -  cali.lcd_x[0])/(pos_temp[1][0]-pos_temp[0][0]);//得到xfac		 
                xoff=(2*cali.lcd_x[4] - xfac*(pos_temp[1][0]+pos_temp[0][0]))/2;//得到xoff
                        
                yfac=(float)( cali.lcd_y[2] -  cali.lcd_y[1])/(pos_temp[2][1]-pos_temp[0][1]);//得到yfac
                yoff=(2*cali.lcd_y[4] - yfac*(pos_temp[2][1]+pos_temp[0][1]))/2;//得到yoff  

                if(abs(xfac)>2||abs(yfac)>2)//触屏和预设的相反了.
                {
                    __err("not right cali data");
                    //return EPDK_FAIL;
                }
                esKRNL_SemDel(cali.tp_sem_adret, OS_DEL_ALWAYS, &err);

                #if 1
                char kxfac[10],kyfac[10],kxoff[10],kyoff[10];
                sprintf(kxfac, "%f", xfac);
                sprintf(kyfac, "%f", yfac);
                sprintf(kxoff, "%f", xoff);
                sprintf(kyoff, "%f", yoff);            
                __log("xfac = %s   yfac = %s   xoff = %s  yoff = %s",kxfac,kyfac,kxoff,kyoff);
                #endif
                fh = fopen("e:\\tp.cfg","wb");
                if(!fh)
                {
                    __log("fh fail");
                    return EPDK_FAIL;;
                }

                //fwrite((void *)kx, 10, 1, fh);
                //fwrite((void *)ky, 10, 1, fh);
                //fseek(fh,10,SEEK_SET);
                fwrite((void *)kxfac, 10, 1, fh);
                fwrite((void *)kyfac, 10, 1, fh);
                fwrite((void *)kxoff, 10, 1, fh);
                fwrite((void *)kyoff, 10, 1, fh);
                fwrite((void *)&cali.lcd_x[4], 4, 1, fh);
                fwrite((void *)&cali.lcd_y[4], 4, 1, fh);                
                fclose(fh);
                fh = NULL;
                cali.flag = 1;
            }            
            break;
        }

        case 6:
        {
            break;
        }

        default:
        {
            __err("Unknown adjust parameter");
            ret = EPDK_FAIL;
            break;
        }
        return EPDK_OK;
    }    
}

//static __u32 dataaa[2];

int rtp_manager_get_data(__u32 data,__u32 channel)
{
    /*
    if(channel != 2)
    {
        dataaa[channel] = data;
        printk("x = %d    y = %d\n",dataaa[0],dataaa[1]);
    }
*/

    rtp_enable_irq(0);    
    if(channel == DATA_UP)
    {
        //__log("channel 2 catch tp up");
        rtpdata->data_flag[DATA_UP] = 1;
        esKRNL_SemPost(rtpdata->data_sem);        
        return 0;
    }

    if((channel != CHANNEL_X)  &&  (channel != CHANNEL_Y))
        return 0;
    if(rtpdata->data_flag[CHANNEL_X] && rtpdata->data_flag[CHANNEL_Y])
        return 0;

    rtpdata->data[channel] = data;
    rtpdata->data_flag[channel] = 1;
    if(rtpdata->data_flag[CHANNEL_X] && rtpdata->data_flag[CHANNEL_Y])
    {
        esKRNL_SemPost(rtpdata->data_sem);
    }

    return 0;
}

static void BubbleSort1(unsigned int *arr,unsigned int sz){
    unsigned int i = 0;
    unsigned int j = 0;
    for(i=0;i<sz-1;i++){
        for(j=0;j<sz-i-1;j++){
            if(arr[j]>arr[j+1]){
                int tmp = arr[j];
                arr[j] = arr[j+1];
                arr[j+1] = tmp;
            }
        }
    }
}

static unsigned int sum_avr_data(unsigned int *arr,unsigned int sz)  //计算除去max , min 后的平均值
{
    /*
    unsigned int i = 1;
    unsigned int sum = 0;

    for(;i<sz-1;i++)
    {
        sum += arr[i];
    }
    return sum/(sz-2);
    */
   return ((arr[1] + arr[2])/2);
}
static void tp_send_msg(__ev_tp_msg_t msg)
{
    console_LTSDevEvent(EV_ABS, ABS_MISC, msg.msg_type);
    console_LTSDevEvent(EV_ABS, ABS_X, msg.xpoint);
    console_LTSDevEvent(EV_ABS, ABS_Y,msg.ypoint);
    console_LTSDevEvent(EV_ABS, ABS_RUDDER,msg.speed_dir);//move down
    console_LTSDevEvent(EV_ABS, ABS_BRAKE, msg.speed_val);//slow
    // console_LTSDevEvent(EV_KEY, BTN_TOUCH, 0);/* KEY UP */
    console_LTSDevEvent(EV_SYN, SYN_REPORT, 0);
    //__log("type = %x, x = %d, y = %d, dir = %x , speed(zoom) = %d \n", msg.msg_type, msg.xpoint, msg.ypoint, msg.speed_dir, msg.speed_val);
}

static int to_lcd_position(float adc_x,float adc_y)
{
    int x,y;    

   // x = (cali.kx*(cali.adc_x[4] - adc_x)) + cali.lcd_x[4];
   // y = 500 - ((cali.ky*(cali.adc_y[4] - adc_y)) + cali.lcd_y[4]);
    x = adc_x * xfac + xoff;
    y = adc_y * yfac + yoff;
   // printk("x:%d  y:%d\n",x,y);
    if(x > (2 *cali.lcd_x[4]) || y > (2 * cali.lcd_y[4]))
    {
        __log("out of scn");
        return -1;
    }
    rtpdata->lcd_posx = x;
    rtpdata->lcd_posy = y;
    return 0;
}
static void rtpdata_sent_thread(void *parg)
{
    __ev_tp_msg_t msg;
    unsigned int i = 0;
    unsigned int xbuf[SAMPLING_CNT];
    unsigned int ybuf[SAMPLING_CNT];
    int ret;
    __s16 firstx,firsty;
    unsigned int sentcnt = 0;
    unsigned int move_cnt = 0;
    unsigned int move_flag = 0;
    __s16 lastx,lasty;
    unsigned int tp_last_status = EV_TP_ACTION_NULL;
    unsigned int dir = 0;
    while(1)
    {
        /*
        if (esKRNL_TDelReq(OS_PRIO_SELF) == OS_TASK_DEL_REQ)
        {
            //杀线程;
            esKRNL_TDel(OS_PRIO_SELF);
        }
*/
        esKRNL_SemPend(rtpdata->data_sem, 0, NULL);
        if(rtpdata->data_flag[DATA_UP] == 1)
        {
            i = 0;
            rtpdata->data_flag[DATA_UP] = 0;
            rtpdata->data_flag[CHANNEL_X] = 0;
            rtpdata->data_flag[CHANNEL_Y] = 0;
            if(!cali.flag)
            {
                esKRNL_SemPost(cali.tp_sem_adret);
                continue;
            }
            msg.msg_type = EV_TP_ACTION_NULL;
            msg.xpoint = rtpdata->lcd_posx;
            msg.ypoint = rtpdata->lcd_posy;
            if(move_flag)
            {
                if(abs(firstx - msg.xpoint) > abs(firsty - msg.ypoint))
                {
                    if((firstx - msg.xpoint) > 50)
                    {
                        msg.speed_dir = LEFTWARD;
                    }
                    else if((msg.xpoint - firstx) > 50)
                    {
                        msg.speed_dir = RIGHTWARD;
                    }
                    else
                    {
                        msg.speed_dir = 0;
                    }                    
                }
                else
                {
                    if((firsty - msg.ypoint) > 50)
                    {
                        msg.speed_dir = UPWARD;
                    }
                    else if((msg.ypoint - firsty) > 50)
                    {
                        msg.speed_dir = DOWNWARD;
                    }
                    else
                    {
                        msg.speed_dir = 0;
                    }
                }

            }
            //msg.speed_val = 0;
            tp_last_status = msg.msg_type;

            tp_send_msg(msg);  

            sentcnt = 0;
            move_cnt = 0;
            lastx = 0;
            lasty = 0;
            firstx = 0;
            firsty = 0;
            move_flag = 0;
            msg.speed_dir = 0;
            msg.speed_val = 0;
            rtp_enable_irq(1);
            continue;
        }       
        //__log("xdata = %d  ydata = %d",rtpdata->data[CHANNEL_X],rtpdata->data[CHANNEL_Y]);
        rtpdata->data_flag[CHANNEL_X] = 0;
        rtpdata->data_flag[CHANNEL_Y] = 0;
        rtpdata->data_flag[DATA_UP] = 0;
        xbuf[i] = rtpdata->data[CHANNEL_X];
        ybuf[i] = rtpdata->data[CHANNEL_Y];
        i++;
        if(i == SAMPLING_CNT)
        {
            i = 0;           
            BubbleSort1(xbuf,SAMPLING_CNT);
            BubbleSort1(ybuf,SAMPLING_CNT);
            rtpdata->avr_data[CHANNEL_X] = sum_avr_data(xbuf,SAMPLING_CNT);//xbuf[(SAMPLING_CNT-1)/2];//
            rtpdata->avr_data[CHANNEL_Y] = sum_avr_data(ybuf,SAMPLING_CNT);//ybuf[(SAMPLING_CNT-1)/2];//

            if(cali.flag)
            {
                ret = to_lcd_position(rtpdata->avr_data[CHANNEL_X],rtpdata->avr_data[CHANNEL_Y]);
                if(ret < 0)
                {
                    rtp_enable_irq(1);
                    continue;
                }
                //__err("lcd:  x= %d  y= %d",rtpdata->lcd_posx,rtpdata->lcd_posy);
#if 1//support slide
                if(sentcnt == 0)
                {
                    msg.msg_type = EV_TP_PRESS_START;
                    firstx = rtpdata->lcd_posx;
                    firsty = rtpdata->lcd_posy;
                }
                else
                {
                    if(abs(rtpdata->lcd_posx - lastx) > 3 || abs(rtpdata->lcd_posy - lasty) > 3)
                    {
                        move_cnt++;
                        if(move_cnt > 3)
                        {
                            move_flag = 1;
                            msg.msg_type = EV_TP_PRESS_MOVE;
                        }
                        else
                        {
                            msg.msg_type = EV_TP_PRESS_HOLD;
                        }
                    }
                    else
                    {
                        move_cnt = 0;
                        msg.msg_type = EV_TP_PRESS_HOLD;
                    }
                }
#else
                msg.msg_type = EV_TP_PRESS_START;                  
#endif          
                msg.xpoint = rtpdata->lcd_posx;
                msg.ypoint = rtpdata->lcd_posy;
                msg.speed_dir = 0;
                msg.speed_val = 0;
                lastx = rtpdata->lcd_posx;
                lasty = rtpdata->lcd_posy;
                tp_last_status = msg.msg_type;
                sentcnt++;
                tp_send_msg(msg);
            }       
            esKRNL_TimeDly(1);
        }
        rtp_enable_irq(1);
    }
}

static void set_cali_data(void)
{
    __hdle fh = NULL;
    char kxfac[10],kyfac[10],kxoff[10],kyoff[10],lcdx[10],lcdy[10];

    cali.flag = 0;
    fh = fopen("e:\\tp.cfg","r");
    if(fh)
    {
        fread((void *)kxfac,1,10,fh);
        fread((void *)kyfac,1,10,fh);
        fread((void *)kxoff,1,10,fh);
        fread((void *)kyoff,1,10,fh);
        fread((void *)&cali.lcd_x[4],1,4,fh);
        fread((void *)&cali.lcd_y[4],1,4,fh);
        fclose(fh);
        sprintf(lcdx, "%f", cali.lcd_x[4]);
        sprintf(lcdy, "%f", cali.lcd_y[4]);
        __log("xfac = %s   yfac = %s   xoff = %s  yoff = %s  lcdx = %s  lcdy = %s",kxfac,kyfac,kxoff,kyoff,lcdx,lcdy);
        xfac = (float)atof(kxfac); 
        yfac = (float)atof(kyfac);
        xoff = (float)atof(kxoff); 
        yoff = (float)atof(kyoff);                    
    }
    else
    {
        __err("open tp.cfg fail");
    }

    if(cali.lcd_x[4] && cali.lcd_x[4])
    {
        __log("cali get success");
        cali.flag = 1;
    }

}
rt_err_t rtp_open(struct rt_device *dev, rt_uint16_t oflag)
{
	rt_size_t ret = 0;

    if (dev == NULL)
    {
		__err("dev == NULL\n");
        return -1;
    }

	if(init_flag > 0)
	{
		return 0;
	}
	init_flag = 1;
    
    sunxi_rtp_init();
    sunxi_set_acqiure_time(0x4f);
    sunxi_set_frequency_divider(0x7);
    sunxi_set_clk_divider(0x2);
    sunxi_set_up_debou_time(0x10);
    drv_tpadc_register_callback(rtp_manager_get_data);
    sunxi_pressure_enable(0x0);
    sunxi_set_sensitivity(0xc); 
    sunxi_set_filter_type(0x10);

/*
    (*((volatile unsigned int *)(0x02009c00))) = 0x0fab0002;
    (*((volatile unsigned int *)(0x02009c04))) = 0x00050321;
    (*((volatile unsigned int *)(0x02009c08))) = 0xf1800fff;
    (*((volatile unsigned int *)(0x02009c0c))) = 0x00000005;
    (*((volatile unsigned int *)(0x02009c10))) = 0x00030183;
    */

	rtpdata = rt_malloc(sizeof(__rtp_data_t));
    if (rtpdata == NULL)
    {
		__err("tpadc rtpdata == NULL\n");
        return -1;
    }	
	rt_memset(rtpdata,0,sizeof(__rtp_data_t));

	rtpdata->data_sem = esKRNL_SemCreate(1);
	if(rtpdata->data_sem == NULL)
	{
	   __err("rtpdata->data_sem == NULL\n");
	   rt_free(rtpdata);
	   return -1;
	}
    rtpdata->thread_id = esKRNL_TCreate(rtpdata_sent_thread, NULL, 0x1000, KRNL_priolevel1);
	if(rtpdata->thread_id == NULL)
	{
		__err("rtp thread create err\n");
	}
    set_cali_data();
    return 0;
}

static rt_err_t rtp_close(struct rt_device *dev)
{
	rt_size_t ret = 0;
    if (dev == NULL)
    {
        while (1);
    }

	hal_tpadc_exit();

	esKRNL_SemDel(rtpdata->data_sem, OS_DEL_ALWAYS,NULL);
	rtpdata->data_sem = NULL;
	rt_free(rtpdata);
    rtpdata = NULL;
	init_flag = 0;
    return 0;
}

static rt_err_t rtp_init(struct rt_device *dev)
{
    return 0;
}

static rt_size_t rtp_read(struct rt_device *dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    return 0;
}

static rt_size_t rtp_write(struct rt_device *dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    return 0;
}

static rt_err_t rtp_control(struct rt_device *dev, int cmd, void *args)
{
	if (dev == NULL)
    {
        __err("dev = NULL\n");
        while (1);
    }

    __u64 para[3] = {0};
    int ret = -1;
    if (args != NULL)
    {
        para[0] = *((__u64 *)args);                //argc0
        para[1] = *((__u64 *)((__u64)args + 8));   //argc1
        para[2] = *((__u64 *)((__u64)args + 16));   //argc2
    }

	switch(cmd)
	{
        case 0:
           ret = tp_adjust_func(para[0],para[1]);
            break;
	}
    return ret;
}

static int init_tpadc_rtp_device(struct rt_device *dev)
{
    rt_err_t ret = -1;
    
    if(!dev)
    {
        return ret;
    }
    dev->init        = rtp_init;
    dev->open        = rtp_open;
    dev->close       = rtp_close;
    dev->read        = rtp_read;
    dev->write       = rtp_write;
    dev->control     = rtp_control;

    ret = rt_device_register(dev, "tpadc_rtp", RT_DEVICE_FLAG_RDWR);
    if (ret != 0)
    {
        __err("fatal error, ret %ld, register device to rtsystem failure.\n", ret);
        software_break();
        return ret;
    }

    return ret;
}

int init_tpadc_rtp(void)
{
    struct rt_device *device;

    device = rt_device_create(RT_Device_Class_Char, 0);
    if (!device)
    {
        __wrn("init_tpadc_rtp err");
        return -1;
    }
    init_tpadc_rtp_device(device);
	return 0;
}
device_initcall(init_tpadc_rtp);