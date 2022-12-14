/*
 * ===========================================================================================
 *
 *       Filename:  pipe_test.c
 *
 *    Description:  pipe test case.
 *
 *        Version:  Melis3.0
 *         Create:  2020-03-11 09:26:14
 *       Revision:  none
 *       Compiler:  GCC:version 7.2.1 20170904 (release),ARM/embedded-7-branch revision 255204
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2020-03-25 14:52:13
 *
 * ===========================================================================================
 */
#include <rtthread.h>
#include <dfs_posix.h>
#include <finsh_api.h>
#include <finsh.h>
#include <debug.h>
#include <pipe.h>
#include <log.h>

unsigned long dma_coherent_alloc(unsigned long *virt_addr, unsigned long size);
void dma_coherent_free(unsigned long virt_addr);

static void dma_alloc_entry(void)
{
    unsigned long phy, virt;
    
    phy = dma_coherent_alloc(&virt, 0x100000);

    if(phy == 0 || virt == 0)
    {
        __err("failure.");
        software_break();
    }

    printf("virt = 0x%08lx, phy = 0x%08lx\n", virt, phy);
    dma_coherent_free(virt);
    return;
}

static int cmd_dma_alloc(int argc, const char **argv)
{
    dma_alloc_entry();
    return 0;
}

FINSH_FUNCTION_EXPORT_ALIAS(cmd_dma_alloc, __cmd_dma_alloc, dma alloc test);
