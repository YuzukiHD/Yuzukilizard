#include <stdio.h>
#include <stdlib.h>
#include <utils/plat_log.h>

#include "MotionDetect.h"
#include "MppHelper.h"

// 移动侦测回调函数
int on_motion_callback(motion_callback_data data) {
    //alogd("%s: total motion area:%d", __func__, data.num);
    /*int i;
    for (i = 0; i < data.num; i++) {
        // alogd("area_%d:[(%d,%d),(%d,%d)]", i, data.rect[i].left, data.rect[i].top, data.rect[i].right, data.rect[i].bottom);
        printf("[(%d,%d),(%d,%d)]", i, data.rect[i].left, data.rect[i].top, data.rect[i].right, data.rect[i].bottom);
    }
    printf("\n");*/
    return 0;
}

// 人形检测回调函数
int on_humanoid_callback(int left, int top, int right, int bottom) {
    //alogd("%s: [%d, %d, %d, %d]", __func__, left, top, right, bottom);
    return 0;
}

// 区域入侵检测回调函数
int on_cross_callback(cross_callback_data data) {
    if (data.mode == CROSS_WORK_MODE_LINE) {
        alogd("%s: LINE[%d] type: %d (%d, %d)", __func__, data.id, data.type, data.x, data.y);
    } else if (data.mode == CROSS_WORK_MODE_RECT) {
        alogd("%s: RECT[%d] type: %d (%d, %d)", __func__, data.id, data.type, data.x, data.y);
    }
    return 0;
}

int main(int argc, char *argv[]) {
    int ret, opt;
    if (argc != 2) {
        goto usage;
    }
    opt = atoi(argv[1]);
    ret = aw_service_init();
    ret = set_motion_callback(on_motion_callback);
    ret = set_humanoid_callback(on_humanoid_callback);
    ret = set_cross_callback(on_cross_callback);

    detect_region_settings settings = {
        .enable = (opt == 0),
        .sensitivity = 90,
        .num = REGIONS_MAX_NUM,
        .regions = { { 120, 80, 720, 600 }, { 1400, 150, 1720, 400 }, { 300, 680, 720, 880 }, { 1200, 560, 1880, 1000 } },
    };

    ret = set_motion_settings(&settings);
    ret = get_motion_settings(&settings);

    cross_settings cs = {
        .enable = (opt == 1 || opt == 2 || opt == 3),
        .sensitivity = 90,
        .outdoor = 0, // 模式 0：室外，1：室内
    };
    if (opt == 1) {
        cs.mode = CROSS_WORK_MODE_RECT;
        cs.num = CROSS_RECT_MAX_NUM;
        cs.settings.rects[0].x[0] = 520;
        cs.settings.rects[0].y[0] = 301;
        cs.settings.rects[0].x[1] = 385;
        cs.settings.rects[0].y[1] = 821;
        cs.settings.rects[0].x[2] = 1382;
        cs.settings.rects[0].y[2] = 896;
        cs.settings.rects[0].x[3] = 1680;
        cs.settings.rects[0].y[3] = 250;
        cs.settings.rects[0].direction = 0;      // 触发方向 0：双向，1：进入，2：离开
    } else if (opt == 2) {
        cs.mode = CROSS_WORK_MODE_LINE;
        cs.num = 2;
        cs.settings.lines[0].x1 = 520;
        cs.settings.lines[0].y1 = 301;
        cs.settings.lines[0].x2 = 385;
        cs.settings.lines[0].y2 = 821;
        cs.settings.lines[0].direction = 0; // 触发方向 0：双向，1：左->右，2：右->左
        cs.settings.lines[1].x1 = 1382;
        cs.settings.lines[1].y1 = 896;
        cs.settings.lines[1].x2 = 1680;
        cs.settings.lines[1].y2 = 250;
        cs.settings.lines[1].direction = 0; // 触发方向 0：双向，1：左->右，2：右->左
    } else if (opt == 3) {
        cs.mode = CROSS_WORK_MODE_HUMANOID;
    }
    ret = set_cross_settings(&cs);
    ret = get_cross_settings(&cs);

    ret = aw_service_start();
    char input;
    while (true) {
        scanf("%c", &input);
        if (input == 'q')
            break;
    }
    ret = aw_service_stop();
    ret = aw_service_uninit();
    return ret;

usage:
    printf("%s [NUM]\n"
            "    0:motion; 1:rect; 2:line; 3:humanoid\n", argv[0]);
    return -1;
}