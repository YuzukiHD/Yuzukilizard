sample_uvc测试流程：
	读取jpg文件，反复送由UVC设备输出到电脑（注意：不是获取UVC摄像头的数据，而是把整个板子当作UVC摄像头，将图像送由电脑显示）
	电脑端使用VLC工具查看。

读取测试参数的流程：
1. 执行命令生成video设备节点
执行run_otg脚本生成设备节点
	./run_otg

2. 启动数据传输（uvc设备号要改为上述步骤生成的video节点）
	./sample_uvc -path ./sample_uvc.conf
3. 使用VLC进行查看

测试参数的说明：
(1)uvc_dev：uvc设备号
(2)vin_dev：摄像头数据采集设备号
(3)cap_format：数据采集格式，nv21, nv12, yu12, yv12
(4)cap_width：图像采集宽度
(5)cap_height：图像采集高度
(6)cap_frame_rate：图像采集帧率
(7)encoder_type：编码类型
(8)enc_frame_quarity：编码质量
(9)enc_width：编码宽度
(10)enc_height：编码高度
(11)enc_frame_rate：编码帧率
(12)enc_bit_rate：编码bit率 4M(4194304) 8M(8388608)
(13)use_eve：是否使用人脸识别，1－使用，0－不使用


==============> update again, step by step.


==>内核配置：

make kernel_menuconfig
[*] Device Driver --->
[*] USB support --->
[*] USB Gadget Support --->
[*] USB functions configuarble through configfs
[*] USB Webcam function


==>删除掉adb 的初始化，防止adb 影响webcam 的功能

rm target/allwinner/v459-perf1/busbox-init-base-files/etc/init.d/S50usb


==>编译sample_uvcout

vim softwinner/eyesee-mpp/middleware/v459/tina.mk

#去掉下面行的注释：
make -c sample/sample_uvcout -f tina.mk all
编译mkmpp， sample_uvcout目录生成sample_uvcout。

==>拷贝sample_uvcout/run_otg 脚本到小机

==>确定小机生成新的video节点

ls /dev/video*
/dev/video0 /dev/video1 /dev/video2

#初始化usb webcam 驱动，生成新的video节点，用于传输数据
./run_otg


==>#查看新增vidoe节点
ls /dev/video*
/dev/video0 /dev/video1 /dev/video2 /dev/video3

==>#确定新节点,是video4
==>修改sample_uvcout.conf 配置文件

vi sample/sample_uvcout/sample_uvcout.conf

[parameter]
uvc_dev = 2 #将新增节点的序号加入这里。
在小机上运行sample

==>./sample_uvcout -path ./sample_uvcout.conf

==>电脑查看，可使用VLC
[媒体] -->
[打开捕获设备] -->
捕获模式[DirectShow] 适配器设备名称 [UVC camera]

################################################################################
2021.12.19更新

1. 删除无用代码
2. 增加H264模式
3. 修改sample使用模式，由之前的sample配置文件定义传输格式，改为可支持PC选择。
目前支持的格式：
	1. MJPEG
		1920x1080 30fps
		1280x720  30fps
		640x480   30fps
	2. YUV2
		320x240   30fps
	3. H264
		1920x1080 30fps
		1280x720  30fps
4. 删除sample配置文件一些参数，保留UVC_DEV、VI_DEV以及MJPEG和H264码率配置参数。
5. 支持 UVC bulk模式。

测试步骤：
	与先前的测试步骤一致，可使用setusbconfig脚本来初始化UVC模式。
		./setusbconfig uvc
		./sample_uvcout -path ./sample_uvcout.conf
	UVC BULK模式：
		初始化UVC bulk模式：
			./setusbconfig uvc bulk
		运行sample：
			./sample_uvcout -b 1 -path ./sample_uvcout.conf
