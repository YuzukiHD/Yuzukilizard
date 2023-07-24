/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2010-03-22     Bernard      first version
 */
#include "finsh_ops.h"
#include "finsh_vm.h"

#define OP_BIN_BYTE(x)  do {\
        (finsh_sp - 2)->char_value = (finsh_sp - 2)->char_value x (finsh_sp - 1)->char_value; \
        finsh_sp--; \
    }while (0)

#define OP_BIN_WORD(x)  do {\
        (finsh_sp - 2)->short_value = (finsh_sp - 2)->short_value x (finsh_sp - 1)->short_value; \
        finsh_sp--; \
    }while (0)

#define OP_BIN_DWORD(x)  do {\
        (finsh_sp - 2)->long_value = (finsh_sp - 2)->long_value x (finsh_sp - 1)->long_value; \
        finsh_sp--; \
    }while (0)

/* --- noop --- */
void OP_no_op(void)
{
    /* none */
    return ;
}

/* --- add --- */
void OP_add_byte(void)
{
    OP_BIN_BYTE(+);

    return ;
}

void OP_add_word(void)
{
    OP_BIN_WORD(+);

    return ;
}

void OP_add_dword(void)
{
    OP_BIN_DWORD(+);

    return ;
}

/* --- sub --- */
void OP_sub_byte(void)
{
    OP_BIN_BYTE(-);

    return ;
}

void OP_sub_word(void)
{
    OP_BIN_WORD(-);

    return ;
}

void OP_sub_dword(void)
{
    OP_BIN_DWORD(-);

    return ;
}

/* --- div --- */
void OP_div_byte(void)
{
    OP_BIN_BYTE(/);

    return ;
}

void OP_div_word(void)
{
    OP_BIN_WORD(/);

    return ;
}

void OP_div_dword(void)
{
    OP_BIN_DWORD(/);

    return ;
}

/* --- mod --- */
void OP_mod_byte(void)
{
    OP_BIN_BYTE(%);

    return ;
}

void OP_mod_word(void)
{
    OP_BIN_WORD(%);

    return ;
}

void OP_mod_dword(void)
{
    OP_BIN_DWORD(%);

    return ;
}

/* --- mul --- */
void OP_mul_byte(void)
{
    OP_BIN_BYTE(*);

    return ;
}

void OP_mul_word(void)
{
    OP_BIN_WORD(*);

    return ;
}

void OP_mul_dword(void)
{
    OP_BIN_DWORD(*);

    return ;
}

/* --- and --- */
void OP_and_byte(void)
{
    OP_BIN_BYTE(&);

    return ;
}

void OP_and_word(void)
{
    OP_BIN_WORD(&);

    return ;
}

void OP_and_dword(void)
{
    OP_BIN_DWORD(&);

    return ;
}

/* --- or --- */
void OP_or_byte(void)
{
    OP_BIN_BYTE(|);

    return ;
}

void OP_or_word(void)
{
    OP_BIN_WORD(|);

    return ;
}

void OP_or_dword(void)
{
    OP_BIN_DWORD(|);

    return ;
}

/* --- xor --- */
void OP_xor_byte(void)
{
    OP_BIN_BYTE(^);

    return ;
}

void OP_xor_word(void)
{
    OP_BIN_WORD(^);

    return ;
}

void OP_xor_dword(void)
{
    OP_BIN_DWORD(^);

    return ;
}

/* --- bw --- */
void OP_bw_byte(void)
{
    (finsh_sp - 1)->char_value = ~ ((finsh_sp - 1)->char_value);

    return ;
}

void OP_bw_word(void)
{
    (finsh_sp - 1)->short_value = ~ ((finsh_sp - 1)->short_value);

    return ;
}

void OP_bw_dword(void)
{
    (finsh_sp - 1)->long_value = ~ ((finsh_sp - 1)->long_value);

    return ;
}

/* --- shl --- */
void OP_shl_byte(void)
{
    OP_BIN_BYTE(<<);

    return ;
}

