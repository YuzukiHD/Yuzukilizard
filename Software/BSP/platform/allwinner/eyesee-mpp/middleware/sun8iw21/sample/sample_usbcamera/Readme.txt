
sample usbcamera
    此sample测试UVC、UAC复合设备。

参数说明：
    uvc_dev             uvc设备号
    vin_de              摄像头数据采集设备号
    enc_bit_rate        JPEG、H264模式编码码率
    enable_uac          是否启用测试uac功能
    uac_dev             uac设备号
    uac_sample_rate     音频采样率
    uac_chn_cnt         音频通道号
    uac_bit_width       音频位宽
    uac_ai_gain         采集音频音量

测试使用步骤

    uvc内核配置:
		CONFIG_USB_CONFIGFS_F_UVC=y

    uac内核配置:
        CONFIG_USB_CONFIGFS_F_UAC1=y
        CONFIG_SND_PROC_FS=y

	初始化uvc、uac复合设备:
		setusbconfig uvc,uac1

	确认生成节点:
		==>#查看新增vidoe节点
		ls /dev/video*
		/dev/video0 /dev/video1 /dev/video2 /dev/video3

		==>#确定新节点,是video4
		==>修改sample_uvcout.conf 配置文件

		vi sample/sample_uvcout/sample_uvcout.conf

		[parameter]
		uvc_dev = 2 #将新增节点的序号加入这里。

		cat /proc/asound/cards
		0 [sun8iw19codec  ]: sun8iw19-codec - sun8iw19-codec
		sun8iw19-codec
		1 [snddaudio0     ]: snddaudio0 - snddaudio0
		snddaudio0
		2 [UAC1Gadget     ]: UAC1_Gadget - UAC1_Gadget
		UAC1_Gadget 0

		enable_uac = 1
	uac_audio_card_name = "hw:UAC1Gadget"

		运行sample
			./sample_usbcamera -path /mnt/extsd/sample_usbcamera.conf

注:
	uvc、uac复合设备只能在uvc bulk模式下使用，sample已改为usb bulk模式。
