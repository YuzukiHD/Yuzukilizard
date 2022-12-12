该sample用于测试4路PTZ与原图预览,并进行4路PTZ与原图录像
读取测试参数的流程：
    sample提供了sample_fisheye.conf，测试参数都写在该文件中。启动sample_fisheye时，在命令行参数中给出sample_fisheye.conf的具体路径，
sample_fish会读取sample_fisheye.conf，完成参数解析，然后按照参数运行测试。

从命令行启动sample_fisheye的指令：
    ./sample_fisheye -path ./sample_fisheye.conf
    "-path ./sample_fisheye.conf"指定了测试参数配置文件的路径。

测试参数说明：
cam_csi_chn:指定csi设备号
cam_isp_dev:指定isp设备号
cam_vipp_num:指定vipp通道个数
cam_cap_width:指定vi源图像宽度
cam_cap_height:指定vi源图像高度
cam_frame_rate:指定vi源图像帧率
cam_buf_num：指定vi buffer个数
cam_disp_enable:使能cam预览
cam_disp_rect_x：预览显示区域x坐标
cam_disp_rect_y：预览显示区域y坐标
cam_disp_rect_width：预览显示区域宽
cam_disp_rect_heigh：预览显示区域高
cam_rec_enable:使能camera录像
cam_rec_v_type：指定camera录像编码格式
cam_rec_v_width：指定camera录像分辨率宽度
cam_rec_v_height：指定camera录像分辨率高度
cam_rec_v_bitrate：指定camera录像码率
cam_rec_v_framerate：指定camera录像帧率
cam_rec_a_typ：指定camera音频编码格式
cam_rec_a_sr：指定camera音频编码采样率
cam_rec_a_chn：指定camera音频编码通道
cam_rec_a_br：指定camera音频编码码率

# dest parameter
# ise_dewarp_mode:WARP_PANO180 = 0x0001,
#                 WARP_PANO360 = 0x0002,
#                 WARP_NORMAL  = 0x0003,
#                 WARP_UNDISTORT = 0x0004,
#                 WARP_180WITH2 = 0x0005.
#                 WARP_PTZ4IN1 = 0x0006
# ise_mount_mode:MOUNT_TOP = 0x0001,
#                MOUNT_WALL = 0x0002,
#                MOUNT_BOTTOM = 0x0003
ise_mo_enable:使能ise
ise_group_num:指定ise实例个数
ise_mo_p：指定镜头焦距
ise_mo_cx：指定镜头光心水平坐标
ise_mo_cy：指定镜头光心垂直坐标

ise_mo_groupx_dewarp_mode:指定单目鱼眼校正的模式，包括180模式(WARP_PANO180)/360度左右展开模式(WARP_PANO360)/PTZ模式(WARP_NORMAL)/
                                                                     畸变校正模式(WARP_UNDISTORT)/360度上下展开模式(WARP_180WITH2)  
ise_mo_groupx_mount_type:指定PTZ模式下镜头的安装方式，包括顶装(MOUNT_TOP)/壁装(MOUNT_WALL)/地装(MOUNT_BOTTOM)
                                                                   指定360度上下展开模式下镜头的安装方式，包括顶装(MOUNT_TOP)/壁装(MOUNT_WALL)
                                                                   指定360度左右展开模式下镜头的安装方式，包括顶装(MOUNT_TOP)/壁装(MOUNT_WALL)
ise_mo_groupx_pan:指定Normal模式下视场角的水平转动角度
ise_mo_groupx_tilt:指定Normal模式下视场角的垂直转动角度
ise_mo_groupx_zoom:指定Normal模式下视场角大小
ise_mo_groupx_w：指定ise目标图像宽度
ise_mo_groupx_h：指定ise目标图像高度
ise_mo_groupx_flip：指定ise目标图像水平翻转
ise_mo_groupx_mir：指定ise目标图像垂直翻转

ise_mo_disp_enable:使能ise预览
ise_mo_disp_group_rect_x：预览显示区域x坐标
ise_mo_disp_groupx_rect_y：预览显示区域y坐标
ise_mo_disp_groupx_rect_w：预览显示区域宽
ise_mo_disp_groupx_rect_h：预览显示区域高

ise_rec_enable:使能ise录像
ise_rec_v_type：指定ise录像编码格式
ise_rec_v_width：指定ise录像分辨率宽度
ise_rec_v_height：指定ise录像分辨率高度
ise_rec_v_bitrate：指定ise录像码率
ise_rec_v_framerate：指定ise录像帧率
ise_rec_a_typ：指定ise音频编码格式
ise_rec_a_sr：指定ise音频编码采样率
ise_rec_a_chn：指定ise音频编码通道
ise_rec_a_br：指定ise音频编码码率
