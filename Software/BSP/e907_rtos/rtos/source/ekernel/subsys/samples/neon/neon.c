/*
 * ===========================================================================================
 *
 *       Filename:  neon.c
 *
 *    Description:  neon useage.
 *
 *        Version:  Melis3.0
 *         Create:  2020-03-13 10:49:09
 *       Revision:  none
 *       Compiler:  GCC:version 7.2.1 20170904 (release),ARM/embedded-7-branch revision 255204
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2020-03-20 09:04:00
 *
 * ===========================================================================================
 */
#include <rtthread.h>
#include <arm_neon.h>
#include <dfs_posix.h>
#include <finsh_api.h>
#include <finsh.h>
#include <debug.h>
#include <log.h>

uint32_t kernel_neon_begin(void);
void kernel_neon_end(uint32_t saved_fpexc);

static float sum_array(float *arr, int len)
{
    uint32_t flags;
    if (NULL == arr || len < 1)
    {
        __err("parameter error.");
        return 0;
    }

    flags = kernel_neon_begin();
    int dim4 = len >> 2; // 数组长度除4整数
    int left4 = len & 3; // 数组长度除4余数
    float32x4_t sum_vec = vdupq_n_f32(0.0);//定义用于暂存累加结果的寄存器且初始化为0
    for (; dim4 > 0; dim4--, arr += 4) //每次同时访问4个数组元素
    {
        float32x4_t data_vec = vld1q_f32(arr); //依次取4个元素存入寄存器vec
        sum_vec = vaddq_f32(sum_vec, data_vec);//ri = ai + bi 计算两组寄存器对应元素之和并存放到相应结果
    }
    float sum = vgetq_lane_f32(sum_vec, 0) + vgetq_lane_f32(sum_vec, 1) + vgetq_lane_f32(sum_vec, 2) + vgetq_lane_f32(sum_vec, 3); //将累加结果寄存器中的所有元素相加得到最终累加值
    for (; left4 > 0; left4--, arr++)
    {
        sum += (*arr) ;    //对于剩下的少于4的数字，依次计算累加即可
    }
    kernel_neon_end(flags);
    return sum;
}

static void init_array(float *arr, int len)
{
    int i = 0;

    for (i = 0; i < len; i ++)
    {
        arr[i] = (i * 1000) / len * 0.38;
    }
}

static void neon1_entry(void *para)
{
    float array[123];

    init_array(array, 123);

    while (1)
    {
        float sum = sum_array(array, 123);
        printf("[DBG]: %s line %d, float sum %f, size %d.\n", __func__, __LINE__, sum, sizeof(__simd64_float32_t));
    }
}

static void neon2_entry(void *para)
{
    float array[255];

    init_array(array, 255);

    while (1)
    {
        float sum = sum_array(array, 255);
        printf("[DBG]: %s line %d, float sum %f, size %d.\n", __func__, __LINE__, sum, sizeof(__simd64_float32_t));
    }
}

static void armneon_entry(void)
{
    rt_thread_t neon_thread1, neon_thread2;

    rt_enter_critical();
    neon_thread1 = rt_thread_create("neon1", neon1_entry, RT_NULL, 0x1000, 1, 10);
    RT_ASSERT(neon_thread1 != RT_NULL);
    rt_thread_startup(neon_thread1);

    neon_thread2 = rt_thread_create("neon2", neon2_entry, RT_NULL, 0x1000, 1, 10);
    RT_ASSERT(neon_thread2 != RT_NULL);
    rt_thread_startup(neon_thread2);
    rt_exit_critical();

    return;
}


static int cmd_armneon(int argc, const char **argv)
{
    armneon_entry();
    return 0;
}

FINSH_FUNCTION_EXPORT_ALIAS(cmd_armneon, __cmd_armneon, armneon test);

