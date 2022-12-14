#ifndef SUNXI_RPROC_H_
#define SUNXI_RPROC_H_

#ifdef __cplusplus
extern "C" {
#endif

struct rproc_global_impls {
	void *ops;
	void *priv;
};

extern struct rproc_global_impls sunxi_rproc_impls[];
extern const size_t sunxi_rproc_impls_size;

#define GET_RPROC_GLOBAL_IMPLS_ITEMS(impls_array) \
        sizeof(impls_array) / sizeof(impls_array[0]);

#ifdef __cplusplus
}
#endif


#endif /* SUNXI_RPROC_H_ */
