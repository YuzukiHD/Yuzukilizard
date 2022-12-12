sample_driverVipp：
    该sample演示直接调用linux内核驱动获取frame。按下ctrl+c，终止测试。
    每隔若干帧保存一帧到指定的目录。

读取测试参数的流程：
    sample只支持命令行模式输入参数。如果不输入参数，会提示输入。
    从命令行启动sample_driverVipp的指令：
    ./sample_driverVipp
    或
    ./sample_driverVipp 0 1920 1080 8 60 0 10 60 /mnt/extsd

测试参数的说明：
(1)video device: 0~3 (vipp0~vipp3)
(2)capture_width：指定camera采集的图像宽度
(3)capture_height：指定camera采集的图像高度
(4)pixel_format：指定camera采集的图像格式
(5)fps：指定camera采集的帧率
(6)test frame count：指定测试采集的frame总数，0表示无限。
(7)store count: 指定保存的图像数量。
(8)store interval: 指定保存图像的周期，即每n帧图像保存1帧。
(9)frame saving path: 指定保存图像的目录，该目录要确保存在。
