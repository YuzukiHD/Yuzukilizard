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
#include <log.h>
#include <init.h>
#include <kapi.h>
#include <fcntl.h>
#include "mod_touchpanel.h"
#include <sunxi_hal_twi.h>
#include <dfs_posix.h>

#define MAX_FINGER_NUM 1				//最大支持手指数(<=5)
#define GTP_ADDR_LENGTH 2   //2 byte addr

//#define REG_MAX_NUM 186

//获取数据的地址
#define READ_TOUCH_ADDR_H   0x81
#define READ_TOUCH_ADDR_L   0x4E

#define RIGHTWARD						0x13
#define LEFTWARD						0x03
#define UPWARD							0x12
#define DOWNWARD						0x02

/*
typedef struct
{
	__u8* addr;
	__s32 len;
}iic_init_data_t;

typedef struct
{
    __u8* addr;
	__s32 len;
}iic_init_data_addr_t;
*/
typedef struct TP_PRIVATE_DATA_SET
{
    __u32             main_tid;           //主线程的id
    __krnl_event_t   *tp_sem_main;        //用于通知采集数据线程

    __s32             hIIC;				  //iic接口句柄
    __hdle			  hReset;			  //reset pin
    __hdle			  hInt;				  //int pin
    __u8              mapping_en;      //0: off    1: on 
    __s32             screen_max_x;
    __s32             screen_max_y;
}_tp_private_data_set_t;

/*
static __u8 g_iic_init_data_gt928[REG_MAX_NUM]=
{

	0x55,0x47,0x04,0x58,0x02,0x0A,0x3D,0x00,0x01,0x08,0x28,
	0x08,0x5F,0x50,0x03,0x05,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x15,0x16,0x17,0x14,0x89,0x29,0x0A,0x23,0x20,0xC1,
	0x0E,0x00,0x00,0x01,0x21,0x03,0x1D,0x00,0x01,0x00,0x00,
	0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x19,0x64,0x94,0xC5,
	0x02,0x08,0x00,0x00,0x00,0x8F,0x1C,0x00,0x79,0x22,0x00,
	0x63,0x2B,0x00,0x53,0x36,0x00,0x47,0x43,0x00,0x47,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x02,0x04,0x06,0x08,0x0A,0x0C,0x0E,0x10,0x12,
	0x14,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x22,
	0x21,0x20,0x1F,0x1E,0x1D,0x1C,0x18,0x16,0x12,0x10,0x0F,
	0x0A,0x08,0x06,0x04,0x02,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x9F,0x01

};
*/
static bool ctp_init_flag = 0;
static  __u32      modify_reg   = 0;/*tp配置寄存器开关，sys_config.fex 中配置时使用*/
static  __u32      pre_action   = 0;/* 前一次的动作类型 */
static  __u32      sid          = 0;/* sid--是否安装反了 */
static  __u16      ctp_twi_addr = 0;
static  __u32       x,y;
_tp_private_data_set_t  tp_private = {0};

extern int32_t console_LTSDevEvent(uint32_t type, uint32_t code, int32_t value);
static __s32 i2c_read(int f_i2c,__u16 slave_addr,__u8 *reg_addr, __s32 addr_len, __u8*data,__s32 n_byte);
static __s32 i2c_write(int f_i2c,__u16 slave_addr,__u8 *reg_addr, __s32 addr_len, __u8*data,__s32 n_byte);
static __s32 ctp_reset(void);
static void ctp_interrupt_callback(void);

