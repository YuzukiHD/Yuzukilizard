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


/* 纯C环境下，定义宏NO_CPP_DEMANGLE */
#if (!defined(__cplusplus)) && (!defined(NO_CPP_DEMANGLE))
# define NO_CPP_DEMANGLE
#endif

#ifndef NO_CPP_DEMANGLE
# include <cxxabi.h>
# ifdef __cplusplus
	using __cxxabiv1::__cxa_demangle;
# endif
#endif

#if (defined HAS_ULSLIB)
# include <uls/logger.h>
# define sigsegv_outp(x)	sigsegv_outp(, gx)
#else
# define sigsegv_outp(x, ...) 	fprintf(stderr, x"\n", ##__VA_ARGS__)
#endif

#if (defined (__x86_64__))
# define REGFORMAT   "%016lx"
#elif (defined (__i386__))
# define REGFORMAT   "%08x"
#elif (defined (__arm__))
# define REGFORMAT   "%lx"
#endif

static void print_reg(const ucontext_t *uc)
{
#if (defined (__x86_64__)) || (defined (__i386__))
	int i;
	for (i = 0; i < NGREG; i++) {
		sigsegv_outp("reg[%02d]: 0x"REGFORMAT, i, uc->uc_mcontext.gregs[i]);
	}
#elif (defined (__arm__))
	sigsegv_outp("reg[%02d]		= 0x"REGFORMAT, 0, uc->uc_mcontext.arm_r0);
	sigsegv_outp("reg[%02d]		= 0x"REGFORMAT, 1, uc->uc_mcontext.arm_r1);
	sigsegv_outp("reg[%02d]		= 0x"REGFORMAT, 2, uc->uc_mcontext.arm_r2);
	sigsegv_outp("reg[%02d]		= 0x"REGFORMAT, 3, uc->uc_mcontext.arm_r3);
	sigsegv_outp("reg[%02d]		= 0x"REGFORMAT, 4, uc->uc_mcontext.arm_r4);
	sigsegv_outp("reg[%02d]		= 0x"REGFORMAT, 5, uc->uc_mcontext.arm_r5);
	sigsegv_outp("reg[%02d]		= 0x"REGFORMAT, 6, uc->uc_mcontext.arm_r6);
	sigsegv_outp("reg[%02d]		= 0x"REGFORMAT, 7, uc->uc_mcontext.arm_r7);
	sigsegv_outp("reg[%02d]		= 0x"REGFORMAT, 8, uc->uc_mcontext.arm_r8);
	sigsegv_outp("reg[%02d]		= 0x"REGFORMAT, 9, uc->uc_mcontext.arm_r9);
	sigsegv_outp("reg[%02d]		= 0x"REGFORMAT, 10, uc->uc_mcontext.arm_r10);
	sigsegv_outp("FP		= 0x"REGFORMAT, uc->uc_mcontext.arm_fp);
	sigsegv_outp("IP		= 0x"REGFORMAT, uc->uc_mcontext.arm_ip);
	sigsegv_outp("SP		= 0x"REGFORMAT, uc->uc_mcontext.arm_sp);
	sigsegv_outp("LR		= 0x"REGFORMAT, uc->uc_mcontext.arm_lr);
	sigsegv_outp("PC		= 0x"REGFORMAT, uc->uc_mcontext.arm_pc);
	sigsegv_outp("CPSR		= 0x"REGFORMAT, uc->uc_mcontext.arm_cpsr);
	sigsegv_outp("Fault Address	= 0x"REGFORMAT, uc->uc_mcontext.fault_address);
	sigsegv_outp("Trap no		= 0x"REGFORMAT, uc->uc_mcontext.trap_no);
	sigsegv_outp("Err Code	= 0x"REGFORMAT, uc->uc_mcontext.error_code);
	sigsegv_outp("Old Mask	= 0x"REGFORMAT, uc->uc_mcontext.oldmask);
#endif
}

static void print_call_link(const ucontext_t *uc)
{
	int i = 0;
	Dl_info	dl_info;

#if (defined (__i386__))
	const void **frame_pointer = (const void **)uc->uc_mcontext.gregs[REG_EBP];
	const void *return_address = (const void *)uc->uc_mcontext.gregs[REG_EIP];
#elif (defined (__x86_64__))
	const void **frame_pointer = (const void **)uc->uc_mcontext.gregs[REG_RBP];
	const void *return_address = (const void *)uc->uc_mcontext.gregs[REG_RIP];
#elif (defined (__arm__))
/* sigcontext_t on ARM:
        unsigned long trap_no;
        unsigned long error_code;
        unsigned long oldmask;
        unsigned long arm_r0;
        ...
        unsigned long arm_r10;
        unsigned long arm_fp;
        unsigned long arm_ip;
        unsigned long arm_sp;
        unsigned long arm_lr;
        unsigned long arm_pc;
        unsigned long arm_cpsr;
        unsigned long fault_address;
*/
	const void **frame_pointer = (const void **)uc->uc_mcontext.arm_fp;
	const void *return_address = (const void *)uc->uc_mcontext.arm_pc;
#endif

	sigsegv_outp("\nStack trace:");
	while (return_address) {
		memset(&dl_info, 0, sizeof(Dl_info));
		if (!dladdr((void *)return_address, &dl_info))	break;
		const char *sname = dl_info.dli_sname;
#if (!defined NO_CPP_DEMANGLE)
		int status;
		char *tmp = __cxa_demangle(sname, NULL, 0, &status);
		if (status == 0 && tmp) {
			sname = tmp;
		}
#endif
		/* No: return address <sym-name + offset> (filename) */
		sigsegv_outp("%02d: %p <%s + %lu> (%s)", ++i, return_address, sname,
			(unsigned long)return_address - (unsigned long)dl_info.dli_saddr,
													dl_info.dli_fname);
#if (!defined NO_CPP_DEMANGLE)
		if (tmp)	free(tmp);
#endif
		if (dl_info.dli_sname && !strcmp(dl_info.dli_sname, "main")) break;

		if (!frame_pointer)	break;
#if (defined (__x86_64__)) || (defined (__i386__))
		return_address = frame_pointer[1];
		frame_pointer = (const void **)frame_pointer[0];
#elif (defined (__arm__))
		return_address = frame_pointer[-1];
		frame_pointer = (const void **)frame_pointer[-3];
#endif
	}
	sigsegv_outp("Stack trace end.");
}

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

	if (context) {
		const ucontext_t *uc = (const ucontext_t *)context;

		print_reg(uc);
		print_call_link(uc);
	}

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
#if 0
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_sigaction = sigsegv_handler;
	sa.sa_flags = SA_SIGINFO;
	if (sigaction(SIGSEGV, &sa, NULL) < 0) {
		perror("sigaction: ");
	}
#endif
}

#if 0
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
