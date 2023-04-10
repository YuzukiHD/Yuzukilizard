sample_venc测试流程：
	从yuv(sample中格式限定为yuv420p)原始数据文件xxx.yuv中读取视频帧，编码，将取得的编码往输出文件里面直接写。
	生成裸码流视频文件。如果是h264 or h265编码sample会自动在生成文件的开始加上spspps信息，其他格式则不加。

读取测试参数的流程：
	sample提供了sample_venc.conf，测试参数都写在该文件中。
	启动sample_venc时，在命令行参数中给出sample_venc.conf的具体路径，sample_venc会读取sample_venc.conf，完成参数解析。
	然后按照参数运行测试。
	从命令行启动sample_venc的指令：
	./sample_venc -path /mnt/extsd/sample_venc.conf
	"-path /mnt/extsd/sample_venc.conf"指定了测试参数配置文件的路径。

测试参数的说明：
src_file: 指定原始yuv文件的路径
src_width: 指定原始视频文件的宽度
src_height: 指定原始视频文件的高度
dst_file: 生成的裸码流视频文件路径
dst_width: 生成的裸码流视频文件的宽度
dst_height: 生成的裸码流视频文件的高度
src_pixfmt: 像素格式
encoder: 编码格式
profile: profile等级
framerate: 帧率，fps
bitrate: 码率，bps
rotate: 旋转角度
rc_mode: 码率控制模式
gop_mode: GOP模式
gop_size: GOP大小
product_mode: 产品模式
sensor_type: sensor类型
key_frame_interval: 关键帧间隔

## VBR/CBR ##
init_qp: 初始QP
min_i_qp: I帧最小QP
max_i_qp: I帧最大QP
min_p_qp: P帧最小QP
max_p_qp: P帧最大QP

## FixQp ##
i_qp: I帧QP
p_qp: P帧QP

## VBR ##
moving_th: MAD的阈值
quality: 静态P帧中的比特系数
p_bits_coef: 移动P帧中的比特系数
i_bits_coef: I帧中的比特系数

test_duration: sample一次测试时间，单位：s