/*
//init addr
static __u8 g_iic_init_addr_gt928[]=
{
	0x47, 0x80
};
*/
static __s32 iic_write_bytes_end_cmd(__u8*  addr, __s32 addr_len, __u8 *data, __s32 data_len)
{
    __s32 ret;

    ret = i2c_write(tp_private.hIIC,ctp_twi_addr>>1,addr, addr_len, data, data_len);
    if(EPDK_FAIL == ret)
    {
        __log("iic_write_bytes fail...\n");
        return EPDK_FAIL;
    }
    return EPDK_OK;
}
static __s32 iic_read_bytes_end_cmd(__u8*  addr, __s32 addr_len, __u8 *data, __s32 data_len)
{
    __s32 ret;

	ret = i2c_read(tp_private.hIIC,ctp_twi_addr>>1,addr, addr_len, data, data_len);
    if(EPDK_FAIL == ret)
    {
        __log("iic_read_bytes fail...\n");
        return EPDK_FAIL;
    }
   	
	return EPDK_OK;
}

__s32 i2c_read(int f_i2c,__u16 slave_addr,__u8 *reg_addr, __s32 addr_len, __u8*data,__s32 n_byte)
{
    ioctl(f_i2c, I2C_SLAVE, &slave_addr);
    
    {
        twi_msg_t msg;

        msg.flags  =  0;
        msg.addr   =  slave_addr;
        msg.flags &= ~(TWI_M_RD);
        msg.len    = addr_len;
        msg.buf    = reg_addr;
        ioctl(f_i2c, I2C_RDWR, &msg); 

        msg.flags  = 0;
        msg.addr   =  slave_addr;
        msg.flags |= (TWI_M_RD);
        msg.len    = n_byte;
        msg.buf    = data;
        ioctl(f_i2c, I2C_RDWR, &msg);
    }        
    return EPDK_OK;
}


__s32 i2c_write(int f_i2c,__u16 slave_addr,__u8 *reg_addr, __s32 addr_len, __u8*data,__s32 n_byte)
{
    ioctl(f_i2c, I2C_SLAVE, &slave_addr);

    {
        twi_msg_t msg; 
        int i =0 ;
        uint8_t *buf = malloc(n_byte+addr_len);
        if(buf == NULL)
        {
            return EPDK_FAIL;
        }
        
    	if(addr_len ==0)
    	{
        	for(i=0;i<n_byte;i++)
        	{
            	buf[i] = data[i];
        	}
    	}
    	else if(addr_len ==1)
    	{
    		buf[0] = reg_addr[0];

        	for(i=0;i<n_byte;i++)
        	{
            	buf[i+1] = data[i];
        	}
    	}
    	else if(addr_len == 2)
    	{
    		buf[0] = reg_addr[0];
    		buf[1] = reg_addr[1];

        	for(i=0;i<n_byte;i++)
        	{
            	buf[i+2] = data[i];
        	}
    	}
        msg.flags  = 0;
        msg.addr   =  slave_addr;
        msg.flags &= ~(TWI_M_RD);
        msg.len    = n_byte+addr_len;
        msg.buf    = buf;
        ioctl(f_i2c, I2C_RDWR, &msg); 
        
        free(buf);
    }    
	return EPDK_OK;
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
    __inf("type = %x, x = %d, y = %d, dir = %x , speed(zoom) = %d \n", msg.msg_type, msg.xpoint, msg.ypoint, msg.speed_dir, msg.speed_val);
}

