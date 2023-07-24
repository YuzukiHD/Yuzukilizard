/*
 * ===========================================================================================
 *
 *       Filename:  epos.h
 *
 *    Description:  epos.c typedef.
 *
 *        Version:  Melis3.0
 *         Create:  2018-03-30 20:34:51
 *       Revision:  none
 *       Compiler:  GCC:version 7.2.1 20170904 (release),ARM/embedded-7-branch revision 255204
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2018-09-28 10:50:56
 *
 * ===========================================================================================
 */

#ifndef EPOS_H
#define EPOS_H

#include <kapi.h>
#include <aw_list.h>

#include <hal_status.h>
#include <hal_debug.h>
#include <hal_queue.h>
#include <hal_time.h>
#include <hal_thread.h>

#define  USE_MELIS_SHORT_THREAD_ID
#define  MELIS_TASK_PREFIX                  "mlstsk_"
#define  MELIS_THREAD_COUNT                 (255u)
#define  MEMLEAK_STK_DUMPSZ                 (1024)
#define  MAX_ICACHE_LOCKED_WAY              3       /* max locked d-cache ways number, * NOTE : must reserve one way for normal use */
#define  MAX_DCACHE_LOCKED_WAY              3
#define  CONFIG_MEMORY_LEAK_BACKTRACE_LEVEL 16

typedef struct
{
    hal_mailbox_t        mb_in;
    hal_mailbox_t        mb_out;
    uint32_t            depth;
    uint32_t            bufsize;
    uint32_t            msgsize;
    uint32_t            dbufmod;
    uint32_t            mbufmod;
    uint32_t            err;
    void                *dbufaddr;
    void                *mbufaddr;
} __krnl_skt_t;

typedef struct  __CACHE_WAY
{
    void                *addr;      /* start address of lock    */
    int32_t             lockbit;    /* locked way bit value     */
} __cache_way_t;

typedef struct thread_id
{
    uint8_t             running;
    uint16_t            task_pid;
    hal_thread_t        thread_id;
} thread_id_t;

typedef struct melis_thread_obj
{
    thread_id_t         thr_id[MELIS_THREAD_COUNT];
} melis_thread_t;

typedef struct melis_malloc_context
{
    struct list_head    list;
    int32_t             ref_cnt;
    int32_t             open_flag;
    int32_t             double_free_check;
} melis_malloc_context_t;

typedef struct melis_heap_buffer_node
{
    struct list_head    i_list;
    hal_tick_t           tick;
    void                *vir;
    uint32_t            size;
    unsigned long       entry;
    int8_t              *name;
    uint8_t             *stk;
#ifdef CONFIG_DEBUG_BACKTRACE
    void                *caller[CONFIG_MEMORY_LEAK_BACKTRACE_LEVEL];
#endif
} melis_heap_buffer_node_t;
#endif  /*EPOS_H*/
