sample_OnlineVenc测试流程：
	该sample演示IPC在线编码使用场景：主码流在线编码、子码流离线编码。
    针对主码流和子码流分别创建mpi_vi和mpi_venc，将它们绑定，再分别启动。mpi_vi采集图像，直接传输给mpi_venc进行编码。

读取测试参数的流程：
    sample提供了配置文件sample_OnlineVenc.conf，测试参数都写在该文件中。

从命令行启动sample_OnlineVenc的指令：
    ./sample_OnlineVenc -path /mnt/extsd/sample_OnlineVenc.conf

测试参数的说明：
main_vipp: 主码流VIPP号
main_viChn：主码流VIPP虚拟通道号
main_src_width: 主VIPP输出图像宽度
main_src_height: 主VIPP输出图像高度
main_pixel_format: 主VIPP输出图像格式。nv21, lbc25等。
main_online_en：主码流开启/关闭在线编码
main_online_share_buf_num：主码流在线编码时，配置单/双buffer。
main_wdr_enable：主码流WDR使能开关。
main_vi_buf_num：配置主码流VI BUFFER个数。

sub_vipp: 子码流VIPP号
sub_viChn：子码流VIPP虚拟通道号
sub_src_width: 子VIPP输出图像宽度
sub_src_height: 子VIPP输出图像高度
sub_pixel_format: 子VIPP输出图像格式。nv21, lbc25等。
sub_wdr_enable：子VIPP WDR使能开关。
sub_vi_buf_num：子VIPP配置VI BUFFER个数。

src_frame_rate: 采集帧率

main_venc_chn：主码流编码通道号
main_encode_type: 主码流编码类型, H.265, H.264等。
main_encode_width: 主码流的编码目标宽度
main_encode_height: 主码流的编码目标高度
main_encode_frame_rate: 主码流的编码目标帧率
main_encode_bitrate: 主码流的编码目标码率, bit/s
main_file_path: 保存主码流的文件路径，空表示不保存主码流。

sub_venc_chn：子码流编码通道号
sub_encode_type: 子码流编码类型, H.265, H.264等。
sub_encode_width: 子码流的编码目标宽度
sub_encode_height: 子码流的编码目标高度
sub_encode_frame_rate: 子码流的编码目标帧率
sub_encode_bitrate: 子码流的编码目标码率, bit/s
sub_file_path: 保存子码流的文件路径，空表示不保存子码流。

test_duration: 测试时间，0表示无限。单位：秒。