static __s32 goodix_ts_version(void)
{
    __u8 buf[8]={0};

    buf[0] = 0x81;//	4;//gt827:0x7d

    buf[1] = 0x40;//0x81;  //gt827:0x0f 

    iic_read_bytes_end_cmd(buf, 2, buf+2, 3);

    __wrn("**************PID:%x, VID:%x%x****************", buf[2], buf[3], buf[4]);

    return 1;
}
/*
static iic_init_data_addr_t g_iic_init_data_addr[]=
{
   {g_iic_init_addr_gt928, sizeof(g_iic_init_addr_gt928)},//GT928 init addr
};

static iic_init_data_t g_iic_init_data[]=
{
	{g_iic_init_data_gt928, sizeof(g_iic_init_data_gt928)},//GT928
	
};
*/
static __s32 ctp_get_last_para(void)
{
    __s32 ret;

	if(EPDK_FAIL == esCFG_GetKeyValue("ctp_para", "ctp_modify_reg", (__s32 *)&modify_reg, 1))
	{
		__log("get cfg fail, tp_para, modify_reg\n");
		modify_reg = 0;
	}     
	__wrn("modify_reg=%d\n", modify_reg);

	if(EPDK_FAIL == esCFG_GetKeyValue("ctp_para", "ctp_exchange_x_y_flag", (__s32 *)&sid, 1))
	{
		__log("get cfg fail, tp_para, change_xy\n");
		sid = 0;
	}     
	__wrn("sid=%d\n", sid);

	if(EPDK_FAIL == esCFG_GetKeyValue("ctp_para", "ctp_screen_max_x", (__s32 *)&tp_private.screen_max_x, 1))
	{
		__log("get cfg fail, tp_para, screen_max_x\n");
		tp_private.screen_max_x = 800;
	} 

	if(EPDK_FAIL == esCFG_GetKeyValue("ctp_para", "ctp_screen_max_y", (__s32 *)&tp_private.screen_max_y, 1))
	{
		__log("get cfg fail, tp_para, screen_max_y\n");
		tp_private.screen_max_y = 480;
	} 

    return EPDK_OK;
}

static __s32 tp_clear_interrupt(void)
{
	__u8 touch_data[2 + 1] = {READ_TOUCH_ADDR_H,READ_TOUCH_ADDR_L, 0};
	__s32 ret;

	//touch_data[2] &= ~0xff;
	__msg("begin to clear data status now !\n");
	ret = iic_write_bytes_end_cmd(touch_data, 2, touch_data+2, 1);
	if(ret == EPDK_FAIL)
	{
		__log("clear data status fail !\n");
		return EPDK_FAIL;
	}
	return EPDK_OK;
}
/*
static __u8 reg_check_sum(__u8 init_data_addr[])
{
	__u32 i,len;
	__u8 check_sum = 0;

	len = sizeof(g_iic_init_data_gt928) - 2;
	//eLIBs_printf("L:%d len is %d\n",__LINE__,len);	
	for(i = 0; i < len; i++)
	{
		if(modify_reg)
		{
			check_sum += init_data_addr[i];
		}
		else	
		{
			check_sum += g_iic_init_data_gt928[i];
		}
	}
	__log("L:%d check_sum is 0x%x\n",__LINE__,check_sum);
	check_sum = ~(check_sum);
	__log("L:%d  f m check_sum is 0x%x\n",__LINE__,check_sum);//反码
	check_sum = check_sum + 1;
	__log("L:%d b m check_sum is 0x%x\n",__LINE__,check_sum);//补码
	return check_sum;
 }
*/

