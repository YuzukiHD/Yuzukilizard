#include <string.h>

#include <changhong.h>

static changhong_result_t g_changhong_result;

void changhong_set_result(const void* p) {
    memcpy(&g_changhong_result, p, sizeof(g_changhong_result));
    LOGE("changhong_set_result: state: %d\n", g_changhong_result.es_result.state);
}

int changhong_get_sec_mode(uint8* sec) {
    if (g_changhong_result.es_result.state != EASY_SETUP_STATE_DONE) {
        LOGE("easy setup data unavailable\n");
        return -1;
    }

    *sec = g_changhong_result.sec_mode;
    return 0;
}
