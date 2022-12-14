/*
 * =====================================================================================
 *
 *       Filename:  wqtest.c
 *
 *    Description:  waitqueue test.
 *
 *        Version:  1.0
 *        Created:  2019年10月18日 14时08分13秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (),
 *   Organization:
 *
 * =====================================================================================
 */
#include <typedef.h>
#include <rtthread.h>
#include <waitqueue.h>
#include <module/mod_slib.h>
#include <finsh_api.h>
#include <finsh.h>
#include <stdio.h>
#include <rthw.h>
#include <arch.h>
#include <log.h>

#define TEST_ADDR0     (0xe2001000)
#define TEST_ADDR1     (0xe2700000)
static rt_wqueue_t wqueue_test;
static void memory_info(void)
{
    rt_uint32_t total, used, max_used, aval;
    rt_memory_info(&total, &used, &max_used);
    aval = total - used;
    printf("\tavalmemory 0x%x.\n", aval);
}

static void tlb_task(void *ARG_UNUSED(para))
{
    unsigned char *paddr0, *paddr1;
    void *module_alloc(void *target_vmaddr, unsigned long map_size);
    int module_free(void *target_vmaddr, unsigned long free_size);

    while (1)
    {
        paddr0 = module_alloc((unsigned char *)TEST_ADDR0, 1.5 * 1024 * 1024 - 0x1000);
        if (paddr0 != (unsigned char *)TEST_ADDR0)
        {
            printf("%s line %d, fatal error addr 0x%08x.\n", __func__, __LINE__,(unsigned int)paddr0);
            while (1);
        }

        int i = 0;
        for (i = 0; i < 1.5 * 1024 * 1024 - 0x1000; i ++)
        {
            paddr0[i] = 0xa5;
        }

        for (i = 0; i < 1.5 * 1024 * 1024 - 0x1000; i ++)
        {
            if (paddr0[i] != 0xa5)
            {
                printf("%s line %d, i = %d. fatal error, data 0x%02x.\n", __func__, __LINE__, i, paddr0[i]);
                while (1);
            }
        }

        paddr1 = module_alloc((unsigned char *)TEST_ADDR1, 0.5 * 1024 * 1024 + 0x1000);
        if (paddr1 != (unsigned char *)TEST_ADDR1)
        {
            printf("%s line %d, fatal error, addr1 0x%08x.\n", __func__, __LINE__, (unsigned int)paddr1);
            while (1);
        }

        for (i = 0; i < 0.5 * 1024 * 1024 + 0x1000; i ++)
        {
            paddr1[i] = 0x5a;
        }

        for (i = 0; i < 0.5 * 1024 * 1024 + 0x1000; i ++)
        {
            if (paddr1[i] != 0x5a)
            {
                printf("%s line %d, i = %d. fatal error, data 0x%02x.\n", __func__, __LINE__, i, paddr1[i]);
                while (1);
            }
        }
        printf("PASS:");
        memory_info();
        module_free((void *)TEST_ADDR0, 1.5 * 1024 * 1024 - 0x1000);
        module_free((void *)TEST_ADDR1 , 0.5 * 1024 * 1024 + 0x1000);
        memory_info();
        rt_thread_delay(10);
    }

    unsigned int *psrc = rt_malloc(4000);
    unsigned int *pdst = rt_malloc(4000);

    if (psrc == NULL || pdst == NULL)
    {
        printf("%s line %d. fatal error\n", __func__, __LINE__);
        while (1);
    }

    while (1)
    {
        int i  = 0;
        for (i = 0; i < 1000; i ++)
        {
            psrc[i] = 0x5a5a5a5a;
        }

        SLIB_memcpy(pdst, psrc, 4000);

        for (i = 0; i < 1000; i ++)
        {
            psrc[i] = 0x5a5a5a5a;
        }

        for (i = 0; i < 1000; i ++)
        {
            if (pdst[i] != 0x5a5a5a5a)
            {
                printf("%s line %d. fatal error\n", __func__, __LINE__);
                while (1);
            }
        }

        SLIB_memset(pdst, 0x00, 4000);

        printf("PASS: pdst 0x%p, psrc 0x%p\n", pdst, psrc);
        rt_thread_delay(10);
    }

}

/* ----------------------------------------------------------------------------*/
/* ----------------------------------------------------------------------------*/
static void tlbtest_entry(void)
{
    rt_thread_t thread;

    thread = rt_thread_create("tlb", tlb_task, RT_NULL, 0x1000, 1, 10);
    rt_thread_startup(thread);
}

static int cmd_tlbtest(int argc, const char **argv)
{
    tlbtest_entry();
    return 0;
}

FINSH_FUNCTION_EXPORT_ALIAS(cmd_tlbtest, __cmd_tlbtest, insmod test);
