sample_RegionDetect：
	演示区域检测功能。

环境准备
1. 板端接好camera和屏幕。
2. 将./package/allwinner/libsdk-viplite-driver/algo_aw/pdet/human.nb复制到tf卡根目录中。
3. 将external/eyesee-mpp/middleware/sun8iw21/sample/bin/sample_RegionDetect复制到tf卡中。


demo演示
1. 加载npu驱动：insmod /lib/modules/4.9.191/vipcore.ko
1. shell进入/mnt/extsd/目录
2. 执行./sample_RegionDetect N开始区域检测demo演示，其中N∈{0，1，2，3}。分别为：
移动侦测，区域检测，越界检测，人形检测。
3. 检测demo会显示预览界面，其中红色为配置区域或界线，绿色正方形小框显示移动侦测区域，绿
色矩形框为人形区域，中心为绿色小点。越界算法以人形中心点为准计算，蓝色点为越界坐标。
4. 录像自动保存为motion.h265。 5. 查看log打印，区域检测打印为：<on_cross_callback> on_cross_callback: RECT[0] type: 1；其中
RECT[0]表示第0个区域，type: 1表示进入区域，2表示离开区域。越界检测打印为：
<on_cross_callback> on_cross_callback: LINE[0] type: 1。其中LINE[0]表示第一条线，type:1表
示从左到右，2表示从右到左。

