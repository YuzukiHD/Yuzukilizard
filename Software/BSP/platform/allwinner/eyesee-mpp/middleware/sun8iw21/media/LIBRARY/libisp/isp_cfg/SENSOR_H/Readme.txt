一、安装方法：
1.将creat_bin.sh 脚本放到目录：softwinner/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libisp/isp_cfg/SENSOR_H
2.给予可执行权限： chmod +x creat_bin.sh

二、使用方法
1. cd softwinner/eyesee-mpp/middleware/sun8iw19p1/media/LIBRARY/libisp/isp_cfg/SENSOR_H
2. 获取帮助： ./creat_bin.sh -h
3. 转换单个效果头文件 ./creat_bin.sh -t sp2305/sp2305_20fps_ini_ipc_day.h 
生成的效果文件在(4个)：
sp2305/sp2305_mipi_day_isp_3a_settings.bin
sp2305/sp2305_mipi_day_isp_iso_settings.bin
sp2305/sp2305_mipi_day_isp_test_settings.bin
sp2305/sp2305_mipi_day_isp_tuning_settings.bin
4.若需要把整个sp2305 的头文件都转换，则： ./creat_bin.sh -t sp2305/
会在sp2305目录里面生成每个头文件的bin文件。

三、修改Mpp代码，加载这些头文件。
将load_extern_isp.diff 打到mpp目录下。
代码会在启动ISP服务的时候，检查是否有下列文件：
+#define ISP_TEST_PATH    "/mnt/extsd/isp/isp_test_settings.bin"
+#define ISP_3A_PATH      "/mnt/extsd/isp/isp_3a_settings.bin"
+#define ISP_ISO_PATH     "/mnt/extsd/isp/isp_iso_settings.bin"
+#define ISP_TUNNING_PATH "/mnt/extsd/isp/isp_tuning_settings.bin"

如果有，则读取加载以上目录的头文件。若没有，则使用app内部的头文件。




四、转换原理：
如何将头文件的参数结构体转换成二进制数据？
①将头文件编译成 c的obj文件，即.o文件

②通过readelf -s 读取 .o文件的，把里面的实例的符号表展示出来：
   Num:    Value  Size Type    Bind   Vis      Ndx Name
    10: 00000000   216 OBJECT  GLOBAL DEFAULT    2 sc2232_mipi_isp_20fps_night_test_settings
    11: 000000d8  5832 OBJECT  GLOBAL DEFAULT    2 sc2232_mipi_isp_20fps_night_3a_settings
    12: 000017a0  8468 OBJECT  GLOBAL DEFAULT    2 sc2232_mipi_isp_20fps_night_iso_settings
    13: 000038b4 0x1ced0 OBJECT  GLOBAL DEFAULT    2 sc2232_mipi_isp_20fps_night_tuning_settings
从此次可知道 全局变量 sc2232_mipi_isp_20fps_night_test_settings 的地址为0x00000000  大小为：216

③通过readelf -S 读取 .o文件的 file's section headers ，可知道.data 数据位于文件哪里。 全局变量是存储在.data段的数据里面的：
 [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
[ 2] .data             PROGBITS        00000000 00004c 020794 00  WA  0   0  4
解析可知 .data端在偏移文件0x00004c  位置。

则sc2232_mipi_isp_20fps_night_test_settings 这部分数据则在 .o 文件中 0x00004c + 0x00000000 的位置。 其大小为：216。用dd取出来即可。

④通过这个方法，可以将剩余的3个全局变量的二进制能全部取出来保存。