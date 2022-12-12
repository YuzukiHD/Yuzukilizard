sample_takePicture测试流程：
         演示拍照功能。支持通过配置开启单拍和连拍测试。

如果想指定conf文件，则如下操作：
读取测试参数的流程：
         sample提供了sample_takePicture.conf，测试参数都写在该文件中。
启动sample_takePicture时，在命令行参数中给出sample_takePicture.conf的具体路径，sample_takePicture会读取sample_takePicture.conf，完成参数解析。
然后按照参数运行测试。
         从命令行启动sample_takePicture的指令：
        ./sample_takePicture -path ./sample_takePicture.conf
        "-path ./sample_takePicture.conf"指定了测试参数配置文件的路径。


测试参数的说明：
auto_test_count: 指定自动测试次数
get_frame_count: 指定每次测试获取图像的次数
dev_num: 指定VI Dev设备节点 
src_width: 指定camera采集的图像宽度
src_height: 指定camera采集的图像高度
frame_rate: 指定camera采集图像的帧率
src_format: 指定camera采集图像像素格式
color_space：指定颜色空间类型
drop_frm_num：指定VIPP丢帧的帧数

take_picture_mode：拍照模式（0：不拍，1：单拍，2：连拍）
take_picture_num：拍照的图片张数。
take_picture_interval：拍照时间间隔，单位:ms
jpeg_width: jpeg拍照图片的宽度
jpeg_height: jpeg拍照图片的高度
store_dir: jpeg拍照文件的存储路径（单拍：图片编号pic[n][0]，连拍：图片编号pic[0][n]）
