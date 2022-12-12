sample_vencQpMap：
    演示调用VENC MPI 接口，测试视频编码QpMap模式。

说明：若想直接使用编码库接口开发QpMap功能，请参考sample_QpMap（默认不编译）

测试流程：
    从yuv原始数据文件xxx.yuv中读取视频帧，编码，将取得的编码往输出文件里面直接写，生成裸码流视频文件。
    开启QpMap模式下，在编码完一帧后到编码下一帧前，获取上一帧（刚已编码完成）的编码统计信息，用户需要自己处理这个信息，然后设定下一帧（即将送去编码）的编码方式（QP、skip和en等）。
    如果是H264或H265编码sample会自动在生成文件的开始加上spspps信息，其他格式则不加。QpMap模式只有H264和H265两种编码格式支持。
    到达测试时间后，分别停止运行并销毁。也可以按下ctrl+c，终止测试。

读取测试参数的流程：
    sample提供了sample_vencQpMap.conf，测试参数都写在该文件中。
    启动sample_vencQpMap 时，在命令行参数中给出sample_vencQpMap.conf的具体路径，sample_vencQpMap会读取sample_vencQpMap.conf，完成参数解析。
    然后按照参数运行测试。
    从命令行启动sample_vencQpMap 的指令：
    ./sample_vencQpMap -path /mnt/extsd/sample_vencQpMap.conf
    "-path /mnt/extsd/sample_vencQpMap.conf" 指定了测试参数配置文件的路径。

测试参数的说明：

test_duration：指定测试时间

src_file：指定原始yuv文件的路径
src_width：指定原始视频文件的宽度
src_height：指定原始视频文件的高度
src_pixfmt：指定原始视频图像的像素格式

dst_file：指定生成的裸码流视频文件路径
dest_width：指定视频编码输出图像的宽度
dest_height：指定视频编码输出图像的高度
dest_pixfmt：指定视频编码输出图像的像素格式

encoder：指定视频编码的类型
profile：指定视频编码质量等级
framerate：指定视频编码的帧率
bitrate：指定视频编码的码率
