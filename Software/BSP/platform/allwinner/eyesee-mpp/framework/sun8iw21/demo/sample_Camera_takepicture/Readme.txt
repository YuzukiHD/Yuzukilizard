sample_Camera_takepicture测试流程：


1、功能说明：
（1）sample支持单独获取Raw，单独获取JPEG，同时预览；支持获取JPEG，获取RAW，同时预览。
（2）三路分别使用不同的VIPP，使用两个ISP，对应关系如下：
    - ISP0和ISP1都连接到CSI1；
    - VIPP1和VIPP3连接到ISP0，VIPP0和VIPP2连接到ISP1。
（3）VIPP通道使用推荐方案：
    - Raw data is get by ScalerOutChns 0
    - JPEG data is get by ScalerOutChns 1
    - Preview data is  get by ScalerOutChns 3

2、使用方法
（1）修改 ipc-v5/lichee/tools/pack/chips/sun8iw12p1/configs/方案/sys_config.fex 中，
    - sys_config.fex配置参考

        vinc0_used      = 1
        vinc0_csi_sel       = 1
        vinc0_mipi_sel      = 1
        vinc0_isp_sel       = 1
        vinc0_isp_tx_ch     = 0
        vinc0_rear_sensor_sel   = 1
        vinc0_front_sensor_sel  = 1
        vinc0_sensor_list   = 0
        
        [vind0/vinc1]
        vinc1_used      = 1
        vinc1_csi_sel       = 1
        vinc1_mipi_sel      = 1
        vinc1_isp_sel       = 0
        vinc1_isp_tx_ch     = 0
        vinc1_rear_sensor_sel   = 1
        vinc1_front_sensor_sel  = 1
        vinc1_sensor_list   = 0
        
        [vind0/vinc2]
        vinc2_used      = 1
        vinc2_csi_sel       = 1
        vinc2_mipi_sel      = 1
        vinc2_isp_sel       = 1
        vinc2_isp_tx_ch     = 0
        vinc2_rear_sensor_sel   = 1
        vinc2_front_sensor_sel  = 1
        vinc2_sensor_list   = 0
        
        [vind0/vinc3]
        vinc3_used      = 1
        vinc3_csi_sel       = 1
        vinc3_mipi_sel      = 1
        vinc3_isp_sel       = 0
        vinc3_isp_tx_ch     = 0
        vinc3_rear_sensor_sel   = 1
        vinc3_front_sensor_sel  = 1
        vinc3_sensor_list   = 0

（2）获取4K(3840*2160) Raw格式方法，修改驱动配置，取消isp驱动中的规格限制，patch如下：
    - 修改文件 ipc-v5/lichee/linux-4.4/drivers/media/platform/sunxi-vin/vin-isp/sunxi_isp.c

        diff --git a/drivers/media/platform/sunxi-vin/vin-isp/sunxi_isp.c b/drivers/media/platform/sunxi-vin/vin-isp/sunxi_isp.c
        index eb3c0e6..b818075 100755
        --- a/drivers/media/platform/sunxi-vin/vin-isp/sunxi_isp.c
        +++ b/drivers/media/platform/sunxi-vin/vin-isp/sunxi_isp.c
        @@ -433,8 +433,8 @@ static struct isp_pix_fmt *__isp_try_format(struct isp_dev *isp,
        
                if (!isp->large_image) {
                        if (isp->id == 1) {
        -                       mf->width = clamp_t(u32, mf->width, MIN_IN_WIDTH, 3264);
        -                       mf->height = clamp_t(u32, mf->height, MIN_IN_HEIGHT, 3264);
        +                       mf->width = clamp_t(u32, mf->width, MIN_IN_WIDTH, 4224);
        +                       mf->height = clamp_t(u32, mf->height, MIN_IN_HEIGHT, 4224);
                        } else {
                                mf->width = clamp_t(u32, mf->width, MIN_IN_WIDTH, 4224);
                                mf->height = clamp_t(u32, mf->height, MIN_IN_HEIGHT, 4224);

