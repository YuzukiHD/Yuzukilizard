add_executable(sample_venc sample_venc.c sample_venc_mem.c)
target_link_libraries(sample_venc ${SUNXIMPP_LIB} ${COMMON_LIB})