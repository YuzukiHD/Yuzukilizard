sample_virvi2fish2venc测试流程：
          该sample测试mpi_vi、mpi_ise、mpi_venc组件的绑定组合。创建mpi_vi、mpi_ise和mpi_venc，将它们绑定，再分别启动。mpi_vi采集图像，
传输给mpi_ise对鱼眼图像进行校正，mpi_ise将校正后的图像传给mpi_venc进行编码，到达编码帧数后，分别停止运行并销毁,同时保存裸码流视频
文件。也可以手动按ctrl-c，终止测试。
          由于硬件设计的原因，一个ISE组件对应一个Group，一个Group最多可以创建4个Port，其中Port 0为ISE硬件模块处理后输出的图像，
Port 1~Port 3是对Port 0输出的图像进行缩放而得到的。Group创建完后必须创建Port 0，Port 1~ Port3可以根据实际需要依次创建，
Port 1~Port 3支持无极缩放，图像缩放的范围为Port 0宽高的1/8 ~ 1，各个Port之间相互独立，互不影响。ISE组件端口ID号和VENC组件通
道ID号一一对应，ISE组件端口个数必须与VENC组件通道个数相等。

读取测试参数的流程：
    sample提供了sample_virvi2fish2venc.conf，测试参数都写在该文件中。
启动sample_virvi2fish2venc时，在命令行参数中给出sample_virvi2fish2venc.conf的具体路径，sample_virvi2fish2venc会读取sample_virvi2fish2venc.conf，完成参数解析。
然后按照参数运行测试。

从命令行启动sample_virvi2fish2venc的指令：
    ./sample_virvi2fish2venc -path ./sample_virvi2fish2venc.conf
    "-path ./sample_virvi2fish2venc.conf"指定了测试参数配置文件的路径。

测试参数的说明：
1.auto_test_count：指定自动测试次数
2.dev_id：指定VI Dev设备节点 
3.src_width：指定camera采集的图像宽度,由于VI模块硬件设计的原因,src_width必须是32的倍数
4.src_height：指定camera采集的图像高度
5.src_frame_rate：指定camera采集图像的帧率
6.ISEPortNum：指定ISE组件端口个数
7.ise_dewarp_mode：指定单目鱼眼校正的模式，包括180模式(WARP_PANO180)/360度左右展开模式(WARP_PANO360)/PTZ模式(WARP_NORMAL)/
                                           畸变校正模式(WARP_UNDISTORT)/360度上下展开模式(WARP_180WITH2)
8.ise_mount_mode：指定PTZ模式下镜头的安装方式，包括顶装(MOUNT_TOP)/壁装(MOUNT_WALL)/地装(MOUNT_BOTTOM)
                          指定360度上下展开模式下镜头的安装方式，包括顶装(MOUNT_TOP)/壁装(MOUNT_WALL)
                          指定360度左右展开模式下镜头的安装方式，包括顶装(MOUNT_TOP)/壁装(MOUNT_WALL)
9 .ise_normal_pan：左右移动的角度
10.ise_normal_tilt：上下移动的角度
11.ise_normal_zoom：镜头变焦倍数
12.ise_portx_width：指定校正后图像的宽度
13.ise_portx_height：指定校正后图像的高度
14.ise_portx_stride：指定校正后图像的stride值，该值必须是32的倍数
15.ise_portx_flip_enable：指定是否使能图像翻转
16.ise_portx_mirror_enable：指定是否使能图像镜像
17.VencChnNum：指定VENC组件通道个数
18.encoder_type：指定编码格式
19.encoder_count：指定每次测试编码帧数，-1为循环编码，不退出sample_virvi2fish2venc
20.picture_format：指定编码后图像的格式
21.venc_chnx_dest_width：指定编码后图像的宽度
22.venc_chnx_dest_height：指定编码后图像的高度
23.venc_chnx_dest_frame_rate：指定编码帧率
24.venc_chnx_dest_bit_rate：指定编码码率
25.venc_chnx_output_file_path：指定编码后视频文件的路径
注：每个ISE端口图像翻转和镜像只能使能一个其中一个