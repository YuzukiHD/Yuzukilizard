sample_virvi测试流程：
         演示VI采集YUV数据。

如果想指定conf文件，则如下操作：
读取测试参数的流程：
         sample提供了sample_virvi.conf，测试参数都写在该文件中。
启动sample_virvi时，在命令行参数中给出sample_virvi.conf的具体路径，sample_virvi会读取sample_virvi.conf，完成参数解析。
然后按照参数运行测试。
         从命令行启动sample_virvi的指令：
        ./sample_virvi -path ./sample_virvi.conf
        "-path ./sample_virvi.conf"指定了测试参数配置文件的路径。


测试参数的说明：
dev_num: 指定VI Dev设备节点
pic_width: 指定camera采集的图像宽度
pic_height: 指定camera采集的图像高度
frame_rate: 指定camera采集图像的帧率
pic_format: 指定camera采集图像像素格式
color_space：指定颜色空间类型
enable_wdr_mode: 是否启用WDR模式
drop_frm_num：指定VIPP丢帧的帧数

save_pic_dev: 指定保存视频帧的VI Dev设备节点
yuv_frm_count：指定存储到一个文件中的YUV帧数
yuv_file：指定单个YUV文件的路径

raw_store_count: 保存yuv数据的帧数
raw_store_interval: 保存yuv数据的帧间隔，单位:帧
store_dir: yuv数据存储路径

save_pic_buffer_len：写卡缓存buffer长度
save_pic_buffer_num：写卡缓存buffer个数

test_duration: 测试时长

注：
    1. 支持测试录制格式
        nv21(yvu420sp) yu12(yuv420p) yv12 nv12
        LBC: aw_afbc, aw_lbc_1_0x, aw_lbc_1_5x, aw_lbc_2_0x, aw_lbc_2_5x
        RAW: srggb10
        AW1886 只支持 vipp0与vipp1使用LBC格式

    2. 支持测试录制色彩空间
        jpeg, rec709, rec709_part_range

    3. 如不启用一组配置，将dev_num设置为-1或者将pic_width和pic_height设置为0
       如不保存vi视频帧，将
       save_pic_dev设为-1或者将save_pic_buffers_len、save_pic_buffer_num设置为0

2022.01.10更新
	1. 支持最大四路vipp-vir0测试
    2. 支持测试WDR模式
	3. 分离写卡为一个新的线程
