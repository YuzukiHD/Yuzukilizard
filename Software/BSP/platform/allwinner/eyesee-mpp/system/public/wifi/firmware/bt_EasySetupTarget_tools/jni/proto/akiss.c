#include <string.h>

#include <akiss.h>

static akiss_param_t g_akiss_param;
static akiss_result_t g_akiss_result;

void akiss_get_param(void* p) {
    memcpy(p, &g_akiss_param, sizeof(g_akiss_param));
}

void akiss_set_result(const void* p) {
    memcpy(&g_akiss_result, p, sizeof(g_akiss_result));
}

int akiss_set_key(const char* key) {
    int len = sizeof(g_akiss_param.key_bytes);
    if (len > strlen(key))
        len = strlen(key);
    memcpy(g_akiss_param.key_bytes, key, len);

    return 0;
}

int akiss_get_random(uint8* random) {
    if (g_akiss_result.es_result.state != EASY_SETUP_STATE_DONE) {
        LOGE("easy setup data unavailable\n");
        return -1;
    }

    *random = g_akiss_result.random;
    return 0;
}