static __s32 ctp_board_init(void)
{
    __s32 ret = -1;
//	static __u8 g_iic_init_temp_data_gt928[REG_MAX_NUM]={0};

    ret = goodix_ts_version();
#if 0

    if(EPDK_FAIL == ret)
    {
        __log("ctp iic test once fail...\n");
    }
    else
    {
        __log("ctp iic test ok...\n");
        goto begin_write_init_data;
    }


begin_write_init_data:;

    {
        __u8* p_data_buf;
        __u32 data_addr;
	  

//        p_data_buf = g_iic_init_data[tp_private.type].addr;
//        data_addr = *(__u16*)(g_iic_init_data_addr[tp_private.type].addr);

        //modify int mode by config
        //modify resolation
        __log("tp_private.screen_max_x=%d\n", tp_private.screen_max_x);
        __log("tp_private.screen_max_y=%d\n", tp_private.screen_max_y);

    }
	if(modify_reg)
	{
		__s32 touch_level;
		
		memcpy(g_iic_init_temp_data_gt928,g_iic_init_data_gt928,g_iic_init_data[tp_private.type].len);
		if(EPDK_FAIL == esCFG_GetKeyValue("ctp_para", "touch_level", (__s32 *)&touch_level, 1))
		{
			__log("get cfg fail, tp_para, touch_level\n");
			touch_level = 0;
		}
		else
		{
			__log("touch_level = 0x%x \n",touch_level);
			g_iic_init_temp_data_gt928[12] = touch_level;
		}
		__log(" check_sum = 0x%x \n",g_iic_init_temp_data_gt928[REG_MAX_NUM-2] );
		g_iic_init_temp_data_gt928[REG_MAX_NUM-2] = reg_check_sum(g_iic_init_temp_data_gt928);
		__log(" check_sum = 0x%x \n",g_iic_init_temp_data_gt928[REG_MAX_NUM-2] );
		__log(" flush_flag = 0x%x \n",g_iic_init_temp_data_gt928[REG_MAX_NUM-1] );
		g_iic_init_temp_data_gt928[REG_MAX_NUM-1] = 0x1;
		__log(" flush_flag = 0x%x \n",g_iic_init_temp_data_gt928[REG_MAX_NUM-1] );
	}
	else
	{
#define CHECK_SUM
		//需要更改写校验码的寄存器时，可打开查看校验码结果
		//check_sum 为补码
#ifdef CHECK_SUM
		reg_check_sum(g_iic_init_data_gt928);
#endif
	}
#endif
__wrn("ctp reset func finish");
err_out:
    return ret;    
}

static void ctp_interrupt_callback(void)
{
    __s32 err;
     esPINS_DisbaleInt(tp_private.hInt); 
    __wrn("interrupt post sem..");
    esKRNL_SemPost(tp_private.tp_sem_main);
}

static __s32 ctp_reset(void)
{
	__s32 ret;
	user_gpio_set_t  gpio_set[1] = {0};  
	
	if(NULL == tp_private.hReset)
	{
		__log("reset pin is null...\n");
		return EPDK_FAIL;
	} 
	if(NULL == tp_private.hInt)
	{
		__log("int pin is null...\n");
		return EPDK_FAIL;
	}

	ret = esPINS_WritePinData(tp_private.hInt, 0, NULL);
	if(EPDK_FAIL == ret)
	{
		__log("1788 int fail...\n");
		return EPDK_FAIL;
	}
	ret = esPINS_WritePinData(tp_private.hReset, 0, NULL);
	if(EPDK_FAIL == ret)
	{
		__log("1794 reset fail...\n");
		return EPDK_FAIL;
	}
	//INT 和RESET 同为低保持10ms+100us,最少为20ms
	esKRNL_TimeDly(2);
	//之后拉高REST,INT继续保持低,选择IIC 地址为0xBA/0xBB
	ret = esPINS_WritePinData(tp_private.hReset, 1, NULL);
	if(EPDK_FAIL == ret)
	{
		__log("1803 reset fail...\n");
		return EPDK_FAIL;
	}
	ret = esPINS_WritePinData(tp_private.hInt, 0, NULL);
	if(EPDK_FAIL == ret)
	{
		__log("1809 int fail...\n");
		return EPDK_FAIL;
	}
	//REST保持拉高至少5ms
	esKRNL_TimeDly(1);
	//设置INT 为中断状态,设置为输入状态主控不会进中断
	esPINS_SetPinIO(tp_private.hInt, 0, NULL);   //input
	ret = esCFG_GetKeyValue("ctp_para", "ctp_int_port", (int *)gpio_set, sizeof(user_gpio_set_t)/4);
	if (EPDK_FAIL == ret)
	{
		__log("read cfg file fail:tp_para int...\n");
		return EPDK_FAIL;
	}
	else
	{
        __wrn("name :%s  port:%d  portnum:%d   mux:0x%x",gpio_set[0].gpio_name,gpio_set[0].port,gpio_set[0].port_num,gpio_set[0].mul_sel);
		gpio_set[0].mul_sel     = 6;	
		gpio_set[0].pull	    = 1;
		gpio_set[0].drv_level   = 1;
		gpio_set[0].data	    = 1;	
		tp_private.hInt = esPINS_PinGrpReq(gpio_set, 1);
		if (!tp_private.hInt)
		{
			__log("request tp_int pin failed\n");
			return EPDK_FAIL;
		}
	}	
       //设置完成后需延时50ms 以上才能发送配置信息
	esKRNL_TimeDly(5);
	__wrn("ctp_reset ok...\n");
	return EPDK_OK;
}

