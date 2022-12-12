/*
 * ===========================================================================================
 *
 *       Filename:  det_res.h
 *
 *    Description:  nna detect result structure definition.
 *
 *        Version:  Melis3.0
 *         Create:  2021-09-16 10:45:49
 *       Revision:  none
 *       Compiler:  GCC:version 7.2.1 20170904 (release),ARM/embedded-7-branch revision 255204
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
  Last Modified : 2021-09-16 16:00:38
 *
 * ===========================================================================================
 */

#ifndef __DETECT_RESULT__
#define __DETECT_RESULT__

#define MAX_OBJECT_DET_NUM  20
typedef struct __object_result
{
    int   label;
    float prob;
    int   left_up_x;
    int   left_up_y;
    int   right_down_x;
    int   right_down_y;
    int   width;
    int   height;
} object_res_t;

typedef struct __detect_result
{
    int num;
    object_res_t objs[MAX_OBJECT_DET_NUM];
} detect_res_t;

#endif
