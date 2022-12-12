/*
 * ring_buffer.c -- a ring buffer manager
 *
 * Copyright 2018-2021 version 0.1
 *
 * Created by YellowMax <huangbohan2001@163.com>
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#include "ring_buffer.h"

#define min(a, b)              \
    ({                         \
        typeof(a) __a = (a);   \
        typeof(b) __b = (b);   \
        __a < __b ? __a : __b; \
    })

#define max(a, b)              \
    ({                         \
        typeof(a) __a = (a);   \
        typeof(b) __b = (b);   \
        __a > __b ? __a : __b; \
    })


static inline struct ring_buffer_entity 
    *rb_hd_to_ent(struct ring_buffer *rb_hd)
{
    return (struct ring_buffer_entity *)rb_hd->private;
}

static int rb_in(struct ring_buffer *rb_hd,
        void *buf_in, unsigned int elem_num)
{
    struct ring_buffer_entity *rb_ent = rb_hd_to_ent(rb_hd);

    if (elem_num == 0 || elem_num > rb_ent->elem_cnt) {
        nb_loge("wrong elem_num (%d)!!\r\n", (int)elem_num);
        return -1;
    }

    unsigned int cp_cnt   = min(elem_num, rb_ent->elem_cnt-(rb_ent->rb_in-rb_ent->rb_out));
    unsigned int cp_step1 = min(cp_cnt, rb_ent->elem_cnt - (rb_ent->rb_in%rb_ent->elem_cnt));

    memcpy(rb_ent->rb_buf + (rb_ent->rb_in%rb_ent->elem_cnt)*rb_ent->elem_size,
        buf_in, cp_step1*rb_ent->elem_size);
    memcpy(rb_ent->rb_buf, (unsigned char *)buf_in+cp_step1*rb_ent->elem_size,
        (cp_cnt-cp_step1)*rb_ent->elem_size);

    rb_ent->rb_in += cp_cnt;

    return cp_cnt;
}

static int rb_in_lock(struct ring_buffer *rb_hd,
        void *buf_in, unsigned int elem_num)
{
    struct ring_buffer_entity *rb_ent = rb_hd_to_ent(rb_hd);
    int iRet = 0;

    pthread_mutex_lock(&rb_ent->rb_lock);
    iRet = rb_in(rb_hd, buf_in, elem_num);
    pthread_mutex_unlock(&rb_ent->rb_lock);

    return iRet;
}


static int rb_in_f(struct ring_buffer *rb_hd,
        void *buf_in, unsigned int elem_num)
{
    struct ring_buffer_entity *rb_ent = rb_hd_to_ent(rb_hd);

    // TODO: fix it check if can use <if (likely(val))>
    if (elem_num == 0 ||
        elem_num > rb_ent->elem_cnt) {
        nb_loge("wrong elem_num (%d)!!\r\n", (int)elem_num);
        return -1;
    }

    pthread_mutex_lock(&rb_ent->rb_lock);

    if (rb_ent->rb_in-rb_ent->rb_out+elem_num >= rb_ent->elem_cnt)
        rb_ent->rb_out = rb_ent->rb_in + elem_num - rb_ent->elem_cnt;

    unsigned int cp_cnt   = elem_num;
    unsigned int cp_step1 = min(cp_cnt,
        rb_ent->elem_cnt - (rb_ent->rb_in%rb_ent->elem_cnt));
    memcpy(rb_ent->rb_buf+
        (rb_ent->rb_in%rb_ent->elem_cnt)*rb_ent->elem_size,
        buf_in, cp_step1*rb_ent->elem_size);
    memcpy(rb_ent->rb_buf,
        (unsigned char *)buf_in+cp_step1*rb_ent->elem_size,
        (cp_cnt-cp_step1)*rb_ent->elem_size);
    rb_ent->rb_in += cp_cnt;

    pthread_mutex_unlock(&rb_ent->rb_lock);

    return cp_cnt;
}

static int rb_out(struct ring_buffer *rb_hd,
        void *buf_out, unsigned int elem_num)
{
    struct ring_buffer_entity *rb_ent = rb_hd_to_ent(rb_hd);

    if (elem_num == 0 ||
        elem_num > rb_ent->elem_cnt) {
        nb_loge("wrong elem_num (%d)!!\r\n", (int)elem_num);
        return -1;
    }

    unsigned int cp_cnt   = min(elem_num, rb_ent->rb_in-rb_ent->rb_out);
    unsigned int cp_step1 = min(cp_cnt, rb_ent->elem_cnt - (rb_ent->rb_out%rb_ent->elem_cnt));

    /* If [buf_out] is NULL, we just drop [elem_num] number datas. */
    if (buf_out) {
        memcpy(buf_out, rb_ent->rb_buf + (rb_ent->rb_out%rb_ent->elem_cnt)*rb_ent->elem_size,
            cp_step1*rb_ent->elem_size);
        memcpy((unsigned char *)buf_out+cp_step1*rb_ent->elem_size, rb_ent->rb_buf,
            (cp_cnt-cp_step1)*rb_ent->elem_size);
    }

    rb_ent->rb_out += cp_cnt;

