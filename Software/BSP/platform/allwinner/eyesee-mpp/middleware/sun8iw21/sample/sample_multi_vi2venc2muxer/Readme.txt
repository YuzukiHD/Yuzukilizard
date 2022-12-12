sample_multi_vi2venc2muxer最多允许4路编码加1路jpeg拍照。每个编码通道都可以选择vipp，以及timelapse模式等。默认的配置如下：
  VIPP0->venc0->muxerGrp0：bufferNum=5，采集分辨率1920x1080，格式LBC2.5，h264编码帧率30，码率1Mbit/s，threshSize：w*h/10, 普通NormalP编码，vbv缓存1秒。
  VIPP1->venc1->muxerGrp1: bufferNum=5，采集分辨率640x480，格式NV21，h264编码帧率30，码率500Kbit/s，threshSize：w*h/10, 普通NormalP编码，vbv缓存1秒。
  VIPP1->venc2->muxerGrp2: h264编码帧率30，码率500Kbit/s，threshSize：w*h/10，普通NormalP编码，vbv缓存1秒，timelapse = 200ms。
  VIPP1->venc3->muxerGrp3: h264编码帧率30，码率500Kbit/s，threshSize：w*h/10，普通NormalP编码，vbv缓存1秒，timelapse = 1000ms。
  VIPP0->venc_jpeg：每隔30秒拍照一张。

读取测试参数的流程：
	sample提供了sample_multi_vi2venc2muxer.conf，测试参数都写在该文件中。
	从命令行启动sample_multi_vi2venc2muxer的指令（"-path /mnt/extsd/sample_v459_QFN.conf"指定了测试参数配置文件的路径）：
	./sample_multi_vi2venc2muxer -path /mnt/extsd/sample_multi_vi2venc2muxer.conf
也可以不带配置文件参数，sample使用默认参数：
	./sample_multi_vi2venc2muxer

测试参数的说明：
(1)vipp0：第一条vpp通路的视频数据的获取节点（0:表示/dev/video0  1:表示/dev/video1）
(2)vipp0_format：图像采集格式
(3)vipp0_capture_width：图像采集宽度
(4)vipp0_capture_height：图像采集高度
(5)vipp0_framerate：图像采集帧率
(6)videoA_vipp：视频编码通路连接的vipp号。
(7)videoA_file：录制的视频文件路径
(8)videoA_file_cnt：循环录制允许的最大文件数目，超过就开始删除最早的文件。
(9)videoA_framerate：编码目标帧率
(10)videoA_bitrate：编码码率
(11)videoA_width；编码输出宽度
(12)videoA_height：编码输出高度
(13)videoA_encoder：编码格式
(14)videoA_rc_mode；码率模式，0:CBR  1:VBR
(15)videoA_duration：文件时长。
(16)videoA_timelapse：timelapse模式，-1:禁止timelapse; 0:慢摄影; >0:timelapse模式, 数值为采集帧的帧间隔，单位us。
(17)videoE_photo_interval：拍照时间间隔，单位：秒
(18)test_duration：测试时长，0表示无限时长。
