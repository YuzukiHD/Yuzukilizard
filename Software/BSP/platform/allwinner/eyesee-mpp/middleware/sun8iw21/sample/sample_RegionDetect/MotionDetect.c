#include <unistd.h>
#include <string.h>

#include <sys/time.h>
#include <utils/plat_log.h>

#include <pthread.h>

#include "MotionDetect.h"
#include "MppHelper.h"
#include "awnn_det.h"
#include "cross_utils.h"

#define PREVIEW_WIDTH 720
#define PREVIEW_HEIGHT 405
#define SRC_WIDTH 1920
#define SRC_HEIGHT 1080
#define OUTPUT_FILE_PATH "/mnt/extsd/motion/motion.h265"
#define NETWORK_HUMAN "/mnt/extsd/human.nb"

#define VI_NPU_PIC_WIDTH     320
#define VI_NPU_PIC_HEIGHT    192
#define VIPP_INPUT_FPS       30

//#define SHOW_FPS             1
#define FRAME_COUNT          30

#define MILLION              1000000L
static long get_time() {
    struct timeval time;
    gettimeofday(&time, NULL);
    return (time.tv_usec + time.tv_sec * MILLION);
}

detect_region_settings m_motion_settings;
cross_settings m_cross_settings;
int mStop;

// 服务初始化
int aw_service_init() {
    if (MPP_HELPER_INIT()) {
        aloge("sys Init failed!");
        return -1;
    }
    memset(&m_motion_settings, 0, sizeof(detect_region_settings));
    memset(&m_cross_settings, 0, sizeof(cross_settings));
    return 0;
}

// 服务反初始化
int aw_service_uninit() {
    /* exit mpp systerm */
    if (MPP_HELPER_UNINIT()) {
        aloge("sys exit failed!");
    }
    return 0;
}


on_motion_callback_t m_motion_callback;
// 设置移动侦测回调函数
int set_motion_callback(on_motion_callback_t callback) {
    m_motion_callback = callback;
    return 0;
}

// 设置移动侦测参数设置
int set_motion_settings(detect_region_settings *settings) {
    alogd("%s", __func__);
    alogd("enable = %d", settings->enable);
    alogd("sensitivity = %d", settings->sensitivity);
    alogd("num = %d", settings->num);
    int i;
    for (i = 0; i < settings->num; i++) {
        motion_rect rect = settings->regions[i];
        alogd("Motion Region[%d] = [%d, %d, %d, %d]", i, rect.left, rect.top, rect.right, rect.bottom);
    }
    memcpy(&m_motion_settings, settings, sizeof(detect_region_settings));
    return 0;
}

// 获取移动侦测参数设置
int get_motion_settings(detect_region_settings *settings) {
    if (settings)
        memcpy(settings, &m_motion_settings, sizeof(detect_region_settings));
    return 0;
}

on_cross_callback_t m_cross_callback;
// 设置区域入侵检测回调函数
int set_cross_callback(on_cross_callback_t callback) {
    m_cross_callback = callback;
    return 0;
}

on_humanoid_callback_t m_humanoid_callback;
// 设置人形检测回调函数
int set_humanoid_callback(on_humanoid_callback_t callback) {
    m_humanoid_callback = callback;
    return 0;
}

