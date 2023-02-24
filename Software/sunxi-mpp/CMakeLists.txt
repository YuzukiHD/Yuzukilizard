cmake_minimum_required(VERSION 3.10)
project(sunxi_mpp C CXX)

set(file_sh ${CMAKE_CURRENT_BINARY_DIR}/STAGING_DIR.sh)
file(WRITE ${file_sh} "#!/usr/bin/env bash\n")
file(APPEND ${file_sh} "export STAGING_DIR=build")
execute_process(COMMAND chmod a+x ${file_sh} RESULT_VARIABLE res)

if(DEFINED ENV{STAGING_DIR})
    message("-- STAGING_DIR: $ENV{STAGING_DIR}")
else()
    message(FATAL_ERROR  "NOT DEFINED `STAGING_DIR` VARIABLES, Please `source STAGING_DIR.sh` before build")
endif()

set(CMAKE_C_COMPILER ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/toolchain-sunxi-musl-gcc-830/toolchain/bin/arm-openwrt-linux-gcc)
set(CMAKE_CXX_COMPILER ${CMAKE_CURRENT_SOURCE_DIR}toolchain/toolchain-sunxi-musl-gcc-830/toolchain/bin/arm-openwrt-linux-g++)
set(CMAKE_SYSROOT ${CMAKE_CURRENT_SOURCE_DIR}toolchain/toolchain-sunxi-musl-gcc-830/toolchain)

include(CMake/sunxi_mpp_include.cmake)
include(CMake/sunxi_mpp_lib.cmake)

link_directories(lib)

set(COMMON_LIB 
        libglog.so
        libasound.so
        libz.so
        libstdc++.so
)

add_subdirectory(sample)
