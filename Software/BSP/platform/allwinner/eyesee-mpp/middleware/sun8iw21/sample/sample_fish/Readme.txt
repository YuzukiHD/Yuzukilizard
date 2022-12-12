sample_fish测试流程：
	该sample测试mpi_ise组件单目鱼眼功能。创建ise组件，将图像传输给mpi_ise，ISE组件对图像进行校正，
通过调用mpi获取ISE组件处理后的数据，到达指定次数后，停止运行并销毁ISE组件。也可以手动按ctrl-c，终止测试。
    如果需要获取ISE组件处理后的YUV数据，需要将sample_fish.c中的Save_Picture宏打开；如果需要在Normal模式下对PTZ参数进行调节，
需要将sample_fish.c中的Dynamic_PTZ宏打开。
ISE GDC补充说明：对于新增的ISE GDC算法，由于Warp_Ptz4In1这个模式要求输出分辨率是输入分辨率的两倍，而现有的2048原图对于内存
以及硬件方面性能有所增加，所以针对Warp_Ptz4In1增加了一张1024原图以及sample_fish_ptz4in1.conf，若需要运行Warp_Ptz4In1这个模式可以
使用fisheye_1024x1024.yuv420这张图像以及sample_fish_ptz4in1.conf。

读取测试参数的流程：
	sample提供了sample_fish.conf，测试参数都写在该文件中。
	启动sample_fish时，在命令行参数中给出sample_fish.conf的具体路径，sample_fish会读取sample_fish.conf，完成参数解析。
	然后按照参数运行测试。
从命令行启动sample_fish的指令：
	./sample_fish -path ./sample_fish.conf
	"-path ./sample_fish.conf"指定了测试参数配置文件的路径。

测试参数的说明：
在C源文件里面的ISE_GROUP_INS_CNT宏可以控制测试开启的Ise 实例数量，目前默认为2
1.auto_test_count：指定自动化测试次数
2.process_count: 指定ISE组件处理次数
3.pic_width：指定源图像宽度
4.pic_height：指定源图像高度
5.pic_stride：指定源图像的stride值，该值必须是32的倍数
6.pic_frame_rate：指定发送源图像的帧率
7.pic_file_path：指定源图像的路径
8.ise_dewarp_mode：指定单目鱼眼校正的模式，包括180模式(WARP_PANO180)/360度左右展开模式(WARP_PANO360)/PTZ模式(WARP_NORMAL)/
                                                  畸变校正模式(WARP_UNDISTORT)/360度上下展开模式(WARP_180WITH2)
9.ise_mount_mode：指定PTZ模式下镜头的安装方式，包括顶装(MOUNT_TOP)/壁装(MOUNT_WALL)/地装(MOUNT_BOTTOM)
                                               指定360度上下展开模式下镜头的安装方式，包括顶装(MOUNT_TOP)/壁装(MOUNT_WALL)
                                               指定360度左右展开模式下镜头的安装方式，包括顶装(MOUNT_TOP)/壁装(MOUNT_WALL)
10.ise_normal_pan：左右移动的角度
11.ise_normal_tilt：上下移动的角度
12.ise_normal_zoom：镜头变焦倍数
13.ise_port_num：指定ISE组件端口个数
14.ise_output_file_path：指定ISE组件输出图像的路径
15.ise_portx_width：指定校正处理后图像的宽度
16.ise_portx_height：指定校正处理后图像的高度
17.ise_portx_stride：指定校正处理后图像的stride值，该值必须是32的倍数
18.ise_portx_flip_enable：指定是否使能图像翻转
19.ise_portx_mirror_enable：指定是否使能图像镜像
注：每个ISE端口图像翻转和镜像只能使能其中一个,测试时需要拷贝yuv文件到测试路径下面，程序默认打开程序所在文件夹的yuv文件