//    /* ring buffer empty, just reset in&out pointer */
//    if (rb_ent->rb_out == rb_ent->rb_in)
//        rb_ent->rb_out = rb_ent->rb_in = 0;

    return cp_cnt;
}

static int rb_out_fake(struct ring_buffer *rb_hd, void *buf_out)
{
    struct ring_buffer_entity *rb_ent = rb_hd_to_ent(rb_hd);
    int elem_num = 1;

    unsigned int cp_cnt   = min(elem_num, rb_ent->rb_in-rb_ent->rb_out);
    if (cp_cnt) {
        memcpy(buf_out, rb_ent->rb_buf+
            (rb_ent->rb_out%rb_ent->elem_cnt)*rb_ent->elem_size, rb_ent->elem_size);
    }

    return cp_cnt;
}

static int rb_out_lock(struct ring_buffer *rb_hd,
        void *buf_out, unsigned int elem_num)
{
    struct ring_buffer_entity *rb_ent = rb_hd_to_ent(rb_hd);
    int iRet = 0;

    pthread_mutex_lock(&rb_ent->rb_lock);
    iRet = rb_out(rb_hd, buf_out, elem_num);
    pthread_mutex_unlock(&rb_ent->rb_lock);

    return iRet;
}

static void rb_reset(struct ring_buffer *rb_hd)
{
    struct ring_buffer_entity *rb_ent = rb_hd_to_ent(rb_hd);

    rb_ent->rb_out = rb_ent->rb_in = 0;
}

static void rb_reset_lock(struct ring_buffer *rb_hd)
{
    struct ring_buffer_entity *rb_ent = rb_hd_to_ent(rb_hd);

    pthread_mutex_lock(&rb_ent->rb_lock);
    rb_reset(rb_hd);
    pthread_mutex_unlock(&rb_ent->rb_lock);
}

static unsigned int rb_g_bufsize(struct ring_buffer *rb_hd)
{
    struct ring_buffer_entity *rb_ent = rb_hd_to_ent(rb_hd);

    return (rb_ent->elem_size*rb_ent->elem_cnt);
}

static unsigned int rb_g_validnum(struct ring_buffer *rb_hd)
{
    struct ring_buffer_entity *rb_ent = rb_hd_to_ent(rb_hd);

    /* do not worry it will be overflow, it always be right even if
    * the overflow occurred. also, when rb_ent->rb_in == rb_ent->rb_out,
    * how do we know it is full? or empty? I can say it must be empty, because
    * the rb_ent->elem_cnt has the same type with rb_ent->rb_in&rb_ent->rb_out,
    * it means, if the elem_cnt has a range of 0~5, it means there has 6
    * elems in real, but, rb_in&rb_out&elem_cnt never get the index value 6,
    * think about the unsigned int type, they got the same principle. and this
    * guarantee that the elems real count number is always 1 more biger than
    * rb_in&rb_out&elem_cnt. now you know why you cannt use
    * (rb_in-rb_out)%elem_cnt to represent valid num.
    */
    return (rb_ent->rb_in-rb_ent->rb_out);
}

