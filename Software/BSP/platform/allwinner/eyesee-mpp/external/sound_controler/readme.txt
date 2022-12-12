1.txz 语音识别库V.1
libtxzEngine.so
libuvdz.so
不需要额外激活步骤，直接使用。
支持的命令词： { "小志开始录像", "小志停止录像", "小志拍照", "小志连拍", "小志关机" }
pcm_sample_rate = 16000
pcm_bit_width = 16

2.txz 语音识别库V.2
vvvvvery important激活时用的电脑，必须能访问外网和USB
同一份code，一个是静态库 一个是动态库
libactivate.a
libtxzEngineUSB.so
需要激活！
激活步骤：
	* 1. 机器上电，连接串口
	* 2. 机器选择大容量存储模式，电脑运行“全志激活.exe”
	* 3. 如果激活成功，“全志激活.exe”会提示success
	* 4. 机器退出大容量存储模式，进入充电模式，运行demo程序！
支持的命令词：显示前录 显示后录 打开屏幕 关闭屏幕 我要拍照 紧急录像 打开录音 关闭录音 打开WIFI 关闭WIFI
