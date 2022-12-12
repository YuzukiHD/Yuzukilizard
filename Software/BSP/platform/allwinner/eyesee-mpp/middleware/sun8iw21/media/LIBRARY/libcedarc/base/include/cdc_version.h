/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : cdc_version.h
* Description :
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2016/04/13
*   Comment :
*
*
*/

#ifndef CDC_VERSION_H
#define CDC_VERSION_H

#ifdef __cplusplus
extern "C" {
#endif

#define REPO_TAG ""
#define REPO_PATCH ""
#define REPO_BRANCH "tina-v853-dev"
#define REPO_COMMIT "ee230a47e5a2392ec315795121c2d112492fac1c"
#define REPO_DATE "Fri Jul 22 10:37:50 2022 +0800"
#define REPO_AUTHOR "wuguanjian"
#define REPO_CHANGE_ID "I28fe0c31c718f097d61ad6bdbcb4cb5df0bd3575"
#define RELEASE_AUTHOR "lichaopdc"

static inline void LogVersionInfo(void)
{
    logw("\n"
         ">>>>>>>>>>>>>>>>>>>>>>>>>>>>> Cedar Codec <<<<<<<<<<<<<<<<<<<<<<<<<<<<\n"
         "tag   : %s\n"
         "branch: %s\n"
         "commit: %s\n"
         "date  : %s\n"
         "author: %s\n"
         "change-id : %s\n"
         "release_author : %s\n"
         "patch : %s\n"
         "----------------------------------------------------------------------\n",
         REPO_TAG, REPO_BRANCH, REPO_COMMIT, REPO_DATE, REPO_AUTHOR, REPO_CHANGE_ID, RELEASE_AUTHOR, REPO_PATCH);
}

/* usage: TagVersionInfo(myLibTag) */
#define TagVersionInfo(tag) \
    static void VersionInfo_##tag(void) __attribute__((constructor));\
    void VersionInfo_##tag(void) \
    { \
        logd("-------library tag: %s-------", #tag);\
        LogVersionInfo(); \
    }


#ifdef __cplusplus
}
#endif

#endif

