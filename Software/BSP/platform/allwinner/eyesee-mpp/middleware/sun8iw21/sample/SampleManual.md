# sample分类

## 视频

* sample_driverVipp。演示直接调用linux内核驱动获取frame。
* sample_virvi。视频采集。
* sample_virvi2vo。演示视频采集预览。
* sample_virvi2vo_zoom。演示缩放。
* sample_vi_reset。演示mpi_vi组件的reset流程。
* sample_isposd。测试isp相关。
* sample_vin_isp_test。通过调用isp接口抓图的demo。
* sample_region。测试osd。
* sample_venc。视频编码。
* sample_venc2muxer。演示视频编码和封装mp4。
* sample_virvi2venc。演示采集到编码。
* sample_timelapse。演示缩时录影。
* sample_virvi2venc2muxer。演示采集到编码到封装mp4。
* sample_multi_vi2venc2muxer。演示多路编码。
* sample_rtsp。演示视频编码后的rtsp传输。
* sample_CodecParallel。演示同编同解。
* sample_vdec。视频解码。
* sample_demux2vdec。演示解封装和解码。
* sample_demux2vdec_saveFrame。从原始文件中分离出视频数据帧并解码生成yuv文件。
* sample_demux2vdec2vo。演示解码显示。
* sample_vencQpMap。演示视频编码QPMAP模式。
* sample_OnlineVenc。在线编码。
* sample_vencGdcZoom。编码GDC数字变焦。
* sample_takePicture。演示单拍和连拍。
* sample_recorder。演示四路录制编码封装或者预览显示。

## 音频

* sample_ai
* sample_ao
* sample_aoSync。演示采用同步的方式send pcm frame。而sample_ao是采用异步的方式。
* sample_ao_resample_mixer。演示读取pcm数据，然后播放声音，从耳机口输出声音。
* sample_ao2ai_aec。回声消除，初版
* sample_ao2ai_aec_rate_mixer。回声消除，初版基础上测试打开音频输出重采样和混音功能后，回声消除是否正常。
* sample_aec。回声消除，修改版。
* sample_aenc。音频编码。
* sample_ai2aenc。音频采集和编码。
* sample_ai2aenc2muxer。音频采集、编码和封装。
* sample_select。演示多路音频编码，用select()方式获取编码码流。
* sample_adec。音频解码。
* sample_demux2adec。从原始文件中分离出音频数据帧并解码。
* sample_demux2adec2ao。从原始文件中分离出音频数据帧并解码输出音频。

## ISE和EIS

* sample_fish。演示单目鱼眼功能。
* sample_virvi2fish2venc。演示鱼眼图像畸形校正后做编码。
* sample_virvi2fish2vo。演示鱼眼图像畸形校正后预览。
* sample_virvi2eis2venc。演示防抖功能。

## 视频显示

* sample_vo。演示视频YUV预览。
* sample_UILayer。验证UILayer的格式。

## g2d

* sample_g2d。演示直接用G2D处理YUV文件做透明叠加、旋转和镜像、缩放等。
* sample_vi_g2d。演示从VI获取视频帧，用G2D做旋转、裁剪、缩放等处理后送VO显示。

## CE

* sample_twinchn_virvi2venc2ce。两路ce加解密测试。
* sample_virvi2venc2ce。单路ce加解密测试。

## uvc和uac

* sample_uvc2vdec_vo。做主，mjpeg解码显示。
* sample_uvc2vdenc2vo。做主，mjpeg解码显示。
* sample_uvc2vo。做主，yuv显示。
* sample_uvc_vo。做主，yuv显示。
* sample_uvcout。做从，mjpeg编码输出。
* sample_uac。演示uac测试。
* sample_usbcamera。测试UVC、UAC复合设备。

## 多媒体文件

* sample_demux。解复用mp4文件。
* sample_file_repair。修复mp4文件。

## AI demo

* sample_odet_demo。目标检测演示示例。
* sample_RegionDetect。演示区域检测。

## 其他

* sample_glog。演示glog库的用法。
* sample_hello。演示MPP helloworld程序。
* sample_pthread_cancel。测试pthread_cancel()
* sample_motor。电机测试。
* sample_sound_controler，语音识别。
* sample_ai_Demo_0.9.2。智能算子demo
* sample_ai_Demo_0.9.6。智能算子demo
* sample_MotionDetect。移动侦测。
* sample_PersonDetect。人形检测。
* sample_directIORead。演示使用directIO方式读文件。

## deprecated

准备删除以下sample：

* sample_ai2ao。引起啸叫，无意义。sample_aec会测试ai和ao同时工作的场景。
* sample_ao2ai。ao和ai的绑定是为了回声消除，而现在回声消除在底层完成，和mpp组件没有关系。mpi_ao和mpi_ai的绑定已经废除。
* sample_face_detect。已废弃，sample_ai_Demo包含了人脸检测。