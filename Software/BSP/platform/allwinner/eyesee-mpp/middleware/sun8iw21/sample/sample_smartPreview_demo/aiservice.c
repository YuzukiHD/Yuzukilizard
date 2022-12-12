#include "aiservice.h"
#include "aiservice_detect.h"

int ai_service_start(ai_service_attr_t *pattr)
{
    int ret = -1;

    ret = aw_service_init();
    ret = aw_service_start(pattr);
    if (ret != 0)
    {
        aw_service_uninit();
    }

    return ret;
}

int ai_service_stop(void)
{
    int ret = -1;

    ret = aw_service_stop();
    ret = aw_service_uninit();

    return ret;
}

