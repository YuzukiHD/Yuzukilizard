sample_vi2venc2muxer测试流程：
	从camera节点取vi输入数据，并对数据进行编码封装，生成对应的视频输出文件。

目前支持测试的功能:
	RC控制
	QP设置
	GOP设置
	高级跳帧参考
	Rotate
	Mirror
	彩转灰
	3D降噪
	ROI
	ROI背景帧率
	P帧帧内刷新
	ORL
	Crop

加配置文件路径的参数，读取测试参数的流程：
	sample提供了sample_vi2venc2muxer.conf，测试参数都写在该文件中。
	启动sample_vi2venc2muxer时，在命令行参数中给出sample_vi2venc2muxer.conf的具体路径，sample_vi2venc2muxer会读取sample_vi2venc2muxer.conf，完成参数解析。
	然后按照参数运行测试。
	从命令行启动sample_vi2venc2muxer的指令：
	./sample_vi2venc2muxer -path /mnt/extsd/sample_vi2venc2muxer.conf
	"-path /mnt/extsd/sample_vi2venc2muxer.conf"指定了测试参数配置文件的路径。

测试参数的说明：
(1)dev_node：视频数据的获取节点（0:表示/dev/video0  1:表示/dev/video1）
(2)src_size：设定源视频数据的大小，如1080/720
(3)vi_buffer_num: 指定VI buffer个数
(4)src_pixfmt: 指定源视频像素格式
(5)color_space: 指定颜色空间类型
(6)video_dst_file: 生成的视频文件路径（默认录制一个文件的时长为1分钟，如果sample测试超过一分钟，则下个文件会自动的在此文件名上加上数字后缀，以示区分）
(7)dst_file_max_cnt: 指定最多文件个数
(8)video_framerate: 生成视频文件的帧率
(9)video_bitrate: 生成视频文件的码率
(10)video_size: 生成的视频文件大小，如1080p
(11)video_encoder: 视频编码方式
(12)profile: 指定视频编码profile
(13)product_mode: 指定视频编码产品模式
(14)key_frame_interval: 指定视频编码关键帧间隔，单位：帧数
(15)rc_mode: 指定视频编码码率控制模式
(16)qp0: 指定视频编码QP
(17)qp1: 指定视频编码QP
(18)gop_mode: 指定视频编码GOP模式
(19)gop_size: 指定视频编码GOP大小
(20)AdvancedRef_Base:高级跳帧参考的base设置,0表示关闭; 如果开启高级跳帧参考,建议10.
(21)AdvancedRef_Enhance:高级跳帧参考的enhance设置,建议1.
(22)AdvancedRef_RefBaseEn:高级跳帧参考的RefBase设置. 1:开启,0:关闭.
(23)enable_fast_enc: 视频编码快速编码开关
(24)enable_smart: 视频编码smart功能开关
(25)svc_layer: 指定SVC layer
(26)encode_rotate: 指定视频编码旋转角度
(27)mirror: 视频编码水平镜像开关
(28)video_duration: 视频时长，单位:s
(29)test_duration: 测试时间，单位: s
(30)color2grey: 视频编码彩转灰开关
(31)3dnr: 视频编码3D降噪开关
(32)roi_num: 指定视频编码ROI个数
(33)roi_BgFrameRate: 指定视频编码ROI背景帧率
(34)IntraRefresh_BlockNum: 视频编码P帧帧内刷新宏块数
(35)orl_num: 指定视频编码ORL数量
(36)vbvBufferSize: 指定视频编码VBV buffer大小
(37)vbvThreshSize: 指定视频编码VBV buffer阈值
(38)crop_en: 视频编码crop使能
(39)crop_rect_x: 指定视频编码crop矩形X
(40)crop_rect_y: 指定视频编码crop矩形Y
(41)crop_rect_w: 指定视频编码crop矩形宽
(42)crop_rect_h: 指定视频编码crop矩形高
