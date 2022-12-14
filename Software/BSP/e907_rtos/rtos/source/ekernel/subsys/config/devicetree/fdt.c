/*
 * Functions for working with the Flattened Device Tree data format
 *
 * Copyright 2009 Benjamin Herrenschmidt, IBM Corp
 * benh@kernel.crashing.org
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#include <common/list.h>
#include <rtthread.h>
#include <typedef.h>
#include <stdbool.h>
#include <string.h>
#include <init.h>
#include <log.h>
#include "libfdt/libfdt.h"
#include <of/of.h>
#include <kconfig.h>
#include "of_private.h"

#define PTR_ALIGN(p, a)         ((typeof(p))RT_ALIGN((unsigned long)(p), (a)))
#define CONFIG_OF_EARLY_FLATTREE

const void *of_get_flat_dt_prop(unsigned long node, const char *name, int *size);
u64 dt_mem_next_cell(int s, const unsigned int **cellp);
/**
 * of_fdt_is_compatible - Return true if given node from the given blob has
 * compat in its compatible list
 * @blob: A device tree blob
 * @node: node to test
 * @compat: compatible string to compare with compatible list.
 *
 * On match, returns a non-zero value with smaller values returned for more
 * specific compatible values.
 */
static int of_fdt_is_compatible(const void *blob,
		      unsigned long node, const char *compat)
{
	const char *cp;
	int cplen;
	unsigned long l, score = 0;

	cp = fdt_getprop(blob, node, "compatible", &cplen);
	if (cp == NULL)
		return 0;
	while (cplen > 0) {
		score++;
		if (of_compat_cmp(cp, compat, strlen(compat)) == 0)
			return score;
		l = strlen(cp) + 1;
		cp += l;
		cplen -= l;
	}

	return 0;
}

/**
 * of_fdt_is_big_endian - Return true if given node needs BE MMIO accesses
 * @blob: A device tree blob
 * @node: node to test
 *
 * Returns true if the node has a "big-endian" property, or if the kernel
 * was compiled for BE *and* the node has a "native-endian" property.
 * Returns false otherwise.
 */
bool of_fdt_is_big_endian(const void *blob, unsigned long node)
{
	if (fdt_getprop(blob, node, "big-endian", NULL))
		return true;
	return false;
}

static bool of_fdt_device_is_available(const void *blob, unsigned long node)
{
	const char *status = fdt_getprop(blob, node, "status", NULL);

	if (!status)
		return true;

	if (!strcmp(status, "ok") || !strcmp(status, "okay"))
		return true;

	return false;
}

/**
 * of_fdt_match - Return true if node matches a list of compatible values
 */
int of_fdt_match(const void *blob, unsigned long node,
                 const char *const *compat)
{
	unsigned int tmp, score = 0;

	if (!compat)
		return 0;

	while (*compat) {
		tmp = of_fdt_is_compatible(blob, node, *compat);
		if (tmp && (score == 0 || (tmp < score)))
			score = tmp;
		compat++;
	}

	return score;
}

static void *unflatten_dt_alloc(void **mem, unsigned long size,
				       unsigned long align)
{
	void *res;

	*mem = PTR_ALIGN(*mem, align);
	res = *mem;
	*mem += size;

	return res;
}

static void populate_properties(const void *blob,
				int offset,
				void **mem,
				struct device_node *np,
				const char *nodename,
				bool dryrun)
{
	struct property *pp, **pprev = NULL;
	int cur;
	bool has_name = false;

	pprev = &np->properties;
	for (cur = fdt_first_property_offset(blob, offset);
	     cur >= 0;
	     cur = fdt_next_property_offset(blob, cur)) {
		const unsigned int *val;
		const char *pname;
		int sz;

		val = fdt_getprop_by_offset(blob, cur, &pname, &sz);
		if (!val) {
			__wrn("Cannot locate property at 0x%x", cur);
			continue;
		}

		if (!pname) {
			__wrn("Cannot find property name at 0x%x", cur);
			continue;
		}

		if (!strcmp(pname, "name"))
			has_name = true;

		pp = unflatten_dt_alloc(mem, sizeof(struct property),
					__alignof__(struct property));
		if (dryrun)
			continue;

		/* We accept flattened tree phandles either in
		 * ePAPR-style "phandle" properties, or the
		 * legacy "linux,phandle" properties.  If both
		 * appear and have different values, things
		 * will get weird. Don't do that.
		 */
		if (!strcmp(pname, "phandle") ||
		    !strcmp(pname, "linux,phandle")) {
			if (!np->phandle)
				np->phandle = be32_to_cpup(val);
		}

		/* And we process the "ibm,phandle" property
		 * used in pSeries dynamic device tree
		 * stuff
		 */
		if (!strcmp(pname, "ibm,phandle"))
			np->phandle = be32_to_cpup(val);

		pp->name   = (char *)pname;
		pp->length = sz;
		pp->value  = (unsigned int *)val;
		*pprev     = pp;
		pprev      = &pp->next;
	}

	/* With version 0x10 we may not have the name property,
	 * recreate it here from the unit name if absent
	 */
	if (!has_name) {
		const char *p = nodename, *ps = p, *pa = NULL;
		int len;

		while (*p) {
			if ((*p) == '@')
				pa = p;
			else if ((*p) == '/')
				ps = p + 1;
			p++;
		}

		if (pa < ps)
			pa = p;
		len = (pa - ps) + 1;
		pp = unflatten_dt_alloc(mem, sizeof(struct property) + len,
					__alignof__(struct property));
		if (!dryrun) {
			pp->name   = "name";
			pp->length = len;
			pp->value  = pp + 1;
			*pprev     = pp;
			pprev      = &pp->next;
			memcpy(pp->value, ps, len - 1);
			((char *)pp->value)[len - 1] = 0;
			__log("fixed up name for %s -> %s",
				 nodename, (char *)pp->value);
		}
	}

	if (!dryrun)
		*pprev = NULL;
}

