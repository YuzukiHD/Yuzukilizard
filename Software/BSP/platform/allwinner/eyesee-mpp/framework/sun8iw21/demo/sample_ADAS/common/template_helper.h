/*
 * template_helper.h
 *
 *  Created on: 2017年11月9日
 *      Author: liuyangcheng
 */

#ifndef CUSTOM_AW_APPS_NEWIPC_SOURCE_COMMON_TEMPLATE_HELPER_H_
#define CUSTOM_AW_APPS_NEWIPC_SOURCE_COMMON_TEMPLATE_HELPER_H_

#include <stdio.h>
#include <iostream>
#include <vector>
#include <memory>
#include <string.h>

#define varName(x) (#x)
#define MAX_CONST_CHAR 25
#define H_MIN(a, b) (a)<(b)?(a):(b)
#define _T(s) \
    getChr(s,0), \
    getChr(s,1), \
    getChr(s,2), \
    getChr(s,3), \
    getChr(s,4), \
    getChr(s,5), \
    getChr(s,6), \
    getChr(s,7), \
    getChr(s,8), \
    getChr(s,9), \
    getChr(s,10), \
    getChr(s,11), \
    getChr(s,12), \
    getChr(s,13), \
    getChr(s,14), \
    getChr(s,15), \
    getChr(s,16), \
    getChr(s,17), \
    getChr(s,18), \
    getChr(s,19), \
    getChr(s,20), \
    getChr(s,21), \
    getChr(s,22), \
    getChr(s,23), \
    getChr(s,24), \
    getChr(s,25)

#define getChr(name, ii) ((H_MIN(ii, MAX_CONST_CHAR)) < strlen(name)?name[ii]:0)

#define IObserverWrap(type) EyeseeLinux::IObserver_d<_T(varName(type))>

#define AsyncObserverWrap(type) AsyncObserver<_T(varName(type))>

#define ISubjectWrap(type) EyeseeLinux::ISubject_d<_T(varName(type))>

#endif /* CUSTOM_AW_APPS_NEWIPC_SOURCE_COMMON_TEMPLATE_HELPER_H_ */