void OP_shl_word(void)
{
    OP_BIN_WORD(<<);

    return ;
}

void OP_shl_dword(void)
{
    OP_BIN_DWORD(<<);

    return ;
}

/* --- shr --- */
void OP_shr_byte(void)
{
    OP_BIN_BYTE(>>);

    return ;
}

void OP_shr_word(void)
{
    OP_BIN_WORD(>>);

    return ;
}

void OP_shr_dword(void)
{
    OP_BIN_DWORD(>>);

    return ;
}

/* --- ld --- */
void OP_ld_byte(void)
{
    finsh_sp->char_value = *finsh_pc;

    finsh_sp++;
    finsh_pc++;

    return ;
}

void OP_ld_word(void)
{
    finsh_sp->short_value = FINSH_GET16(finsh_pc);

    finsh_sp ++;
    finsh_pc += 2;

    return ;
}

void OP_ld_dword(void)
{
    finsh_sp->long_value = FINSH_GET32(finsh_pc);

    finsh_sp ++;
    finsh_pc += 4;

    return ;
}

void OP_ld_value_byte(void)
{
    char* c;

    c = (char*) (FINSH_GET32(finsh_pc));

    finsh_sp->char_value = *c;

    finsh_sp ++;
    finsh_pc += 4;

    return;
}

void OP_ld_value_byte_stack(void)
{
    char* c;

    c = (char *)(finsh_sp - 1)->long_value;
    (finsh_sp - 1)->char_value = *c;

    return;
}

void OP_ld_value_word(void)
{
    short* s;

    s = (short*) (FINSH_GET32(finsh_pc));

    finsh_sp->short_value = *s;

    finsh_sp ++;
    finsh_pc += 4;

    return;
}

void OP_ld_value_word_stack(void)
{
    short* s;

    s = (short *)(finsh_sp - 1)->long_value;
    (finsh_sp - 1)->short_value = *s;

    return;
}

void OP_ld_value_dword(void)
{
    long* l;

    l = (long*) (FINSH_GET32(finsh_pc));

    finsh_sp->long_value = *l;

    finsh_sp ++;
    finsh_pc += 4;

    return;
}

void OP_ld_value_dword_stack(void)
{
    long* l;

    l = (long *)(finsh_sp - 1)->long_value;
    (finsh_sp - 1)->long_value = *l;

    return;
}

/* --- st --- */
/*
 * 2006-4-16 bernard
 * fixed the sp move bug
 */
void OP_st_byte(void)
{
    *(char*)((finsh_sp - 2)->long_value) = (finsh_sp - 1)->char_value;
    finsh_sp --;

    return ;
}

/*
 * 2006-4-16 bernard
 * fixed the sp move bug
 */
void OP_st_word(void)
{
    *(short*)((finsh_sp - 2)->long_value) = (finsh_sp - 1)->short_value;
    finsh_sp --;

    return ;
}

/*
 * 2006-4-16 bernard
 * fixed the sp move bug
 */
void OP_st_dword(void)
{
    *(long*)((finsh_sp - 2)->long_value) = (finsh_sp - 1)->long_value;
    finsh_sp --;

    return ;
}

/* --- pop --- */
void OP_pop(void)
{
    finsh_sp --;
    return ;
}

