sample_CodecParallel 演示同编同解

测试通路：
    录制通路：vipp0->venc->muxer
    预览通路：vipp1->vo
    播放通路：demuxer->vdec->vo

测试流程：
    该sample测试同编同解场景，即：同时测试录制、预览和播放三个场景。
	录制：创建mpi_vi、mpi_venc 和mpi_mux，将它们绑定，再分别启动。mpi_vi采集图像，经过mpi_venc 编码，最后由mpi_mux 封装成mp4 格式的文件。
	预览：创建mpi_vi和mpi_vo，将它们绑定，再分别启动。mpi_vi采集图像，直接传输给mpi_vo显示。
	播放：创建mpi_demux、mpi_vdec 和mpi_vo，将它们绑定，再分别启动。从本地U盘读取一个mp4 文件，数据分别经过mpi_demux 解封装、mpi_vdec 解码再到mpi_vo 显示。
    到达测试时间后，分别停止运行并销毁。也可以按下ctrl+c，终止测试。

读取测试参数的流程：
    sample提供了sample_CodecParallel.conf，测试参数都写在该文件中。
    启动sample_CodecParallel 时，在命令行参数中给出sample_CodecParallel.conf的具体路径，sample_CodecParallel会读取sample_CodecParallel.conf，完成参数解析。
    然后按照参数运行测试。
    从命令行启动sample_CodecParallel 的指令：
    ./sample_CodecParallel -path /mnt/extsd/sample_CodecParallel.conf
    "-path /mnt/extsd/sample_CodecParallel.conf" 指定了测试参数配置文件的路径。

测试参数的说明：

# 通用配置
test_duration：指定测试时间

# 录制配置
record_vipp_id：指定VIPP设备号
record_vipp_format：指定camera采集的图像格式
record_vipp_capture_width：指定camera采集的图像宽度
record_vipp_capture_height：指定camera采集的图像高度
record_vipp_frame_rate：指定camera采集的帧率
record_dest_file：指定录制文件的路径
record_video_file_format：指定视频封装格式
record_video_frame_rate：指定视频编码的帧率
record_video_bitrate：指定视频编码的码率
record_video_width：指定视频编码的图像宽度
record_video_heigth：指定视频编码的图像高度
record_video_encoder_type：指定视频编码格式
record_video_rc_mode：指定视频编码的码率控制模式
record_video_duration：指定视频编码的图像宽度

# 预览配置
preview_vipp_id：指定VIPP设备号
preview_vipp_format：指定camera采集的图像格式
preview_vipp_capture_width：指定camera采集的图像宽度
preview_vipp_capture_height：指定camera采集的图像高度
preview_vipp_frame_rate：指定camera采集的帧率
preview_vo_dev_id：指定VO设备号
preview_vo_display_x：指定输出图像的X 轴位置
preview_vo_display_y：指定输出图像的Y 轴位置
preview_vo_display_width：指定输出图像宽度
preview_vo_display_height：指定输出图像高度
preview_vo_layer_num：指定图像显示的图层
preview_vo_disp_type：指定显示设备（hdmi, lcd, cvbs）

# 播放配置
play_src_file：指定原始视频文件的路径
play_vo_dev_id：指定VO设备号
play_vo_display_x：指定输出图像的X 轴位置
play_vo_display_y：指定输出图像的Y 轴位置
play_vo_display_width：指定输出图像宽度
play_vo_display_height：指定输出图像高度
play_vo_layer_num：指定图像显示的图层
play_vo_disp_type：指定显示设备（hdmi, lcd, cvbs）

