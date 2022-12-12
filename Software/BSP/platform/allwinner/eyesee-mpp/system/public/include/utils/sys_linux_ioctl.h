/*
 * @description This file is used to compress compile conflicts between sys/ioctl.h and linux/ioctl.h, 
                I want you to use sys/ioctl.h priorly, but if source file must contain linux/ioctl.h, 
                then include this file to avoid compile warning of redefined macro.
 */
#ifndef _SYS_LINUX_IOCTL_H_
#define _SYS_LINUX_IOCTL_H_

#include <sys/ioctl.h>

//out/v853-perf1/compile_dir/target/linux-v853-perf1/linux-4.9.191/user_headers/include/asm-generic/ioctl.h
//                                                   linux-4.9.191 -> lichee/linux-4.9/
//glibc headers will include "user_headers/include/asm-generic/ioctl.h", 
//but musl don't, musl will define _IOC/_IOWR self, and will lead to redefinition conflicts.
#ifndef _ASM_GENERIC_IOCTL_H

#ifdef _IOC
#undef _IOC
#endif
#ifdef _IO
#undef _IO
#endif
#ifdef _IOR
#undef _IOR
#endif
#ifdef _IOW
#undef _IOW
#endif
#ifdef _IOWR
#undef _IOWR
#endif

#endif //!_ASM_GENERIC_IOCTL_H

#include <linux/ioctl.h>

#endif  /* _SYS_LINUX_IOCTL_H_ */
    
