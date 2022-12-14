#ifndef _INITCALL_H
#define _INITCALL_H

#include <compiler.h>

typedef int (*initcall_t)(void);
typedef void (*exitcall_t)(void);

typedef initcall_t initcall_entry_t;
static inline initcall_t initcall_from_entry(initcall_entry_t *entry)
{           
    return *entry;
}   

#define ___define_initcall(fn, id, __sec) \
	static initcall_t __initcall_##fn##id __used 		\
		__attribute__((__section__(#__sec ".init"))) = fn;

#define __define_initcall(fn, id) ___define_initcall(fn, id, .initcall##id)

#define early_initcall(fn)		__define_initcall(fn, early)

#define pure_initcall(fn)		__define_initcall(fn, 0)
#define core_initcall(fn)		__define_initcall(fn, 1)
#define core_initcall_sync(fn)		__define_initcall(fn, 1s)
#define postcore_initcall(fn)		__define_initcall(fn, 2)
#define postcore_initcall_sync(fn)	__define_initcall(fn, 2s)
#define arch_initcall(fn)		__define_initcall(fn, 3)
#define arch_initcall_sync(fn)		__define_initcall(fn, 3s)
#define subsys_initcall(fn)		__define_initcall(fn, 4)
#define subsys_initcall_sync(fn)	__define_initcall(fn, 4s)
#define fs_initcall(fn)			__define_initcall(fn, 5)
#define fs_initcall_sync(fn)		__define_initcall(fn, 5s)
#define rootfs_initcall(fn)		__define_initcall(fn, rootfs)
#define device_initcall(fn)		__define_initcall(fn, 6)
#define device_initcall_sync(fn)	__define_initcall(fn, 6s)
#define late_initcall(fn)		__define_initcall(fn, 7)
#define late_initcall_sync(fn)		__define_initcall(fn, 7s)
#define __initcall(fn)			device_initcall(fn)
#define __exitcall(fn)			static exitcall_t __exitcall_##fn __exit_call = fn

#define console_initcall(fn)	___define_initcall(fn,, .con_initcall)

extern int app_entry(void *param);
extern void do_initcalls(void);

#endif /* _INITCALL_H */