__s32 ctp_iic_dev_cfg(void)
{
    __s32 twi_no;
    char twi_path[20] = {0};

    if(EPDK_FAIL == esCFG_GetKeyValue("ctp_para", "ctp_twi_id", (__s32 *)&twi_no, 1))
    {
        __log("get cfg fail, tp_para, ctp_twi_id\n");
        twi_no = 0;
        return EPDK_FAIL;
    }
    
    __wrn("twi no : %d",twi_no);
    sprintf(twi_path, "/dev/twi%d", twi_no);
    __wrn("twi path : %s",twi_path);

	tp_private.hIIC = open(twi_path,O_RDWR);
	if(0 == tp_private.hIIC)
	{
		__log("open twi%d fail...\n",twi_no);
		return EPDK_FAIL;
	}

    if(EPDK_FAIL == esCFG_GetKeyValue("ctp_para", "ctp_twi_addr", (__s32 *)&ctp_twi_addr, 1))
    {
        __log("get cfg fail, tp_para, ctp_twi_id\n");
        ctp_twi_addr = 0;
        return EPDK_FAIL;
    }
    __wrn("ctp_twi_addr = 0x%x",ctp_twi_addr);    
    return EPDK_TRUE;
}

__s32 ctp_io_cfg(void)
{
    __s32 ret;
    user_gpio_set_t  gpio_set[1] = {0};

	ret = esCFG_GetKeyValue("ctp_para", "ctp_wakeup", (int *)gpio_set, sizeof(user_gpio_set_t)/4);
	if (EPDK_FAIL == ret)
	{
		__log("read cfg file fail:tp_para reset...\n");
		goto out;
	}
    tp_private.hReset = esPINS_PinGrpReq(gpio_set, 1);
    esPINS_SetPinIO(tp_private.hReset, 1, NULL);   //output
    esPINS_WritePinData(tp_private.hReset, 0, NULL);

	ret = esCFG_GetKeyValue("ctp_para", "ctp_int_port", (int *)gpio_set, sizeof(user_gpio_set_t)/4);
	if (EPDK_FAIL == ret)
	{
		__log("read cfg file fail:tp_para int...\n");
		goto out;
	}
    tp_private.hInt = esPINS_PinGrpReq(gpio_set, 1);
    esPINS_SetPinIO(tp_private.hInt, 1, NULL);   //output
    esPINS_WritePinData(tp_private.hInt, 0, NULL);

    ret = ctp_reset();    
	if (EPDK_FAIL == ret)
	{
		__log("ctp reset fail...\n");
		goto out;
	}

	ret = tp_clear_interrupt();
    if(tp_private.hInt)
    {
        esPINS_SetPinPull(tp_private.hInt,GPIO_PULL_UP,0);
        if(esPINS_SetIntMode(tp_private.hInt, IRQ_TYPE_EDGE_BOTH) == EPDK_OK)	//IRQ_TYPE_EDGE_FALLING
        {
            __wrn("set ENT as negative edge succeed...\n");
        }
        else
        {
            __wrn("set int mode failed!!!\n");
        }
        
        //register pin int handler
        ret = esPINS_RegIntHdler(tp_private.hInt, (__pCBK_t)ctp_interrupt_callback, NULL);
        if(ret == EPDK_OK)
        {
            __wrn("reg int hdl succeed...\n");
            ret = esPINS_EnbaleInt(tp_private.hInt); 
            if(ret == EPDK_OK)
            {
                tp_clear_interrupt();
                __wrn("enable int succeed...\n");
            }
            else
            {
            __log("enable int failed!!!\n");
            }
        }
        else
        {
        __log("reg int hdl failed!!!\n");
        }
	}
    ret = EPDK_TRUE;
out:
    return ret;
}

