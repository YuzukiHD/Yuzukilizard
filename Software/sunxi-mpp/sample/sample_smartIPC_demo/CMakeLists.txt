add_executable(
    sample_smartIPC_demo
    aiservice_detect.c
    aiservice_hw_scale.c
    aiservice_mpp_helper.c
    aiservice.c
    record.c
    rtsp_server.cpp
    sample_smartIPC_demo.c
    sdcard_manager.c
)
target_link_libraries(sample_smartIPC_demo ${SUNXIMPP_LIB} ${COMMON_LIB} ${SUNXI_NPU_LIB} ${SUNXI_RTSP_LIB})