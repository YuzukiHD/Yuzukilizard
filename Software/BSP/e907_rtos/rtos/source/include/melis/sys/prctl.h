/*
 * =====================================================================================
 *
 *       Filename:  prctl.h
 *
 *    Description:  prctl impl.
 *
 *        Version:  1.0
 *        Created:  2020年03月09日 19时56分21秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef __PRCTL_H__
#define __PRCTL_H__

#ifdef __cplusplus
extern "C" {
#endif

#define PR_SET_NAME    15               /* Set process name */
#define PR_GET_NAME    16               /* Get process name */

int prctl(int option, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5);

#ifdef __cplusplus
}
#endif

#endif  /*__PRCTL_H__*/
