	.section ".piggydata", "a"
	.globl	input_data
input_data:
	.incbin	"ekernel/arch/riscv/compressed/piggy_data"
	.globl	input_data_end
input_data_end:
