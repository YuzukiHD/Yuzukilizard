/* RT-Thread config file */
#ifndef __RTTHREAD_CFG_H__
#define __RTTHREAD_CFG_H__
#include <kconfig.h>

#define RT_GDB_ICACHE
/* RT_NAME_MAX*/
#define RT_NAME_MAX CONFIG_RT_NAME_MAX

/* RT_ALIGN_SIZE*/
#define RT_ALIGN_SIZE   CONFIG_RT_ALIGN_SIZE

#ifdef CONFIG_MODULES
#define RT_USING_MODULE         CONFIG_MODULES
#endif

/* PRIORITY_MAX */
#define RT_THREAD_PRIORITY_MAX  CONFIG_RT_THREAD_PRIORITY_MAX

/* Tick per Second */
#define RT_TICK_PER_SECOND  CONFIG_HZ

/* SECTION: RT_DEBUG */
/* Thread Debug */
#ifdef CONFIG_RT_DEBUG
#define RT_DEBUG
#endif
//#define SCHEDULER_DEBUG
/* #define RT_THREAD_DEBUG */
#ifdef CONFIG_RT_DEBUG_INIT_CONFIG
#define RT_DEBUG_INIT 1
#else
#define RT_DEBUG_INIT 0
#endif
#ifdef CONFIG_RT_DEBUG_THREAD_CONFIG
#define RT_DEBUG_THREAD 1
#else
#define RT_DEBUG_THREAD 0
#endif
#ifdef CONFIG_RT_DEBUG_SCHEDULER_CONFIG
#define RT_DEBUG_SCHEDULER 1
#else
#define RT_DEBUG_SCHEDULER 0
#endif
#ifdef CONFIG_RT_DEBUG_IPC_CONFIG
#define RT_DEBUG_IPC 1
#else
#define RT_DEBUG_IPC 0
#endif
#ifdef CONFIG_RT_DEBUG_TIMER_CONFIG
#define RT_DEBUG_TIMER 1
#else
#define RT_DEBUG_TIMER 0
#endif
#ifdef CONFIG_RT_DEBUG_IRQ_CONFIG
#define RT_DEBUG_IRQ 1
#else
#define RT_DEBUG_IRQ 0
#endif
#ifdef CONFIG_RT_DEBUG_MEM_CONFIG
#define RT_DEBUG_MEM 1
#else
#define RT_DEBUG_MEM 0
#endif
#ifdef CONFIG_RT_DEBUG_SLAB_CONFIG
#define RT_DEBUG_COLOR 1
#else
#define RT_DEBUG_COLOR 0
#endif
#ifdef CONFIG_RT_DEBUG_MEMHEAP_CONFIG
#define RT_DEBUG_MEMHEAP 1
#else
#define RT_DEBUG_MEMHEAP 0
#endif
#ifdef CONFIG_RT_DEBUG_MODULE_CONFIG
#define RT_DEBUG_MODULE 1
#else
#define RT_DEBUG_MODULE 0
#endif

#ifdef CONFIG_RT_USING_OVERFLOW_CHECK
#define RT_USING_OVERFLOW_CHECK
#endif

#define RT_USING_INTERRUPT_INFO

/* Using Hook */
#ifdef CONFIG_RT_USING_HOOK
#define RT_USING_HOOK
#endif
#ifdef CONFIG_RT_USING_IDLE_HOOK
#define RT_USING_IDLE_HOOK
#endif

#ifdef CONFIG_RT_USING_TIMER_SOFT
#define RT_USING_TIMER_SOFT
#define RT_TIMER_THREAD_PRIO        CONFIG_RT_TIMER_THREAD_PRIO
#define RT_TIMER_THREAD_STACK_SIZE  CONFIG_RT_TIMER_THREAD_STACK_SIZE
#endif

/* SECTION: IPC */
#ifdef CONFIG_RT_USING_SEMAPHORE
/* Using Semaphore */
#define RT_USING_SEMAPHORE
#endif
#ifdef CONFIG_RT_USING_MUTEX
/* Using Mutex */
#define RT_USING_MUTEX
#endif
#ifdef CONFIG_RT_USING_EVENT
/* Using Event */
#define RT_USING_EVENT
#endif
#ifdef CONFIG_RT_USING_MAILBOX
/* Using MailBox */
#define RT_USING_MAILBOX
#endif
#ifdef CONFIG_RT_USING_MESSAGEQUEUE
/* Using Message Queue */
#define RT_USING_MESSAGEQUEUE
#endif
#ifdef CONFIG_RT_USING_PIPE
#define RT_USING_PIPE
#endif
#ifdef CONFIG_RT_USING_RINGBUFFER
#define RT_USING_RINGBUFFER
#endif
#ifdef CONFIG_RT_USING_WAITQUEUE
#define RT_USING_WAITQUEUE
#endif
#ifdef CONFIG_RT_USING_WORKQUEUE
#define RT_USING_WORKQUEUE
#endif
#ifdef CONFIG_RT_USING_COMPLETION
#define RT_USING_COMPLETION
#endif
#ifdef CONFIG_RT_USING_SIGNALS
#define RT_USING_SIGNALS
#endif

/* SECTION: Memory Management */
#ifdef CONFIG_RT_USING_MEMPOOL
#define RT_USING_MEMPOOL
#endif
#ifdef CONFIG_RT_USING_MEMHEAP
#define RT_USING_MEMHEAP
#endif
#ifdef CONFIG_RT_USING_NOHEAP
#define RT_USING_NOHEAP
#endif
#ifdef CONFIG_RT_USING_SMALL_MEM
#define RT_USING_SMALL_MEM
#endif
#ifdef CONFIG_RT_USING_SLAB
#define RT_USING_SLAB
#endif
#ifdef CONFIG_RT_USING_MEMHEAP_AS_HEAP
#define RT_USING_MEMHEAP_AS_HEAP
#endif
#ifdef CONFIG_RT_USING_MEMTRACE
#define RT_USING_MEMTRACE
#endif
#ifdef CONFIG_RT_USING_HEAP
#define RT_USING_HEAP
#endif

/* SECTION: Device System */
#ifdef CONFIG_RT_USING_DEVICE
#define RT_USING_DEVICE
#endif
#ifdef CONFIG_RT_USING_INTERRUPT_INFO
#define RT_USING_INTERRUPT_INFO
#endif

/* SECTION: Console options */
#ifdef CONFIG_RT_USING_CONSOLE
#define RT_USING_CONSOLE
#endif


#ifdef CONFIG_RT_CONSOLEBUF_SIZE
#define RT_CONSOLEBUF_SIZE CONFIG_RT_CONSOLEBUF_SIZE
#endif
#ifdef CONFIG_RT_CONSOLE_DEVICE_NAME
#define RT_CONSOLE_DEVICE_NAME CONFIG_RT_CONSOLE_DEVICE_NAME
#endif

/* SECTION: finsh, a C-Express shell */
/* Using FinSH as Shell*/
#ifdef CONFIG_RT_USING_FINSH
#define RT_USING_FINSH

#ifdef CONFIG_FINSH_USING_SYMTAB
#define FINSH_USING_SYMTAB
#endif
//#define FINSH USING DESCRIPTION
#define FINSH_THREAD_NAME CONFIG_FINSH_THREAD_NAME

#ifdef CONFIG_FINSH_USING_HISTORY
#define FINSH_USING_HISTORY
#endif

#define FINSH_HISTORY_LINES CONFIG_FINSH_HISTORY_LINES

#ifdef CONFIG_FINSH_USING_DESCRIPTION
#define FINSH_USING_DESCRIPTION
#endif

/* FINSH_ECHO_DISABLE_DEFAULT is not set */
#define FINSH_THREAD_PRIORITY CONFIG_FINSH_THREAD_PRIORITY
#define FINSH_THREAD_STACK_SIZE CONFIG_FINSH_THREAD_STACK_SIZE
#define FINSH_CMD_SIZE CONFIG_FINSH_CMD_SIZE
/* FINSH_USING_AUTH is not set */
#ifdef CONFIG_FINSH_USING_MSH
#define FINSH_USING_MSH
#ifdef CONFIG_FINSH_USING_MSH_DEFAULT
#define FINSH_USING_MSH_DEFAULT
#endif
#ifdef CONFIG_FINSH_USING_MSH_DEFAULT
#define FINSH_USING_MSH_ONLY
#endif
#endif
#define FINSH_ARG_MAX CONFIG_FINSH_ARG_MAX
#endif

/* SECTION: a runtime libc library */
/* a runtime libc library */
#define RT_USING_NEWLIB

/* SECTION: a mini libc */

/* SECTION: C++ support */
/* Using C++ support */
/* #define RT_USING_CPLUSPLUS */

#ifdef CONFIG_RT_USING_DFS
/* SECTION: Device filesystem support */
/* using DFS support */
#define RT_USING_DFS
//#define RT_USING_DFS_ELMFAT
/* use long file name feature */
//#define RT_DFS_ELM_USE_LFN          2
//#define RT_DFS_ELM_REENTRANT
/* define OEM code page */
//#define RT_DFS_ELM_CODE_PAGE    936
/* Using OEM code page file */
// #define RT_DFS_ELM_CODE_PAGE_FILE
/* the max number of file length */
//#define RT_DFS_ELM_MAX_LFN          128
/* #define RT_USING_DFS_YAFFS2 */
/* #define RT_USING_DFS_UFFS */
#define RT_USING_DFS_DEVFS

/* #define RT_USING_DFS_NFS */
//#define RT_NFS_HOST_EXPORT        "192.168.1.5:/"

#define DFS_USING_WORKDIR

#define RT_DFS_ELM_USE_LFN 1
#define RT_DFS_ELM_MAX_LFN 255

#define RT_USING_POSIX

#endif

/* the max number of mounted filesystem */
//#define DFS_FILESYSTEMS_MAX     4
/* the max number of opened files */
//#define DFS_FD_MAX                  16
/* the max number of cached sector */
//#define DFS_CACHE_MAX_NUM           4

/* SECTION: lwip, a lighwight TCP/IP protocol stack */
/* Using lighweight TCP/IP protocol stack */
#define RT_USING_LWIP

#define RT_LWIP_DNS   //enable dns server
#define RT_LWIP_RAW   //choice RAW sockets
#define RT_LWIP_ICMP
#define RT_LWIP_TCP
#define RT_LWIP_UDP

/* CHECKSUM options */
//#define RT_LWIP_USING_HW_CHECKSUM

/*=================DEBUG options=================*/
/* Trace LwIP protocol */
//#define RT_LWIP_DEBUG

/* Enable DNS server */
//#define RT_LWIP_DNS_DEBUG

/* Enable NETIF protocol */
//#define RT_LWIP_NETIF_DEBUG

/* Enable ETHARP protocol */
//#define RT_LWIP_ETHARP_DEBUG

/* Enable SOCKETS protocol */
//#define RT_LWIP_SOCKETS_DEBUG

/* Enable TCPIP protocol */
//#define RT_LWIP_TCPIP_DEBUG

/* Enable APILIB protocol */
//#define RT_LWIP_API_LIB_DEBUG

/* Enable ICMP protocol */
//#define RT_LWIP_ICMP_DEBUG

/* Enable IGMP protocol */
//#define RT_LWIP_IGMP_DEBUG

/* Enable IP protocol */
//#define RT_LWIP_IP_DEBUG

/* Enable UDP protocol */
//#define RT_LWIP_UDP_DEBUG

/* Enable TCP protocol */
//#define RT_LWIP_TCP_DEBUG

/* the number of simulatenously active TCP connections*/

/* TCP sender buffer space */
//#define RT_LWIP_TCP_SND_BUF 1024*10

/* TCP receive window. */
//#define RT_LWIP_TCP_WND 1024*8

/* Enable SNMP protocol */
/* #define RT_LWIP_SNMP */

/* Using DHCP */
/* #define RT_LWIP_DHCP */

/* ip address of target */

/* RT_USING_MODBUS is not set */


/* gateway address of target */

/* mask address of target */

/* the number of blocks for pbuf */

/* thread priority of tcpip thread */

/* mail box size of tcpip thread to wait for */

/* thread stack size of tcpip thread */

/* thread priority of ethnetif thread */


/* mail box size of ethnetif thread to wait for */

/* thread stack size of ethnetif thread */


/* SECTION: RTGUI support */
/* using RTGUI support */
/* #define RT_USING_RTGUI */

/* name length of RTGUI object */
//#define RTGUI_NAME_MAX        16
/* support 16 weight font */
//#define RTGUI_USING_FONT16
/* support 16 weight font */
//#define RTGUI_USING_FONT12
/* support Chinese font */
//#define RTGUI_USING_FONTHZ
/* use DFS as file interface */
//#define RTGUI_USING_DFS_FILERW
/* use font file as Chinese font */
/* #define RTGUI_USING_HZ_FILE */
/* use Chinese bitmap font */
//#define RTGUI_USING_HZ_BMP
/* use small size in RTGUI */
/* #define RTGUI_USING_SMALL_SIZE */
/* use mouse cursor */
/* #define RTGUI_USING_MOUSE_CURSOR */

/* SECTION: FTK support */
/* using FTK support */
/* #define RT_USING_FTK */

/*
 * Note on FTK:
 *
 * FTK depends :
 * #define RT_USING_NEWLIB
 * #define DFS_USING_WORKDIR
 *
 * And the maximal length must great than 64
 * #define RT_DFS_ELM_MAX_LFN   128
 */

#define RT_USING_LIBC

#define RT_USING_PTHREADS
#define RT_START_FROM_IDLE

// workqueue definition.
#define RT_USING_SYSTEM_WORKQUEUE
#define RT_SYSTEM_WORKQUEUE_PRIORITY 23
#define RT_SYSTEM_WORKQUEUE_STACKSIZE 2048

#endif