static unsigned int rb_g_emptynum(struct ring_buffer *rb_hd)
{
    struct ring_buffer_entity *rb_ent = rb_hd_to_ent(rb_hd);

    /* do not worry it will be overflow, it always be right even if
    * the overflow occurred. also, when rb_ent->rb_in == rb_ent->rb_out,
    * how do we know it is full? or empty? I can say it must be empty, because
    * the rb_ent->elem_cnt has the same type with rb_ent->rb_in&rb_ent->rb_out,
    * it means, if the elem_cnt has a range of 0~5, it means there has 6
    * elems in real, but, rb_in&rb_out&elem_cnt never get the index value 6,
    * think about the unsigned int type, they got the same principle. and this
    * guarantee that the elems real count number is always 1 more biger than
    * rb_in&rb_out&elem_cnt. now you know why you cannt use
    * (rb_in-rb_out)%elem_cnt to represent valid num.
    */
    return rb_ent->elem_cnt-(rb_ent->rb_in-rb_ent->rb_out);
}

static unsigned int rb_g_elemsize(struct ring_buffer *rb_hd)
{
    struct ring_buffer_entity *rb_ent = rb_hd_to_ent(rb_hd);

    return (rb_ent->elem_size);
}

static bool rb_is_full(struct ring_buffer *rb_hd)
{
    struct ring_buffer_entity *rb_ent = rb_hd_to_ent(rb_hd);

    return ((rb_ent->rb_in - rb_ent->rb_out) >= rb_ent->elem_cnt);
}

static bool rb_is_empty(struct ring_buffer *rb_hd)
{
    struct ring_buffer_entity *rb_ent = rb_hd_to_ent(rb_hd);

    return (rb_ent->rb_in == rb_ent->rb_out);
}

struct ring_buffer *ring_buffer_create
    (unsigned int elem_size, unsigned int elem_num, unsigned long flag)
{
    struct ring_buffer_entity *rb_ent;
    struct ring_buffer *rb_hd;

    if (elem_size == 0 || elem_num == 0) {
        nb_loge("wrong elem setting number:"
            "(elem_size=%d, elem_num=%d)\r\n", (int)elem_size, (int)elem_num);
        return NULL;
    }

    rb_ent = malloc(sizeof(struct ring_buffer_entity));
    if (NULL == rb_ent) {
        nb_loge("malloc struct ring_buffer_entity failed.\r\n");
        goto Ealloc_rb_ent;
    }
    memset(rb_ent, 0, sizeof(struct ring_buffer_entity));

    rb_ent->rb_buf = malloc(elem_size*elem_num);
    if (NULL == rb_ent->rb_buf) {
        nb_loge("malloc ring buffer failed.\r\n");
        goto Ealloc_rb;
    }
    memset(rb_ent->rb_buf, 0, elem_size*elem_num);

    pthread_mutex_init(&rb_ent->rb_lock, NULL);
    rb_ent->elem_cnt  = elem_num;
    rb_ent->elem_size = elem_size;
    rb_hd = &rb_ent->rb_handle;

    rb_hd->rb_in    = rb_in;
    rb_hd->rb_in_f  = rb_in_f;
    rb_hd->rb_out   = rb_out;
    rb_hd->rb_out_fake   = rb_out_fake;
    rb_hd->rb_reset      = rb_reset;
    rb_hd->rb_g_bufsize  = rb_g_bufsize;
    rb_hd->rb_g_validnum = rb_g_validnum;
    rb_hd->rb_g_emptynum = rb_g_emptynum;
    rb_hd->rb_g_elemsize = rb_g_elemsize;
    rb_hd->is_full       = rb_is_full;
    rb_hd->is_empty      = rb_is_empty;

    switch (flag) {
        case RB_FL_FORCE_WRITE:
        case RB_FL_THREAD_SAFE:
        case RB_FL_THREAD_SAFE | RB_FL_FORCE_WRITE:
            rb_hd->rb_in    = rb_in_lock;
            rb_hd->rb_out   = rb_out_lock;
            rb_hd->rb_reset = rb_reset_lock;
            break;

        case RB_FL_NONE:
            break;

        default: break;
    }

    rb_hd->private  = rb_ent;


    return rb_hd;

    free(rb_ent->rb_buf);
Ealloc_rb:
    free(rb_ent);
Ealloc_rb_ent:
    return NULL;
}

void ring_buffer_destroy(struct ring_buffer *rb_hd)
{
    struct ring_buffer_entity *rb_ent = rb_hd_to_ent(rb_hd);

    pthread_mutex_destroy(&rb_ent->rb_lock);
    free(rb_ent->rb_buf);
    rb_ent->rb_buf = NULL;
    free(rb_ent);
    rb_ent = NULL;
}


