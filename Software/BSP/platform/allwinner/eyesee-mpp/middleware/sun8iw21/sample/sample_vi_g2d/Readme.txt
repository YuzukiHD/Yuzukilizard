sample_vi_g2d测试流程：
    该sample从mpi_vi组件获取视频帧, 
    调用g2d驱动做旋转、剪切、缩放等处理，处理后的图像送mpi_vo显示，也可保存为原始图片供分析。
    sample可设定测试时间，到达测试时间或手动按ctrl-c，终止测试。

    关于g2d旋转、剪切、缩放，可以通过配置
    (5)src_width,src_height：指定camera采集的源图宽度和高度
    (6)src_rect_x,src_rect_y,src_rect_w,src_rect_h：指定g2d处理的源图区域
    (7)dst_rotate：指定g2d旋转角度
    (8)dst_width,dst_height：指定g2d处理后的目标buffer的宽度和高度
    (9)dst_rect_x,dst_rect_y,dst_rect_w,dst_rect_h：指定g2d处理的目标图区域
    来组合实现。

允许不指定conf文件：
./sample_vi_g2d
按默认参数运行。

指定conf文件：
./sample_vi_g2d -path /mnt/extsd/sample_vi_g2d.conf
按conf文件的参数运行。

测试参数的说明：
(1)dev_num：指定VI Dev设备节点 
(2)frame_rate:指定camera采集图像的帧率
(3)pic_format:指定camera采集图像像素格式
(4)drop_frame_num：指定vi组件丢弃的帧数
(5)src_width,src_height：指定camera采集的源图宽度和高度
(6)src_rect_x,src_rect_y,src_rect_w,src_rect_h：指定g2d处理的源图区域
(7)dst_rotate：指定g2d旋转角度
(8)dst_width,dst_height：指定g2d处理后的目标buffer的宽度和高度
(9)dst_rect_x,dst_rect_y,dst_rect_w,dst_rect_h：指定g2d处理的目标图区域
(10)dst_store_count：指定保存的图片数量
(11)dst_store_interval：指定保存图片的帧间隔
(12)store_dir：指定保存目录，目录必须已经存在
(13)display_flag：是否VO显示
(14)display_x,display_y,display_w,display_h：指定VO的显示区域
(15)test_duration：指定测试时间，单位：秒。0表示无限时长。

