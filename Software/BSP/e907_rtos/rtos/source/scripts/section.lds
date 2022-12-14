/*
 * =====================================================================================
 *
 *       Filename:  section.lds
 *
 *    Description:  common sections definition for each module. 
 *
 *        Version:  Melis3.0 
 *         Create:  2017-11-15 17:26:13
 *       Revision:  none
 *       Compiler:  gcc version 6.3.0 (crosstool-NG crosstool-ng-1.23.0)
 *
 *         Author:  linzhihang@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2019-02-26 12:50:14
 *
 * =====================================================================================
 */

SECTIONS
{
    .mod.text __DRAM0_MOD_BASE : AT(__DRAM0_MOD_BASE)
    {
        *(.text)
        *(.text.*)
        *(.stub)
        *(.gnu.warning)
        *(.gnu.linkonce.t*)
        . = ALIGN(4);
    } > dram0_2_seg 
    
    .mod.rodata  ADDR(.mod.text) + SIZEOF(.mod.text) : AT(LOADADDR(.mod.text) + SIZEOF(.mod.text))
    {
        *(.rodata)
        *(.rodata.*)
        *(.gnu.linkonce.r*)
        . = ALIGN(4);
    } > dram0_2_seg
    
    .mod.data  ADDR(.mod.rodata) + SIZEOF(.mod.rodata) : AT(LOADADDR(.mod.rodata) + SIZEOF(.mod.rodata))
    {
        *(.data) 
        *(.data.*) 
        *(.gnu.linkonce.d.*)
        *(.sdata) 
        *(.sdata.*) 
        *(.gnu.linkonce.s.*)
        *(.sdata2) 
        *(.sdata2.*) 
        *(.gnu.linkonce.s2.*)
        . = ALIGN(4);
    } > dram0_2_seg
    
    .mod.bss ADDR(.mod.data) + SIZEOF(.mod.data) (NOLOAD) :
    {
        __bss_start = ABSOLUTE(.);
        *(.bss)
        *(.bss.*)
        *(.gnu.linkonce.b.*)
        *(.sbss)
        *(.sbss.*)
        *(.gnu.linkonce.sb.*)
        *(.sbss2)
        *(.sbss2.*)
        *(.gnu.linkonce.sb2.*)
        *(COMMON)
        *(.scommon)
        . = ALIGN(4);
    } > dram0_2_seg

    __bss_end = ABSOLUTE(.);
    _end = ABSOLUTE(.);

    MAGIC (INFO) :
    {
        KEEP(*.o(.magic))
    } > magic_info_seg

    .note 0 : { *(.note) }
    .stab 0 : { *(.stab) }
    .stabstr 0 : { *(.stabstr)  }
    .stab.excl 0 : { *(.stab.excl)  }
    .stab.exclstr 0 : { *(.stab.exclstr)  }
    .stab.index 0 : { *(.stab.index)  }
    .stab.indexstr 0 : { *(.stab.indexstr)  }
    .mdebug  0 : { *(.mdebug)  }
    .reginfo 0 : { *(.reginfo) }
    .comment 0 : { *(.comment) }
    .ARM.attributes 0 : { *(.ARM.attributes) }
    .riscv.attributes 0 : { *(.riscv.attributes) }
    /DISCARD/ : { *(.note.GNU-stack) *(.ARM.exidx* .gnu.linkonce.armexidx.*) }
}