// 设置区域入侵检测参数设置
int set_cross_settings(cross_settings *settings) {
    alogd("%s", __func__);
    alogd("mode = %s", settings->mode == CROSS_WORK_MODE_LINE ? "LINE" : settings->mode == CROSS_WORK_MODE_RECT ? "RECT" : settings->mode == CROSS_WORK_MODE_HUMANOID ? "HUMANOID" : "UNKNOWN");
    alogd("enable = %d", settings->enable);
    alogd("sensitivity = %d", settings->sensitivity);
    alogd("outdoor = %d", settings->outdoor);
    alogd("num = %d", settings->num);
    int i;
    if (settings->mode == CROSS_WORK_MODE_LINE) {
        for (i = 0; i < settings->num; i++) {
            cross_line line = settings->settings.lines[i];
            alogd("LINE[%d]: (%d, %d) -> (%d, %d) direction = %s", i, line.x1, line.y1, line.x2, line.y2, line.direction == 0 ? "all" : line.direction == 1 ? "left" : line.direction == 2 ? "right" : "unknown");
        }
    } else if (settings->mode == CROSS_WORK_MODE_RECT) {
        for (i = 0; i < settings->num; i++) {
            cross_rect rect = settings->settings.rects[i];
            int j, index;
            char buffer[128];
            index = sprintf(buffer, "RECT[%d]: ", i);
            for (j = 0; j < CROSS_RECT_MAX_POINT_NUM; j++) {
                index += sprintf(buffer + index, "(%d, %d) ", rect.x[j], rect.y[j]);
            }
            index += sprintf(buffer + index, "direction = %s", rect.direction == 0 ? "all" : rect.direction == 1 ? "in" : rect.direction == 2 ? "out" : "unknown");
            alogd(buffer);
        }
    } else if (settings->mode == CROSS_WORK_MODE_HUMANOID) {
        // TODO I don't know what we need to config human detect area
    }
    memcpy(&m_cross_settings, settings, sizeof(cross_settings));
    return 0;
}

// 获取区域入侵检测参数设置
int get_cross_settings(cross_settings *settings) {
    if (settings)
        memcpy(settings, &m_cross_settings, sizeof(cross_settings));
    return 0;
}




///////////////////////////////////// thread function /////////////////////////////////////

static int nv12_draw_point(unsigned char* yBuffer, unsigned char* uvBuffer, int x, int y, int yColor, int uColor, int vColor) {
    if (x < 0 || x >= PREVIEW_WIDTH) return -1;
    if (y < 0 || y >= PREVIEW_HEIGHT) return -1;
    yBuffer[y*PREVIEW_WIDTH+x] = yColor;
    uvBuffer[(y/2)*PREVIEW_WIDTH+x/2*2] = uColor;
    uvBuffer[(y/2)*PREVIEW_WIDTH+x/2*2+1] = vColor;
    return 0;
}

// NV12 draw red line
static int nv12_draw_line(unsigned char* yBuffer, unsigned char* uvBuffer, int x0, int y0, int x1, int y1) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y1 - y0 : y0 - y1, sy = y0 < y1 ? 1 : -1;
    int err = (dx > dy ? dx : -dy) / 2;

    while (nv12_draw_point(yBuffer, uvBuffer, x0, y0, 0x4C, 0x55, 0xFF), x0 != x1 || y0 != y1) {
        int e2 = err;
        if (e2 > -dx) { err -= dy; x0 += sx; }
        if (e2 <  dy) { err += dx; y0 += sy; }
    }
    return 0;
}

static int nv12_draw_rect(unsigned char* yBuffer, unsigned char* uvBuffer, int left, int top, int right, int bottom, int yColor, int uColor, int vColor) {
    int i;
    for (i = left; i <= right; i++) {
        nv12_draw_point(yBuffer, uvBuffer, i, top, yColor, uColor, vColor);
        nv12_draw_point(yBuffer, uvBuffer, i, bottom, yColor, uColor, vColor);
    }
    for (i = top; i <= bottom; i++) {
        nv12_draw_point(yBuffer, uvBuffer, left, i, yColor, uColor, vColor);
        nv12_draw_point(yBuffer, uvBuffer, right, i, yColor, uColor, vColor);
    }
    return 0;
}

static int nv12_draw_cycle(unsigned char* yBuffer, unsigned char* uvBuffer, int x, int y, int r, int yColor, int uColor, int vColor) {
    for (int i = x - r; i <= x + r; i++)
        for (int j = y - r; j <= y + r; j++) {
            if ((x - i) * (x - i) + (y - j) * (y - j) <= r * r)
                nv12_draw_point(yBuffer, uvBuffer, i, j, yColor, uColor, vColor);
        }
    return 0;
}

