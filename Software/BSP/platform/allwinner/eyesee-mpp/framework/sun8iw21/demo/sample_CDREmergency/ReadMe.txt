############ sample_RecordDemo支持三种模式 ############
模式1：分辨率1080P@60fps + 音频
1、启动一路Recorder，以20s一个文件的规格录制5个文件后结束
2、在第10s和第50s（两次碰撞时间可以配置）分别启动两次碰撞录像，录制前后15s的视频

模式2：分辨率1080P@1fps
（在1fps编码问题解决之前，可以使用5fps测试。不需要音频）
1、启动一路Recorder，以20s一个文件个规则录制5个文件后结束，所有文件保持1fps
2、在第10s和第50s（两次碰撞时间可以配置）分别启动两次碰撞录像，录制前后15s的视频。发生碰撞之后，VENC从1fps恢复60fps（帧率和I帧间隔同时改变），所以录制的碰撞录像是前半部分1fps（均为I帧），后半部分60fps
模式2已不支持，因为muxer不支持选帧写文件。
模式3：分辨率1080P@60fps
1、启动一路Recorder，以20s一个文件个规则录制5个文件后结束，所有文件保持1fps
2、在第10s和第50s（两次碰撞时间可以配置）分别启动两次碰撞录像，录制前后15s的视频。发生碰撞之后，VENC为60fps（帧率和I帧间隔同时改变），所以录制的碰撞录像是60fps
模式3已不支持，因为muxer不支持选帧写文件。


############ 三种模式配置方式 ############
1、通过宏定义 SAMPLE_RECODER_MODE 配置使用三种模式
#define SAMPLE_RECODER_MODE SAMPLE_RECODER_3_MODE   // set test mode


############ 碰撞录像配置 ############
1、修改 sample_RecordDemo.conf
frist_emergency_time = 0  // 第一次碰撞的发生时间，配置为0表示不发生碰撞
secend_emergency_time = 0  // 第二次碰撞的发生时间


############ 普通录像配置 ############
1、修改 sample_RecordDemo.conf
folder = "/mnt/extsd/sample_RecordDemo"  // 保存路径
count = 3                                // 保存文件数目
duration = 20	#unit:s                  // 每个录像文件的时长控制

