/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "fan_control.h"

int main(int argc, char** argv)
{
    int ret = 0;
    ret = AW_MPI_InitMotor();
    if(ret < 0){
        aloge("init motor fail!\n");
        return -1;
    }
    int count = 0;
    aloge("run motor 1 90");
    ret = AW_MPI_EnableMotor(1, 90, 0);  //Group 1是原型机主板上的电机
    if(ret < 0){
       aloge("enable motor fail!\n");
    }
    aloge("run motor 2 180");
    ret = AW_MPI_EnableMotor(2, 180, 1); //Group 2是sensor子板上的电机
    if(ret < 0){
       aloge("enable motor fail!\n");
    }
    sleep(1);
    /*ret = AW_MPI_DisableMotor(1);
    if(ret < 0){
     aloge("disable motor fail!\n");
     return ret;
    }
    ret = AW_MPI_DisableMotor(2);
    if(ret < 0){
     aloge("disable motor fail!\n");
     return ret;
    }*/
    alogd("%s test result: %s", argv[0], ((0 == ret) ? "success" : "fail"));
    return ret;
}


