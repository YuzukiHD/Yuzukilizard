sample_virvi2vo_zoom 测试流程：
    演示直接使用VI 和VO 实现4K 图像的缩放。
    mpi_vi 与mpi_vo 非绑定模式下，获取mpi_vi 采集的图像，修改起始坐标和区域大小后，再送给mpi_vo 去显示。通过修改起始坐标和区域大小，来达到缩放的效果。
    到达测试时间后，分别停止运行并销毁。也可以手动按下ctrl+c，终止测试。

读取测试参数的流程：
    sample提供了sample_virvi2vo_zoom.conf，测试参数都写在该文件中。
    启动sample_virvi2vo_zoom时，在命令行参数中给出sample_virvi2vo_zoom.conf的具体路径，sample_virvi2vo_zoom会读取sample_virvi2vo_zoom.conf，完成参数解析。
    然后按照参数运行测试。
    从命令行启动sample_virvi2vo_zoom的指令：
    ./sample_virvi2vo_zoom -path /mnt/extsd/sample_virvi2vo_zoom.conf
    "-path /mnt/extsd/sample_virvi2vo_zoom.conf"指定了测试参数配置文件的路径。

测试参数的说明：
(1)capture_width：指定camera采集的图像宽度
(2)capture_height：指定camera采集的图像高度
(3)display_width：指定输出图像宽度
(4)display_height：指定输出图像高度
(5)dev_num：指定VIPP设备号
(6)disp_type：指定显示设备（hdmi, lcd, cvbs）
(7)pic_format：指定camera采集的图像格式
(8)frame_rate：指定camera采集的帧率
(9)test_duration：指定测试时间
(10)zoom_speed：缩放的速度，一般建议设置为10。值越小，速度越快。
(11)zoom_max_cnt：缩放的次数。