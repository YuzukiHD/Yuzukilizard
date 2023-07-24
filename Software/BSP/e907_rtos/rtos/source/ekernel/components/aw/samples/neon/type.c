/*
 * ===========================================================================================
 *
 *       Filename:  type.c
 *
 *    Description:  this file confirm that the "__simdxxxx " for neon is gcc internal recognized
 *                  for there are no include header files.
 *
 *        Version:  Melis3.0
 *         Create:  2020-03-13 11:57:59
 *       Revision:  none
 *       Compiler:  GCC:version 7.2.1 20170904 (release),ARM/embedded-7-branch revision 255204
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2020-03-13 11:58:50
 *
 * ===========================================================================================
 */
__simd64_float32_t test_float;
