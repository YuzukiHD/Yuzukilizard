sample_UILayer测试UILayer的格式。sample自己创建指定格式的RGB图，设置给UILayer。

读取测试参数的流程：
	sample提供了sample_UILayer.conf，测试参数都写在该文件中。
	启动sample_UILayer时，在命令行参数中给出sample_UILayer.conf的具体路径，sample_UILayer会读取sample_UILayer.conf，完成参数解析。
	然后按照参数运行测试。
	从命令行启动sample_UILayer的指令：
	./sample_UILayer -path /mnt/extsd/sample_UILayer.conf
	"-path /mnt/extsd/sample_UILayer.conf"指定了测试参数配置文件的路径。

测试参数的说明：
(1)pic_width：源图的宽度
(4)pic_height：源图的高度
(5)display_width：指定输出图像的宽度
(6)display_height：指定输出图像的高度
(7)bitmap_format:源图格式