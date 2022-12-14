#include <stdio.h>

#include <backtrace.h>

int backtrace_exception(print_function print_func, unsigned long cpsr,
                        unsigned long sp,
                        unsigned long pc,
                        unsigned long lr)
{
#ifdef CONFIG_DEBUG_BACKTRACE
    return arch_backtrace_exception(print_func, cpsr, sp, pc, lr);
#else
    if (print_func)
    {
        print_func("not support backtrace!\n");
    }
    return 0;
#endif
}

/*
 * backtrace: get call stack information.
 * @taskname: target task. NULL: current task.
 * @output: call stack output buffer.
 * @offset:
 * @print_func: print function
 */
int backtrace(char *taskname, void *output[], int size, int offset, print_function print_func)
{
#ifdef CONFIG_DEBUG_BACKTRACE
    return arch_backtrace(taskname, output, size, offset, print_func);
#else
    if (print_func)
    {
        print_func("not support backtrace!\n");
    }
    return 0;
#endif
}
