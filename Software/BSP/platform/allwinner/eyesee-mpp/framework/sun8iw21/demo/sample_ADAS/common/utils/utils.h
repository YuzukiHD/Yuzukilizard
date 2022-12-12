#pragma once

#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>

#undef LOG_TAG
#define LOG_TAG "Utils"

#include "common/app_log.h"

#define SDVCAM_BIN_1   "/mnt/extsd/.sdvcam"
#define SDVCAM_BIN_2   "/usr/bin/sdvcam"

/**********c string utils define***************/
#define CON_2STRING(a, b)       a "" b
#define CON_3STRING(a, b, c)    a "" b "" c

#define DIR_2CAT(base, sub1)   base "/" sub1 "/"
#define DIR_3CAT(base, sub1, sub2)   base "/" sub1 "/" sub2 "/"


/**********file utils define***************/
#define FILE_EXIST(PATH)   (access(PATH, F_OK) == 0)

#define FILE_READABLE(PATH)   (access(PATH, R_OK) == 0)

#define MAX_ONE_FILE_SIZE       (2*1024-200)

/**********dir utils define***************/
int8_t create_dir(const char *dirpath);

int getoneline(const char *path, char *buf, int size);

inline void set_all_fd_cloexec() {
    for (int i = 3; i < getdtablesize(); i++) {
        int flags;
        if ((flags = fcntl(i, F_GETFD)) != -1)
            fcntl(i, F_SETFD, flags | FD_CLOEXEC);
    }
}

uint32_t calc_record_time_by_size(uint32_t bitrate, uint32_t size);
int64_t calc_record_size_by_time(uint32_t bitrate, uint32_t ms);
int get_cmd_result(const char *cmd, char *buf, uint16_t size);
bool verify_bin_md5();

namespace EyeseeLinux {

typedef struct TimeTest
{
    public:
        TimeTest(const char *msg) {
            this->msg_ = strdup(msg);
            gettimeofday(&s_tv, NULL);
        }
        ~TimeTest() {
            gettimeofday(&e_tv, NULL);
            uint64_t diff_usec = (e_tv.tv_sec - s_tv.tv_sec) * 1000000 + (e_tv.tv_usec - s_tv.tv_usec);
            db_msg("%s, spend time: %lld usec", msg_, diff_usec);

            free(msg_);
            msg_ = NULL;
        }
    private:
        char *msg_;
        struct timeval s_tv;
        struct timeval e_tv;
} TimeTest;
}

