sample_virvi2fish2vo测试流程：
	该sample测试mpi_vi、mpi_ise、mpi_vo组件的绑定组合。创建mpi_vi、mpi_ise和mpi_vo，将它们绑定，再分别启动。mpi_vi采集图像，
传输给mpi_ise对鱼眼图像进行校正，mpi_ise将校正后的图像传给mpi_vo进行显示预览，到达指定时间后，分别停止运行并销毁，也可以手动按
ctrl-c，终止测试。

读取测试参数的流程：
	sample提供了sample_virvi2fish2vo.conf，测试参数都写在该文件中。
	启动sample_virvi2fish2vo时，在命令行参数中给出sample_virvi2fish2vo.conf的具体路径，sample_virvi2fish2vo会读取sample_virvi2fish2vo.conf，完成参数解析。
          然后按照参数运行测试。
从命令行启动sample_virvi2fish2vo的指令：
	./sample_virvi2fish2vo -path ./sample_virvi2fish2vo.conf
	"-path ./sample_virvi2fish2vo.conf"指定了测试参数配置文件的路径。

测试参数的说明：
1.auto_test_count：指定自动化测试次数
2.dev_id：指定VI Dev设备节点 
3.src_width：指定camera采集的图像宽度,由于VI模块硬件设计的原因,src_width必须是32的倍数
4.src_height：指定camera采集的图像高度
5.src_frame_rate：指定camera采集图像的帧率
6.ise_dewarp_mode：指定单目鱼眼校正的模式，包括180模式(WARP_PANO180)/360度左右展开模式(WARP_PANO360)/Normal模式(WARP_NORMAL)/
                                                  畸变校正模式(WARP_UNDISTORT)/360度上下展开模式(WARP_180WITH2)
7.ise_mount_mode：指定Normal模式下镜头的安装方式，包括顶装(MOUNT_TOP)/壁装(MOUNT_WALL)/地装(MOUNT_BOTTOM)
                                                指定360度上下展开模式下镜头的安装方式，包括顶装(MOUNT_TOP)/壁装(MOUNT_WALL)
                                                指定360度左右展开模式下镜头的安装方式，包括顶装(MOUNT_TOP)/壁装(MOUNT_WALL)
8.ise_normal_pan：左右移动的角度
9.ise_normal_tilt：上下移动的角度
10.ise_normal_zoom：镜头变焦倍数
11.ise_width：指定校正后图像的宽度
12.ise_height：指定校正后图像的高度
13.ise_stride：指定校正后图像的stride值，该值必须是32的倍数
14.ise_flip_enable：指定是否使能图像翻转
15.ise_mirror_enable：指定是否使能图像镜像
16.display_width：指定显示宽度
17.display_height：指定显示高度
18.vo_test_duration：指定测试时间，0为循环显示，不退出sample_virvi2fish2vo