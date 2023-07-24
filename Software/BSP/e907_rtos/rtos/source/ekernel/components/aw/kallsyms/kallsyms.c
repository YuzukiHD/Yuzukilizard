#include <kallsyms.h>
#include <stdio.h>
#include <string.h>

#ifdef CONFIG_KALLSYMS_ALL
#define all_var 1
#else
#define all_var 0
#endif
#ifndef BUG
#define BUG() do { \
        printk("BUG: failure at %s:%d/%s()!\n", __FILE__, __LINE__, __func__); \
        while(1); \
    } while (0)
#endif
#ifndef BUG_ON
#define BUG_ON(condition)       do { if (unlikely(condition)) BUG(); } while (0)
#endif

/*
 * These will be re-linked against their real values
 * during the second link stage.
 */
extern const unsigned long kallsyms_addresses[] __attribute__((weak));
extern const int kallsyms_offsets[] __attribute__((weak));
extern const uint8_t kallsyms_names[] __attribute__((weak));

/*
 * Tell the compiler that the count isn't in the small data section if the arch
 * has one (eg: FRV).
 */
extern const unsigned long kallsyms_num_syms
__attribute__((weak, section(".rodata")));

extern const unsigned long kallsyms_relative_base
__attribute__((weak, section(".rodata")));

extern const uint8_t kallsyms_token_table[] __attribute__((weak));
extern const uint16_t kallsyms_token_index[] __attribute__((weak));

extern const unsigned long kallsyms_markers[] __attribute__((weak));

extern char _firmware_start[], _firmware_end[];
extern char __code_start[], __code_end[];
extern char __readonly_area_start[], __readonly_area_end[];

static inline int is_kernel_inittext(unsigned long addr)
{
	if (addr >= (unsigned long)__readonly_area_start
	    && addr <= (unsigned long)__readonly_area_end)
		return 1;
	return 0;
}

static inline int is_kernel_text(unsigned long addr)
{
	if ((addr >= (unsigned long)__code_start && addr <= (unsigned long)__code_end))
		return 1;
	return 0;
}

static inline int is_kernel(unsigned long addr)
{
	if (addr >= (unsigned long)_firmware_start && addr <= (unsigned long)_firmware_end)
		return 1;
	return 0;
}

static int is_ksym_addr(unsigned long addr)
{
	if (all_var)
		return is_kernel(addr);

	return is_kernel_text(addr) || is_kernel_inittext(addr);
}

/*
 * Expand a compressed symbol data into the resulting uncompressed string,
 * if uncompressed string is too long (>= maxlen), it will be truncated,
 * given the offset to where the symbol is in the compressed stream.
 */
static unsigned int kallsyms_expand_symbol(unsigned int off,
					   char *result, size_t maxlen)
{
	int len, skipped_first = 0;
	const uint8_t *tptr, *data;

	/* Get the compressed symbol length from the first symbol byte. */
	data = &kallsyms_names[off];
	len = *data;
	data++;

	/*
	 * Update the offset to return the offset for the next symbol on
	 * the compressed stream.
	 */
	off += len + 1;

	/*
	 * For every byte on the compressed symbol data, copy the table
	 * entry for that byte.
	 */
	while (len) {
		tptr = &kallsyms_token_table[kallsyms_token_index[*data]];
		data++;
		len--;

		while (*tptr) {
			if (skipped_first) {
				if (maxlen <= 1)
					goto tail;
				*result = *tptr;
				result++;
				maxlen--;
			} else
				skipped_first = 1;
			tptr++;
		}
	}

tail:
	if (maxlen)
		*result = '\0';

	/* Return to offset to the next symbol. */
	return off;
}

/*
 * Get symbol type information. This is encoded as a single char at the
 * beginning of the symbol name.
 */
static char kallsyms_get_symbol_type(unsigned int off)
{
	/*
	 * Get just the first code, look it up in the token table,
	 * and return the first char from this token.
	 */
	return kallsyms_token_table[kallsyms_token_index[kallsyms_names[off + 1]]];
}


/*
 * Find the offset on the compressed stream given and index in the
 * kallsyms array.
 */
static unsigned int get_symbol_offset(unsigned long pos)
{
	const uint8_t *name;
	int i;

	/*
	 * Use the closest marker we have. We have markers every 256 positions,
	 * so that should be close enough.
	 */
	name = &kallsyms_names[kallsyms_markers[pos >> 8]];

	/*
	 * Sequentially scan all the symbols up to the point we're searching
	 * for. Every symbol is stored in a [<len>][<len> bytes of data] format,
	 * so we just need to add the len to the current pointer for every
	 * symbol we wish to skip.
	 */
	for (i = 0; i < (pos & 0xFF); i++)
		name = name + (*name) + 1;

	return name - kallsyms_names;
}

