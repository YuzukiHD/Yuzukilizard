add_executable(
    sample_smartPreview_demo
    aiservice_detect.c
    aiservice_hw_scale.c
    aiservice_mpp_helper.c
    aiservice.c
    sample_smartPreview_demo.c
)
target_link_libraries(sample_smartPreview_demo ${SUNXIMPP_LIB} ${COMMON_LIB} ${SUNXI_NPU_LIB})