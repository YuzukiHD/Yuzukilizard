sample_g2d 通过直接处理image图片来演示g2d模块常用功能的使用方式：
g2d 模块功能的选择主要通过参数的组合来实现，主要的功能有透明叠加、旋转和镜像、
缩放等功能，主要是通过配置g2d_mod这个参数。g2d模块支持最大2K输入输出。


1) g2d_mod = 0 旋转功能
	配置conf文件中的dst_rotate 配置项，设置要旋转的角度，支持90、180、270旋转.
	g2d支持设置镜像翻转，设置dst_filp为"H"、"V"分别代表水平、垂直翻转，如果不需要翻转则给一个空字符即可。
	旋转和镜像翻转不支持同时设置，需要进行两次调用。

	Note： 此时 dst_width 及 dst_height配置项要分别与src_width及src_height一致，否则
	会同时启动scale功能，但此时会产生错误结果(g2d模块不能同时做rotation及scaling).
翻转90度示例参数配置如下：
[parameter]
	pic_format = nv21
	dst_format = nv21
	src_width = 1920
	src_height = 1080
	src_rect_x = 0
	src_rect_y = 0
	src_rect_w = 1920
	src_rect_h = 1080
	dst_rotate = 90 #0, 90, 180, 270, clock-wise.
	dst_flip   = " "   #H=horizontal flip, V=vertical flip, N=none
	dst_width = 1080
	dst_height = 1920
	dst_rect_x = 0
	dst_rect_y = 0
	dst_rect_w = 1080
	dst_rect_h = 1920

	yuv_src_file = "/mnt/extsd/board_1920x1080.nv21"
	yuv_dst_file = "/mnt/extsd/dst.yuv"

	g2d_mod = 0

水平翻转示例参数配置如下：                                                      
[parameter]                                                                     
    pic_format = nv21                                                       
    dst_format = nv21                                                       
    src_width = 1920                                                        
    src_height = 1080                                                       
    src_rect_x = 0                                                          
    src_rect_y = 0                                                          
    src_rect_w = 1920                                                       
    src_rect_h = 1080                                                       
    dst_rotate = -1 #0, 90, 180, 270, clock-wise.                           
    dst_flip   = "H"   #H=horizontal flip, V=vertical flip, N=none          
    dst_width = 1920                                                        
    dst_height = 1080                                                       
    dst_rect_x = 0                                                          
    dst_rect_y = 0                                                          
    dst_rect_w = 1920                                                       
    dst_rect_h = 1080                                                       
   																		  
    yuv_src_file = "/mnt/extsd/board_1920x1080.nv21"                        
    yuv_dst_file = "/mnt/extsd/dst.yuv"                                     
   																		  
    g2d_mod = 0                  

	此外g2d_mod = 0 还可以设置cut功能用来截取部分src内容，如下配置是从1080p图片中以(10,10)这个为起点截取一块720p区域：

	pic_format = nv21
	dst_format = nv21
	src_width = 1920
	src_height = 1080
	src_rect_x = 10
	src_rect_y = 10
	src_rect_w = 1280
	src_rect_h = 720
	dst_rotate = 0 #0, 90, 180, 270, clock-wise.
	dst_flip   = " "   #H=horizontal flip, V=vertical flip
	dst_width = 1280
	dst_height = 720
	dst_rect_x = 0
	dst_rect_y = 0
	dst_rect_w = 1280
	dst_rect_h = 720

	yuv_src_file = "/mnt/extsd/board_1920x1080.nv21"
	yuv_dst_file = "/mnt/extsd/dst.yuv"

	g2d_mod = 0

2) g2d_mod = 1 缩放功能
	配置conf文件中的dst_rotate为0（disable rotation）,
	配置dst_width dst_height目标大小，缩放比例为1/16-32x

[parameter]
	pic_format = nv21
	dst_format = nv21
	src_width = 1920
	src_height = 1080
	src_rect_x = 0
	src_rect_y = 0
	src_rect_w = 1920
	src_rect_h = 1080
	dst_rotate = 0 #0, 90, 180, 270, clock-wise.
	dst_flip   = " "   #H=horizontal flip, V=vertical flip
	dst_width = 1280
	dst_height = 720
	dst_rect_x = 0
	dst_rect_y = 0
	dst_rect_w = 1280
	dst_rect_h = 720

	yuv_src_file = "/mnt/extsd/board_1920x1080.nv21"
	yuv_dst_file = "/mnt/extsd/dst.yuv"

	g2d_mod = 1

3) g2d_mod = 2 格式转换功能，实现rgb和yuv格式转换

[parameter]
	pic_format = nv21
	dst_format = rgb888
	src_width = 1920
	src_height = 1080
	src_rect_x = 0
	src_rect_y = 0
	src_rect_w = 1920
	src_rect_h = 1080
	dst_rotate = 0 #0, 90, 180, 270, clock-wise.
	dst_flip   = " "   #H=horizontal flip, V=vertical flip
	dst_width = 1920
	dst_height = 1080
	dst_rect_x = 0
	dst_rect_y = 0
	dst_rect_w = 1920
	dst_rect_h = 1080

	yuv_src_file = "/mnt/extsd/board_1920x1080.nv21"
	yuv_dst_file = "/mnt/extsd/dst.rgb"

	g2d_mod = 2

4) g2d_mod = 3 透明叠加功能，实现两个rgb图片叠加，sample默认会在/mnt/extsd/目录下上成一个叠加后的图片g2d_bld_test.rgb

[parameter]
	pic_format = rgb888
	dst_format = rgb888
	src_width = 64
	src_height = 64
	src_rect_x = 0
	src_rect_y = 0
	src_rect_w = 64
	src_rect_h = 64
	dst_rotate = 0 #0, 90, 180, 270, clock-wise.
	dst_flip   = " "   #H=horizontal flip, V=vertical flip
	dst_width = 800
	dst_height = 480
	dst_rect_x = 80
	dst_rect_y = 90
	dst_rect_w = 800
	dst_rect_h = 480


	yuv_src_file = "/mnt/extsd/64x64_rgb888_color.rgb"
	yuv_dst_file = "mnt/extsd800x480_rgb888_color.rgb"

	g2d_mod = 3
	g2d_bld_mode = 0

5) g2d_mod = 4 缩放批处理功能，实现对图像进行批处理缩放功能，图像格式可以是rdb或者yuv，示例参数如下：

[parameter]
	pic_format = nv21
	dst_format = nv21
	src_width = 1920
	src_height = 1080
	src_rect_x = 0
	src_rect_y = 0
	src_rect_w = 1920
	src_rect_h = 1080
	dst_rotate = 0 #0, 90, 180, 270, clock-wise.
	dst_flip   = " "   #H=horizontal flip, V=vertical flip
	dst_width = 1280
	dst_height = 720
	dst_rect_x = 0
	dst_rect_y = 0
	dst_rect_w = 1280
	dst_rect_h = 720


	yuv_src_file = "/mnt/extsd/board_1920x1080.nv21"
	yuv_dst_file = "/mnt/extsd/800x480_rgb888_color.rgb"

	g2d_mod = 4
	g2d_bld_mode = 0

批处理缩放后的图像默认保存在mnt/extsd目录下，实现的函数位于saveDstFrm中，代码默认是关闭的。