int check_tp_data_ready(void)
{
    __u8 touch_data[2 + 1 + 8*MAX_FINGER_NUM ] = {READ_TOUCH_ADDR_H,READ_TOUCH_ADDR_L, 0, 0};
    while(1)
    {
        iic_read_bytes_end_cmd(touch_data, 2, touch_data+2, 1);
        if(0x80 == (0x80&touch_data[2]))
        {
            __wrn("data ready ok...");
            return EPDK_TRUE;
        }
        esKRNL_TimeDly(1);
    }
    return EPDK_FAIL;

}

void read_tp_xy(void)
{
    __u8 touch_data[2 + 1 + 8*MAX_FINGER_NUM ] = {READ_TOUCH_ADDR_H,READ_TOUCH_ADDR_L, 0, 0};

    iic_read_bytes_end_cmd(touch_data, 2, touch_data+2, sizeof(touch_data)-2);
    if(sid)
    {
        y = (touch_data[5] << 8) | touch_data[4];
        x = (touch_data[7] << 8) | touch_data[6];
    }
    else
    {
        x = (touch_data[5] << 8) | touch_data[4];
        y = (touch_data[7] << 8) | touch_data[6];
    }
   // if((x || y ) && (x < tp_private.screen_max_x) && (y < tp_private.screen_max_y))
    __wrn("x = %d   y = %d",x,y);
}

