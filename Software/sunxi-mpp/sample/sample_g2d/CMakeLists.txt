add_executable(sample_g2d sample_g2d_mem.c sample_g2d.c)
target_link_libraries(sample_g2d ${SUNXIMPP_LIB} ${COMMON_LIB})