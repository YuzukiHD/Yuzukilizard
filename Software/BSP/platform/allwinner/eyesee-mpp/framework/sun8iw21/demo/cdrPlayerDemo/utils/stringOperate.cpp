/*
********************************************************************************
*                           Android multimedia module
*
*          (c) Copyright 2010-2015, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : stringOperate.cpp
* Version: V1.0
* By     : eric_wang
* Date   : 2015-7-13
* Description:
********************************************************************************
*/
#include "stringOperate.h"
#include <string.h>

using namespace std;

namespace EyeseeLinux {

int removeCharFromCString(char *pString, char c)
{
    int length = strlen(pString);
    int i, j;
    for(i=0;i<length;i++)
    {
        if(pString[i] == c)
        {
            pString[i] = '\0';
            for(j=i+1;j<length;j++)
            {
                pString[j-1] = pString[j];
            }
            i--;
            length--;
        }
    }
    return (int)strlen(pString);
}

int removeCharFromString(string& nString, char c)
{
    string::size_type   pos;
    while(1)
    {
        pos = nString.find(c);
        if(pos != string::npos)
        {
            nString.erase(pos, 1);
        }
        else
        {
            break;
        }
    }
    return (int)nString.size();
}


};

