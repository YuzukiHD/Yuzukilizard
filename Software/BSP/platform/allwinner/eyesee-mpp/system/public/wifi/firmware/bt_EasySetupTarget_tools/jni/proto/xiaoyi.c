#include <string.h>

#include <xiaoyi.h>

static xiaoyi_result_t g_xiaoyi_result;

void xiaoyi_set_result(const void* p) {
    memcpy(&g_xiaoyi_result, p, sizeof(g_xiaoyi_result));
    LOGE("xiaoyi_set_result: state: %d\n", g_xiaoyi_result.es_result.state);
}

int xiaoyi_get_buffer(uint8* len, uint8* buf) {
    if (g_xiaoyi_result.es_result.state != EASY_SETUP_STATE_DONE) {
        LOGE("easy setup data unavailable\n");
        return -1;
    }

    if (!len || !buf) {
        return -1;
    }

    *len = g_xiaoyi_result.len;
    memcpy(buf, g_xiaoyi_result.buf, *len);
    return 0;
}
