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
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

#include <excep.h>

//#define BACKTRACE_DEBUG 1

extern void putstr(const char *ptr);
#define print_func   putstr

#ifndef BACKTRACE_DEBUG
#define backtrace_debug(fmt, ...)
#else
#define backtrace_debug(fmt, ...) \
    printf("[%s:%d]:"fmt, __func__, __LINE__, ##__VA_ARGS__)
#endif

#define BT_SCAN_MAX_LIMIT   0x2000
#define BT_LEVEL_LIMIT      64

#define PC2ADDR(pc)          ((char *)(((uintptr_t)(pc)) & 0xfffffffe))

#define IS_VALID_TEXT_ADDRESS(pc)     backtrace_check_address((uintptr_t)(pc))

int32_t check_virtual_address(uintptr_t vaddr);

int backtrace_check_address(uintptr_t pc)
{
#ifdef CONFIG_SOC_SUN20IW1P1
    if (pc < (CONFIG_DRAM_PHYBASE + 64 * 1024))
    {
        return 0;
    }
    if (pc > (0xFFFFFFFF))
    {
        return 0;
    }
    return check_virtual_address(pc);
#else
    if ((pc > CONFIG_DRAM_PHYBASE) && (pc < (CONFIG_DRAM_PHYBASE + CONFIG_DRAM_SIZE)))
    {
        return 1;
    }
    return 0;
#endif
}

#define IS_COMPREEES_ADDR(pc)     ((uintptr_t)(pc) & 0x1)
#define MAKE_COMPRESSED_ADDR(pc)    do{(pc) = (void *)((uintptr_t)(pc) | 0x1);}while(0)

#define insn_length(x) \
    (((x) & 0x03) < 0x03 ? 2 : \
     ((x) & 0x1f) < 0x1f ? 4 : \
     ((x) & 0x3f) < 0x3f ? 6 : \
     8)

#define BITS(x, high, low) ((x) & (((1<<((high)-(low)+1))-1) << (low)))
#define BITS_SHIFT(x, high, low) (((x) >> (low)) & ((1<<((high)-(low)+1))-1))
#define SIGN_EXTEND(val, topbit) (BIT(val, topbit) ? ((val) | (0xffffffff << (topbit))) : (val))

typedef struct
{
    switch_ctx_regs_t regs_ctx;
} switch_context_t;

enum task_states
{
    TASK_SUSPEND = 0,
    TASK_READY,
    TASK_INTERRUPTED,
    TASK_RUNNING,
};

extern unsigned long riscv_cpu_handle_interrupt(unsigned long scause, unsigned long sepc, unsigned long stval, irq_regs_t *regs);

/*
 * convert long to string
 */
static char *long2str(long num, char *str)
{
    char         index[] = "0123456789ABCDEF";
    unsigned long usnum   = (unsigned long)num;

    str[7] = index[usnum % 16];
    usnum /= 16;
    str[6] = index[usnum % 16];
    usnum /= 16;
    str[5] = index[usnum % 16];
    usnum /= 16;
    str[4] = index[usnum % 16];
    usnum /= 16;
    str[3] = index[usnum % 16];
    usnum /= 16;
    str[2] = index[usnum % 16];
    usnum /= 16;
    str[1] = index[usnum % 16];
    usnum /= 16;
    str[0] = index[usnum % 16];
    usnum /= 16;

    return str;
}

static int find_lr_offset(char *LR)
{
    char *LR_fixed;
    unsigned short ins16;
    char backtrace_output_buf[] = "backtrace : 0X         \r\n";
    int offset = 4;
#ifdef CONFIG_SOC_SUN20IW1
    unsigned long *irq_entry = (unsigned long *)&riscv_cpu_handle_interrupt;
#else
    unsigned long *irq_entry = NULL;
#endif

    irq_entry ++;
    LR_fixed = LR;
    /* meet irq_entry, it is irq handler exit address. */
    if (LR_fixed == PC2ADDR(irq_entry))
    {
        long2str((long)irq_entry, &backtrace_output_buf[14]);
        print_func(backtrace_output_buf);
        return 0;
    }

    if (IS_VALID_TEXT_ADDRESS(LR_fixed - 4) == 0)
    {
        return 0;
    }

    ins16 = *(unsigned short *)(LR_fixed - 4);
    offset = insn_length(ins16);

    long2str((long)LR_fixed - offset, &backtrace_output_buf[14]);
    print_func(backtrace_output_buf);

    return offset;
}

int riscv_ins32_get_push_lr_framesize(unsigned int inst, int *offset)
{
    int ret = -1;

    backtrace_debug("inst:0x%x\n", inst);

    if ((inst & 0x01FFF07F) == 0x113023)
    {
        /* sd ra, (offset)sp  */
        int immed = (inst & 0xF80);
        immed >>= 7;
        immed |= ((inst & 0xFE000000) >> 25) << 5;
        if (((immed >> 11) & 0x01) != 0)
        {
            immed = 0xFFF - immed + 1;
        }
        *offset = immed / sizeof(long);
        ret = -1;
    }
    else if ((inst & 0x000FFFFF) == 0x10113)
    {
        /*  addi sp, sp, #imm  */
        int immed = BITS(inst, 31, 20);
        immed >>= 20;
        immed &= 0xFFF;
        if ((immed >> 11) != 0)
        {
            immed = 0xFFF - immed + 1;
            ret = 0;
        }
        else
        {
            ret = -1;
        }
    }
    else if ((inst & 0x000FFFFF) == 0x1011B)
    {
        /*  addiw sp, sp, #imm  */
        int immed = BITS(inst, 31, 20);
        immed >>= 20;
        immed &= 0xFFF;
        if ((immed >> 11) != 0)
        {
            immed = 0xFFF - immed + 1;
            ret = 0;
        }
        else
        {
            ret = -1;
        }
    }

    return ret;
}

static int riscv_ins16_get_push_lr_framesize(unsigned short inst, int *offset)
{
    int ret = -1;

    backtrace_debug("inst:0x%x: ", inst);

    if ((inst & 0xE07E) == 0xE006)
    {
        /* c.sdsp ra, (offset)sp  */
        int immed_6_8 = (inst >> 7) & 0x07 ;
        int immed_3_5 = (inst >> 10) & 0x07 ;
        int immed = immed_6_8 << 6 | immed_3_5 << 3;
        *offset = immed / sizeof(long);
        backtrace_debug("immed=%d ", immed);
        ret = -1;
    }
    else if ((inst & 0xE07E) == 0xC006)
    {
        /* c.swsp ra, (offset)sp  */
        int immed_6_7 = (inst >> 7) & 0x03 ;
        int immed_2_5 = (inst >> 9) & 0x0f ;
        int immed = immed_6_7 << 6 | immed_2_5 << 2;
        *offset = immed / sizeof(long);
        backtrace_debug("immed=%d ", immed);
        ret = -1;
    }
    else if ((inst & 0xEF83) == 0x6101)
    {
        /*  c.addi16sp #imm  */
        int immed_5 = (inst >> 2) & 0x01 ;
        int immed_7_8 = (inst >> 3) & 0x3 ;
        int immed_6 = (inst >> 5) & 0x1 ;
        int immed_4 = (inst >> 6) & 0x1 ;
        int immed_9 = (inst >> 12) & 0x1 ;
        int immed = immed_5 << 5 | immed_7_8 << 7 | immed_6 << 6 | immed_4 << 4 | immed_9 << 9;

        backtrace_debug("immed=%d ", immed);

        if ((immed >> 9) != 0)
        {
            immed = 0x3FF - immed + 1;
            ret = 0;
        }
        else
        {
            ret = -1;
        }
    }
    else if ((inst & 0xEF03) == 0x101)
    {
        /*  c.addi sp, sp, #imm  */
        int immed_0_4 = (inst >> 2) & 0x1F ;
        int immed_5 = (inst >> 12) & 0x1 ;
        int immed = immed_5 << 5 | immed_0_4;

        backtrace_debug("immed=%d ", immed);

        if ((immed >> 5) != 0)
        {
            immed = 0x3F - immed + 1;
            ret = 0;
        }
        else
        {
            ret = -1;
        }
    }
    else if ((inst & 0xEF03) == 0x2101)
    {
        /*  c.addiw sp, #imm  */
        int immed_0_4 = (inst >> 2) & 0x1F ;
        int immed_5 = (inst >> 12) & 0x1 ;
        int immed = immed_5 << 5 | immed_0_4;

        backtrace_debug("immed=%d ", immed);

        if ((immed >> 5) != 0)
        {
            immed = 0x3F - immed + 1;
            ret = 0;
        }
        else
        {
            ret = -1;
        }
    }
#if 0
    else if ((inst & 0xE01F) == 0x0)
    {
        /*  c.addi4spn #imm  */
        return 0;
    }
#endif
    backtrace_debug("ret = %d\n", ret);
    return ret;
}

static int riscv_ins32_backtrace_return_pop(unsigned int inst)
{
    backtrace_debug("inst:0x%x\n", inst);
#if 0
    /*  addi sp, sp, #imm  */
    if ((inst & 0x000FFFFF) == 0x10113)
    {
        int immed = BITS(inst, 31, 20);
        immed >>= 20;
        immed &= 0xFFF;
        if ((immed >> 11) != 0)
        {
            immed = 0xFFF - immed + 1;
        }
        return immed / sizeof(long);
    }
#endif
    /* ret */
    if ((inst) == 0x00008067)
    {
        return 0;
    }

    return -1;
}

static int riscv_ins16_backtrace_return_pop(unsigned short inst)
{
    int ret = -1;

    if ((inst) == 0x8082)
    {
        ret = 0;;
    }

    backtrace_debug("inst:0x%x, ret = %d\n", inst);
    return ret;
}

int riscv_ins32_backtrace_stask_push(unsigned int inst)
{
    int ret = -1;

    if ((inst & 0x000FFFFF) == 0x10113)
    {
        /*  addi sp, sp, #imm  */
        int immed = BITS(inst, 31, 20);
        immed >>= 20;
        immed &= 0xFFF;
        if ((immed >> 11) != 0)
        {
            immed = 0xFFF - immed + 1;
            ret = immed / sizeof(long);
        }
        else
        {
            ret = -1;
        }
    }
    else if ((inst & 0x000FFFFF) == 0x1011B)
    {
        /*  addiw sp, sp, #imm  */
        int immed = BITS(inst, 31, 20);
        immed >>= 20;
        immed &= 0xFFF;
        if ((immed >> 11) != 0)
        {
            immed = 0xFFF - immed + 1;
            ret = immed / sizeof(long);
        }
        else
        {
            ret = -1;
        }
    }

    backtrace_debug("inst:0x%x, ret = %d\n", inst, ret);

    return ret;
}
static int riscv_ins16_backtrace_stask_push(unsigned int inst)
{
    int ret = -1;

    if ((inst & 0xEF83) == 0x6101)
    {
        /*  c.addi16sp #imm  */
        int immed_4 = (inst >> 6) & 0x01;
        int immed_5 = (inst >> 2) & 0x01;
        int immed_6 = (inst >> 5) & 0x01;
        int immed_7_8 = (inst >> 3) & 0x3;
        int immed_9 = (inst >> 12) & 0x1;
        int immed = (immed_4 << 4) | (immed_5 << 5) | (immed_6 << 6) | (immed_7_8 << 7) | (immed_9 << 9);
        if ((immed >> 9) != 0)
        {
            immed = 0x3FF - immed + 1;
            ret = immed / sizeof(long);
        }
        else
        {
            ret = -1;
        }
    }
    else if ((inst & 0xEF03) == 0x101)
    {
        /*  c.addi sp, sp, #imm  */
        int immed_5 = (inst >> 12) & 0x01;
        int immed_0_4 = (inst >> 2) & 0x1F;
        int immed = (immed_0_4) | (immed_5 << 5);
        if ((immed >> 5) != 0)
        {
            immed = 0x3F - immed + 1;
            ret = immed / sizeof(long);
        }
        else
        {
            ret = -1;
        }
    }
    else if ((inst & 0xEF03) == 0x2101)
    {
        /*  c.addiw sp, #imm  */
        int immed_5 = (inst >> 12) & 0x01;
        int immed_0_4 = (inst >> 2) & 0x1F;
        int immed = (immed_0_4) | (immed_5 << 5);
        if ((immed >> 5) != 0)
        {
            immed = 0x3F - immed + 1;
            ret = immed / sizeof(long);
        }
        else
        {
            ret = -1;
        }
    }
#if 0
    else if ((inst & 0xE01F) == 0x0)
    {
        /*  c.addi4spn #imm  */
        return 0;
    }
#endif
    backtrace_debug("inst:0x%x, ret = %d\n", inst, ret);
    return ret;
}

static int riscv_ins32_backtrace_stack_pop(unsigned int inst)
{
    int ret = -1;

    /*  addi sp, sp, #imm  */
    if ((inst & 0x000FFFFF) == 0x10113)
    {
        int immed = BITS(inst, 31, 20);
        immed >>= 20;
        immed &= 0xFFF;
        if ((immed >> 11) != 0)
        {
            ret = -1;
        }
        else
        {
            immed = 0xFFF - immed + 1;
            ret = immed / sizeof(long);
        }
    }
    else if ((inst & 0x000FFFFF) == 0x1011B)
    {
        /*  addiw sp, sp, #imm  */
        int immed = BITS(inst, 31, 20);
        immed >>= 20;
        immed &= 0xFFF;
        if ((immed >> 11) != 0)
        {
            ret = -1;
        }
        else
        {
            immed = 0xFFF - immed + 1;
            ret = immed / sizeof(long);
        }
    }

    backtrace_debug("inst:0x%x\n", inst);

    return ret;
}

static int riscv_ins16_backtrace_stack_pop(unsigned short inst)
{
    int ret = -1;

    backtrace_debug("inst:0x%x\n", inst);

    if ((inst & 0xEF83) == 0x6101)
    {
        /*  c.addi16sp #imm  */
        int immed_4 = (inst >> 6) & 0x01;
        int immed_5 = (inst >> 2) & 0x01;
        int immed_6 = (inst >> 5) & 0x01;
        int immed_7_8 = (inst >> 3) & 0x3;
        int immed_9 = (inst >> 12) & 0x1;
        int immed = (immed_4 << 4) | (immed_5 << 5) | (immed_6 << 6) | (immed_7_8 << 7) | (immed_9 << 9);
        if ((immed >> 9) != 0)
        {
            immed = 0x3FF - immed + 1;
        }
        ret = immed / sizeof(long);
    }
    else if ((inst & 0xEF03) == 0x101)
    {
        /*  c.addi sp, sp, #imm  */
        int immed_5 = (inst >> 12) & 0x01;
        int immed_0_4 = (inst >> 2) & 0x1F;
        int immed = (immed_0_4) | (immed_5 << 5);
        if ((immed >> 5) != 0)
        {
            ret = -1;
        }
        else
        {
            immed = 0x3F - immed + 1;
        }
        ret = immed / sizeof(long);
    }
    else if ((inst & 0xEF03) == 0x2101)
    {
        /*  c.addiw sp, #imm  */
        int immed_5 = (inst >> 12) & 0x01;
        int immed_0_4 = (inst >> 2) & 0x1F;
        int immed = (immed_0_4) | (immed_5 << 5);
        if ((immed >> 5) != 0)
        {
            ret = -1;
        }
        else
        {
            immed = 0x3F - immed + 1;
        }
        ret = immed / sizeof(long);
    }
#if 0
    else if ((inst & 0xE01F) == 0x0)
    {
        /*  c.addi4spn #imm  */
        return 0;
    }
#endif
    backtrace_debug("inst:0x%x\n", inst);

    return ret;
}

static int riscv_backtrace_from_stack(long **pSP, char **pPC)
{
    char *parse_addr = NULL;
    long  *SP = *pSP;
    char *PC = *pPC;
    char *LR;
    int i;
    int temp;
    int framesize = 0;
    int offset = 0;
    unsigned int ins32 = 0;
    unsigned short ins16 = 0;
    unsigned short ins16_h = 0;
    unsigned short ins16_l = 0;
    char backtrace_output_buf[] = "backtrace : 0X         \r\n";

    for (i = 2; i < BT_SCAN_MAX_LIMIT; i += 2)
    {
        int result = 0;
        parse_addr = PC - i;
        if (IS_VALID_TEXT_ADDRESS(parse_addr) == 0)
        {
            print_func("backtrace fail!\r\n");
            return -1;
        }
        ins16_h = *(unsigned short *)parse_addr;

        if (IS_VALID_TEXT_ADDRESS(parse_addr - 2) == 0)
        {
            print_func("backtrace fail!\r\n");
            return -1;
        }
        ins16_l = *(unsigned short *)(parse_addr - 2);

        backtrace_debug("parse_addr = %p:", parse_addr);

        if (insn_length(ins16_l) == 4)
        {
            ins32 = (ins16_h << 16) | ins16_l;
            result = riscv_ins32_get_push_lr_framesize(ins32, &offset);
            i += 2;
        }
        else
        {
            ins16 = ins16_h;
            result = riscv_ins16_get_push_lr_framesize(ins16, &offset);
        }

        if (result >= 0)
        {
            break;
        }
    }

    parse_addr = PC - i;

    backtrace_debug("i = %d, parse_addr = %p, PC = %p, offset = %d\n", i, parse_addr, PC, offset);

    if (i == BT_SCAN_MAX_LIMIT)
    {
        print_func("backtrace fail!\r\n");
        return -1;
    }

    for (i = 0; parse_addr + i < PC; i += 2)
    {
        if (IS_VALID_TEXT_ADDRESS(parse_addr + i) == 0)
        {
            print_func("backtrace fail!\r\n");
            return -1;
        }
        ins16_l = *(unsigned short *)(parse_addr + i);

        if (IS_VALID_TEXT_ADDRESS(parse_addr + i + 2) == 0)
        {
            print_func("backtrace fail!\r\n");
            return -1;
        }
        ins16_h = *(unsigned short *)(parse_addr + i + 2);

        if (insn_length(ins16_l) == 4 || ins16_l == 0)
        {
            ins32 = (ins16_h << 16) | ins16_l;
            temp = riscv_ins32_backtrace_stask_push(ins32);
            i += 2;
        }
        else
        {
            ins16 = ins16_l;
            temp = riscv_ins16_backtrace_stask_push(ins16);
        }
        if (temp >= 0)
        {
            framesize += temp;
        }
    }

    backtrace_debug("i = %d, framesize = %d, SP = %p\n", i, framesize, SP);

    if (!offset)
    {
        return -1;
    }

    if (IS_VALID_TEXT_ADDRESS(SP + offset) == 0)
        print_func("backtrace : invalid lr\r\n");

    LR  = (char *) * (SP + offset);
    if (IS_VALID_TEXT_ADDRESS(LR) == 0)
    {
        print_func("backtrace : invalid lr\r\n");
        return -1;
    }
    *pSP   = SP + framesize;
    offset = find_lr_offset(LR);
    *pPC   = LR - offset;

    backtrace_debug("*pSP = %p, offset = %d, *pPC = %p\n", *pSP, offset, *pPC);

    return offset == 0 ? 1 : 0;
}

static int riscv_backtrace_from_lr(long **pSP, char **pPC, char *LR)
{
    long *SP = *pSP;
    char *PC = *pPC;
    char *parse_addr = NULL;
    int i;
    int temp;
    int framesize = 0;
    int offset;
    int result = 0;
    unsigned int ins32 = 0;
    unsigned short ins16 = 0;
    unsigned short ins16_h = 0;
    unsigned short ins16_l = 0;

    if (IS_VALID_TEXT_ADDRESS(PC) == 0)
    {
        if (IS_VALID_TEXT_ADDRESS(LR) == 0)
        {
            print_func("backtrace : invalid lr\r\n");
            return -1;
        }
        offset = find_lr_offset(LR);
        PC     = LR - offset;
        *pPC   = PC;
        return offset == 0 ? 1 : 0;
    }

    for (i = 0; i < BT_SCAN_MAX_LIMIT; i += 2)
    {
        parse_addr = PC + i;

        if (IS_VALID_TEXT_ADDRESS(parse_addr) == 0)
        {
            print_func("backtrace fail!\r\n");
            return -1;
        }

        if (IS_VALID_TEXT_ADDRESS(parse_addr + 2) == 0)
        {
            print_func("backtrace fail!\r\n");
            return -1;
        }

        ins16_l = *(unsigned short *)parse_addr;
        ins16_h = *(unsigned short *)(parse_addr + 2);

        if (insn_length(ins16_l) == 4 || ins16_l == 0)
        {
            ins32 = (ins16_h << 16) | ins16_l;
            result = riscv_ins32_backtrace_return_pop(ins32);
            i += 2;
            parse_addr -= 4;
        }
        else
        {
            ins16 = ins16_l;
            result = riscv_ins16_backtrace_return_pop(ins16);
            parse_addr -= 2;
        }

        if (result >= 0)
        {
            break;
        }
    }

    backtrace_debug("i = %d, parse_addr = %p, PC = %p, framesize = %d\n", i, parse_addr, PC, framesize);

    framesize = result;

    if (i == BT_SCAN_MAX_LIMIT)
    {
        print_func("Backtrace fail!\r\n");
        return -1;
    }

    for (i = 0; parse_addr - i >= PC; i += 2)
    {
        if (IS_VALID_TEXT_ADDRESS(parse_addr - i) == 0)
        {
            print_func("Backtrace fail!\r\n");
            return -1;
        }

        if (IS_VALID_TEXT_ADDRESS(parse_addr - i - 2) == 0)
        {
            print_func("Backtrace fail!\r\n");
            return -1;
        }

        ins16_l = *(unsigned short *)(parse_addr - i - 2);
        ins16_h = *(unsigned short *)(parse_addr - i);

        if (insn_length(ins16_l) == 4)
        {
            ins32 = (ins16_h << 16) | ins16_l;
            temp = riscv_ins32_backtrace_stack_pop(ins32);
            i += 2;
        }
        else
        {
            ins16 = ins16_h;
            temp = riscv_ins16_backtrace_stack_pop(ins16);
        }

        if (temp >= 0)
        {
            backtrace_debug("framesize add %d\n", temp);
            framesize += temp;
        }
    }

    backtrace_debug("i = %d, parse_addr = %p, PC = %p, SP = %p, framesize = %d\n", i, parse_addr, PC, framesize);

    if (IS_VALID_TEXT_ADDRESS(LR) == 0)
    {
        print_func("backtrace : invalid lr\r\n");
        return -1;
    }
    *pSP   = SP + framesize;
    offset = find_lr_offset(LR);
    *pPC   = LR - offset;

    backtrace_debug("*pSP = %p, offset = %d, *pPC = %p\n", *pSP, offset, *pPC);

    return offset == 0 ? 1 : 0;
}

static int backtrace_from_stack(long **pSP, char **pPC)
{
    if (IS_VALID_TEXT_ADDRESS(*pPC) == 0)
    {
        return -1;
    }

    return riscv_backtrace_from_stack(pSP, pPC);
}

static int backtrace_from_lr(long **pSP, char **pPC, char *LR)
{
    return riscv_backtrace_from_lr(pSP, pPC, LR);
}

/*
 * backtrace function
 */
int backtrace(void)
{
    char *PC = NULL;
    long  *SP = NULL;
    char *saved_pc;
    long  *saved_sp;
    unsigned long CPSR = 0;
    int   level;
    int   ret;
    char    *LR = NULL;
    char backtrace_output_buf[] = "backtrace : 0X         \r\n";

    __asm__ volatile("mv %0, sp\n" : "=r"(SP));
    __asm__ volatile("auipc %0, 0\n" : "=r"(PC));
    __asm__ volatile("mv %0, ra\n" : "=r"(LR));

    if (SP == NULL)
        return 0;

    ret = -1;
    level = 0;

    long2str((long)PC, &backtrace_output_buf[14]);
    print_func(backtrace_output_buf);

    for (level = 1; level < BT_LEVEL_LIMIT; level++)
    {
        ret = backtrace_from_stack(&SP, &PC);
        if (ret != 0)
            break;
    }

out:
    return level < 0 ? 0 : level ;
}
