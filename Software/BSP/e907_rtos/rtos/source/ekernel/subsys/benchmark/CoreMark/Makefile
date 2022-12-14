PORT_CFLAGS = -O3 -funroll-all-loops -fgcse-sm -finline-limit=500 -fno-schedule-insns --param max-rtl-if-conversion-unpredictable-cost=100 -msignedness-cmpiv -fno-code-hoisting -mno-thread-jumps1 -mno-iv-adjust-addr-cost -mno-expand-split-imm -fno-tree-loop-distribution
FLAGS_STR = "$(PORT_CFLAGS) $(XCFLAGS) $(XLFLAGS) $(LFLAGS_END)"

ccflags-y = $(PORT_CFLAGS) -I$(PORT_DIR) -I. -DFLAGS_STR=\"$(FLAGS_STR)\" -DMEM_METHOD=0

subdir-ccflags-y :=     -I$(srctree)/include/melis/kernel \
    			-I$(obj)/arm \
    			-I$(obj) \

obj-y +=  core_list_join.o
obj-y +=  core_main.o
obj-y +=  core_state.o
obj-y +=  core_util.o
obj-y +=  core_matrix.o
obj-y +=  arm/core_portme.o
