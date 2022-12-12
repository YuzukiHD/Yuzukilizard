sample_uvc2denc2vo测试流程：
    测试mpi_uvc->mpi_vdec的绑定方式传输数据，mpi_vdec->mpi_vo也采用绑定方式。
	从mpi_uvc组件获取mjpeg编码格式图片，交给mpi_vdec组件解码，再传给mpi_vo组件显示。

读取测试参数的流程：
	在文件sample_uvc2denc2vo.conf填写配置参数。
	启动app时，在命令行参数中给出sample_uvc2denc2vo.conf的具体路径，app读取sample_uvc2denc2vo.conf，完成参数解析。
	然后按照参数运行测试。
	从命令行启动sample_uvc2denc2vo的指令：
	./sample_uvc2denc2vo -path /mnt/extsd/sample_uvc2denc2vo.conf

测试参数的说明：
(1)dev_name: uvc设备字符串
(2)pic_format: uvc输出图像格式
(3)capture_width: uvc输出图像宽度
(4)capture_height: uvc输出图像高度
(5)capture_framerate: uvc采集帧率
(6)decode_out_width: vdec解码输出图像的宽度
(6)decode_out_height: vdec解码输出图像的高度
(5)display_width:显示宽度
(6)display_height:显示高度
(7)test_duration:测试时间，0代表无限。

