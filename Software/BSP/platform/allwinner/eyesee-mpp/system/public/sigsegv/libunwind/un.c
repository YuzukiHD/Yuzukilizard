#ifndef _GNU_SOURCE
	#define _GNU_SOURCE         /* See feature_test_macros(7) */
#endif
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#define UNW_LOCAL_ONLY
#include <libunwind.h>

void show_backtrace (void)
{
#if 1
	unw_context_t uc;
	unw_cursor_t cursor;
	unw_word_t ip, sp;

	int i = 0;

	unw_getcontext(&uc);
	unw_init_local(&cursor, &uc);
	while (unw_step(&cursor) > 0) {
		unw_get_reg(&cursor, UNW_REG_IP, &ip);
		unw_get_reg(&cursor, UNW_REG_SP, &sp);
		if (ip == (unw_word_t)0) break;

#if 0
//		printf ("ip = %lx, sp = %lx\n", (long)ip, (long)sp);
		char bufp[64];
		unw_word_t offp;
		if (unw_get_proc_name(&cursor, bufp, 64, &offp) == 0) {
			fprintf(stderr, "%02d: %p <%s + %lu>\n", ++i, (void *)ip, bufp, (unsigned long)offp);
		}
		if (strcmp(bufp, "__libc_start_main") == 0) {
			break;
		}
#else
		Dl_info dl_info = { 0 };
		if (!dladdr((void *)ip, &dl_info))	break;
        const char *sname = dl_info.dli_sname;
        fprintf(stderr, "%2d: %p <%s + %lu> (%s)\n", ++i, (void *)ip, sname,
                (unsigned long)ip - (unsigned long)dl_info.dli_saddr, dl_info.dli_fname);
        if (dl_info.dli_sname && !strcmp(dl_info.dli_sname, "__libc_start_main")) {
        	break;
        }
#endif
	}
#else
	void *bufp[16];
	void **pbufp = bufp;
	int depth, ii, i = 0;
	depth = unw_backtrace(pbufp, 16);
	for (ii = 0; ii < depth; ++ii) {
		Dl_info dl_info = { 0 };
		if (!dladdr(bufp[ii], &dl_info))	break;
        const char *sname = dl_info.dli_sname;
        fprintf(stderr, "%2d: %p <%s + %lu> (%s)\n", ++i, bufp[ii], sname,
                (unsigned long)bufp[ii] - (unsigned long)dl_info.dli_saddr, dl_info.dli_fname);
        if (dl_info.dli_sname && !strcmp(dl_info.dli_sname, "__libc_start_main")) {
        	break;
        }
	}
#endif
}

static void sigsegv_handler(int signo, siginfo_t *info, void *ptr)
{
	show_backtrace();
	_exit(0);
}

void func2(void)
{
	char *p = (char *)0x12345678;
	*p = '\0';
}

void func1(void)
{
	func2();
}

void func0(void)
{
	func1();
}

/* Set a signal handler. */
#define SETSIG(sa, sig, fun, flags)     \
        do {                            \
            sa.sa_sigaction = fun;      \
            sa.sa_flags = flags;        \
            sigemptyset(&sa.sa_mask);   \
            sigaction(sig, &sa, NULL);  \
        } while(0)

int main(int ac, const char *av[])
{
    struct sigaction sa;
    SETSIG(sa, SIGSEGV, sigsegv_handler, SA_SIGINFO);

	func0();
	exit(EXIT_SUCCESS);
}