static motion_callback_data lastData;
static cross_callback_data m_cross_callback_data;
static BBoxResults_t *lastResult;
static int preview_callback(unsigned char* yBuffer, unsigned char* uBuffer, unsigned char* vBuffer) {
    if (m_motion_settings.enable) {
        for (int i = 0; i < m_motion_settings.num; i++) {
            nv12_draw_rect(yBuffer, uBuffer,
                    m_motion_settings.regions[i].left * PREVIEW_WIDTH / SRC_WIDTH,
                    m_motion_settings.regions[i].top * PREVIEW_HEIGHT / SRC_HEIGHT,
                    m_motion_settings.regions[i].right * PREVIEW_WIDTH / SRC_WIDTH,
                    m_motion_settings.regions[i].bottom * PREVIEW_HEIGHT / SRC_HEIGHT,
                    0x4C, 0x55, 0xFF // red
            );
        }
        for (int i = 0; i < lastData.num; i++) {
            nv12_draw_rect(yBuffer, uBuffer,
                    lastData.rect[i].left * PREVIEW_WIDTH / SRC_WIDTH,
                    lastData.rect[i].top * PREVIEW_HEIGHT / SRC_HEIGHT,
                    lastData.rect[i].right * PREVIEW_WIDTH / SRC_WIDTH,
                    lastData.rect[i].bottom * PREVIEW_HEIGHT / SRC_HEIGHT,
                    0x96, 0x2C, 0x15 // green
            );
        }
    }
    if (m_cross_settings.enable) {
        for (int i = 0; i < m_cross_settings.num; i++) {
            if (m_cross_settings.mode == CROSS_WORK_MODE_LINE) {
                nv12_draw_line(yBuffer, uBuffer,
                        m_cross_settings.settings.lines[i].x1 * PREVIEW_WIDTH / SRC_WIDTH,
                        m_cross_settings.settings.lines[i].y1 * PREVIEW_HEIGHT / SRC_HEIGHT,
                        m_cross_settings.settings.lines[i].x2 * PREVIEW_WIDTH / SRC_WIDTH,
                        m_cross_settings.settings.lines[i].y2 * PREVIEW_HEIGHT / SRC_HEIGHT
                );
            } else if (m_cross_settings.mode == CROSS_WORK_MODE_RECT) {
                for (int j = 0; j < 4; j++)
                    nv12_draw_line(yBuffer, uBuffer,
                            m_cross_settings.settings.rects[i].x[j] * PREVIEW_WIDTH / SRC_WIDTH,
                            m_cross_settings.settings.rects[i].y[j] * PREVIEW_HEIGHT / SRC_HEIGHT,
                            m_cross_settings.settings.rects[i].x[(j + 1) % 4] * PREVIEW_WIDTH / SRC_WIDTH,
                            m_cross_settings.settings.rects[i].y[(j + 1) % 4] * PREVIEW_HEIGHT / SRC_HEIGHT
                    );
            }
        }
        if (lastResult) {
            for (int i = 0; i < lastResult->valid_cnt; i++) {
                nv12_draw_rect(yBuffer, uBuffer,
                        lastResult->boxes[i].xmin * PREVIEW_HEIGHT / VI_NPU_PIC_HEIGHT,
                        lastResult->boxes[i].ymin * PREVIEW_HEIGHT / VI_NPU_PIC_HEIGHT,
                        lastResult->boxes[i].xmax * PREVIEW_HEIGHT / VI_NPU_PIC_HEIGHT,
                        lastResult->boxes[i].ymax * PREVIEW_HEIGHT / VI_NPU_PIC_HEIGHT,
                        0x96, 0x2C, 0x15 // green
                );
                int cx = (lastResult->boxes[i].xmin + lastResult->boxes[i].xmax) / 2 * PREVIEW_HEIGHT / VI_NPU_PIC_HEIGHT;
                int cy = (lastResult->boxes[i].ymin + lastResult->boxes[i].ymax) / 2 * PREVIEW_HEIGHT / VI_NPU_PIC_HEIGHT;
                for (int x = cx - 5; x < cx +5; x++)
                    for (int y = cy -5; y < cy + 5; y++)
                        nv12_draw_point(yBuffer, uBuffer, x, y, 0x96, 0x2C, 0x15);
            }
        }
        if (m_cross_callback_data.x > 0 && m_cross_callback_data.y > 0) {
            nv12_draw_cycle(yBuffer, uBuffer, m_cross_callback_data.x * PREVIEW_WIDTH / SRC_WIDTH, m_cross_callback_data.y * PREVIEW_HEIGHT / SRC_HEIGHT, 15, 0x1D, 0xFF, 0x6B);
        }
    }
    return 0;
}