static void  tp_task(void *p_arg)
{
    int ret;
    __s8 err;
    __ev_tp_msg_t msg;
    __u8 up_down;
    __u32 cnt = 0;
    __s32 first_x=0,first_y=0,last_x=0,last_y=0;
    __u32 move_flag=0,move_cnt=0;

    while(1)
    {

        esKRNL_SemPend(tp_private.tp_sem_main, 0, &err);
        ret = check_tp_data_ready();
        if(ret != EPDK_TRUE)
        {
            //esKRNL_TimeDly(1);
            esPINS_EnbaleInt(tp_private.hInt);
            continue;
        }
        read_tp_xy();
        cnt++;
        tp_clear_interrupt();
        esKRNL_TimeDly(2);
        if(cnt == 1)//down
        {
            __wrn("ctp first down!");
            msg.msg_type = EV_TP_PRESS_START;
            msg.xpoint = x;
            msg.ypoint = y;
            msg.speed_dir = 0;
            msg.speed_val = 0;
            tp_send_msg(msg);
            first_x = last_x = x;
            first_y = last_y= y;
            move_cnt = 0;
            move_flag = 0;
        }
        else
        {
            up_down = esPINS_ReadPinData(tp_private.hInt,NULL);
            __wrn("up_down = %s",up_down ? "up":"down");
            if(up_down)//up
            {
                cnt = 0;
                msg.msg_type = EV_TP_ACTION_NULL;
                msg.xpoint = x;
                msg.ypoint = y;
                if(move_flag)
                {
                    if(abs(first_x - msg.xpoint) > abs(first_y - msg.ypoint))
                    {
                        if((first_x - msg.xpoint) > 25)
                        {
                            msg.speed_dir = LEFTWARD;
                        }
                        else if((msg.xpoint - first_x) > 25)
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
                        if((first_y - msg.ypoint) > 25)
                        {
                            msg.speed_dir = UPWARD;
                        }
                        else if((msg.ypoint - first_y) > 25)
                        {
                            msg.speed_dir = DOWNWARD;
                        }
                        else
                        {
                            msg.speed_dir = 0;
                        }
                    }

                }
                else
                {
                    msg.speed_dir = 0;
                }
                msg.speed_val = 0;
                first_x = 0;
                first_y = 0;
                last_x  = 0;
                last_y  = 0;
                move_flag = 0;
                tp_send_msg(msg);
            }
            else//hold
            {
                if(abs(x - last_x) > 3 || abs(y - last_y) > 3)//move judge
                {
                        move_cnt++;
                        if(move_cnt > 3)//move
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
                msg.xpoint = x;
                msg.ypoint = y;
                msg.speed_dir = 0;
                msg.speed_val = 0;
                last_x  = x;
                last_y  = y;
                tp_send_msg(msg);
            }
        }
        esPINS_EnbaleInt(tp_private.hInt);
    }
}

rt_err_t ctp_open(struct rt_device *dev, rt_uint16_t oflag)
{
	__s32 ret = -1;

    if (dev == NULL)
    {
		__err("dev == NULL\n");
        return -1;
    }

	if(ctp_init_flag > 0)
	{
		return 0;
	}
	ctp_init_flag = 1;

    ret = ctp_iic_dev_cfg();
    if(ret < 0)
	{
        __err("ctp iic init fail\n");
		goto out;
	}

    ret = ctp_io_cfg();
    if(ret < 0)
	{
        __err("ctp io init fail\n");
		goto out;
	}

    ret = ctp_get_last_para();
    if(ret < 0)
	{
        __err("get sys config para fail\n");
		goto out;
	}
    ret = ctp_board_init();   
    if(EPDK_FAIL == ret)
    {
        __log("ctp_init fail...\n");
		goto out;
    }
    tp_private.tp_sem_main  = esKRNL_SemCreate(1);
    tp_private.main_tid = esKRNL_TCreate(tp_task, 0, 0x800, KRNL_priolevel1);
    esKRNL_TaskNameSet(tp_private.main_tid, "ctp_task");
    __log("ctp init finish");
out:
    return ret;
}

static rt_err_t ctp_close(struct rt_device *dev)
{
	rt_size_t ret = 0;
    if (dev == NULL)
    {
        while (1);
    }
    ctp_init_flag = 0;
    return 0;
}

static rt_err_t ctp_init1(struct rt_device *dev)
{
    return 0;
}

static rt_size_t ctp_read(struct rt_device *dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    return 0;
}

static rt_size_t ctp_write(struct rt_device *dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    return 0;
}

static rt_err_t ctp_control(struct rt_device *dev, int cmd, void *args)
{
	if (dev == NULL)
    {
        __err("dev = NULL\n");
        while (1);
    }
    int ret = -1;
    return ret;
}

static int init_ctp_device(struct rt_device *dev)
{
    rt_err_t ret = -1;
    if(!dev)
    {
        return ret;
    }
    //dev->init        = ctp_init;
    dev->open        = ctp_open;
    dev->close       = ctp_close;
    dev->read        = ctp_read;
    dev->write       = ctp_write;
    dev->control     = ctp_control;

    ret = rt_device_register(dev, "ctp", RT_DEVICE_FLAG_RDWR);
    if (ret != 0)
    {
        __err("fatal error, ret %ld, register device to rtsystem failure.\n", ret);
        while(1);
        return ret;
    }

    return ret;
}

int init_ctp(void)
{
    struct rt_device *device;

    device = rt_device_create(RT_Device_Class_Char, 0);
    if (!device)
    {
        __log("init_tpadc_ctp err");
        return -1;
    }
    init_ctp_device(device);
	return 0;
}
device_initcall(init_ctp);

#if 0
void ctp_test(int argc, char **argv)
{
    int fd = -1;

    fd = open("/dev/ctp", O_WRONLY);
    __log("open ctp  fd = %d",fd);
}
FINSH_FUNCTION_EXPORT_ALIAS(ctp_test, __cmd_ctp_test, ctp_test);
#endif