# Yuzukilizard
Yuzukilizard is a Small Heterogeneous, AI Powered Dev Board

Mirror: [https://gitee.com/GloomyGhost/Yuzukilizard](https://gitee.com/GloomyGhost/Yuzukilizard)

![@7OI1656AFCA6ZEPBX73Z`8](https://user-images.githubusercontent.com/12003087/204966483-ad38dac5-753b-414b-ac92-19cfefb8ae4e.jpg)

## Features
![image](https://user-images.githubusercontent.com/12003087/205477494-dda777c6-f7a6-48e2-b17c-c5807ccb7f92.png)

- Cortex-A7 Core@900MHz + RISC-V E907GC@600MHz + 0.5Tops@int8 NPU
- Built in 64M DDR2 memory
- 128MByte SPI NAND
- One TF Card Slot, Support UHS-SDR104
- On board XR829 WiFi, BT, up to 150Mbps
- One CTP Connector
- On board USB to UART bridge chip
- One PA for speaker
- Supports 2-lane MIPI DSI, up to 1280x720@60fps
- Supports one 2-lane MIPI CSI inputs
- Supports 1 individual ISP, with maximum resolution of 2560 x 1440
- H.264/H.265 decoding at 4096x4096
- H.264/H.265 encoder supports 3840x2160@20fps@400MHz

### V851s
V851S is a new generation of high-performance H.264/H.265 encoding SoC targeted for the field of IP Camera.

It integrates the single Cortex-A7 core@900MHz, RISC-V@600MHz and 0.5 Tops NPU and supports various intelligent application such as human detection and crossing alarm. V851S is also designed with a new generation of high-performance ISP image processor and video encoder with professional encoding quality, low encoding bit rate and mainstream-level image processing capability. In addition, V851S supports 64MB DDR2 and rich peripheral interfaces, such as USB, SDIO and Ethernet, to meet the requirements of various IP Camera products.

![image](https://user-images.githubusercontent.com/12003087/204967059-b9f3298d-efd7-484e-8942-1f9c516d7ddf.png)

## Operating System
Yuzukilizard Running Tina Linux
```
 _____  _              __     _
|_   _||_| ___  _ _   |  |   |_| ___  _ _  _ _
  | |   _ |   ||   |  |  |__ | ||   || | ||_'_|
  | |  | || | || _ |  |_____||_||_|_||___||_,_|
  |_|  |_||_|_||_|_|  Tina is Based on OpenWrt!
 ----------------------------------------------
 Tina Linux (5.0, r0-7277fac)
 ----------------------------------------------
```

### Docker image

To facilitate development, we have prepared a Docker image for use. Docker has built a basic environment and SDK, which can be developed directly

```
docker pull gloomyghost/yuzukilizard
```

## License
Hardware design files of these projects under the CERN Open Hardware Licence Version 2 - Strongly Reciprocal License
