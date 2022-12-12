/*
********************************************************************************
*                           Android multimedia module
*
*          (c) Copyright 2010-2015, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : stringOperate.h
* Version: V1.0
* By     : eric_wang
* Date   : 2015-7-13
* Description:
********************************************************************************
*/
#ifndef _STRINGOPERATE_H_
#define _STRINGOPERATE_H_

#include <string>

namespace EyeseeLinux {

extern int removeCharFromCString(char *pString, char c);
extern int removeCharFromString(std::string& nString, char c);

};

#endif  /* _STRINGOPERATE_H_ */


