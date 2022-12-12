#include <string.h>

#include <jd.h>

static jd_param_t g_jd_param;
static jd_result_t g_jd_result;

void jd_get_param(void* p) {
    memcpy(p, &g_jd_param, sizeof(g_jd_param));
}

void jd_set_result(const void* p) {
    memcpy(&g_jd_result, p, sizeof(g_jd_result));
}

int jd_set_key(const char* key) {
    int len = sizeof(g_jd_param.key_bytes);
    if (len > strlen(key))
        len = strlen(key);
    memcpy(g_jd_param.key_bytes, key, len);

    return 0;
}

int jd_get_crc(uint8* crc) {
    if (g_jd_result.es_result.state != EASY_SETUP_STATE_DONE) {
        LOGE("easy setup data unavailable\n");
        return -1;
    }

    *crc = g_jd_result.crc;
    return 0;
}

int jd_get_ip(uint32* ip) {
    if (g_jd_result.es_result.state != EASY_SETUP_STATE_DONE) {
        LOGE("easy setup data unavailable\n");
        return -1;
    }

    *ip = g_jd_result.ip;
    return 0;
}

int jd_get_port(uint16* port) {
    if (g_jd_result.es_result.state != EASY_SETUP_STATE_DONE) {
        LOGE("easy setup data unavailable\n");
        return -1;
    }

    *port = g_jd_result.port;
    return 0;
}
