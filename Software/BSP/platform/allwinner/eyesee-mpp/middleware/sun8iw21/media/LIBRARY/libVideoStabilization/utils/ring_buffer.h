/* ring buffer manager
 *
 *
 */

#ifndef _AW_RING_BUFFER_
#define _AW_RING_BUFFER_

#include <stdbool.h>

#define nb_loge printf
#define nb_logw printf
#define nb_logv printf

/* max ring buffer size 3GB */
#define MAX_RB_SIZE  (3*1024*1024*1024)

/* maybe a interlaced ring buffer should be used
*1.interlaced read&write
*2.pre-read
*/

/**
 * struct ring_buffer - ring buffer implement
 *!!!!!!!!!!!!!!!!!!the read write oprtation is not thread safe.!!!!!!!!!
 * @rb_in:  input data to ring buffer
 * @rb_in_f:    force input data to ring buffer,
 * even it will be overwritten(if it was used,RB_FL_FORCE_WRITE flag must be set)
 * @rb_out: output data from ring buffer(not use until now)
 * @rb_out_fake: output one data elem, but do not add the out pointer.
 * @rb_reset:   reset the whole ring buffer
 * @rb_g_bufsize:  get the ring buffer length(the whole buffer size in bytes)
 * @rb_g_validnum: get valid data size in bytes
 * @rb_g_elemsize: get elem size in bytes
 * @is_full: if this ring buffer is full
 * @is_empty: if this ring buffer is empty
 * @private:user should not access it
 */
struct ring_buffer {
    int (*rb_in)(struct ring_buffer *rb_hd,
            void *buf_in, unsigned int elem_num);
    int (*rb_in_f)(struct ring_buffer *rb_hd,
            void *buf_in, unsigned int elem_num);
    int (*rb_out)(struct ring_buffer *rb_hd,
            void *buf_out, unsigned int elem_num);
    int (*rb_out_fake)(struct ring_buffer *rb_hd, void *buf_out);
    void (*rb_reset)(struct ring_buffer *rb_hd);

    unsigned int (*rb_g_bufsize)(struct ring_buffer *rb_hd);
    unsigned int (*rb_g_validnum)(struct ring_buffer *rb_hd);
    unsigned int (*rb_g_emptynum)(struct ring_buffer *rb_hd);
    unsigned int (*rb_g_elemsize)(struct ring_buffer *rb_hd);
    bool (*is_full)(struct ring_buffer *rb_hd);
    bool (*is_empty)(struct ring_buffer *rb_hd);

    void *private;
};

/*
*/
#define RB_FL_NONE        0UL
#define RB_FL_FORCE_WRITE 1<<0UL
#define RB_FL_THREAD_SAFE 1<<1UL

/**
 * struct ring_buffer_attr - ring buffer's attribution
 * @force_w:    bool type, choice if use force write operation
 * @safe_t: bool type, is the ring buffer operation thread safe
 */
struct ring_buffer_attr {
    bool force_w;
    bool safe_t;
};

/**
 * struct ring_buffer_entity - ring buffer internal implement
 * @rb_lock: lock of read write operation.
 * @rb_in:  input index
 * @rb_out: output index
 * @elem_size: meta data size in bytes
 * @elem_cnt:     how many meta data in this ring buffer
 * @rb_full: is this ring buffer is full?
 * @rb_buf: the real buffer address
 * @rb_handle: used by users, include some callbacks
 */
struct ring_buffer_entity {
    pthread_mutex_t rb_lock;
    unsigned int rb_in;
    unsigned int rb_out;

    unsigned int elem_size;
    unsigned int elem_cnt;
    bool rb_full;

    unsigned char *rb_buf;

    struct ring_buffer_attr rb_attr;
    struct ring_buffer rb_handle;
};

#define ring_buffer_in(rb_hd, buf_in, elem_num) \
    rb_hd->rb_in(rb_hd, buf_in, elem_num)
#define ring_buffer_out(rb_hd, buf_out, elem_num) \
    rb_hd->rb_out(rb_hd, buf_out, elem_num)
#define ring_buffer_reset(rb_hd) \
    rb_hd->rb_reset(rb_hd)
#define ring_buffer_empty(rb_hd) \
    rb_hd->is_empty(rb_hd)
#define ring_buffer_full(rb_hd) \
    rb_hd->is_full(rb_hd)
#define ring_buffer_g_validnum(rb_hd) \
    rb_hd->rb_g_validnum(rb_hd)
#define ring_buffer_g_emptynum(rb_hd) \
    rb_hd->rb_g_emptynum(rb_hd)

/*
*@flag: look about RB_FL_XXX
*/
struct ring_buffer *ring_buffer_create(unsigned int elem_size, unsigned int elem_num, unsigned long flag);
void ring_buffer_destroy(struct ring_buffer *rb_hd);

#endif /* end of _AW_RING_BUFFER_ */

