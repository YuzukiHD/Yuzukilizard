#ifndef _MELIS_KALLSYMS_H
#define _MELIS_KALLSYMS_H

#include <util.h>
#include <errno.h>
#include <init.h>
#include <log.h>

#ifdef CONFIG_ARM
#define BITS_PER_LONG 32
#else
#ifdef CONFIG_RV32
#define BITS_PER_LONG 32
#else
#define BITS_PER_LONG 64
#endif
#endif

#define KSYM_NAME_LEN 128
#define KSYM_SYMBOL_LEN (sizeof("%s+%#lx/%#lx [%s]") + (KSYM_NAME_LEN - 1) + \
			 2*(BITS_PER_LONG*3/10)  + 1)

#ifdef CONFIG_KALLSYMS
/* Lookup the address for a symbol. Returns 0 if not found. */
unsigned long kallsyms_lookup_name(const char *name);

/* Call a function on each kallsyms symbol in the core kernel */
int kallsyms_on_each_symbol(int (*fn)(void *, const char *, unsigned long),
			    void *data);

extern int kallsyms_lookup_size_offset(unsigned long addr,
				  unsigned long *symbolsize,
				  unsigned long *offset);

const char *kallsyms_lookup(unsigned long addr,
			    unsigned long *symbolsize,
			    unsigned long *offset,
			    char *namebuf);

/* Look up a kernel symbol and return it in a text buffer. */
extern int sprint_symbol(char *buffer, unsigned long address);
extern int sprint_symbol_no_offset(char *buffer, unsigned long address);
extern int sprint_backtrace(char *buffer, unsigned long address);

/* Look up a kernel symbol and print it to the kernel messages. */
extern void __print_symbol(const char *fmt, unsigned long address);

int lookup_symbol_name(unsigned long addr, char *symname);
int lookup_symbol_attrs(unsigned long addr, unsigned long *size, unsigned long *offset, char *name);

#else /* !CONFIG_KALLSYMS */

static inline unsigned long kallsyms_lookup_name(const char *name)
{
	return 0;
}

static inline int kallsyms_on_each_symbol(int (*fn)(void *, const char *, unsigned long),
					  void *data)
{
	return 0;
}

static inline int kallsyms_lookup_size_offset(unsigned long addr,
					      unsigned long *symbolsize,
					      unsigned long *offset)
{
	return 0;
}

static inline const char *kallsyms_lookup(unsigned long addr,
					  unsigned long *symbolsize,
					  unsigned long *offset,
					  char *namebuf)
{
	return NULL;
}

static inline int sprint_symbol(char *buffer, unsigned long addr)
{
	*buffer = '\0';
	return 0;
}

static inline int sprint_symbol_no_offset(char *buffer, unsigned long addr)
{
	*buffer = '\0';
	return 0;
}

static inline int sprint_backtrace(char *buffer, unsigned long addr)
{
	*buffer = '\0';
	return 0;
}

static inline int lookup_symbol_name(unsigned long addr, char *symname)
{
	return -ERANGE;
}

static inline int lookup_symbol_attrs(unsigned long addr, unsigned long *size, unsigned long *offset, char *name)
{
	return -ERANGE;
}

/* Stupid that this does nothing, but I didn't create this mess. */
#define __print_symbol(fmt, addr)
#endif /*CONFIG_KALLSYMS*/

void print_symbol(const char *fmt, unsigned long addr);
void print_ip_sym(unsigned long ip);

#endif /*_MELIS_KALLSYMS_H*/
