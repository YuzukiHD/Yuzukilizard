#include "easy_setup.h"

#include "bcast.h"
#include "neeze.h"
#include "akiss.h"
#include "changhong.h"
#include "jingdong.h"
#include "jd.h"
#include "xiaoyi.h"

extern uint16 g_protocol_mask;

void easy_setup_enable_bcast() {
    g_protocol_mask |= (1<<EASY_SETUP_PROTO_BCAST);
}

void easy_setup_enable_neeze() {
    g_protocol_mask |= (1<<EASY_SETUP_PROTO_NEEZE);
}

void easy_setup_enable_akiss() {
    g_protocol_mask |= (1<<EASY_SETUP_PROTO_AKISS);
}

void easy_setup_enable_changhong() {
    g_protocol_mask |= (1<<EASY_SETUP_PROTO_CHANGHONG);
}

void easy_setup_enable_jingdong() {
    g_protocol_mask |= (1<<EASY_SETUP_PROTO_JINGDONG);
}

void easy_setup_enable_xiaoyi() {
    g_protocol_mask |= (1<<EASY_SETUP_PROTO_XIAOYI);
}

void easy_setup_enable_jd() {
    g_protocol_mask |= (1<<EASY_SETUP_PROTO_JD);
}

void easy_setup_enable_protocols(uint16 proto_mask) {
    g_protocol_mask |= proto_mask;
}

void easy_setup_get_param(uint16 proto_mask, tlv_t** pptr) {
    tlv_t* t = *pptr;
    int i=0; 
    for (i=0; i<EASY_SETUP_PROTO_MAX; i++) {
        if (proto_mask & (1<<i)) {
            t->type = i;

            if (i==EASY_SETUP_PROTO_BCAST) {
                t->length = sizeof(bcast_param_t);
                bcast_get_param(t->value);
            } else if (i==EASY_SETUP_PROTO_NEEZE) {
                t->length = sizeof(neeze_param_t);
                neeze_get_param(t->value);
            } else if (i==EASY_SETUP_PROTO_AKISS) {
                t->length = sizeof(akiss_param_t);
                akiss_get_param(t->value);
            } else if (i==EASY_SETUP_PROTO_JINGDONG) {
                t->length = sizeof(jingdong_param_t);
                //jingdong_get_param(t->value);
            } else if (i==EASY_SETUP_PROTO_JD) {
                t->length = sizeof(jd_param_t);
                //jd_get_param(t->value);
            } else {
                t->length = 0;
            }

            t = (tlv_t*) (t->value + t->length);
        }
    }

    *pptr = t;
}

void easy_setup_set_result(uint8 protocol, void* p) {
    if (protocol == EASY_SETUP_PROTO_BCAST) {
        bcast_set_result(p);
    } else if (protocol == EASY_SETUP_PROTO_NEEZE) {
        neeze_set_result(p);
    } else if (protocol == EASY_SETUP_PROTO_AKISS) {
        akiss_set_result(p);
    } else if (protocol == EASY_SETUP_PROTO_CHANGHONG) {
        changhong_set_result(p);
    } else if (protocol == EASY_SETUP_PROTO_JINGDONG) {
        //jingdong_set_result(p);
    } else if (protocol == EASY_SETUP_PROTO_JD) {
        //jd_set_result(p);
    } else if (protocol == EASY_SETUP_PROTO_XIAOYI) {
        //xiaoyi_set_result(p);
    } else {
        ;// nothing done
    }
}
