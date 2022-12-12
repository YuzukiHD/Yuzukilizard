#include <string.h>

#include <jingdong.h>

static jingdong_param_t g_jingdong_param = {
    .key_bytes = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    },
    .random_bytes = {
        'b', '7', '0', 'e', 'c', '5', '6', '3',
        '2', '6', 'b', '2', '4', 'f', '3', '9',
    },
};
static jingdong_result_t g_jingdong_result;

void jingdong_get_param(void* p) {
    memcpy(p, &g_jingdong_param, sizeof(g_jingdong_param));
}

void jingdong_set_result(const void* p) {
    memcpy(&g_jingdong_result, p, sizeof(g_jingdong_result));
    LOGE("jingdong_set_result: state: %d\n", g_jingdong_result.es_result.state);
}

int jingdong_set_key(const char* key) {
    int len = sizeof(g_jingdong_param.key_bytes);
    if (len > strlen(key))
        len = strlen(key);
    memcpy(g_jingdong_param.key_bytes, key, len);

    return 0;
}

int jingdong_get_sec_mode(uint8* sec) {
    if (g_jingdong_result.es_result.state != EASY_SETUP_STATE_DONE) {
        LOGE("easy setup data unavailable\n");
        return -1;
    }

    *sec = g_jingdong_result.sec_mode;
    return 0;
}
