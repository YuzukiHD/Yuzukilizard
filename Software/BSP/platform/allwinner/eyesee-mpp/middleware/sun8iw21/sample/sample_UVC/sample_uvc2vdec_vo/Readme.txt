sample_uvc2dec_vo测试流程：
    测试mpi_uvc->mpi_vdec的绑定方式传输数据，mpi_vdec->mpi_vo采用非绑定方式。
	从mpi_uvc组件获取mjpeg编码格式图片，交给mpi_vdec组件解码，配置vdec解码缩小，并且大小图两路输出，mpi_vdec的输出采用非绑定方式，app主动获取大小图，
	再交给mpi_vo组件的两个图层分别显示。

读取测试参数的流程：
	在文件sample_uvc2dec_vo.conf填写配置参数。
	启动app时，在命令行参数中给出sample_uvc2dec_vo.conf的具体路径，app读取sample_uvc2dec_vo.conf，完成参数解析。
	然后按照参数运行测试。
	从命令行启动sample_uvc2dec_vo的指令：
	./sample_uvc2dec_vo -path /mnt/extsd/sample_uvc2dec_vo.conf

测试参数的说明：
(1)dev_name: uvc设备字符串
(2)pic_format: uvc输出图像格式
(3)capture_width: uvc输出图像宽度
(4)capture_height: uvc输出图像高度
(5)capture_framerate: uvc采集帧率
(6)decode_sub_out_width: vdec解码输出的小图宽度
(7)decode_sub_out_height: vdec解码输出的小图高度
(8)display_main_x: 主图显示区域的左上角起点坐标x
(9)display_main_y: 主图显示区域的左上角起点坐标y
(10)display_main_width: 主图显示区域宽度
(11)display_main_height: 主图显示区域高度
(12)display_sub_x: 子图显示区域的左上角起点坐标x
(13)display_sub_y: 子图显示区域的左上角起点坐标y
(14)display_sub_width: 子图显示区域宽度
(15)display_sub_height: 子图显示区域高度
(16)test_frame_count: 测试帧数，0代表无限。