/* --- call --- */
void OP_call(void)
{
    /* the max number of arg*/
    unsigned long parameterv[16];
    unsigned int  parameters, i;

    typedef unsigned long var_t;

    typedef var_t (*op_func_0)(unsigned long);
    typedef var_t (*op_func_1)(unsigned long);
    typedef var_t (*op_func_2)(unsigned long, unsigned long);
    typedef var_t (*op_func_3)(unsigned long, unsigned long, unsigned long);
    typedef var_t (*op_func_4)(unsigned long, unsigned long, unsigned long, unsigned long);
    typedef var_t (*op_func_5)(unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);
    typedef var_t (*op_func_6)(unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);
    typedef var_t (*op_func_7)(unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);
    typedef var_t (*op_func_8)(unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);
    typedef var_t (*op_func_9)(unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);
    typedef var_t (*op_func_10)(unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);
    typedef var_t (*op_func_11)(unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);
    typedef var_t (*op_func_12)(unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);
    typedef var_t (*op_func_13)(unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);
    typedef var_t (*op_func_14)(unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);
    typedef var_t (*op_func_15)(unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);
    typedef var_t (*op_func_16)(unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);

    op_func_0 f_0;
    op_func_1 f_1;
    op_func_2 f_2;
    op_func_3 f_3;
    op_func_4 f_4;
    op_func_5 f_5;
    op_func_6 f_6;
    op_func_7 f_7;
    op_func_8 f_8;
    op_func_9 f_9;
    op_func_10 f_10;
    op_func_11 f_11;
    op_func_12 f_12;
    op_func_13 f_13;
    op_func_14 f_14;
    op_func_15 f_15;
    op_func_16 f_16;
    var_t   r;

    parameters = *finsh_pc ++;

    i = 0; finsh_sp --;
    while (i < parameters)
    {
        parameterv[parameters - 1 - i] = finsh_sp->long_value;
        finsh_sp --;
        i++;
    }

    f_0 = (op_func_0)(finsh_sp->long_value);
    f_1 = (op_func_1)(finsh_sp->long_value);
    f_2 = (op_func_2)(finsh_sp->long_value);
    f_3 = (op_func_3)(finsh_sp->long_value);
    f_4 = (op_func_4)(finsh_sp->long_value);
    f_5 = (op_func_5)(finsh_sp->long_value);
    f_6 = (op_func_6)(finsh_sp->long_value);
    f_7 = (op_func_7)(finsh_sp->long_value);
    f_8 = (op_func_8)(finsh_sp->long_value);
    f_9 = (op_func_9)(finsh_sp->long_value);
    f_10 = (op_func_10)(finsh_sp->long_value);
    f_11 = (op_func_11)(finsh_sp->long_value);
    f_12 = (op_func_12)(finsh_sp->long_value);
    f_13 = (op_func_13)(finsh_sp->long_value);
    f_14 = (op_func_14)(finsh_sp->long_value);
    f_15 = (op_func_15)(finsh_sp->long_value);
    f_16 = (op_func_16)(finsh_sp->long_value);
    switch (parameters)
    {
    case 0:
        r = f_0(0);
        break;

    case 1:
        r = f_1(parameterv[0]);
        break;

    case 2:
        r = f_2(parameterv[0], parameterv[1]);
        break;

    case 3:
        r = f_3(parameterv[0], parameterv[1], parameterv[2]);
        break;

    case 4:
        r = f_4(parameterv[0], parameterv[1], parameterv[2], parameterv[3]);
        break;

    case 5:
        r = f_5(parameterv[0], parameterv[1], parameterv[2], parameterv[3],
                parameterv[4]);
        break;

    case 6:
        r = f_6(parameterv[0], parameterv[1], parameterv[2], parameterv[3],
                parameterv[4], parameterv[5]);
        break;

    case 7:
        r = f_7(parameterv[0], parameterv[1], parameterv[2], parameterv[3],
                parameterv[4], parameterv[5], parameterv[6]);
        break;

    case 8:
        r = f_8(parameterv[0], parameterv[1], parameterv[2], parameterv[3],
                parameterv[4], parameterv[5], parameterv[6], parameterv[7]);
        break;

    case 9:
        r = f_9(parameterv[0], parameterv[1], parameterv[2], parameterv[3],
                parameterv[4], parameterv[5], parameterv[6], parameterv[7],
                parameterv[8]);
        break;

    case 10:
        r = f_10(parameterv[0], parameterv[1], parameterv[2], parameterv[3],
                parameterv[4], parameterv[5], parameterv[6], parameterv[7],
                parameterv[8], parameterv[9]);
        break;

    case 11:
        r = f_11(parameterv[0], parameterv[1], parameterv[2], parameterv[3],
                parameterv[4], parameterv[5], parameterv[6], parameterv[7],
                parameterv[8], parameterv[9], parameterv[10]);
        break;

    case 12:
        r = f_12(parameterv[0], parameterv[1], parameterv[2], parameterv[3],
                parameterv[4], parameterv[5], parameterv[6], parameterv[7],
                parameterv[8], parameterv[9], parameterv[10], parameterv[11]);
        break;

    case 13:
        r = f_13(parameterv[0], parameterv[1], parameterv[2], parameterv[3],
                parameterv[4], parameterv[5], parameterv[6], parameterv[7],
                parameterv[8], parameterv[9], parameterv[10], parameterv[11],
                parameterv[12]);
        break;

    case 14:
        r = f_14(parameterv[0], parameterv[1], parameterv[2], parameterv[3],
                parameterv[4], parameterv[5], parameterv[6], parameterv[7],
                parameterv[8], parameterv[9], parameterv[10], parameterv[11],
                parameterv[12], parameterv[13]);
        break;

    case 15:
        r = f_15(parameterv[0], parameterv[1], parameterv[2], parameterv[3],
                parameterv[4], parameterv[5], parameterv[6], parameterv[7],
                parameterv[8], parameterv[9], parameterv[10], parameterv[11],
                parameterv[12], parameterv[13], parameterv[14]);
        break;

    case 16:
        r = f_16(parameterv[0], parameterv[1], parameterv[2], parameterv[3],
                parameterv[4], parameterv[5], parameterv[6], parameterv[7],
                parameterv[8], parameterv[9], parameterv[10], parameterv[11],
                parameterv[12], parameterv[13], parameterv[14], parameterv[15]);
        break;

    default:
        r = 0;
        break;
    }

    finsh_sp->long_value = r;
    finsh_sp ++;

    return ;
}

