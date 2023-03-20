# sunxi-mpp

## How to Compile

1. edit toolchain in `CMakeLists.txt` to your toolchain

```
set(CMAKE_C_COMPILER ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/toolchain-sunxi-musl-gcc-830/toolchain/bin/arm-openwrt-linux-gcc)
set(CMAKE_CXX_COMPILER ${CMAKE_CURRENT_SOURCE_DIR}toolchain/toolchain-sunxi-musl-gcc-830/toolchain/bin/arm-openwrt-linux-g++)
set(CMAKE_SYSROOT ${CMAKE_CURRENT_SOURCE_DIR}toolchain/toolchain-sunxi-musl-gcc-830/toolchain)
```

2. run environment command
```
source STAGING_DIR.sh
```

3. build

```
mkdir build
cd build
cmake ..
make
```