static unsigned long kallsyms_sym_address(int idx)
{
	if (!IS_ENABLED(CONFIG_KALLSYMS_BASE_RELATIVE))
		return kallsyms_addresses[idx];

	/* values are unsigned offsets if --absolute-percpu is not in effect */
	if (!IS_ENABLED(CONFIG_KALLSYMS_ABSOLUTE_PERCPU))
		return kallsyms_relative_base + (uint32_t)kallsyms_offsets[idx];

	/* ...otherwise, positive offsets are absolute values */
	if (kallsyms_offsets[idx] >= 0)
		return kallsyms_offsets[idx];

	/* ...and negative offsets are relative to kallsyms_relative_base - 1 */
	return kallsyms_relative_base - 1 - kallsyms_offsets[idx];
}

/* Lookup the address for this symbol. Returns 0 if not found. */
unsigned long kallsyms_lookup_name(const char *name)
{
	char namebuf[KSYM_NAME_LEN];
	unsigned long i;
	unsigned int off;

	for (i = 0, off = 0; i < kallsyms_num_syms; i++) {
		off = kallsyms_expand_symbol(off, namebuf, ARRAY_SIZE(namebuf));

		if (strcmp(namebuf, name) == 0)
			return kallsyms_sym_address(i);
	}
	/* TODO: Support lookup module symbol */
	return 0;
}

int kallsyms_on_each_symbol(int (*fn)(void *, const char *, unsigned long),
			    void *data)
{
	char namebuf[KSYM_NAME_LEN];
	unsigned long i;
	unsigned int off;
	int ret;

	for (i = 0, off = 0; i < kallsyms_num_syms; i++) {
		off = kallsyms_expand_symbol(off, namebuf, ARRAY_SIZE(namebuf));
		ret = fn(data, namebuf, kallsyms_sym_address(i));
		if (ret != 0)
			return ret;
	}
	/* TODO: Support iter each module symbol */
	return 0;
}

static unsigned long get_symbol_pos(unsigned long addr,
				    unsigned long *symbolsize,
				    unsigned long *offset)
{
	unsigned long symbol_start = 0, symbol_end = 0;
	unsigned long i, low, high, mid;

	/* This kernel should never had been booted. */
	if (!IS_ENABLED(CONFIG_KALLSYMS_BASE_RELATIVE))
		BUG_ON(!kallsyms_addresses);
	else
		BUG_ON(!kallsyms_offsets);

	/* Do a binary search on the sorted kallsyms_addresses array. */
	low = 0;
	high = kallsyms_num_syms;

	while (high - low > 1) {
		mid = low + (high - low) / 2;
		if (kallsyms_sym_address(mid) <= addr)
			low = mid;
		else
			high = mid;
	}

	/*
	 * Search for the first aliased symbol. Aliased
	 * symbols are symbols with the same address.
	 */
	while (low && kallsyms_sym_address(low-1) == kallsyms_sym_address(low))
		--low;

	symbol_start = kallsyms_sym_address(low);

	/* Search for next non-aliased symbol. */
	for (i = low + 1; i < kallsyms_num_syms; i++) {
		if (kallsyms_sym_address(i) > symbol_start) {
			symbol_end = kallsyms_sym_address(i);
			break;
		}
	}

	/* If we found no next symbol, we use the end of the section. */
	if (!symbol_end) {
		if (is_kernel_inittext(addr))
			symbol_end = (unsigned long)__readonly_area_end;
		else if (all_var)
			symbol_end = (unsigned long)_firmware_end;
		else
			symbol_end = (unsigned long)__code_end;
	}

	if (symbolsize)
		*symbolsize = symbol_end - symbol_start;
	if (offset)
		*offset = addr - symbol_start;

	return low;
}

/*
 * Lookup an address but don't bother to find any names.
 */
int kallsyms_lookup_size_offset(unsigned long addr, unsigned long *symbolsize,
				unsigned long *offset)
{
	char namebuf[KSYM_NAME_LEN];
	if (is_ksym_addr(addr))
		return !!get_symbol_pos(addr, symbolsize, offset);

	/* TODO: Support module address lookup */
	return 0;
}

#ifdef CONFIG_CFI_CLANG
/*
 * LLVM appends .cfi to function names when CONFIG_CFI_CLANG is enabled,
 * which causes confusion and potentially breaks user space tools, so we
 * will strip the postfix from expanded symbol names.
 */
static inline void cleanup_symbol_name(char *s)
{
	char *res;

	res = strrchr(s, '.');
	if (res && !strcmp(res, ".cfi"))
		*res = '\0';
}
#else
static inline void cleanup_symbol_name(char *s) {}
#endif

const char *kallsyms_lookup(unsigned long addr,
			    unsigned long *symbolsize,
			    unsigned long *offset,
			    char *namebuf)
{
	namebuf[KSYM_NAME_LEN - 1] = 0;
	namebuf[0] = 0;

	if (is_ksym_addr(addr)) {
		unsigned long pos;

		pos = get_symbol_pos(addr, symbolsize, offset);
		/* Grab name */
		kallsyms_expand_symbol(get_symbol_offset(pos),
				       namebuf, KSYM_NAME_LEN);
		goto found;
	}

	/* TODO: module_address_lookup */
	return NULL;

found:
	cleanup_symbol_name(namebuf);
	return namebuf;
}

