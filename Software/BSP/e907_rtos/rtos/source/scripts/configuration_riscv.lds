OUTPUT_FORMAT("elf64-littleriscv","elf64-littleriscv","elf64-littleriscv")
OUTPUT_ARCH("riscv")
SEARCH_DIR("=/usr/local/lib"); SEARCH_DIR("=/lib"); SEARCH_DIR("=/usr/lib");

__DRAM0_MOD_BASE = ORIGIN(dram0_2_seg);
__DRAM0_INF_BASE = ORIGIN(magic_info_seg);
