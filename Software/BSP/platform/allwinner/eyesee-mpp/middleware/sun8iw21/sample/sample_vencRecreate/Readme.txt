sample_vencRecreate测试流程：
          演示动态配置帧率和码率（应用场景：改善网络环境差时卡顿和花屏）。

读取测试参数的流程：
    sample提供了sample_vencRecreate.conf，测试参数都写在该文件中。
启动sample_vencRecreate时，在命令行参数中给出sample_vencRecreate.conf的具体路径，sample_vencRecreate会读取sample_vencRecreate.conf，完成参数解析。
然后按照参数运行测试。

从命令行启动sample_vencRecreate的指令：
    ./sample_vencRecreate -path /home/sample_vencRecreate.conf
    "-path /home/sample_vencRecreate.conf"指定了测试参数配置文件的路径。

测试参数的说明：
(1)auto_test_count:指定自动测试次数
(2)encoder_count:指定每次测试编码帧数
(3)dev_num:指定VI Dev设备节点 
(4)src_width:指定camera采集的图像宽度
(5)src_height:指定camera采集的图像高度
(6)src_frame_rate:指定camera采集图像的帧率
(7)dest_encoder_type:指定编码格式
(8)dest_width:指定编码后图像的宽度
(9)dest_height:指定编码后图像的高度
(10)dest_frame_rate:指定编码帧率
(11)dest_bit_rate:指定编码码率   
(12)dest_pic_format:指定编码后图像的格式
(13)output_file_path:指定编码后视频文件的路径