static bool populate_node(const void *blob,
			  int offset,
			  void **mem,
			  struct device_node *dad,
			  struct device_node **pnp,
			  bool dryrun)
{
	struct device_node *np;
	const char *pathp;
	unsigned int l, allocl;

	pathp = fdt_get_name(blob, offset, &l);
	if (!pathp) {
		*pnp = NULL;
		return false;
	}

	allocl = ++l;

	np = unflatten_dt_alloc(mem, sizeof(struct device_node) + allocl,
				__alignof__(struct device_node));
	if (!dryrun) {
		char *fn;
		of_node_init(np);
		np->full_name = fn = ((char *)np) + sizeof(*np);

		memcpy(fn, pathp, l);

		if (dad != NULL) {
			np->parent = dad;
			np->sibling = dad->child;
			dad->child = np;
		}
	}

	populate_properties(blob, offset, mem, np, pathp, dryrun);
	if (!dryrun) {
		np->name = of_get_property(np, "name", NULL);
		np->type = of_get_property(np, "device_type", NULL);

		if (!np->name)
			np->name = "<NULL>";
		if (!np->type)
			np->type = "<NULL>";
	}

	*pnp = np;
	return true;
}

static void reverse_nodes(struct device_node *parent)
{
	struct device_node *child, *next;

	/* In-depth first */
	child = parent->child;
	while (child) {
		reverse_nodes(child);

		child = child->sibling;
	}

	/* Reverse the nodes in the child list */
	child = parent->child;
	parent->child = NULL;
	while (child) {
		next = child->sibling;

		child->sibling = parent->child;
		parent->child = child;
		child = next;
	}
}

/**
 * unflatten_dt_nodes - Alloc and populate a device_node from the flat tree
 * @blob: The parent device tree blob
 * @mem: Memory chunk to use for allocating device nodes and properties
 * @dad: Parent struct device_node
 * @nodepp: The device_node tree created by the call
 *
 * It returns the size of unflattened device tree or error code
 */
static int unflatten_dt_nodes(const void *blob,
			      void *mem,
			      struct device_node *dad,
			      struct device_node **nodepp)
{
	struct device_node *root;
	int offset = 0, depth = 0, initial_depth = 0;
#define FDT_MAX_DEPTH	64
	struct device_node *nps[FDT_MAX_DEPTH];
	void *base = mem;
	bool dryrun = !base;

	if (nodepp)
		*nodepp = NULL;

	/*
	 * We're unflattening device sub-tree if @dad is valid. There are
	 * possibly multiple nodes in the first level of depth. We need
	 * set @depth to 1 to make fdt_next_node() happy as it bails
	 * immediately when negative @depth is found. Otherwise, the device
	 * nodes except the first one won't be unflattened successfully.
	 */
	if (dad)
		depth = initial_depth = 1;

	root = dad;
	nps[depth] = dad;

	for (offset = 0;
	     offset >= 0 && depth >= initial_depth;
	     offset = fdt_next_node(blob, offset, &depth)) {
		if ((depth >= FDT_MAX_DEPTH))
			continue;

		if (!of_fdt_device_is_available(blob, offset))
			continue;

		if (!populate_node(blob, offset, &mem, nps[depth],
				   &nps[depth+1], dryrun))
			return mem - base;

		if (!dryrun && nodepp && !*nodepp)
			*nodepp = nps[depth+1];
		if (!dryrun && !root)
			root = nps[depth+1];
	}

	if (offset < 0 && offset != -FDT_ERR_NOTFOUND) {
		__err("Error %d processing FDT", offset);
		return -1;
	}

	/*
	 * Reverse the child list. Some drivers assumes node order matches .dts
	 * node order
	 */
	if (!dryrun)
		reverse_nodes(root);

