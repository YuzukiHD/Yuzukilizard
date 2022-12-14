#include <errno.h>
#include <metal/sys.h>
#include <openamp/sunxi_helper/mem_map.h>
#include <openamp/sunxi_helper/openamp_log.h>

#ifdef CONFIG_ARCH_SUN20IW3
static const struct mem_mapping mem_mappings[] = {
	/* 512M */
	{ .va = 0x40000000, .len = 0x20000000, .pa = 0x40000000, .attr = MEM_CACHEABLE },
};
static const int mem_mappings_size = (sizeof(mem_mappings)/sizeof(mem_mappings[0]));
#else
#error "openamp not support thit platform"
#endif

int mem_va_to_pa(uint32_t va, metal_phys_addr_t *pa, uint32_t *attr)
{
	const struct mem_mapping *map;
	int i;

	for (i = 0; i < mem_mappings_size; ++i) {
		map = &mem_mappings[i];
		if (va >= map->va && va < map->va + map->len) {
			*pa = va - map->va + map->pa;
			if (attr) {
				*attr = map->attr;
			}
			return 0;
		}
	}
	openamp_dbg("Invalid va %p\n", va);
	return -EINVAL;
}

int mem_pa_to_va(uint32_t pa, metal_phys_addr_t *va, uint32_t *attr)
{
	const struct mem_mapping *map;
	int i;

	/*
	 * TODO:
	 * Maybe there are multiple VAs corresponding to one PA.
	 * Only return the first matching one?
	 */
	for (i = 0; i < mem_mappings_size; ++i) {
		map = &mem_mappings[i];
		if (pa >= map->pa && pa < map->pa + map->len) {
			*va = (metal_phys_addr_t)((uintptr_t)(pa - map->pa + map->va));
			if (attr) {
				*attr = map->attr;
			}
			return 0;
		}
	}
	openamp_dbg("Invalid pa 0x%lx\n", pa);
	return -EINVAL;
}

