#include <stdio.h>
#include <rtthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define SUNXI_CCMU_BASE_ADDR       (0x03001000)
#define CCMU_DDR0_REG              (SUNXI_CCMU_BASE_ADDR + 0x0010)
#define OS_CLK_24MHz               (24)
#define CPU_CLK_DIV_RATION_N_BIT                (8)
#define CPU_CLK_DIV_RATION_N_MASK           (0xFF)
#define CPU_CLK_DIV_RATION_N                    (8)

#define CPU_CLK_DIV_RATION_P_BIT                (16)
#define CPU_CLK_DIV_RATION_P_MASK           (0x3)
#define CPU_CLK_DIV_RATION_P                    (2)

#define DDR_CLK_DIV_RATION_N_BIT                (8)
#define DDR_CLK_DIV_RATION_N_MASK           (0xFF)
#define DDR_CLK_DIV_RATION_N                    (8)

#define DDR_CLK_DIV_RATION_M1_BIT               (1)
#define DDR_CLK_DIV_RATION_M1_MASK          (0x1)
#define DDR_CLK_DIV_RATION_M1                   (1)

#define DDR_CLK_DIV_RATION_M0_BIT               (0)
#define DDR_CLK_DIV_RATION_M0_MASK          (0x1)
#define DDR_CLK_DIV_RATION_M0                   (1)

int cpu_clock(int argc, char **argv)
{
    unsigned long reg_val;
    unsigned long n, p;
    unsigned long p_value = 0, n_value;
    unsigned long cpu_clk;
    reg_val = *((volatile unsigned long *)(SUNXI_CCMU_BASE_ADDR));

    n = (reg_val & (((CPU_CLK_DIV_RATION_N_MASK << CPU_CLK_DIV_RATION_N) - 1)) << CPU_CLK_DIV_RATION_N_BIT) >> CPU_CLK_DIV_RATION_N_BIT;
    p = (reg_val & (((CPU_CLK_DIV_RATION_P_MASK << CPU_CLK_DIV_RATION_P) - 1)) << CPU_CLK_DIV_RATION_P_BIT) >> CPU_CLK_DIV_RATION_P_BIT;

    switch (p)
    {
        case 0:
            p_value = 1;
            break;
        case 1:
            p_value = 2;
            break;
        case 2:
            p_value = 4;
            break;
        default:
	    break;
    }

    n_value = n + 1;

    cpu_clk = OS_CLK_24MHz * (n_value) / p_value;

    //printf("reg_val=0x%08lx, n= 0x%08lx, p= 0x%08lx\n", reg_val, n, p);
    printf("cpu_clock = %ld(MHz)\n", cpu_clk);
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cpu_clock, __cmd_cpu, get cpu clock);


int ddr_clock(int argc, char **argv)
{
    unsigned long reg_val;
    unsigned long n, m0, m1;
    unsigned long n_value, m0_value, m1_value;
    unsigned long ddr_clk;
    reg_val = *((volatile unsigned long *)(CCMU_DDR0_REG));

    n = (reg_val & (((DDR_CLK_DIV_RATION_N_MASK << DDR_CLK_DIV_RATION_N) - 1)) << DDR_CLK_DIV_RATION_N_BIT) >> DDR_CLK_DIV_RATION_N_BIT;
    m0 = (reg_val & (((DDR_CLK_DIV_RATION_M0_MASK << DDR_CLK_DIV_RATION_M0) - 1)) << DDR_CLK_DIV_RATION_M0_BIT) >> DDR_CLK_DIV_RATION_M0_BIT;
    m1 = (reg_val & (((DDR_CLK_DIV_RATION_M1_MASK << DDR_CLK_DIV_RATION_M1) - 1)) << DDR_CLK_DIV_RATION_M1_BIT) >> DDR_CLK_DIV_RATION_M1_BIT;

    n_value = n + 1;
    m0_value = m0 + 1;
    m1_value = m1 + 1;

    ddr_clk = OS_CLK_24MHz * (n_value) / m0_value / m1_value;

    //printf("reg_val=0x%08lx, n= 0x%08lx, m0= 0x%08lx, m1= 0x%08lx\n", reg_val, n, m0, m1);
    printf("ddr_clock = %ld(MHz)\n", ddr_clk);
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(ddr_clock, __cmd_ddr, get ddr clock);