	return mem - base;
}

/**
 * __unflatten_device_tree - create tree of device_nodes from flat blob
 *
 * unflattens a device-tree, creating the
 * tree of struct device_node. It also fills the "name" and "type"
 * pointers of the nodes so the normal device-tree walking functions
 * can be used.
 * @blob: The blob to expand
 * @dad: Parent device node
 * @mynodes: The device_node tree created by the call
 * @dt_alloc: An allocator that provides a virtual address to memory
 * for the resulting tree
 * @detached: if true set OF_DETACHED on @mynodes
 *
 * Returns NULL on failure or the memory chunk containing the unflattened
 * device tree on success.
 */
static void *of_dt_alloc(u64 size, u64 align)
{
    void *p;

    p = rt_calloc(size, 1);

    return p;
}

void *__unflatten_device_tree(const void *blob,
			      struct device_node *dad,
			      struct device_node **mynodes,
			      void *(*dt_alloc)(u64 size, u64 align),
			      bool detached)
{
	int size;
	void *mem;

	__log(" -> unflatten_device_tree()");

	if (!blob) {
		__log("No device tree pointer");
		return NULL;
	}

	__log("Unflattening device tree:");
	__log("magic: %08x", fdt_magic(blob));
	__log("size: %08x", fdt_totalsize(blob));
	__log("version: %08x", fdt_version(blob));

	if (fdt_check_header(blob)) {
		__err("Invalid device tree blob header");
		return NULL;
	}

	/* First pass, scan for size */
	size = unflatten_dt_nodes(blob, NULL, dad, NULL);
	if (size < 0)
		return NULL;

	size = RT_ALIGN(size, 4);
	__log("  size is %d, allocating...", size);

	/* Allocate memory for the expanded device tree */
	mem = dt_alloc(size + 4, __alignof__(struct device_node));
	if (!mem)
		return NULL;

	memset(mem, 0, size);

	*(unsigned int *)(mem + size) = 0xdeadbeef;

	__log("  unflattening %p...", mem);

	/* Second pass, do actual unflattening */
	unflatten_dt_nodes(blob, mem, dad, mynodes);
	if (be32_to_cpup(mem + size) != 0xdeadbeef)
		__wrn("End of tree marker overwritten: %08x",
			   be32_to_cpup(mem + size));

	if (detached && mynodes) {
		of_node_set_flag(*mynodes, OF_DETACHED);
		__log("unflattened tree is detached");
	}

	__log(" <- unflatten_device_tree()");
	return mem;
}

static void *kernel_tree_alloc(u64 size, u64 align)
{
	return rt_calloc(size, 1);
}

/**
 * of_fdt_unflatten_tree - create tree of device_nodes from flat blob
 * @blob: Flat device tree blob
 * @dad: Parent device node
 * @mynodes: The device tree created by the call
 *
 * unflattens the device-tree passed by the firmware, creating the
 * tree of struct device_node. It also fills the "name" and "type"
 * pointers of the nodes so the normal device-tree walking functions
 * can be used.
 *
 * Returns NULL on failure or the memory chunk containing the unflattened
 * device tree on success.
 */
void *of_fdt_unflatten_tree(const unsigned long *blob,
			    struct device_node *dad,
			    struct device_node **mynodes)
{
	void *mem;

	mem = __unflatten_device_tree(blob, dad, mynodes, &kernel_tree_alloc,
				      true);
	return mem;
}

/* Everything below here references initial_boot_params directly. */
int dt_root_addr_cells;
int dt_root_size_cells;

void *initial_boot_params;

#ifdef CONFIG_OF_EARLY_FLATTREE

static u32 of_fdt_crc32;

/**
 * res_mem_reserve_reg() - reserve all memory described in 'reg' property
 */
static int __reserved_mem_reserve_reg(unsigned long node,
					     const char *uname)
{
	int t_len = (dt_root_addr_cells + dt_root_size_cells) * sizeof(unsigned int);
	uint32_t base, size;
	int len;
	const unsigned int *prop;
	int nomap, first = 1;

	prop = of_get_flat_dt_prop(node, "reg", &len);
	if (!prop)
		return -1;

	if (len && len % t_len != 0) {
		__err("Reserved memory: invalid reg property in '%s', skipping node.",
		       uname);
		return -1;
	}

	nomap = of_get_flat_dt_prop(node, "no-map", NULL) != NULL;

	while (len >= t_len) {
		base = dt_mem_next_cell(dt_root_addr_cells, &prop);
		size = dt_mem_next_cell(dt_root_size_cells, &prop);

#if 0
		if (size &&
		    early_init_dt_reserve_memory_arch(base, size, nomap) == 0)
			__log("Reserved memory: reserved region for node '%s': base %pa, size %ld MiB",
				uname, &base, (unsigned long)size / SZ_1M);
		else
			__inf("Reserved memory: failed to reserve memory for node '%s': base %pa, size %ld MiB",
				uname, &base, (unsigned long)size / SZ_1M);

		len -= t_len;
		if (first) {
			fdt_reserved_mem_save_node(node, uname, base, size);
			first = 0;
		}
#endif
	}
	return 0;
}

