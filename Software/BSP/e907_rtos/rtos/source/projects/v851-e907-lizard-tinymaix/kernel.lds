/*
 * The OUTPUT_ARCH command specifies the machine architecture where the
 * argument is one of the names used in the T-HEAD library.
 */
OUTPUT_ARCH("riscv")
OUTPUT_FORMAT("elf32-littleriscv","elf64-littleriscv","elf32-littleriscv")

MEMORY
{
   /*DRAM_KERNEL: 4M */
   DRAM_SEG_KRN (rwx) : ORIGIN = 0x43c00000, LENGTH = 0x00400000
}

PHDRS
{
    sbi     PT_LOAD FLAGS(5); /* PF_R|PF_X */
    boot    PT_LOAD FLAGS(5); /* PF_R|PF_X */
    text    PT_LOAD FLAGS(5); /* PF_R|PF_X */
    rodata  PT_LOAD FLAGS(4); /* PF_R */
    data    PT_LOAD FLAGS(6); /* PF_R|PF_W */
    note    PT_NOTE FLAGS(4); /* PF_R */
    debug   PT_NOTE FLAGS(0); /* PF_R */
}

/* This area could be used for idle thead at last.*/
__STACKSIZE__ = 4096;

/* Get Kernel Running Address */
__DRAM_KRN_RUN_ADDRESS = ORIGIN(DRAM_SEG_KRN);

