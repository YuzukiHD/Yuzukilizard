sample_smartIPC_demo：
	演示智能IPC的场景，通过RTSP+NPU方式查看编码码流的场景。

测试方法：
	【PHY】
	1. 配置网络IP：ifconfig eth0 169.254.225.12 netmask 255.255.0.0
	2. 执行测试程序：/mnt/extsd/sample_smartIPC_demo -path /mnt/extsd/sample_smartIPC_demo.conf
	3. 在PC端使用VLC软件的网络串流连接：rtsp://169.254.225.12:8554/ch0
	
	【WLAN】
	1. 配置网络IP：ifconfig wlan0 169.254.225.12 netmask 255.255.0.0
	2. 执行测试程序：/mnt/extsd/sample_smartIPC_demo -path /mnt/extsd/sample_smartIPC_demo.conf
	3. 在PC端使用VLC软件的网络串流连接：rtsp://169.254.225.12:8554/ch0

读取测试参数的流程：
    sample提供了 sample_smartIPC_demo.conf，测试参数都写在该文件中。
启动 sample_smartIPC_demo 时，在命令行参数中给出 sample_smartIPC_demo.conf 的具体路径，sample_smartIPC_demo 会读取 sample_smartIPC_demo.conf，完成参数解析。
然后按照参数运行测试。

从命令行启动 sample_smartIPC_demo 的指令：
    ./sample_smartIPC_demo -path /home/sample_smartIPC_demo.conf
    "-path /home/sample_smartIPC_demo.conf"指定了测试参数配置文件的路径。


配置方法：
	1. 单目IPC
	main_isp = 0
	main_vipp = 0

	sub_isp = 0
	sub_vipp = 4

	nna_isp = 0
	nna_vipp = 8

	2. 双目IPC
	main_isp = 0
	main_vipp = 0

	sub_isp = 1
	sub_vipp = 1

	nna_isp = 0
	nna_vipp = 4