/**
 * __reserved_mem_check_root() - check if #size-cells, #address-cells provided
 * in /reserved-memory matches the values supported by the current implementation,
 * also check if ranges property has been provided
 */
static int  __reserved_mem_check_root(unsigned long node)
{
	const unsigned int *prop;

	prop = of_get_flat_dt_prop(node, "#size-cells", NULL);
	if (!prop || be32_to_cpup(prop) != dt_root_size_cells)
		return -1;

	prop = of_get_flat_dt_prop(node, "#address-cells", NULL);
	if (!prop || be32_to_cpup(prop) != dt_root_addr_cells)
		return -1;

	prop = of_get_flat_dt_prop(node, "ranges", NULL);
	if (!prop)
		return -1;
	return 0;
}

/**
 * fdt_scan_reserved_mem() - scan a single FDT node for reserved memory
 */
#if 0
static int __fdt_scan_reserved_mem(unsigned long node, const char *uname,
					  int depth, void *data)
{
	static int found;
	int err;

	if (!found && depth == 1 && strcmp(uname, "reserved-memory") == 0) {
		if (__reserved_mem_check_root(node) != 0) {
			__err("Reserved memory: unsupported node format, ignoring");
			/* break scan */
			return 1;
		}
		found = 1;
		/* scan next node */
		return 0;
	} else if (!found) {
		/* scan next node */
		return 0;
	} else if (found && depth < 2) {
		/* scanning of /reserved-memory has been finished */
		return 1;
	}

	if (!of_fdt_device_is_available(initial_boot_params, node))
		return 0;

	err = __reserved_mem_reserve_reg(node, uname);
	if (err == -1 && of_get_flat_dt_prop(node, "size", NULL))
		fdt_reserved_mem_save_node(node, uname, 0, 0);

	/* scan next node */
	return 0;
}

/**
 * early_init_fdt_scan_reserved_mem() - create reserved memory regions
 *
 * This function grabs memory from early allocator for device exclusive use
 * defined in device tree structures. It should be called by arch specific code
 * once the early allocator (i.e. memblock) has been fully activated.
 */
void early_init_fdt_scan_reserved_mem(void)
{
	int n;
	u64 base, size;

	if (!initial_boot_params)
		return;

	/* Process header /memreserve/ fields */
	for (n = 0; ; n++) {
		fdt_get_mem_rsv(initial_boot_params, n, &base, &size);
		if (!size)
			break;
		early_init_dt_reserve_memory_arch(base, size, 0);
	}

	of_scan_flat_dt(__fdt_scan_reserved_mem, NULL);
	fdt_init_reserved_mem();
}

/**
 * early_init_fdt_reserve_self() - reserve the memory used by the FDT blob
 */
void early_init_fdt_reserve_self(void)
{
	if (!initial_boot_params)
		return;

	/* Reserve the dtb region */
	early_init_dt_reserve_memory_arch(__pa(initial_boot_params),
					  fdt_totalsize(initial_boot_params),
					  0);
}

#endif
/**
 * of_scan_flat_dt - scan flattened tree blob and call callback on each.
 * @it: callback function
 * @data: context data pointer
 *
 * This function is used to scan the flattened device-tree, it is
 * used to extract the memory information at boot before we can
 * unflatten the tree
 */
int of_scan_flat_dt(int (*it)(unsigned long node,
				     const char *uname, int depth,
				     void *data),
			   void *data)
{
	const void *blob = initial_boot_params;
	const char *pathp;
	int offset, rc = 0, depth = -1;

	if (!blob)
		return 0;

	for (offset = fdt_next_node(blob, -1, &depth);
	     offset >= 0 && depth >= 0 && !rc;
	     offset = fdt_next_node(blob, offset, &depth)) {

		pathp = fdt_get_name(blob, offset, NULL);
		if (*pathp == '/')
			pathp = kbasename(pathp);
		rc = it(offset, pathp, depth, data);
	}
	return rc;
}
/**
 * of_scan_flat_dt_subnodes - scan sub-nodes of a node call callback on each.
 * @it: callback function
 * @data: context data pointer
 *
 * This function is used to scan sub-nodes of a node.
 */
int of_scan_flat_dt_subnodes(unsigned long parent,
				    int (*it)(unsigned long node,
					      const char *uname,
					      void *data),
				    void *data)
{
	const void *blob = initial_boot_params;
	int node;

	fdt_for_each_subnode(node, blob, parent) {
		const char *pathp;
		int rc;

		pathp = fdt_get_name(blob, node, NULL);
		if (*pathp == '/')
			pathp = kbasename(pathp);
		rc = it(node, pathp, data);
		if (rc)
			return rc;
	}
	return 0;
}