static void *preview_worker_thread(void *para) {
    ERRORTYPE ret;
    ret = MPP_HELPER_START_PREVIEW();

#ifdef SHOW_FPS
    int count = 0;
    long last = 0L;
#endif

    while (!mStop) {
        MPP_HELPER_DO_PREVIEW(preview_callback);
#ifdef SHOW_FPS
            if (count++ % FRAME_COUNT == 0) {
                long now = get_time();
                if (last != 0) {
                    int fps = (int)(FRAME_COUNT * MILLION / (now - last));
                    alogd("preview fps = %d", fps);
                }
                last = now;
            }
#endif
    }

    ret = MPP_HELPER_STOP_PREVIEW();
    return NULL;
}

static int check_cross_rect(motion_rect *ra, motion_rect *rb) {
    int left = ra->left > rb->left ? ra->left : rb->left;
    int right = ra->right < rb->right ? ra->right : rb->right;
    int top = ra->top > rb->top ? ra->top : rb->top;
    int bottom = ra->bottom < rb->bottom ? ra->bottom : rb->bottom;
    int width = right > left ? right - left : 0;
    int height = bottom > top ? bottom - top : 0;
    return width * height;
}

FILE* recordFp;
static int venc_record_callback(unsigned char *addr, unsigned int len) {
    if (recordFp) {
        fwrite(addr, len, 1, recordFp);
    }
    return 0;
}

static motion_callback_data cbData;
static int venc_motion_callback(VencMotionSearchResult motionResult) {
    if (!m_motion_settings.enable) {
        return 0;
    }

    int ret = 0;
    int i, j;
    motion_rect rect;
    int num = 0;

    memset(&cbData, 0, sizeof(motion_callback_data));
    for (i = 0; i < MAX_VE_MOTION_NUM; i++) {
        if (motionResult.region[i].is_motion) {
            rect.left = motionResult.region[i].pix_x_bgn;
            rect.top = motionResult.region[i].pix_y_bgn;
            rect.right = motionResult.region[i].pix_x_end;
            rect.bottom = motionResult.region[i].pix_y_end;
            for (j = 0; j < m_motion_settings.num; j++) {
                //m_motion_settings.sensitivity;
                if (check_cross_rect(&m_motion_settings.regions[j], &rect)) {
                    memcpy(&cbData.rect[num], &rect, sizeof(motion_rect));
                    num++;
                    break;
                }
            }
        }
    }
    cbData.num = num;
    ret = memcmp(&cbData, &lastData, sizeof(motion_callback_data));
    if (0 != ret) {
        if (m_motion_callback) {
            m_motion_callback(cbData);
        }
        memcpy(&lastData, &cbData, sizeof(motion_callback_data));
    } else {
        //alogd("same data.");
    }

    return 0;
}

// 移动侦测开始
static void *record_worker_thread(void *para) {
    ERRORTYPE ret;
    int i;

    recordFp = fopen(OUTPUT_FILE_PATH, "wb+");
    memset(&lastData, 0, sizeof(motion_callback_data));

    MPP_HELPER_START_RECORD(venc_record_callback);

#ifdef SHOW_FPS
    int count = 0;
    long last = 0L;
#endif

    while (!mStop) {
        MPP_HELPER_DO_RECORD(venc_record_callback, venc_motion_callback);
#ifdef SHOW_FPS
            if (count++ % FRAME_COUNT == 0) {
                long now = get_time();
                if (last != 0) {
                    int fps = (int)(FRAME_COUNT * MILLION / (now - last));
                    alogd("record fps = %d", fps);
                }
                last = now;
            }
#endif
    }
    if (recordFp) fclose(recordFp);

    MPP_HELPER_STOP_RECORD();

    return NULL;
}

