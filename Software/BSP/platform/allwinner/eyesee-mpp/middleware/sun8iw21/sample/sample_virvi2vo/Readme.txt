sample_virvi2vo测试流程：
    该sample测试mpi_vi和mpi_vo组件的绑定组合。创建mpi_vi和mpi_vo，将它们绑定，再分别启动。mpi_vi采集图像，直接传输给mpi_vo显示。
    到达测试时间后，分别停止运行并销毁。也可以手动输入"99"然后enter，再按下ctrl+c，终止测试。

读取测试参数的流程：
    sample提供了sample_virvi2vo.conf，测试参数都写在该文件中。
    启动sample_virvi2vo时，在命令行参数中给出sample_virvi2vo.conf的具体路径，sample_virvi2vo会读取sample_virvi2vo.conf，完成参数解析。
    然后按照参数运行测试。
    从命令行启动sample_virvi2vo的指令：
    ./sample_virvi2vo -path /mnt/extsd/sample_virvi2vo.conf
    "-path /mnt/extsd/sample_virvi2vo.conf"指定了测试参数配置文件的路径。

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



双目预览的配置：
capture_width = 720
capture_height = 640
display_x = 0
display_y = 640
display_width = 720
display_height = 640
layer_num = 0
isp_dev = 0
vipp_dev = 0

capture_width2 = 720
capture_height2 = 640
display_x2 = 0
display_y2 = 0
display_width2 = 720
display_height2 = 640
layer_num2 = 4
isp_dev2 = 1
vipp_dev2 = 1