/**
 * of_get_flat_dt_subnode_by_name - get the subnode by given name
 *
 * @node: the parent node
 * @uname: the name of subnode
 * @return offset of the subnode, or -FDT_ERR_NOTFOUND if there is none
 */

int of_get_flat_dt_subnode_by_name(unsigned long node, const char *uname)
{
	return fdt_subnode_offset(initial_boot_params, node, uname);
}

/**
 * of_get_flat_dt_root - find the root node in the flat blob
 */
unsigned long of_get_flat_dt_root(void)
{
	return 0;
}

/**
 * of_get_flat_dt_size - Return the total size of the FDT
 */
int of_get_flat_dt_size(void)
{
	return fdt_totalsize(initial_boot_params);
}

/**
 * of_get_flat_dt_prop - Given a node in the flat blob, return the property ptr
 *
 * This function can be used within scan_flattened_dt callback to get
 * access to properties
 */
const void *of_get_flat_dt_prop(unsigned long node, const char *name,
				       int *size)
{
	return fdt_getprop(initial_boot_params, node, name, size);
}

/**
 * of_flat_dt_is_compatible - Return true if given node has compat in compatible list
 * @node: node to test
 * @compat: compatible string to compare with compatible list.
 */
int of_flat_dt_is_compatible(unsigned long node, const char *compat)
{
	return of_fdt_is_compatible(initial_boot_params, node, compat);
}

/**
 * of_flat_dt_match - Return true if node matches a list of compatible values
 */
int of_flat_dt_match(unsigned long node, const char *const *compat)
{
	return of_fdt_match(initial_boot_params, node, compat);
}

/**
 * of_get_flat_dt_prop - Given a node in the flat blob, return the phandle
 */
uint32_t of_get_flat_dt_phandle(unsigned long node)
{
	return fdt_get_phandle(initial_boot_params, node);
}

struct fdt_scan_status {
	const char *name;
	int namelen;
	int depth;
	int found;
	int (*iterator)(unsigned long node, const char *uname, int depth, void *data);
	void *data;
};

const char * of_flat_dt_get_machine_name(void)
{
	const char *name;
	unsigned long dt_root = of_get_flat_dt_root();

	name = of_get_flat_dt_prop(dt_root, "model", NULL);
	if (!name)
		name = of_get_flat_dt_prop(dt_root, "compatible", NULL);
	return name;
}

/**
 * of_flat_dt_match_machine - Iterate match tables to find matching machine.
 *
 * @default_match: A machine specific ptr to return in case of no match.
 * @get_next_compat: callback function to return next compatible match table.
 *
 * Iterate through machine match tables to find the best match for the machine
 * compatible string in the FDT.
 */
const void * of_flat_dt_match_machine(const void *default_match,
		const void * (*get_next_compat)(const char * const**))
{
	const void *data = NULL;
	const void *best_data = default_match;
	const char *const *compat;
	unsigned long dt_root;
	unsigned int best_score = ~1, score = 0;

	dt_root = of_get_flat_dt_root();
	while ((data = get_next_compat(&compat))) {
		score = of_flat_dt_match(dt_root, compat);
		if (score > 0 && score < best_score) {
			best_data = data;
			best_score = score;
		}
	}
	if (!best_data) {
		const char *prop;
		int size;

		__err(" unrecognized device tree list:[ ");

		prop = of_get_flat_dt_prop(dt_root, "compatible", &size);
		if (prop) {
			while (size > 0) {
				printk("'%s' ", prop);
				size -= strlen(prop) + 1;
				prop += strlen(prop) + 1;
			}
		}
		printk("]");
		return NULL;
	}

	__inf("Machine model: %s", of_flat_dt_get_machine_name());

	return best_data;
}

#ifdef CONFIG_BLK_DEV_INITRD
#ifndef __early_init_dt_declare_initrd
static void __early_init_dt_declare_initrd(unsigned long start,
					   unsigned long end)
{
	initrd_start = (unsigned long)__va(start);
	initrd_end = (unsigned long)__va(end);
	initrd_below_start_ok = 1;
}
#endif

/**
 * early_init_dt_check_for_initrd - Decode initrd location from flat tree
 * @node: reference to node containing initrd location ('chosen')
 */
static void early_init_dt_check_for_initrd(unsigned long node)
{
	u64 start, end;
	int len;
	const unsigned int *prop;

	__log("Looking for initrd properties... ");

	prop = of_get_flat_dt_prop(node, "linux,initrd-start", &len);
	if (!prop)
		return;
	start = of_read_number(prop, len/4);

	prop = of_get_flat_dt_prop(node, "linux,initrd-end", &len);
	if (!prop)
		return;
	end = of_read_number(prop, len/4);

	__early_init_dt_declare_initrd(start, end);

	__log("initrd_start=0x%llx  initrd_end=0x%llx",
		 (unsigned long long)start, (unsigned long long)end);
}
#else
static inline void early_init_dt_check_for_initrd(unsigned long node)
{
}
#endif /* CONFIG_BLK_DEV_INITRD */

