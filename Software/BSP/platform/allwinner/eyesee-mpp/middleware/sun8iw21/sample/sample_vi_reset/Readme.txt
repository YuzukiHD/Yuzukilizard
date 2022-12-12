sample_vi_reset测试流程：
         该sample演示mpi_vi组件的reset流程。vi组件运行过程中停止，销毁，再重新创建运行。

本sample不提供参数配置。
         从命令行启动sample_vi_reset的指令：
        ./sample_vi_reset

测试参数的说明：
LoadSampleViResetConfig()中配置参数：
    mTestCount:循环测试reset的次数
    mFrameCountStep1:采集多少帧之后开始stop
    mbRunIsp：是否运行ISP
    mIspDev：ISP编号
    [mVippStart, mVippEnd]：测试的VIPP范围，同时打开范围内的VIPP采集，验证vipp通道并发的情况。
    mPicWidth：采集图像的宽度
    mPicHeight：采集图像的宽度
    mFrameRate：采集帧率
    mPicFormat：图像格式