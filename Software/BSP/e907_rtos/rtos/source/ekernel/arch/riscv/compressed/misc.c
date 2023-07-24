/*
 * misc.c
 *
 */

unsigned int __machine_arch_type;

#include <compiler.h>	/* for inline */
#include <stdint.h>

void putstr(const char *ptr);
extern void error(char *x);
extern int backtrace(void);

#include "uncompress.h"
#include "../../../components/thirdparty/decompress/unaligned.h"

#ifdef CONFIG_KERNEL_COMPRESS_DEBUG
void putstr(const char *ptr)
{
	char c;

	while ((c = *ptr++) != '\0') {
		if (c == '\n')
			putc('\r');
		putc(c);
	}

	flush();
}
#else
void putstr(const char *ptr)
{
	(void)ptr;
}
#endif

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

    str[8] = '\0';

    return str;
}

/*
 * gzip declarations
 */
extern char input_data[];
extern char input_data_end[];

unsigned char *output_data;

extern unsigned long free_mem_ptr;
extern unsigned long free_mem_end_ptr;

void error(char *x)
{
	putstr("\n\n");
#ifdef CONFIG_KERNEL_COMPRESS_DEBUG
	backtrace();
#endif
	putstr(x);
	putstr("\n\n -- System halted");

	while(1);	/* Halt */
}

void __div0(void)
{
	error("Attempting division by 0!");
}

unsigned long __stack_chk_guard;

void __stack_chk_guard_setup(void)
{
	__stack_chk_guard = 0x000a0dff;
}

void __stack_chk_fail(void)
{
	error("stack-protector: Kernel stack is corrupted\n");
}

extern int do_decompress(uint8_t *input, int len, uint8_t *output, void (*error)(char *x));
int load_elf_image(unsigned long img_addr);
unsigned long elf_get_entry_addr(unsigned long base);

static inline uint64_t arch_counter_get_cntvct(void)
{
#ifdef CONFIG_ARM
	uint64_t cval;

	isb();
	asm volatile("mrrc p15, 1, %Q0, %R0, c14" : "=r" (cval));
#else
	uint64_t cval;
	uint32_t hi, lo;

	isb();
	asm volatile("rdtimeh %0" : "=r" (hi):: "memory");
	asm volatile("rdtime  %0" : "=r" (lo):: "memory");
	cval = hi;
	cval = (cval << 32) | lo;
#endif
	return cval;
}

char str[16];
void
decompress_kernel(unsigned long output_start, unsigned long free_mem_ptr_p,
		unsigned long free_mem_ptr_end_p,
		int arch_id)
{
	int ret;
	uint64_t decompress_start_ms;
	uint32_t decompress_done_ms;
	__stack_chk_guard_setup();

	output_data		= (unsigned char *)output_start;
	free_mem_ptr		= free_mem_ptr_p;
	free_mem_end_ptr	= free_mem_ptr_end_p;
	__machine_arch_type	= arch_id;

	arch_decomp_setup();

	decompress_start_ms = arch_counter_get_cntvct();
	putstr("\r\nUncompressing Melis: addr 0x");
	putstr(long2str(output_start, str));
	putstr("\r\n");

	ret = do_decompress(input_data, input_data_end - input_data,
			    output_data, error);
	decompress_done_ms = (uint32_t)(arch_counter_get_cntvct() - decompress_start_ms)/24;
	str[0] = decompress_done_ms/1000000%10 + '0';
	str[1] = decompress_done_ms/100000%10 + '0';
	str[2] = decompress_done_ms/10000%10 + '0';
	str[3] = decompress_done_ms/1000%10 + '0';
	str[4] = '.';
	str[5] = decompress_done_ms/100%10 + '0';
	str[6] = decompress_done_ms/10%10 + '0';
	str[7] = decompress_done_ms%10 + '0';
	str[8] = 'm';
	str[9] = 's';
	str[10] = '\r';
	str[11] = '\n';
	str[12] = 0;
	putstr("\r\nUncompressing Melis...\r\n");
	putstr(str);
	if (ret)
		error("decompressor returned an error");
	else
		putstr(" done, booting the Melis.\n");
}
