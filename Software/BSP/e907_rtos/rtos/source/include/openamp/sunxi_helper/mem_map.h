#ifndef OPENAMP_SUN20IW3_MEM_MAP_H_
#define OPENAMP_SUN20IW3_MEM_MAP_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Memory attribute */
/* bit0: cacheable / non-cacheable */
#define MEM_CACHEABLE		(1U << 0)
#define MEM_NONCACHEABLE	(0U << 0)

/* Helper macros */
#define MEM_ATTR_IS_CACHEABLE(attr)	(!!((attr) & MEM_CACHEABLE))
#define MEM_ATTR_SET_CACHEABLE(attr)	((attr) |= MEM_CACHEABLE)
#define MEM_ATTR_SET_NONCACHEABLE(attr)	((attr) &= ~(MEM_CACHEABLE))

struct mem_mapping {
	uint64_t va;
	uint64_t pa;
	uint32_t len;
	uint32_t attr;
};

#define REGISTER_MEM_MAPPINGS(mappings_array) \
	const struct mem_mapping *_mem_mappings = mappings_array; \
	const size_t _mem_mappings_size = \
		sizeof(mappings_array) / sizeof(mappings_array[0]);


int mem_va_to_pa(uint32_t va, metal_phys_addr_t *pa, uint32_t *attr);
int mem_pa_to_va(uint32_t pa, metal_phys_addr_t *va, uint32_t *attr);

#ifdef __cplusplus
}
#endif

#endif /* OPENAMP_SUN20IW3_MEM_MAP_H_ */
