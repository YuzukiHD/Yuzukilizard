#ifndef _GNU_SOURCE
	#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <ucontext.h>

/* 纯C环境下，不定义宏NO_CPP_DEMANGLE */
#if (!defined(__cplusplus)) && (!defined(NO_CPP_DEMANGLE))
#define NO_CPP_DEMANGLE
#endif

#ifndef NO_CPP_DEMANGLE
    #include <cxxabi.h>
    #ifdef __cplusplus
    	using __cxxabiv1::__cxa_demangle;
    #endif
#endif

#ifdef HAS_ULSLIB
	#include <uls/logger.h>
	#define sigsegv_outp(x)	sigsegv_outp(, gx)
#else
	#define sigsegv_outp(x, ...) 	fprintf(stderr, x"\n", ##__VA_ARGS__)
#endif

static void sigsegv_handler(int signo, siginfo_t *info, void *context)
{
	sigsegv_outp("Segmentation Fault!");
	sigsegv_outp("info.si_signo = %d", signo);
	if (info) {
		sigsegv_outp("info.si_errno = %d", info->si_errno);
		sigsegv_outp("info.si_code  = %d (%s)", info->si_code,
			(info->si_code == SEGV_MAPERR) ? "SEGV_MAPERR" : "SEGV_ACCERR");
		sigsegv_outp("info.si_addr  = %p\n", info->si_addr);
	}

	int i = 0;
	Dl_info	dl_info;

	const void *pc = __builtin_return_address(0);
	const void **fp = (const void **)__builtin_frame_address(1);

	sigsegv_outp("\nStack trace:");
	while (pc) {
		memset(&dl_info, 0, sizeof(Dl_info));
		if (!dladdr((void *)pc, &dl_info))	break;
		const char *sname = dl_info.dli_sname;
#ifndef NO_CPP_DEMANGLE
		int status;
		char *tmp = __cxa_demangle(sname, NULL, 0, &status);
		if (status == 0 && tmp) {
			sname = tmp;
		}
#endif
		/* No: return address <sym-name + offset> (filename) */
		sigsegv_outp("%02d: %p <%s + %lu> (%s)", ++i, pc, sname,
				(unsigned long)pc - (unsigned long)dl_info.dli_saddr, dl_info.dli_fname);
#ifndef NO_CPP_DEMANGLE
		if (tmp)	free(tmp);
#endif
		if (sname && !strcmp(sname, "main")) break;

		if (!fp) break;
#if (defined __x86_64__) || (defined __i386__)
		pc = fp[1];
		fp = (const void **)fp[0];
#elif (defined __arm__)
		pc = fp[-1];
		fp = (const void **)fp[-3];
#endif
	}
	sigsegv_outp("Stack trace end.");

	_exit(0);
}

#define SETSIG(sa, signo, func, flags)	\
        do {                            \
            sa.sa_sigaction = func;  	\
            sa.sa_flags = flags;        \
            sigemptyset(&sa.sa_mask);   \
            sigaction(signo, &sa, NULL);\
        } while(0)

static void __attribute((constructor)) setup_sigsegv(void)
{
	struct sigaction sa;
	SETSIG(sa, SIGSEGV, sigsegv_handler, SA_SIGINFO);
}

#if 1
void func3(void)
{
	char *p = (char *)0x12345678;
	*p = 10;
}

void func2(void)
{
	func3();
}

void func1(void)
{
	func2();
}

int main(int argc, const char *argv[])
{
	func1();
	exit(EXIT_SUCCESS);
}
#endif
