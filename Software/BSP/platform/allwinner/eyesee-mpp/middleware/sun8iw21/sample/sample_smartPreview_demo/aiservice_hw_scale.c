#include <assert.h>
#include <plat_math.h>
#include <plat_log.h>
#include <utils/PIXEL_FORMAT_E_g2d_format_convert.h>
#include <linux/g2d_driver.h>
#include <ion_memmanager.h>
#include <memoryAdapter.h>
#include "aiservice_hw_scale.h"

typedef struct
{
    unsigned long *p_virtaddr_y;
    unsigned long *p_virtaddr_uv;
    unsigned long *p_physaddr_y;
    unsigned long *p_physaddr_uv;
}ion_img_buff_t;

int g2d_mem_open(void)
{
    return ion_memOpen();
}

int g2d_mem_close(void)
{
    return ion_memClose();
}

static unsigned char* g2d_alloc_mem(unsigned int size)
{
    IonAllocAttr alloc_attr;
    memset(&alloc_attr, 0, sizeof(IonAllocAttr));

    alloc_attr.mLen           = size;
    alloc_attr.mAlign         = 0;
    alloc_attr.mIonHeapType   = IonHeapType_IOMMU;
    alloc_attr.mbSupportCache = 0;
    return ion_allocMem_extend(&alloc_attr);
}

static int g2d_free_mem(void *vir_ptr)
{
    return ion_freeMem(vir_ptr);
}

static unsigned int g2d_getphyaddr_by_viraddr(void *vir_ptr)
{
    return ion_getMemPhyAddr(vir_ptr);
}

static int g2d_flush_cache(void *vir_ptr, unsigned int size)
{
    return ion_flushCache(vir_ptr, size);
}

