add_executable(sample_rtsp rtsp_server.cpp sample_rtsp.c)
target_link_libraries(sample_rtsp ${SUNXIMPP_LIB} ${COMMON_LIB} ${SUNXI_RTSP_LIB})