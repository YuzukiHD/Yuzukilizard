# sample分类

## 视频

* sample_driverVipp。演示直接调用linux内核驱动获取frame。
* sample_virvi。视频采集。
* sample_virvi2vo。演示视频采集预览。
* sample_virvi2vo_zoom。演示缩放。
* sample_vi_reset。演示mpi_vi组件的reset流程。
* sample_vin_isp_test。通过调用isp接口抓图的demo。
* sample_venc。视频编码。
* sample_venc2muxer。演示视频编码和封装mp4。
* sample_virvi2venc。演示采集到编码，支持测试GDC畸变矫正。
* sample_timelapse。演示缩时录影。
* sample_virvi2venc2muxer。演示采集到编码到封装mp4，支持测试GDC畸变矫正。
* sample_multi_vi2venc2muxer。演示多路编码。
* sample_rtsp。演示视频编码后的rtsp传输。
* sample_CodecParallel。演示同编同解。
* sample_vdec。视频解码。
* sample_demux2vdec。演示解封装和解码。
* sample_demux2vdec_saveFrame。从原始文件中分离出视频数据帧并解码生成yuv文件。
* sample_demux2vdec2vo。演示解码显示。

## 音频

* sample_ai
* sample_ao
* sample_aoSync。演示采用同步的方式send pcm frame。而sample_ao是采用异步的方式。
* sample_ao_resample_mixer。演示读取pcm数据，然后播放声音，从耳机口输出声音。
* sample_aec。回声消除，修改版。
* sample_aenc。音频编码。
* sample_ai2aenc。音频采集和编码。
* sample_ai2aenc2muxer。音频采集、编码和封装。
* sample_adec。音频解码。
* sample_demux2adec。从原始文件中分离出音频数据帧并解码。
* sample_demux2adec2ao。从原始文件中分离出音频数据帧并解码输出音频。

## 视频显示

* sample_vo。演示视频YUV预览。
* sample_UILayer。验证UILayer的格式。

## g2d

* sample_g2d。演示直接用G2D处理YUV文件做透明叠加、旋转和镜像、缩放等。
* sample_vi_g2d。演示从VI获取视频帧，用G2D做旋转、裁剪、缩放等处理后送VO显示。

## uvc和uac

* sample_uvc2vdec_vo。做主，mjpeg解码显示。
* sample_uvc2vdenc2vo。做主，mjpeg解码显示。
* sample_uvc2vo。做主，yuv显示。
* sample_uvc_vo。做主，yuv显示。
* sample_uvcout。做从，mjpeg编码输出。
* sample_uac。演示uac测试。

## 智能算法 demo

* sample_smartIPC_demo。演示人形检测+人脸检测+画框+RTSP功能。
* sample_smartPreview_demo。演示人形检测+人脸检测+画框+预览功能。

## 其他

* sample_glog。演示glog库的用法。
