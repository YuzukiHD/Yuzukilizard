#define _LINUX_STRING_H_

#include <compiler.h>	/* for inline */
#include <stdint.h>		/* for size_t */
#include <stddef.h>		/* for NULL */
#include <string.h>

extern unsigned long free_mem_ptr;
extern unsigned long free_mem_end_ptr;
extern void error(char *);

#define STATIC static
#define STATIC_RW_DATA	/* non-static please */

/* Diagnostic functions */
#ifdef DEBUG
#  define Assert(cond,msg) {if(!(cond)) error(msg);}
#  define Trace(x) fprintf x
#  define Tracev(x) {if (verbose) fprintf x ;}
#  define Tracevv(x) {if (verbose>1) fprintf x ;}
#  define Tracec(c,x) {if (verbose && (c)) fprintf x ;}
#  define Tracecv(c,x) {if (verbose>1 && (c)) fprintf x ;}
#else
#  define Assert(cond,msg)
#  define Trace(x)
#  define Tracev(x)
#  define Tracevv(x)
#  define Tracec(c,x)
#  define Tracecv(c,x)
#endif

/* Not needed, but used in some headers pulled in by decompressors */
extern char * strstr(const char * s1, const char *s2);

#ifdef CONFIG_KERNEL_COMPRESS_GZIP
#include "../../../components/thirdparty/decompress/decompress_inflate.c"
#endif

#ifdef CONFIG_KERNEL_COMPRESS_LZO
#include "../../../components/thirdparty/decompress/decompress_unlzo.c"
#endif

#ifdef CONFIG_KERNEL_COMPRESS_LZMA
#include "../../../components/thirdparty/decompress/decompress_unlzma.c"
#endif

#ifdef CONFIG_KERNEL_COMPRESS_XZ
#define memmove memmove
#define memcpy memcpy
#include "../../../components/thirdparty/decompress/decompress_unxz.c"
#endif

#ifdef CONFIG_KERNEL_COMPRESS_LZ4
#include "../../../components/thirdparty/decompress/decompress_unlz4.c"
#endif

int do_decompress(uint8_t *input, int len, uint8_t *output, void (*error)(char *x))
{
	return __decompress(input, len, NULL, NULL, output, 0, NULL, error);
}
