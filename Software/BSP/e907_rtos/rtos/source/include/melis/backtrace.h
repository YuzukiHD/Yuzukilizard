#ifndef BACKTRACE_H
#define BACKTRACE_H

typedef int (*print_function)(const char *fmt, ...);

extern int backtrace_exception(print_function print_func,
                        unsigned long arg_cpsr,
                        unsigned long arg_sp,
                        unsigned long arg_pc,
                        unsigned long arg_lr);

extern int backtrace(char *taskname, void *trace[], int size, int offset, print_function print_func);

#ifdef CONFIG_DEBUG_BACKTRACE
extern int arch_backtrace_exception(print_function print_func,
                        unsigned long arg_cpsr,
                        unsigned long arg_sp,
                        unsigned long arg_pc,
                        unsigned long arg_lr);

extern int arch_backtrace(char *taskname, void *trace[], int size, int offset, print_function print_func);
#endif

#endif  /*BACKTRACE_H*/
