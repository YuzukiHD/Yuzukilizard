
V2.
# 全志v535 USB激活方案
vvvvvery important激活时用的电脑，必须能访问外网和USB

## 流程

* 1. 连接串口线与USB,插入带有最终生成的可执行文件(a.out)的sd卡
* 2. 将可执行文件(a.out)复制到板子文件夹/app/中,运行./a.out.
* 3. 在电脑端打开check.exe程序, 板子存储模式切换为大容量存储.电脑端处理完毕会打印ok
* 4. 拔usb再插入,可将板子存储模式重新切换回去激活成功



## 文件

* libactivate.a : 同行者编译好的激活静态库
* a.out : 可执行文件
* test.c :  激活接口
* check.exe : window上激活脚本
* libtxzEngineUSB.so : 同行者编译好的激活动态库

支持的语音：
显示前录
显示后录
打开屏幕
关闭屏幕
我要拍照
紧急录像
打开录音
关闭录音
打开WIFI
关闭WIFI