#ifdef CONFIG_SERIAL_EARLYCON

int early_init_dt_scan_chosen_stdout(void)
{
	int offset;
	const char *p, *q, *options = NULL;
	int l;
	const struct earlycon_id *match;
	const void *fdt = initial_boot_params;

	offset = fdt_path_offset(fdt, "/chosen");
	if (offset < 0)
		offset = fdt_path_offset(fdt, "/chosen@0");
	if (offset < 0)
		return -1;

	p = fdt_getprop(fdt, offset, "stdout-path", &l);
	if (!p)
		p = fdt_getprop(fdt, offset, "linux,stdout-path", &l);
	if (!p || !l)
		return -1;

	q = strchrnul(p, ':');
	if (*q != '\0')
		options = q + 1;
	l = q - p;

	/* Get the node specified by stdout-path */
	offset = fdt_path_offset_namelen(fdt, p, l);
	if (offset < 0) {
		__wrn("earlycon: stdout-path %.*s not found", l, p);
		return 0;
	}

	for (match = __earlycon_table; match < __earlycon_table_end; match++) {
		if (!match->compatible[0])
			continue;

		if (fdt_node_check_compatible(fdt, offset, match->compatible))
			continue;

		of_setup_earlycon(match, offset, options);
		return 0;
	}
	return -1;
}
#endif

/**
 * early_init_dt_scan_root - fetch the top level address and size cells
 */
int early_init_dt_scan_root(unsigned long node, const char *uname,
				   int depth, void *data)
{
	const unsigned int *prop;

	if (depth != 0)
		return 0;

	dt_root_size_cells = OF_ROOT_NODE_SIZE_CELLS_DEFAULT;
	dt_root_addr_cells = OF_ROOT_NODE_ADDR_CELLS_DEFAULT;

	prop = of_get_flat_dt_prop(node, "#size-cells", NULL);
	if (prop)
		dt_root_size_cells = be32_to_cpup(prop);
	__log("dt_root_size_cells = %x", dt_root_size_cells);

	prop = of_get_flat_dt_prop(node, "#address-cells", NULL);
	if (prop)
		dt_root_addr_cells = be32_to_cpup(prop);
	__log("dt_root_addr_cells = %x", dt_root_addr_cells);

	/* break now */
	return 1;
}

u64 dt_mem_next_cell(int s, const unsigned int **cellp)
{
	const unsigned int *p = *cellp;

	*cellp = p + s;
	return of_read_number(p, s);
}

#if 0
/**
 * early_init_dt_scan_memory - Look for and parse memory nodes
 */
int early_init_dt_scan_memory(unsigned long node, const char *uname,
				     int depth, void *data)
{
	const char *type = of_get_flat_dt_prop(node, "device_type", NULL);
	const unsigned int *reg, *endp;
	int l;
	bool hotpluggable;

	/* We are scanning "memory" nodes only */
	if (type == NULL) {
		/*
		 * The longtrail doesn't have a device_type on the
		 * /memory node, so look for the node called /memory@0.
		 */
		if (depth != 1 || strcmp(uname, "memory@0") != 0)
			return 0;
	} else if (strcmp(type, "memory") != 0)
		return 0;

	reg = of_get_flat_dt_prop(node, "linux,usable-memory", &l);
	if (reg == NULL)
		reg = of_get_flat_dt_prop(node, "reg", &l);
	if (reg == NULL)
		return 0;

	endp = reg + (l / sizeof(unsigned int));
	hotpluggable = of_get_flat_dt_prop(node, "hotpluggable", NULL);

	__log("memory scan node %s, reg size %d", uname, l);

	while ((endp - reg) >= (dt_root_addr_cells + dt_root_size_cells)) {
		u64 base, size;

		base = dt_mem_next_cell(dt_root_addr_cells, &reg);
		size = dt_mem_next_cell(dt_root_size_cells, &reg);

		if (size == 0)
			continue;
		__log(" - %llx ,  %llx", (unsigned long long)base,
		    (unsigned long long)size);

		early_init_dt_add_memory_arch(base, size);

		if (!hotpluggable)
			continue;

		if (early_init_dt_mark_hotplug_memory_arch(base, size))
			__wrn("failed to mark hotplug range 0x%llx - 0x%llx",
				base, base + size);
	}

	return 0;
}

