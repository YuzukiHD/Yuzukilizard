sample_vdec测试流程：
	连续解码jpg格式图片或者h264裸码流，然后保存为yuv格式文件。

读取测试参数的流程：
	sample提供了sample_vdec.conf，测试参数包括：原文件路径(jpeg_file_path)和目标文件路径(yuv_file_path)。
	注意：
	1.jpeg解码：
	因解码序号连续的一系列图片，只指定一个源文件路径和目标文件路径，因此jpeg_file_path去掉了“.jpg”的后缀，yuv_file_path也去掉了“.yuv”的后缀，app在打开文件或创建文件时将会自动添加相应的后缀名。
	例如，设置jpeg_file_path = "/mnt/extsd/sample_vdec/pic"，用户需放置pic0.jpg,pic1.jpg,pic2.jpg...的图片；
          设置yuv_file_path = "/mnt/extsd/sample_vdec/pic"将会生成picxy_width_height.yuv的图片，其中xy代表序号(00,01,02...)，width_height为根据解码出来的结果填充宽高信息。
	启动sample_vdec时，在命令行参数中给出sample_vdec.conf的具体路径，sample_vdec会读取该文件，完成参数解析。
	2.h264裸码流解码：
	源文件须包含两个，一个为裸码流文件，另一个为裸码流进行索引大小的文件。参考目录中的文件vbs.h264和vbs.len，其中后个文件中专门用于存放前者文件中每贞裸码流大小的描述。
	（h264裸码流文件须以是spspps开头，然后再跟IDR侦，接下来再跟p桢图像）
	然后按照参数运行测试。

从命令行启动sample_vdec的指令：
	./sample_vdec -path /mnt/extsd/sample_vdec/sample_vdec.conf
	"-path /mnt/extsd/sample_vdec/sample_vdec.conf"指定了测试参数配置文件的路径。

测试参数的说明：
(1)vdecoder_format：指定解码格式，jpeg或者h264。
(2)yuv_file_path：指定解码后生成的yuv文件的路径。

(3)jpeg_file_path：指定原jpg图片路径，该文件必须为jpeg格式的文件。

(4)h264_vbs_path：指定原264裸码流路径。
(5)h264_len_path：指定原264裸码流祯大小描述文件路径。