/* 0x9002 is insn "ebreak". */
ENTRY(_start)
SECTIONS
{
    . = __DRAM_KRN_RUN_ADDRESS;
    PROVIDE(_firmware_start = .);

    .head.text __DRAM_KRN_RUN_ADDRESS : AT(__DRAM_KRN_RUN_ADDRESS)
    {
        . = ALIGN(8);
        KEEP(*(.start))
        . = ALIGN(8);
    } > DRAM_SEG_KRN :boot =0x9002

    .dram_seg.text ADDR(.head.text) + SIZEOF(.head.text) : AT(LOADADDR(.head.text) + SIZEOF(.head.text))
    {
        . = ALIGN(8);
        PROVIDE(__code_start = ABSOLUTE(.));
        *(.init)
        *(.text)
        *(.text.*)
        *(.text*)
        *(.stub)
        *(.eh_frame_hdr)
        *(.eh_frame_entry)
        *(.gnu.warning)
        *(.gnu.linkonce.t*)
        *(.glue_7)
        *(.glue_7t)
        *(.gcc_except_table)
        *(.jcr)
        *(.fini)
        *(.reuse)
        KEEP(*(.eh_frame))
        . = ALIGN(8);
        PROVIDE(__code_end = ABSOLUTE(.));
    } > DRAM_SEG_KRN :text =0x9002

    PROVIDE(__readonly_area_start = .);
    .dram_seg.rodata ADDR(.dram_seg.text) + SIZEOF(.dram_seg.text) : AT(LOADADDR(.dram_seg.text) + SIZEOF(.dram_seg.text))
    {
        . = ALIGN(8);
        *(.rodata)
        *(.rodata.*)
        *(.rodata*)
        *(.srodata)
        *(.srodata.*)
        *(.srodata*)
        *(.gnu.linkonce.r*)
        *(.rel.dyn*)
        *(.flash)
        KEEP(*.o(.openocd_support))
        KEEP(*.o(.ramdisk))
        KEEP(*.o(.dtbcfgs))
        . = ALIGN(8);
    } > DRAM_SEG_KRN :rodata =0x9002

    .dram_seg.initcall ADDR(.dram_seg.rodata) + SIZEOF(.dram_seg.rodata) : AT(LOADADDR(.dram_seg.rodata) + SIZEOF(.dram_seg.rodata))
    {
        . = ALIGN(8);
        __initcall_start = .;
        KEEP(*(.initcallearly.init))
        __initcall0_start = .;
        KEEP(*(.initcall0.init))
        KEEP(*(.initcall0s.init))
        __initcall1_start = .;
        KEEP(*(.initcall1.init))
        KEEP(*(.initcall1s.init))
        __initcall2_start = .;
        KEEP(*(.initcall2.init))
        KEEP(*(.initcall2s.init))
        __initcall3_start = .;
        KEEP(*(.initcall3.init))
        KEEP(*(.initcall3s.init))
        __initcall4_start = .;
        KEEP(*(.initcall4.init))
        KEEP(*(.initcall4s.init))
        __initcall5_start = .;
        KEEP(*(.initcall5.init))
        KEEP(*(.initcall5s.init))
        __initcallrootfs_start = .;
        KEEP(*(.initcallrootfs.init))
        KEEP(*(.initcallrootfss.init))
        __initcall6_start = .;
        KEEP(*(.initcall6.init))
        KEEP(*(.initcall6s.init))
        __initcall7_start = .;
        KEEP(*(.initcall7.init))
        KEEP(*(.initcall7s.init))
        __initcall_end = .;
        __con_initcall_start = .;
        KEEP(*(.con_initcall.init))
        __con_initcall_end = .;
        . = ALIGN(8);
    } > DRAM_SEG_KRN :rodata =0x9002

    .note.gnu.build-id ADDR(.dram_seg.initcall) + SIZEOF(.dram_seg.initcall) : AT(LOADADDR(.dram_seg.initcall) + SIZEOF(.dram_seg.initcall))
    {
        . = ALIGN(8);
        *(.note.gnu.build-id)
        . = ALIGN(8);
    } > DRAM_SEG_KRN :rodata =0x9002

    .dram_seg.ctors ADDR(.note.gnu.build-id) + SIZEOF(.note.gnu.build-id) : AT(LOADADDR(.note.gnu.build-id) + SIZEOF(.note.gnu.build-id))
    {
        . = ALIGN(8);
        PROVIDE(__ctors_start__ = .);
        KEEP(*(SORT(.ctors.*)))
        KEEP(*(SORT(.ctors)))
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array))
        PROVIDE(__ctors_end__ = .);
        . = ALIGN(0x8);
    } > DRAM_SEG_KRN :rodata =0x9002

    .dram_seg.dtors ADDR(.dram_seg.ctors) + SIZEOF(.dram_seg.ctors) : AT(LOADADDR(.dram_seg.ctors) + SIZEOF(.dram_seg.ctors))
    {
        . = ALIGN(8);
        PROVIDE(__dtors_start__ = .);
        KEEP(*(SORT(.dtors.*)))
        KEEP(*(SORT(.dtors)))
        KEEP (*(SORT(.fini_array.*)))
        KEEP (*(.fini_array))
        PROVIDE(__dtors_end__ = .);
        . = ALIGN(0x8);
    } > DRAM_SEG_KRN :rodata =0x9002

    PROVIDE(__readonly_area_end = .);

    .dram_seg.data ADDR(.dram_seg.dtors) + SIZEOF(.dram_seg.dtors) : AT(LOADADDR(.dram_seg.dtors) + SIZEOF(.dram_seg.dtors))
    {
        . = ALIGN(8);
        KEEP(*(*.vectors*))
        *(.data)
        *(.data.*)
        *(.readmostly.data)

        *(.data1)
        *(.data1.*)

        /*Best in the middle of data/sdata area.*/
        PROVIDE( __global_pointer$ = . + 0x400);

        __tdata_start = .;
        *(.tdata)
        *(.tdata.*)
        *(.gnu.linkonce.td.*)
        __tdata_end = .;

        *(.gnu.linkonce.d.*)
        *(.sdata)
        *(.sdata.*)
        *(.sdata*)
        *(.gnu.linkonce.s.*)
        *(.sdata2)
        *(.sdata2.*)
        *(.gnu.linkonce.s2.*)
        *(.got.plt)
        *(.got)

        . = ALIGN(8);
        __fsymtab_start = .;
        KEEP(*(FSymTab))
        __fsymtab_end = .;
        . = ALIGN(8);

        . = ALIGN(8);
        __vsymtab_start = .;
        KEEP(*(VSymTab))
        __vsymtab_end = .;
        . = ALIGN(8);
    } > DRAM_SEG_KRN :data =0x9002

    /* stack for bringup process */
    .dram_seg.stack ADDR(.dram_seg.data) + SIZEOF(.dram_seg.data) : AT(LOADADDR(.dram_seg.data) + SIZEOF(.dram_seg.data))
    {
        . = ALIGN(8);
        __init_process_stack_start__ = .;
        . += __STACKSIZE__;
        __init_process_stack_end__ = .;
        . = ALIGN(8);
    } > DRAM_SEG_KRN :data

    .dram_seg.bss ADDR(.dram_seg.stack) + SIZEOF(.dram_seg.stack) + SIZEOF(.dram_seg.stack) (NOLOAD) :
    {
        . = ALIGN(8);
        *(.bss)
        *(.bss.*)
        *(.gnu.linkonce.b.*)
        *(.sbss)
        *(.sbss.*)
        *(.sbss*)
        *(.gnu.linkonce.sb.*)
        *(.sbss2)
        *(.sbss2.*)
        *(.gnu.linkonce.sb2.*)
        *(.scommon)
        *(.dynbss)
        *(COMMON)
        . = ALIGN(8);
        __tbss_start = .;
        *(.tbss)
        *(.tbss.*)
        *(.gnu.linkonce.tb.*)
        . = ALIGN(8);
        __tbss_end = .;
    } > DRAM_SEG_KRN :data

    PROVIDE(__bss_start = ADDR(.dram_seg.bss));
    PROVIDE(__bss_end = ABSOLUTE(.));
    PROVIDE(_end = ABSOLUTE(.));
    PROVIDE(_firmware_end = .);

    .note 0 : { *(.note)  } :note
    .stab 0 : { *(.stab)  } :note
    .stabstr 0 : { *(.stabstr)  } :note
    .stab.excl 0 : { *(.stab.excl)  } :note
    .stab.exclstr 0 : { *(.stab.exclstr)  } :note
    .stab.index 0 : { *(.stab.index)  } :note
    .stab.indexstr 0 : { *(.stab.indexstr)  } :note
    .reginfo 0 : { *(.reginfo) } :note
    .comment 0 : { *(.comment) } :note
    .mdebug  0 : { *(.mdebug)  } :note
    /* DWARF 1 */
    .debug          0 : { *(.debug) }
    .line           0 : { *(.line) }
    /* GNU DWARF 1 extensions */
    .debug_srcinfo  0 : { *(.debug_srcinfo) }
    .debug_sfnames  0 : { *(.debug_sfnames) }
    /* DWARF 1.1 and DWARF 2 */
    .debug_aranges  0 : { *(.debug_aranges) }
    .debug_pubnames 0 : { *(.debug_pubnames) }
    /* DWARF 2 */
    .debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) }
    .debug_abbrev   0 : { *(.debug_abbrev) }
    .debug_line     0 : { *(.debug_line) }
    .debug_frame    0 : { *(.debug_frame) }
    .debug_str      0 : { *(.debug_str) }
    .debug_loc      0 : { *(.debug_loc) }
    .debug_macinfo  0 : { *(.debug_macinfo) }
    /* SGI/MIPS DWARF 2 extensions */
    .debug_weaknames 0 : { *(.debug_weaknames) }
    .debug_funcnames 0 : { *(.debug_funcnames) }
    .debug_typenames 0 : { *(.debug_typenames) }
    .debug_varnames  0 : { *(.debug_varnames) }
    /* DWARF 3 */
    .debug_pubtypes 0 : { *(.debug_pubtypes) }
    .debug_ranges   0 : { *(.debug_ranges) }
    .riscv.attributes 0 : { KEEP (*(.riscv.attributes)) KEEP (*(.gnu.attributes)) }
    /DISCARD/ : { *(.note.GNU-stack) }
}
