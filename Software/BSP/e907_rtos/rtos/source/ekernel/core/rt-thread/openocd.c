/*
 * ===========================================================================================
 *
 *       Filename:  rt-thread-openocd.c
 *
 *    Description:  OpenOCD stub for RT-Thread, USED for GDB.
 *
 *        Version:  Melis3.0
 *         Create:  2018-04-04 11:52:47
 *       Revision:  none
 *       Compiler:  GCC:version 7.2.1 20170904 (release),ARM/embedded-7-branch revision 255204
 *
 *         Author:  zengzhijin@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2018-04-04 19:22:19
 *
 *  rt-thread does not provide a fixed layout for rt_thread, which makes it
 *  impossible to determine the appropriate offsets within the structure
 *  unaided. A priori knowledge of offsets based on os_dbg.c is tied to a
 *  specific release and thusly, brittle. The constants defined below
 *  provide the neccessary information OpenOCD needs to provide support
 *  in the most robust manner possible.
 *
 *  This file should be linked along with the project to enable RTOS
 *  support for rt-thread.
 * ===========================================================================================
 */

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef OFFSET_OF
#undef OFFSET_OF
#endif

#define OFFSET_OF(type, member) ((rt_size_t)&(((type *)0)->member))

const rt_size_t SECTION(".openocd_support") RT_USED openocd_rt_thread_name_offset = OFFSET_OF(struct rt_thread, name);
const rt_size_t SECTION(".openocd_support") RT_USED openocd_rt_thread_sp_offset = OFFSET_OF(struct rt_thread, sp);
const rt_size_t SECTION(".openocd_support") RT_USED openocd_rt_thread_stat_offset = OFFSET_OF(struct rt_thread, stat);
const rt_size_t SECTION(".openocd_support") RT_USED openocd_rt_thread_current_priority_offset = OFFSET_OF(struct rt_thread, current_priority);
const rt_size_t SECTION(".openocd_support") RT_USED openocd_rt_thread_next_offset = OFFSET_OF(struct rt_thread, list) + OFFSET_OF(rt_list_t, next);
const rt_size_t SECTION(".openocd_support") RT_USED openocd_rt_thread_prev_offset = OFFSET_OF(struct rt_thread, list) + OFFSET_OF(rt_list_t, prev);
const rt_size_t SECTION(".openocd_support") RT_USED openocd_rt_object_prev_offset = OFFSET_OF(struct rt_object_information, object_list) + OFFSET_OF(rt_list_t, prev);
const rt_size_t SECTION(".openocd_support") RT_USED openocd_rt_object_next_offset = OFFSET_OF(struct rt_object_information, object_list) + OFFSET_OF(rt_list_t, next);

#ifdef __cplusplus
}
#endif