int early_init_dt_scan_chosen(unsigned long node, const char *uname,
				     int depth, void *data)
{
	int l;
	const char *p;

	__log("search \"chosen\", depth: %d, uname: %s", depth, uname);

	if (depth != 1 || !data ||
	    (strcmp(uname, "chosen") != 0 && strcmp(uname, "chosen@0") != 0))
		return 0;

	early_init_dt_check_for_initrd(node);

	/* Retrieve command line */
	p = of_get_flat_dt_prop(node, "bootargs", &l);
	if (p != NULL && l > 0)
		strlcpy(data, p, min((int)l, COMMAND_LINE_SIZE));

	/*
	 * CONFIG_CMDLINE is meant to be a default in case nothing else
	 * managed to set the command line, unless CONFIG_CMDLINE_FORCE
	 * is set in which case we override whatever was found earlier.
	 */
#ifdef CONFIG_CMDLINE
#if defined(CONFIG_CMDLINE_EXTEND)
	strlcat(data, " ", COMMAND_LINE_SIZE);
	strlcat(data, CONFIG_CMDLINE, COMMAND_LINE_SIZE);
#elif defined(CONFIG_CMDLINE_FORCE)
	strlcpy(data, CONFIG_CMDLINE, COMMAND_LINE_SIZE);
#else
	/* No arguments from boot loader, use kernel's  cmdl*/
	if (!((char *)data)[0])
		strlcpy(data, CONFIG_CMDLINE, COMMAND_LINE_SIZE);
#endif
#endif /* CONFIG_CMDLINE */

	__log("Command line is: %s", (char*)data);

	/* break now */
	return 1;
}
#endif

#ifdef CONFIG_HAVE_MEMBLOCK
#ifndef MIN_MEMBLOCK_ADDR
#define MIN_MEMBLOCK_ADDR	__pa(PAGE_OFFSET)
#endif
#ifndef MAX_MEMBLOCK_ADDR
#define MAX_MEMBLOCK_ADDR	((uint32_t)~0)
#endif

void early_init_dt_add_memory_arch(u64 base, u64 size)
{
	const u64 phys_offset = MIN_MEMBLOCK_ADDR;

	if (!PAGE_ALIGNED(base)) {
		if (size < PAGE_SIZE - (base & ~PAGE_MASK)) {
			__wrn("Ignoring memory block 0x%llx - 0x%llx",
				base, base + size);
			return;
		}
		size -= PAGE_SIZE - (base & ~PAGE_MASK);
		base = PAGE_ALIGN(base);
	}
	size &= PAGE_MASK;

	if (base > MAX_MEMBLOCK_ADDR) {
		__wrn("Ignoring memory block 0x%llx - 0x%llx",
				base, base + size);
		return;
	}

	if (base + size - 1 > MAX_MEMBLOCK_ADDR) {
		__wrn("Ignoring memory range 0x%llx - 0x%llx",
				((u64)MAX_MEMBLOCK_ADDR) + 1, base + size);
		size = MAX_MEMBLOCK_ADDR - base + 1;
	}

	if (base + size < phys_offset) {
		__wrn("Ignoring memory block 0x%llx - 0x%llx",
			   base, base + size);
		return;
	}
	if (base < phys_offset) {
		__wrn("Ignoring memory range 0x%llx - 0x%llx",
			   base, phys_offset);
		size -= phys_offset - base;
		base = phys_offset;
	}
	memblock_add(base, size);
}

int early_init_dt_mark_hotplug_memory_arch(u64 base, u64 size)
{
	return memblock_mark_hotplug(base, size);
}

int early_init_dt_reserve_memory_arch(uint32_t base,
					uint32_t size, bool nomap)
{
	if (nomap)
		return memblock_remove(base, size);
	return memblock_reserve(base, size);
}

/*
 * called from unflatten_device_tree() to bootstrap devicetree itself
 * Architectures can override this definition if memblock isn't used
 */
void * early_init_dt_alloc_memory_arch(u64 size, u64 align)
{
	return __va(memblock_alloc(size, align));
}
#else
void early_init_dt_add_memory_arch(u64 base, u64 size)
{
}

int early_init_dt_mark_hotplug_memory_arch(u64 base, u64 size)
{
	return -1;
}

int early_init_dt_reserve_memory_arch(uint32_t base,
					uint32_t size, bool nomap)
{
	__err("Reserved memory not supported, ignoring range %pa - %pa%s",
		  &base, &size, nomap ? " (nomap)" : "");
	return -1;
}

void *early_init_dt_alloc_memory_arch(u64 size, u64 align)
{
	return NULL;
}
#endif

bool early_init_dt_verify(void *params)
{
	if (!params)
		return false;

	/* check device tree validity */
	if (fdt_check_header(params))
		return false;

	/* Setup flat device-tree pointer */
	initial_boot_params = params;
	/*of_fdt_crc32 = crc32_be(~0, initial_boot_params,*/
				/*fdt_totalsize(initial_boot_params));*/
	return true;
}