Awnn_Det_Handle bodyDet_handle = NULL;

static int state[4];
static int npu_callback(unsigned char* yBuffer, unsigned char* uvBuffer, void *phuman_run) {
    if (!bodyDet_handle) return 0;

    int i, j;
    unsigned char *human_input_buffers[2];
    human_input_buffers[0] = yBuffer;
    human_input_buffers[1] = uvBuffer;

    ((Awnn_Run_t *)phuman_run)->input_buffers = human_input_buffers;

    BBoxResults_t *res;
    m_cross_callback_data.mode = m_cross_settings.mode;

    /*if (count == 250) {
        FILE* testFp = fopen("/mnt/extsd/motion/test.yuv", "wb+");
        if (testFp) {
            fwrite(pBuffer, sizeof(pBuffer), 1, testFp);
            fclose(testFp);
        }

    }*/

    res = awnn_detect_run(bodyDet_handle, (Awnn_Run_t *)phuman_run);
    //alogd("npu detect end");
    if (res) {
        lastResult = res;

        for (j = 0; j < res->valid_cnt; j++) {
            /*alogd("cls %d, prob %d, rect [%d, %d, %d, %d]\n",res->boxes[j].label, res->boxes[j].score,
                        res->boxes[j].xmin, res->boxes[j].ymin, res->boxes[j].xmax, res->boxes[j].ymax);*/
            int x = (res->boxes[j].xmin + res->boxes[j].xmax) / 2 * SRC_HEIGHT / VI_NPU_PIC_HEIGHT;
            int y = (res->boxes[j].ymin + res->boxes[j].ymax) / 2 * SRC_HEIGHT / VI_NPU_PIC_HEIGHT;
            int side = 0;
            //memset(&m_cross_callback_data, 0, sizeof(cross_callback_data));
            if (m_cross_settings.mode == CROSS_WORK_MODE_LINE) {
                for (i = 0; i < m_cross_settings.num; i++) {
                    cross_line line = m_cross_settings.settings.lines[i];
                    // 0左边范围内，1右边范围内，2左边范围外，3右边范围外
                    side = line_side(line.x1, line.y1, line.x2, line.y2, x, y) + 1;
                    if (state[i]) {
                        // direction 触发方向 0：双向，1：左->右，2：右->左
                        if ((line.direction == 0 || line.direction == 1) && side == 2 && state[i] == 1) {
                            if (m_cross_callback) {
                                m_cross_callback_data.id = i;
                                m_cross_callback_data.type = 1; // 0:方向未知; 1:左->右; 2:右->左
                                m_cross_callback_data.x = x;
                                m_cross_callback_data.y = y;
                                m_cross_callback(m_cross_callback_data);
                            }
                        } else if ((line.direction == 0 || line.direction == 2) && side == 1 && state[i] == 2) {
                            if (m_cross_callback) {
                                m_cross_callback_data.id = i;
                                m_cross_callback_data.type = 2; // 0:方向未知; 1:左->右; 2:右->左
                                m_cross_callback_data.x = x;
                                m_cross_callback_data.y = y;
                                m_cross_callback(m_cross_callback_data);
                            }
                        }
                    }
                    state[i] = side;
                }
            } else if (m_cross_settings.mode == CROSS_WORK_MODE_RECT) {
                for (i = 0; i < m_cross_settings.num; i++) {
                    cross_rect rect = m_cross_settings.settings.rects[i];
                    // 多边形区域内
                    side = pnpoly(4, rect.x, rect.y, x, y) + 1; // 多边形区域内:0,多边形区域外:1
                    if (state[i]) {
                        // direction 触发方向 0：双向，1：进入，2：离开
                        if ((rect.direction == 0 || rect.direction == 1) && side == 2 && state[i] == 1) {
                            if (m_cross_callback) {
                                m_cross_callback_data.id = i;
                                m_cross_callback_data.type = 1; // 0:方向未知; 1:进入; 2:离开
                                m_cross_callback_data.x = x;
                                m_cross_callback_data.y = y;
                                m_cross_callback(m_cross_callback_data);
                            }
                        } else if ((rect.direction == 0 || rect.direction == 2) && side == 1 && state[i] == 2) {
                            if (m_cross_callback) {
                                m_cross_callback_data.id = i;
                                m_cross_callback_data.type = 2; // 0:方向未知; 1:进入; 2:离开
                                m_cross_callback_data.x = x;
                                m_cross_callback_data.y = y;
                                m_cross_callback(m_cross_callback_data);
                            }
                        }
                    }
                    state[i] = side;
                }
            } else if (m_cross_settings.mode == CROSS_WORK_MODE_HUMANOID) {
                if (m_humanoid_callback) {
                    m_humanoid_callback(res->boxes[j].xmin, res->boxes[j].ymin, res->boxes[j].xmax, res->boxes[j].ymax);
                }
            }
        }
        return res->valid_cnt;
    }
    return 0;
}

