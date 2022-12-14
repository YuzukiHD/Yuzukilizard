#include <metal/sys.h>
#include <metal/cache.h>
#include <metal/atomic.h>
#include <hal_osal.h>

#include <openamp/sunxi_helper/openamp_log.h>
#include <openamp/sunxi_helper/shmem_ops.h>

uint64_t shmem_metal_io_read(struct metal_io_region *io, unsigned long offset, memory_order order, int width)
{
	void *ptr = metal_io_virt(io, offset);

	if (!ptr)
		return 0;

//	if (MEM_ATTR_IS_CACHEABLE(io->mem_flags))
		metal_cache_invalidate(ptr, width);

	if (sizeof(atomic_uchar) == width)
		return atomic_load_explicit((atomic_uchar *)ptr, order);
	else if (sizeof(atomic_ushort) == width)
		return atomic_load_explicit((atomic_ushort *)ptr, order);
	else if (sizeof(atomic_uint) == width)
		return atomic_load_explicit((atomic_uint *)ptr, order);
	else if (sizeof(atomic_ulong) == width)
		return atomic_load_explicit((atomic_ulong *)ptr, order);
#ifndef NO_ATOMIC_64_SUPPORT
	else if (sizeof(atomic_ullong) == width)
		return atomic_load_explicit((atomic_ullong *)ptr, order);
#endif
	metal_assert(0);
	return 0; /* quiet compiler */

}

void shmem_metal_io_write(struct metal_io_region *io, unsigned long offset, uint64_t value, memory_order order, int width)
{
	void *ptr = metal_io_virt(io, offset);

	if (!ptr)
		return;
	if (sizeof(atomic_uchar) == width)
		atomic_store_explicit((atomic_uchar *)ptr, (unsigned char)value,
				      order);
	else if (sizeof(atomic_ushort) == width)
		atomic_store_explicit((atomic_ushort *)ptr,
				      (unsigned short)value, order);
	else if (sizeof(atomic_uint) == width)
		atomic_store_explicit((atomic_uint *)ptr, (unsigned int)value,
				      order);
	else if (sizeof(atomic_ulong) == width)
		atomic_store_explicit((atomic_ulong *)ptr, (unsigned long)value,
				      order);
#ifndef NO_ATOMIC_64_SUPPORT
	else if (sizeof(atomic_ullong) == width)
		atomic_store_explicit((atomic_ullong *)ptr,
				      (unsigned long long)value, order);
#endif
	else
		metal_assert(0);

//	if (MEM_ATTR_IS_CACHEABLE(io->mem_flags))
		metal_cache_flush(ptr, width);
}


int shmem_metal_io_block_read(struct metal_io_region *io,
				unsigned long offset, void *restrict dst, memory_order order,int len)
{
	unsigned char *ptr = metal_io_virt(io, offset);
	unsigned char *dest = dst;
	int retlen = len;

//	if (MEM_ATTR_IS_CACHEABLE(io->mem_flags))
		metal_cache_invalidate(ptr, len);

	atomic_thread_fence(memory_order_seq_cst);
	while ( len && (
		((uintptr_t)dest % sizeof(int)) ||
		((uintptr_t)ptr % sizeof(int)))) {
		*(unsigned char *)dest =
			*(const unsigned char *)ptr;
		dest++;
		ptr++;
		len--;
	}
	for (; len >= (int)sizeof(int); dest += sizeof(int),
				ptr += sizeof(int),
				len -= sizeof(int))
		*(unsigned int *)dest = *(const unsigned int *)ptr;
	for (; len != 0; dest++, ptr++, len--)
		*(unsigned char *)dest =
			*(const unsigned char *)ptr;

	return retlen;

}


int shmem_metal_io_block_write(struct metal_io_region *io, unsigned long offset,
				const void *restrict src, memory_order order,int len)
{
	unsigned char *ptr = metal_io_virt(io, offset);
	const unsigned char *source = src;
	int retlen = len;

	while ( len && (
		((uintptr_t)ptr % sizeof(int)) ||
		((uintptr_t)source % sizeof(int)))) {
		*(unsigned char *)ptr =
			*(const unsigned char *)source;
		ptr++;
		source++;
		len--;
	}
	for (; len >= (int)sizeof(int); ptr += sizeof(int),
				source += sizeof(int),
				len -= sizeof(int))
		*(unsigned int *)ptr = *(const unsigned int *)source;
	for (; len != 0; ptr++, source++, len--)
		*(unsigned char *)ptr =
			*(const unsigned char *)source;
	atomic_thread_fence(memory_order_seq_cst);

//	if (MEM_ATTR_IS_CACHEABLE(io->mem_flags))
		metal_cache_flush(ptr, len);

	return retlen;

}

void shmem_metal_io_block_set(struct metal_io_region *io, unsigned long offset, unsigned char value, memory_order order,int len)
{
	unsigned char *ptr = metal_io_virt(io, offset);

	unsigned int cint = value;
	unsigned int i;

	for (i = 1; i < sizeof(int); i++)
		cint |= ((unsigned int)value << (CHAR_BIT * i));

	for (; len && ((uintptr_t)ptr % sizeof(int)); ptr++, len--)
		*(unsigned char *)ptr = (unsigned char) value;
	for (; len >= (int)sizeof(int); ptr += sizeof(int),
					len -= sizeof(int))
		*(unsigned int *)ptr = cint;
	for (; len != 0; ptr++, len--)
		*(unsigned char *)ptr = (unsigned char) value;
	atomic_thread_fence(memory_order_seq_cst);

//	if (MEM_ATTR_IS_CACHEABLE(io->mem_flags))
		metal_cache_flush(ptr, len);
}


struct metal_io_ops shmem_sunxi_io_ops = {
	.read = shmem_metal_io_read,
	.write = shmem_metal_io_write,
	.block_read = shmem_metal_io_block_read,
	.block_write = shmem_metal_io_block_write,
	.block_set = shmem_metal_io_block_set,
	.close = NULL,
	.offset_to_phys = NULL,
	.phys_to_offset = NULL,
};

