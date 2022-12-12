Test sample of UAC, make by wuguanling

1. Need to enable [CONFIG_USB_CONFIGFS_F_UAC1] on kernel config.
   Need to enable [CONFIG_SND_PROC_FS] on kernel config.

2.running command to enable uac function of USB:
./setusbconfig-configfs uac1

3.Now will creat new audio [AC Interface] in PC , board will creat a new audio card at the same time .
check the audio card:
------------------
cat /proc/asound/cards
 0 [sun8iw19codec  ]: sun8iw19-codec - sun8iw19-codec
      sun8iw19-codec
 1 [snddaudio0     ]: snddaudio0 - snddaudio0
      snddaudio0
 2 [UAC1Gadget     ]: UAC1_Gadget - UAC1_Gadget
     UAC1_Gadget 0
------------------

The "2 [UAC1Gadget     ]" is uac audio card,
you need copy it's name to modify {uac_audio_card_name = "hw:UAC1Gadget"} on ./sample_uac.conf

4. running sample : [mic --> uac --> PC]
./sample_uac -path ./sample_uac.conf


5.Using Audio tools of to capture uac pcm in pc.