static ion_img_buff_t src;
static ion_img_buff_t dst;
int scale_picture_nv12_to_nv12_byg2d(const unsigned char *srcpic, int src_width, int src_height, unsigned char *dstpic, int dst_width, int dst_height, int g2d_fd)
{
    int ret = 0;
    static int mem_inited = 0;
    int size;
    g2d_blt_h blit;
    g2d_fmt_enh src_fmt, dst_fmt;

    if(mem_inited == 0)
    {
        size = ALIGN(src_width, 16) * ALIGN(src_height, 16);
        src.p_virtaddr_y  = (void*)g2d_alloc_mem(size);
        src.p_virtaddr_uv = (void*)g2d_alloc_mem(size/2);

        if(!src.p_virtaddr_y || !src.p_virtaddr_uv )
        {
            aloge("fatal error, ion malloc for src image failure.");
            goto EXIT;
        }

        src.p_physaddr_y  = (void *)g2d_getphyaddr_by_viraddr(src.p_virtaddr_y);
        src.p_physaddr_uv = (void *)g2d_getphyaddr_by_viraddr(src.p_virtaddr_uv);

        size = ALIGN(dst_width, 16) * ALIGN(dst_height, 16);
        dst.p_virtaddr_y  = (void*)g2d_alloc_mem(size);
        dst.p_virtaddr_uv = (void*)g2d_alloc_mem(size/2);

        if(!dst.p_virtaddr_y || !dst.p_virtaddr_uv )
        {
            aloge("fatal error, ion malloc for dst image failure.");
            goto EXIT;
        }

        dst.p_physaddr_y  = (void *)g2d_getphyaddr_by_viraddr(dst.p_virtaddr_y);
        dst.p_physaddr_uv = (void *)g2d_getphyaddr_by_viraddr(dst.p_virtaddr_uv);
        mem_inited = 1;
    }

    if(!src.p_virtaddr_y || !src.p_virtaddr_uv || !dst.p_virtaddr_y || !dst.p_virtaddr_uv)
    {
        aloge("src or dst buffer addr corrupted?");
        goto EXIT;
    }

    size = src_width*src_height;
    memcpy(src.p_virtaddr_y, srcpic, size);
    memcpy(src.p_virtaddr_uv, srcpic + size , size / 2);
    g2d_flush_cache(src.p_virtaddr_y, size);
    g2d_flush_cache(src.p_virtaddr_uv, size / 2);

    // MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420. nv12 format.
    ret = convert_PIXEL_FORMAT_E_to_g2d_fmt_enh(MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420 , &src_fmt);
    if(ret != SUCCESS)
    {
        aloge("fatal error, convert pixel fmt from video to g2d failure.");
        goto EXIT;
    }

    ret = convert_PIXEL_FORMAT_E_to_g2d_fmt_enh(MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420, &dst_fmt);
    if(ret != SUCCESS)
    {
        aloge("fatal error, convert pixel fmt from video to g2d failure.");
        goto EXIT;
    }

    memset(&blit, 0, sizeof(g2d_blt_h));
    blit.flag_h = G2D_BLT_NONE_H;

    blit.src_image_h.format       = src_fmt;
    blit.src_image_h.laddr[0]     = (unsigned long)src.p_physaddr_y;
    blit.src_image_h.laddr[1]     = (unsigned long)src.p_physaddr_uv;
    blit.src_image_h.laddr[2]     = (unsigned long)0;

    blit.src_image_h.width        = src_width;
    blit.src_image_h.height       = src_height;
    blit.src_image_h.align[0]     = 0;
    blit.src_image_h.align[1]     = 0;
    blit.src_image_h.align[2]     = 0;
    blit.src_image_h.clip_rect.x  = 0;
    blit.src_image_h.clip_rect.y  = 0;
    blit.src_image_h.clip_rect.w  = src_width;
    blit.src_image_h.clip_rect.h  = src_height;
    blit.src_image_h.gamut        = G2D_BT601;
    blit.src_image_h.bpremul      = 0;

    blit.src_image_h.mode         = G2D_PIXEL_ALPHA;
    blit.src_image_h.fd           = -1;
    blit.src_image_h.use_phy_addr = 1;

    blit.dst_image_h.format       = dst_fmt;
    blit.dst_image_h.laddr[0]     = (unsigned long)dst.p_physaddr_y;
    blit.dst_image_h.laddr[1]     = (unsigned long)dst.p_physaddr_uv;
    blit.dst_image_h.laddr[2]     = (unsigned long)0;

    blit.dst_image_h.width        = dst_width;
    blit.dst_image_h.height       = dst_height;
    blit.dst_image_h.align[0]     = 0;
    blit.dst_image_h.align[1]     = 0;
    blit.dst_image_h.align[2]     = 0;
    blit.dst_image_h.clip_rect.x  = 0;
    blit.dst_image_h.clip_rect.y  = 0;
    blit.dst_image_h.clip_rect.w  = dst_width;
    blit.dst_image_h.clip_rect.h  = dst_height;
    blit.dst_image_h.gamut        = G2D_BT601;
    blit.dst_image_h.bpremul      = 0;

    blit.dst_image_h.mode         = G2D_PIXEL_ALPHA;
    blit.dst_image_h.fd           = -1;
    blit.dst_image_h.use_phy_addr = 1;

    ret = ioctl(g2d_fd, G2D_CMD_BITBLT_H, (unsigned long)&blit);
    if(ret < 0)
    {
        aloge("fatal error! bit-block(image) transfer failed[%d]", ret);
        goto EXIT;
    }

    size = dst_width*dst_height;

    g2d_flush_cache(dst.p_virtaddr_y, size);
    g2d_flush_cache(dst.p_virtaddr_uv, size / 2);

    memcpy(dstpic, dst.p_virtaddr_y, size);
    memcpy(dstpic + size, dst.p_virtaddr_uv, size / 2);

    return 0;

EXIT:
    if(src.p_virtaddr_y)
    {
        g2d_free_mem(src.p_virtaddr_y);
    }
    if(src.p_virtaddr_uv)
    {
        g2d_free_mem(src.p_virtaddr_uv);
    }
    if(dst.p_virtaddr_y)
    {
        g2d_free_mem(dst.p_virtaddr_y);
    }
    if(dst.p_virtaddr_uv)
    {
        g2d_free_mem(dst.p_virtaddr_uv);
    }

    memset(&src, 0x00, sizeof(src));
    memset(&dst, 0x00, sizeof(dst));

    return -1;
}

// dummy function,nothing need todo.
void scale_create_buffer(void)
{
    return;
}

void scale_destory_buffer(void)
{
    if(src.p_virtaddr_y)
    {
        g2d_free_mem(src.p_virtaddr_y);
    }
    if(src.p_virtaddr_uv)
    {
        g2d_free_mem(src.p_virtaddr_uv);
    }
    if(dst.p_virtaddr_y)
    {
        g2d_free_mem(dst.p_virtaddr_y);
    }
    if(dst.p_virtaddr_uv)
    {
        g2d_free_mem(dst.p_virtaddr_uv);
    }

    memset(&src, 0x00, sizeof(src));
    memset(&dst, 0x00, sizeof(dst));
    return;
}
