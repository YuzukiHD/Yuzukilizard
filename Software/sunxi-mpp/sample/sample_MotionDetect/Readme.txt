sample_MotionDetect测试流程：
    该sample演示视频编码库的移动侦测功能的使用。创建mpi_vi和mpi_venc，将它们绑定，再分别启动。mpi_vi采集图像，直接传输给mpi_venc进行编码。
编码过程中获取移动侦测信息，可保存裸码流视频文件。支持手动按ctrl-c，终止测试。
    务必注意：视频编码驱动只支持在VBR, IPC-MODE模式下做移动侦测。

读取测试参数的流程：
    sample提供了sample_MotionDetect.conf，测试参数都写在该文件中。
启动sample_MotionDetect时，在命令行参数中给出sample_MotionDetect.conf的具体路径，sample_MotionDetect会读取sample_MotionDetect.conf，完成参数解析。
然后按照参数运行测试。

从命令行启动sample_MotionDetect的指令：
    ./sample_MotionDetect -path /mnt/extsd/sample_MotionDetect.conf
    "-path /mnt/extsd/sample_MotionDetect.conf"指定了配置文件的路径。

测试参数的说明：
(1)encoder_count:指定每次测试编码帧数
(2)dev_num:指定VI Dev设备节点 
(3)src_pic_format:指定采集图像的格式
(4)src_width:指定camera采集的图像宽度
(5)src_height:指定camera采集的图像高度
(6)src_frame_rate:指定camera采集图像的帧率
(7)dst_encoder_type:指定编码格式
(8)dst_width:指定编码后图像的宽度
(9)dst_height:指定编码后图像的高度
(10)dst_frame_rate:指定编码帧率
(11)dst_bit_rate:指定编码码率   
(12)output_file_path:指定编码后视频文件的路径