void early_init_dt_scan_nodes(void)
{
	/* Retrieve various information from the /chosen node */
	of_scan_flat_dt(NULL, NULL);

	/* Initialize {size,address}-cells info */
	of_scan_flat_dt(NULL, NULL);

	/* Setup memory, calling early_init_dt_add_memory_arch */
	of_scan_flat_dt(NULL, NULL);
}

bool early_init_dt_scan(void *params)
{
	bool status;

	status = early_init_dt_verify(params);
	if (!status)
		return false;

	early_init_dt_scan_nodes();
	return true;
}

/**
 * unflatten_device_tree - create tree of device_nodes from flat blob
 *
 * unflattens the device-tree passed by the firmware, creating the
 * tree of struct device_node. It also fills the "name" and "type"
 * pointers of the nodes so the normal device-tree walking functions
 * can be used.
 */
extern unsigned char blob_input_dtb_start[];
extern unsigned char blob_input_dtb_end[];
void unflatten_device_tree(void)
{
	__unflatten_device_tree(blob_input_dtb_start, NULL, &of_root,
				of_dt_alloc, false);

	/* Get pointer to "/chosen" and "/aliases" nodes for use everywhere */
	of_alias_scan(early_init_dt_alloc_memory_arch);

	unittest_unflatten_overlay_base();
}

/**                                                           
 * of_irq_find_parent - Given a device node, find its interrupt parent node     
 * @child: pointer to device node                                               
 *                                                                              
 * Returns a pointer to the interrupt parent node, or NULL if the interrupt      
 * parent could not be determined.           
 */                                              
struct device_node *of_irq_find_parent(struct device_node *child)
{                                                                                                                                                     
        struct device_node *p;                                                                                                                        
        phandle parent;                      

        if (!of_node_get(child))                                        
                return NULL;                         

        do {                                 
                if (of_property_read_u32(child, "interrupt-parent", &parent)) {    
                        p = of_get_parent(child);              
                } else  {                    
                        p = of_find_node_by_phandle(parent);    
                }                                                         
                of_node_put(child);                                        
                child = p;                                              
        } while (p && of_get_property(p, "#interrupt-cells", NULL) == NULL);       
                                             
        return p;                            
}

void dt_list_all_nodes(void)
{
	struct device_node *dn = of_find_all_nodes(NULL);
	const void *compatable = NULL;
	
	while (dn) {     
		compatable = of_get_property(dn, "compatible", NULL);
		__log("name %s, type %s, full_name %s, cmpat:%s, secodary %p, ops %p", \
			dn->name, dn->type, dn->full_name, compatable, dn->fwnode.secondary, dn->fwnode.ops);
		dn = of_find_all_nodes(dn);
	}

}

/**
 * unflatten_and_copy_device_tree - copy and create tree of device_nodes from flat blob
 *
 * Copies and unflattens the device-tree passed by the firmware, creating the
 * tree of struct device_node. It also fills the "name" and "type"
 * pointers of the nodes so the normal device-tree walking functions
 * can be used. This should only be used when the FDT memory has not been
 * reserved such is the case when the FDT is built-in to the kernel init
 * section. If the FDT memory is reserved already then unflatten_device_tree
 * should be used instead.
 */
#if 0
void unflatten_and_copy_device_tree(void)
{
	int size;
	void *dt;

	if (!initial_boot_params) {
		__wrn("No valid device tree found, continuing without");
		return;
	}

	size = fdt_totalsize(initial_boot_params);
	dt = early_init_dt_alloc_memory_arch(size,
					     roundup_pow_of_two(FDT_V17_SIZE));

	if (dt) {
		memcpy(dt, initial_boot_params, size);
		initial_boot_params = dt;
	}
	unflatten_device_tree();
}
#endif
#ifdef CONFIG_SYSFS
static ssize_t of_fdt_raw_read(struct file *filp, struct kobject *kobj,
			       struct bin_attribute *bin_attr,
			       char *buf, loff_t off, size_t count)
{
	memcpy(buf, initial_boot_params + off, count);
	return count;
}

static int of_fdt_raw_init(void)
{
	static struct bin_attribute of_fdt_raw_attr =
		__BIN_ATTR(fdt, S_IRUSR, of_fdt_raw_read, NULL, 0);

	if (!initial_boot_params)
		return 0;

	if (of_fdt_crc32 != crc32_be(~0, initial_boot_params,
				     fdt_totalsize(initial_boot_params))) {
		__wrn("not creating '/sys/firmware/fdt': CRC check failed");
		return 0;
	}
	of_fdt_raw_attr.size = fdt_totalsize(initial_boot_params);
	return sysfs_create_bin_file(firmware_kobj, &of_fdt_raw_attr);
}
late_initcall(of_fdt_raw_init);
#endif

#endif /* CONFIG_OF_EARLY_FLATTREE */