static void *npu_worker_thread(void *para) {
    if (!m_cross_settings.enable) {
        if (m_cross_settings.mode == CROSS_WORK_MODE_LINE) {
            alogd("line settings disabled");
        } else if (m_cross_settings.mode == CROSS_WORK_MODE_RECT) {
            alogd("rect settings disabled");
        } else if (m_cross_settings.mode == CROSS_WORK_MODE_HUMANOID) {
            alogd("human settings disabled");
        } else {
            aloge("unknown mode");
        }
        // m_cross_settings.sensitivity, m_cross_settings.outdoor
        return NULL;
    }

    ERRORTYPE ret;

#ifdef SHOW_FPS
    int count = 0;
    long last = 0L;
#endif

    ret = MPP_HELPER_START_NPU(VI_NPU_PIC_WIDTH, VI_NPU_PIC_HEIGHT, VIPP_INPUT_FPS);

	Awnn_Init_t init_t;
	memset(&init_t, 0, sizeof(Awnn_Init_t));
	init_t.human_nb = NETWORK_HUMAN;
	awnn_detect_init(&bodyDet_handle, &init_t);
    if (bodyDet_handle) {
        memset(state, 0, sizeof(state));
        Awnn_Run_t human_run_t;
        awnn_create_run(&human_run_t, AWNN_HUMAN_DET);
        while (!mStop) {
            MPP_HELPER_DO_NPU((void *)&human_run_t, npu_callback);
#ifdef SHOW_FPS
            if (count++ % FRAME_COUNT == 0) {
                long now = get_time();
                if (last != 0) {
                    int fps = (int)(FRAME_COUNT * MILLION / (now - last));
                    alogd("npu fps = %d", fps);
                }
                last = now;
            }
#endif
        }
        awnn_destroy_run(&human_run_t);
        awnn_detect_exit(bodyDet_handle);
    }

    ret = MPP_HELPER_STOP_NPU();

    return NULL;
}

pthread_t preview_thread;
pthread_t record_thread;
pthread_t nna_detect_thread;

int aw_service_start() {
    int ret;
    mStop = 0;
    ret = pthread_create(&preview_thread, NULL, preview_worker_thread, NULL);
    ret = pthread_create(&record_thread, NULL, record_worker_thread, NULL);
    ret = pthread_create(&nna_detect_thread, NULL, npu_worker_thread, NULL);
    return ret;
}


int aw_service_stop() {
    int ret;
    unsigned long value;
    mStop = 1;
    ret = pthread_join(nna_detect_thread, (void **)&value);
    ret = pthread_join(record_thread, (void **)&value);
    ret = pthread_join(preview_thread, (void **)&value);
    return ret;
}
