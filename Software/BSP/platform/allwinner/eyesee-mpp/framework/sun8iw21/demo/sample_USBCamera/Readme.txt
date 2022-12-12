sample_USBCamera测试流程：
    测试EyeseeUSBCamera的预览、拍照，和EyeseeRecorder协作录制的功能。
    EyeseeUSBCamera最多允许3个通道输出，固定为通道0是uvc采集的图像，通道1是解码器的主图输出图像，通道2是解码器的子图输出图像。
    3个uvcChannel都允许预览、拍照。如果通道0的数据是编码后的数据例如mjpeg,h264，那么app不要设置通道0预览，可以设置通道0拍照，这时拍照就是直接保存uvc的采集数据。

读取测试参数的流程：
	在文件sample_USBCamera.conf填写配置参数。
	启动app时，在命令行参数中给出sample_USBCamera.conf的具体路径，app读取sample_USBCamera.conf，完成参数解析。
	然后按照参数运行测试。
	从命令行启动sample_USBCamera的指令：
	./sample_USBCamera -path /mnt/extsd/sample_USBCamera.conf

测试参数的说明：
(1)usbcam_devname: uvc设备字符串
(2)capture_width: uvc输出图像宽度
(3)capture_height: uvc输出图像高度
(4)pic_format: uvc输出图像格式
(5)frame_rate: uvc采集帧率
(6)decode_sub_out_width: vdec解码输出的小图宽度
(7)vdec_flag: 是否设置了解码参数
(8)vdec_bufsize: 解码输入缓冲区的大小
(9)vdec_priority: 无意义。
(10)vdec_pic_width: 解码主路输出的最大图像宽度
(11)vdec_pic_height: 解码主路输出的最大图像高度
(12)vdec_output_pixelformat: 解码主路输出的图像格式
(13)vdec_subpic_enable:是否激活解码子路输出
(14)vdec_subpic_width:解码子路输出的最大图像宽度
(15)vdec_subpic_height:解码子路输出的最大图像高度
(16)vdec_suboutput_pixelformat:解码子路输出的图像格式
(17)vdec_extra_frame_num: 配置额外增加的解码输出buffer数量