const op_func op_table[] =
{
    /* 00 */ OP_no_op,
    /* 01 */ OP_add_byte,
    /* 02 */ OP_add_word,
    /* 03 */ OP_add_dword,
    /* 04 */ OP_sub_byte,
    /* 05 */ OP_sub_word,
    /* 06 */ OP_sub_dword,
    /* 07 */ OP_div_byte,
    /* 08 */ OP_div_word,
    /* 09 */ OP_div_dword,
    /* 10 */ OP_mod_byte,
    /* 11 */ OP_mod_word,
    /* 12 */ OP_mod_dword,
    /* 13 */ OP_mul_byte,
    /* 14 */ OP_mul_word,
    /* 15 */ OP_mul_dword,
    /* 16 */ OP_and_byte,
    /* 17 */ OP_and_word,
    /* 18 */ OP_and_dword,
    /* 19 */ OP_or_byte,
    /* 20 */ OP_or_word,
    /* 21 */ OP_or_dword,
    /* 22 */ OP_xor_byte,
    /* 23 */ OP_xor_word,
    /* 24 */ OP_xor_dword,
    /* 25 */ OP_bw_byte,
    /* 26 */ OP_bw_word,
    /* 27 */ OP_bw_dword,
    /* 28 */ OP_shl_byte,
    /* 29 */ OP_shl_word,
    /* 30 */ OP_shl_dword,
    /* 31 */ OP_shr_byte,
    /* 32 */ OP_shr_word,
    /* 33 */ OP_shr_dword,
    /* 34 */ OP_ld_byte,
    /* 35 */ OP_ld_word,
    /* 36 */ OP_ld_dword,
    /* 37 */ OP_ld_value_byte,
    /* 38 */ OP_ld_value_word,
    /* 39 */ OP_ld_value_dword,
    /* 40 */ OP_st_byte,
    /* 41 */ OP_st_word,
    /* 42 */ OP_st_dword,
    /* 43 */ OP_pop,
    /* 44 */ OP_call,
    /* 45 */ OP_ld_value_byte_stack,
    /* 46 */ OP_ld_value_word_stack,
    /* 47 */ OP_ld_value_dword_stack,
    NULL
};
