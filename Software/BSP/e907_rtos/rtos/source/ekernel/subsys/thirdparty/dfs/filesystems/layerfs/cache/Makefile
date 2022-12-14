ccflags-y += -I$(obj)

obj-y += cache_fs.o
obj-y += cache_fs_base.o
obj-y += cache_fs_direct.o

obj-$(CONFIG_LAYERFS_CACHE_SECTOR_ALIGN) += cache_fs_align.o
obj-$(CONFIG_LAYERFS_CACHE_NORMAL) += cache_fs_normal.o

obj-$(CONFIG_LAYERFS_CACHE_HERB) += herb_fs_cache.o
obj-$(CONFIG_LAYERFS_CACHE_HERB) += cache_lock.o

obj-$(CONFIG_LAYERFS_CACHE_DEBUG) += cache_fs_debug.o
