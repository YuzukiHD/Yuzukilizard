add_executable(sample_demux2vdec sample_demux2vdec.c)
target_link_libraries(sample_demux2vdec ${SUNXIMPP_LIB} ${COMMON_LIB})

add_executable(yuv420pTobmp yuv420pTobmp.c)
target_link_libraries(yuv420pTobmp ${SUNXIMPP_LIB} ${COMMON_LIB})