int lookup_symbol_name(unsigned long addr, char *symname)
{
	int res;

	symname[0] = '\0';
	symname[KSYM_NAME_LEN - 1] = '\0';

	if (is_ksym_addr(addr)) {
		unsigned long pos;

		pos = get_symbol_pos(addr, NULL, NULL);
		/* Grab name */
		kallsyms_expand_symbol(get_symbol_offset(pos),
				       symname, KSYM_NAME_LEN);
		goto found;
	}

	/* TODO: lookup_module_symbol_name */
	return -ENONET;

found:
	cleanup_symbol_name(symname);
	return 0;
}

int lookup_symbol_attrs(unsigned long addr, unsigned long *size,
			unsigned long *offset, char *name)
{
	int res;

	name[0] = '\0';
	name[KSYM_NAME_LEN - 1] = '\0';

	if (is_ksym_addr(addr)) {
		unsigned long pos;

		pos = get_symbol_pos(addr, size, offset);
		/* Grab name */
		kallsyms_expand_symbol(get_symbol_offset(pos),
				       name, KSYM_NAME_LEN);
		goto found;
	}
	/* TODO: lookup_module_symbol_attrs */
	return -ENONET;

found:
	cleanup_symbol_name(name);
	return 0;
}

/* Look up a kernel symbol and return it in a text buffer. */
static int __sprint_symbol(char *buffer, unsigned long address,
			   int symbol_offset, int add_offset)
{
	const char *name;
	unsigned long offset, size;
	int len;

	address += symbol_offset;
	name = kallsyms_lookup(address, &size, &offset, buffer);
	if (!name)
		return sprintf(buffer, "0x%lx", address - symbol_offset);

	if (name != buffer)
		strcpy(buffer, name);
	len = strlen(buffer);
	offset -= symbol_offset;

	if (add_offset)
		len += sprintf(buffer + len, "+%#lx/%#lx", offset, size);

	return len;
}

/**
 * sprint_symbol - Look up a kernel symbol and return it in a text buffer
 * @buffer: buffer to be stored
 * @address: address to lookup
 *
 * This function looks up a kernel symbol with @address and stores its name,
 * offset, size and module name to @buffer if possible. If no symbol was found,
 * just saves its @address as is.
 *
 * This function returns the number of bytes stored in @buffer.
 */
int sprint_symbol(char *buffer, unsigned long address)
{
	return __sprint_symbol(buffer, address, 0, 1);
}

/**
 * sprint_symbol_no_offset - Look up a kernel symbol and return it in a text buffer
 * @buffer: buffer to be stored
 * @address: address to lookup
 *
 * This function looks up a kernel symbol with @address and stores its name
 * and module name to @buffer if possible. If no symbol was found, just saves
 * its @address as is.
 *
 * This function returns the number of bytes stored in @buffer.
 */
int sprint_symbol_no_offset(char *buffer, unsigned long address)
{
	return __sprint_symbol(buffer, address, 0, 0);
}

/**
 * sprint_backtrace - Look up a backtrace symbol and return it in a text buffer
 * @buffer: buffer to be stored
 * @address: address to lookup
 *
 * This function is for stack backtrace and does the same thing as
 * sprint_symbol() but with modified/decreased @address. If there is a
 * tail-call to the function marked "noreturn", gcc optimized out code after
 * the call so that the stack-saved return address could point outside of the
 * caller. This function ensures that kallsyms will find the original caller
 * by decreasing @address.
 *
 * This function returns the number of bytes stored in @buffer.
 */
int sprint_backtrace(char *buffer, unsigned long address)
{
	return __sprint_symbol(buffer, address, -1, 1);
}

/* Look up a kernel symbol and print it to the kernel messages. */
void __print_symbol(const char *fmt, unsigned long address)
{
	char buffer[KSYM_SYMBOL_LEN];

	sprint_symbol(buffer, address);

	printk(fmt, buffer);
}

/* This macro allows us to keep printk typechecking */
static void __check_printsym_format(const char *fmt, ...)
{
}

void print_symbol(const char *fmt, unsigned long addr)
{
	__check_printsym_format(fmt, "");
	__print_symbol(fmt, (unsigned long)
		       __builtin_extract_return_addr((void *)addr));
}

void print_ip_sym(unsigned long ip)
{
	printk("[<%p>] %pS\n", (void *) ip, (void *) ip);
}

static int kallsyms_init(void)
{
	return 0;
}
device_initcall(kallsyms_init);

#include <rtthread.h>
static int kallsyms_dump_one(void *data, const char *name, unsigned long addr)
{
	static int index = 1;
	printk("\t(%5d\\%5d)\t%50s: 0x%08x\r\n", index++, kallsyms_num_syms,
					name, addr);
	return 0;
}

static int kallsyms_dump(int argc, char *argv)
{
	printk("All Symbols (%d):\n", kallsyms_num_syms);
	kallsyms_on_each_symbol(kallsyms_dump_one, NULL),
	printk("\n");

	return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(kallsyms_dump, dump_syms, dump all kernel symbol);
