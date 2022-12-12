sample_PersonDetect测试流程：
    该sample演示使用人形检测库，将检测的人形区域在主码流和子码流上分别画框、编码。
    针对主码流和子码流分别创建mpi_vi和mpi_venc，将它们绑定，再分别启动。mpi_vi采集图像，直接传输给mpi_venc进行编码。
    子码流的mpi_vi再开一路虚通道用于人形检测，再开一路虚通道进行预览.

读取测试参数的流程：
    sample提供了配置文件sample_PersonDetect.conf，测试参数都写在该文件中。

从命令行启动sample_PersonDetect的指令：
    ./sample_PersonDetect -path /mnt/extsd/sample_PersonDetect.conf

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

main_vipp: 主码流VIPP号
main_src_width: 主VIPP输出图像宽度
main_src_height: 主VIPP输出图像高度
main_pixel_format: 主VIPP输出图像格式。nv21, lbc25等。
sub_vipp: 子码流VIPP号
sub_src_width: 子VIPP输出图像宽度
sub_src_height: 子VIPP输出图像高度
sub_pixel_format: 子VIPP输出图像格式。nv21, lbc25等。
src_frame_rate: 采集帧率

orl_color: 指定画人形框的颜色
enable_main_vipp_orl: 是否在主VIPP画人形框
enable_sub_vipp_orl: 是否在子VIPP画人形框

preview_vipp：用于预览的vipp通道号
preview_width: 预览的显示宽度，0表示不预览
preview_height: 预览的显示高度，0表示不预览

main_encode_type: 主码流编码类型, H.265, H.264等。
main_encode_width: 主码流的编码目标宽度
main_encode_height: 主码流的编码目标高度
main_encode_frame_rate: 主码流的编码目标帧率
main_encode_bitrate: 主码流的编码目标码率, bit/s
main_file_path: 保存主码流的文件路径，空表示不保存主码流。
sub_encode_type: 子码流编码类型, H.265, H.264等。
sub_encode_width: 子码流的编码目标宽度
sub_encode_height: 子码流的编码目标高度
sub_encode_frame_rate: 子码流的编码目标帧率
sub_encode_bitrate: 子码流的编码目标码率, bit/s
sub_file_path: 保存子码流的文件路径，空表示不保存子码流。

test_duration: 测试时间，0表示无限。单位：秒。

