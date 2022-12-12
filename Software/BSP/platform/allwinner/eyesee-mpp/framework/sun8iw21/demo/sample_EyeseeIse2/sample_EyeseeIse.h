
#ifndef _SAMPLE_EYESEEISE_H_
#define _SAMPLE_EYESEEISE_H_

#include <string>
#include <deque>

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_sys.h>
#include <mpi_clock.h>
#include <mpi_vo.h>
#include <PictureRegionCallback.h>

#include <Errors.h>

#define DEFAULT_SAMPLE_EYESEEISE_CONF_PATH  "/mnt/extsd/sample_EyeseeIse/sample_EyeseeIse.conf"

/******************************************************************************/
// cam0
#define SAMPLE_EYESEEISE_CAM0_ENABLE                "cam0_enable"
// cam0 device param
#define SAMPLE_EYESEEISE_CAM0_CAP_WIDTH             "cam0_cap_width"
#define SAMPLE_EYESEEISE_CAM0_CAP_HEIGHT            "cam0_cap_height"
#define SAMPLE_EYESEEISE_CAM0_PREV_WIDTH            "cam0_prev_width"
#define SAMPLE_EYESEEISE_CAM0_PREV_HEIGHT           "cam0_prev_height"
#define SAMPLE_EYESEEISE_CAM0_FRAME_RATE            "cam0_frame_rate"
#define SAMPLE_EYESEEISE_CAM0_BUF_NUM               "cam0_buf_num"
#define SAMPLE_EYESEEISE_CAM0_RORATION              "cam0_prev_rotation"
#define SAMPLE_EYESEEISE_CAM0_ID                    "cam0_id"
#define SAMPLE_EYESEEISE_CAM0_CSI_CHN               "cam0_csi_chn"
#define SAMPLE_EYESEEISE_CAM0_ISP_DEV               "cam0_isp_dev"
#define SAMPLE_EYESEEISE_CAM0_ISP_SCALE0            "cam0_isp_scale0"
#define SAMPLE_EYESEEISE_CAM0_ISP_SCALE1            "cam0_isp_scale1"
// cam0 preview param
#define SAMPLE_EYESEEISE_CAM0_DISP_ENABLE           "cam0_disp_enable"
#define SAMPLE_EYESEEISE_CAM0_DISP_RECT_X           "cam0_disp_rect_x"
#define SAMPLE_EYESEEISE_CAM0_DISP_RECT_Y           "cam0_disp_rect_y"
#define SAMPLE_EYESEEISE_CAM0_DISP_RECT_WIDTH       "cam0_disp_rect_width"
#define SAMPLE_EYESEEISE_CAM0_DISP_RECT_HEIGHT      "cam0_disp_rect_height"
#define SAMPLE_EYESEEISE_CAM0_DISP_VO_LAYER         "cam0_disp_vo_layer"
// cam0 photo param
#define SAMPLE_EYESEEISE_CAM0_PHOTO_ENABLE          "cam0_photo_enable"
#define SAMPLE_EYESEEISE_CAM0_PHOTO_MODE            "cam0_photo_mode"
#define SAMPLE_EYESEEISE_CAM0_PHOTO_CHNID           "cam0_photo_chnid"
#define SAMPLE_EYESEEISE_CAM0_PHOTO_WIDTH           "cam0_photo_width"
#define SAMPLE_EYESEEISE_CAM0_PHOTO_HEIGHT          "cam0_photo_height"
#define SAMPLE_EYESEEISE_CAM0_PHOTO_PATH            "cam0_photo_path"
#define SAMPLE_EYESEEISE_CAM0_PHOTO_CONT_CNT        "cam0_photo_cont_cnt"
#define SAMPLE_EYESEEISE_CAM0_PHOTO_CONT_ITL        "cam0_photo_cont_itl"
// cam0 region param
#define SAMPLE_EYESEEISE_CAM0_RGN_ENABLE            "cam0_rgn_enable"
#define SAMPLE_EYESEEISE_CAM0_RGN_OVLY_ENABLE       "cam0_rgn_ovly_enable"
#define SAMPLE_EYESEEISE_CAM0_RGN_OVLY_X            "cam0_rgn_ovly_x"
#define SAMPLE_EYESEEISE_CAM0_RGN_OVLY_Y            "cam0_rgn_ovly_y"
#define SAMPLE_EYESEEISE_CAM0_RGN_OVLY_W            "cam0_rgn_ovly_w"
#define SAMPLE_EYESEEISE_CAM0_RGN_OVLY_H            "cam0_rgn_ovly_h"
#define SAMPLE_EYESEEISE_CAM0_RGN_OVLY_PIX          "cam0_rgn_ovly_pix"
#define SAMPLE_EYESEEISE_CAM0_RGN_OVLY_COR          "cam0_rgn_ovly_cor"
#define SAMPLE_EYESEEISE_CAM0_RGN_OVLY_INV_COR_EN   "cam0_rgn_ovly_inv_cor_en"
#define SAMPLE_EYESEEISE_CAM0_RGN_OVLY_PRI          "cam0_rgn_ovly_pri"
#define SAMPLE_EYESEEISE_CAM0_RGN_COV_ENABLE        "cam0_rgn_cov_enable"
#define SAMPLE_EYESEEISE_CAM0_RGN_COV_X             "cam0_rgn_cov_x"
#define SAMPLE_EYESEEISE_CAM0_RGN_COV_Y             "cam0_rgn_cov_y"
#define SAMPLE_EYESEEISE_CAM0_RGN_COV_W             "cam0_rgn_cov_w"
#define SAMPLE_EYESEEISE_CAM0_RGN_COV_H             "cam0_rgn_cov_h"
#define SAMPLE_EYESEEISE_CAM0_RGN_COV_COR           "cam0_rgn_cov_cor"
#define SAMPLE_EYESEEISE_CAM0_RGN_COV_PRI           "cam0_rgn_cov_pri"
// cam0 record param
#define SAMPLE_EYESEEISE_CAM0_REC_ENABLE            "cam0_rec_enable"
#define SAMPLE_EYESEEISE_CAM0_REC_PATH              "cam0_rec_path"
#define SAMPLE_EYESEEISE_CAM0_REC_V_TYPE            "cam0_rec_v_type"
#define SAMPLE_EYESEEISE_CAM0_REC_V_WIDTH           "cam0_rec_v_width"
#define SAMPLE_EYESEEISE_CAM0_REC_V_HEIGHT          "cam0_rec_v_height"
#define SAMPLE_EYESEEISE_CAM0_REC_V_BITRATE         "cam0_rec_v_bitrate"
#define SAMPLE_EYESEEISE_CAM0_REC_V_FRAMERATE       "cam0_rec_v_framerate"
#define SAMPLE_EYESEEISE_CAM0_REC_A_TYPE            "cam0_rec_a_type"
#define SAMPLE_EYESEEISE_CAM0_REC_A_SR              "cam0_rec_a_sr"
#define SAMPLE_EYESEEISE_CAM0_REC_A_CHN             "cam0_rec_a_chn"
#define SAMPLE_EYESEEISE_CAM0_REC_A_BR              "cam0_rec_a_br"


// cam1
#define SAMPLE_EYESEEISE_CAM1_ENABLE                "cam1_enable"
// cam1 device param
#define SAMPLE_EYESEEISE_CAM1_CAP_WIDTH             "cam1_cap_width"
#define SAMPLE_EYESEEISE_CAM1_CAP_HEIGHT            "cam1_cap_height"
#define SAMPLE_EYESEEISE_CAM1_PREV_WIDTH            "cam1_prev_width"
#define SAMPLE_EYESEEISE_CAM1_PREV_HEIGHT           "cam1_prev_height"
#define SAMPLE_EYESEEISE_CAM1_FRAME_RATE            "cam1_frame_rate"
#define SAMPLE_EYESEEISE_CAM1_BUF_NUM               "cam1_buf_num"
#define SAMPLE_EYESEEISE_CAM1_RORATION              "cam1_prev_rotation"
#define SAMPLE_EYESEEISE_CAM1_ID                    "cam1_id"
#define SAMPLE_EYESEEISE_CAM1_CSI_CHN               "cam1_csi_chn"
#define SAMPLE_EYESEEISE_CAM1_ISP_DEV               "cam1_isp_dev"
#define SAMPLE_EYESEEISE_CAM1_ISP_SCALE0            "cam1_isp_scale0"
#define SAMPLE_EYESEEISE_CAM1_ISP_SCALE1            "cam1_isp_scale1"
// cam1 preview param
#define SAMPLE_EYESEEISE_CAM1_DISP_ENABLE           "cam1_disp_enable"
#define SAMPLE_EYESEEISE_CAM1_DISP_RECT_X           "cam1_disp_rect_x"
#define SAMPLE_EYESEEISE_CAM1_DISP_RECT_Y           "cam1_disp_rect_y"
#define SAMPLE_EYESEEISE_CAM1_DISP_RECT_WIDTH       "cam1_disp_rect_width"
#define SAMPLE_EYESEEISE_CAM1_DISP_RECT_HEIGHT      "cam1_disp_rect_height"
#define SAMPLE_EYESEEISE_CAM1_DISP_VO_LAYER         "cam1_disp_vo_layer"
// cam1 photo param
#define SAMPLE_EYESEEISE_CAM1_PHOTO_ENABLE          "cam1_photo_enable"
#define SAMPLE_EYESEEISE_CAM1_PHOTO_MODE            "cam1_photo_mode"
#define SAMPLE_EYESEEISE_CAM1_PHOTO_CHNID           "cam1_photo_chnid"
#define SAMPLE_EYESEEISE_CAM1_PHOTO_WIDTH           "cam1_photo_width"
#define SAMPLE_EYESEEISE_CAM1_PHOTO_HEIGHT          "cam1_photo_height"
#define SAMPLE_EYESEEISE_CAM1_PHOTO_PATH            "cam1_photo_path"
#define SAMPLE_EYESEEISE_CAM1_PHOTO_CONT_CNT        "cam1_photo_cont_cnt"
#define SAMPLE_EYESEEISE_CAM1_PHOTO_CONT_ITL        "cam1_photo_cont_itl"
// cam1 region param
#define SAMPLE_EYESEEISE_CAM1_RGN_ENABLE            "cam1_rgn_enable"
#define SAMPLE_EYESEEISE_CAM1_RGN_OVLY_ENABLE       "cam1_rgn_ovly_enable"
#define SAMPLE_EYESEEISE_CAM1_RGN_OVLY_X            "cam1_rgn_ovly_x"
#define SAMPLE_EYESEEISE_CAM1_RGN_OVLY_Y            "cam1_rgn_ovly_y"
#define SAMPLE_EYESEEISE_CAM1_RGN_OVLY_W            "cam1_rgn_ovly_w"
#define SAMPLE_EYESEEISE_CAM1_RGN_OVLY_H            "cam1_rgn_ovly_h"
#define SAMPLE_EYESEEISE_CAM1_RGN_OVLY_PIX          "cam1_rgn_ovly_pix"
#define SAMPLE_EYESEEISE_CAM1_RGN_OVLY_COR          "cam1_rgn_ovly_cor"
#define SAMPLE_EYESEEISE_CAM1_RGN_OVLY_INV_COR_EN   "cam1_rgn_ovly_inv_cor_en"
#define SAMPLE_EYESEEISE_CAM1_RGN_OVLY_PRI          "cam1_rgn_ovly_pri"
#define SAMPLE_EYESEEISE_CAM1_RGN_COV_ENABLE        "cam1_rgn_cov_enable"
#define SAMPLE_EYESEEISE_CAM1_RGN_COV_X             "cam1_rgn_cov_x"
#define SAMPLE_EYESEEISE_CAM1_RGN_COV_Y             "cam1_rgn_cov_y"
#define SAMPLE_EYESEEISE_CAM1_RGN_COV_W             "cam1_rgn_cov_w"
#define SAMPLE_EYESEEISE_CAM1_RGN_COV_H             "cam1_rgn_cov_h"
#define SAMPLE_EYESEEISE_CAM1_RGN_COV_COR           "cam1_rgn_cov_cor"
#define SAMPLE_EYESEEISE_CAM1_RGN_COV_PRI           "cam1_rgn_cov_pri"
// cam1 record param
#define SAMPLE_EYESEEISE_CAM1_REC_ENABLE            "cam1_rec_enable"
#define SAMPLE_EYESEEISE_CAM1_REC_PATH              "cam1_rec_path"
#define SAMPLE_EYESEEISE_CAM1_REC_V_TYPE            "cam1_rec_v_type"
#define SAMPLE_EYESEEISE_CAM1_REC_V_WIDTH           "cam1_rec_v_width"
#define SAMPLE_EYESEEISE_CAM1_REC_V_HEIGHT          "cam1_rec_v_height"
#define SAMPLE_EYESEEISE_CAM1_REC_V_BITRATE         "cam1_rec_v_bitrate"
#define SAMPLE_EYESEEISE_CAM1_REC_V_FRAMERATE       "cam1_rec_v_framerate"
#define SAMPLE_EYESEEISE_CAM1_REC_A_TYPE            "cam1_rec_a_type"
#define SAMPLE_EYESEEISE_CAM1_REC_A_SR              "cam1_rec_a_sr"
#define SAMPLE_EYESEEISE_CAM1_REC_A_CHN             "cam1_rec_a_chn"
#define SAMPLE_EYESEEISE_CAM1_REC_A_BR              "cam1_rec_a_br"


/******************************************************************************/
// ise
#define SAMPLE_EYESEEISE_ISE_MODE                   "ise_mode"
// ise-mo
#define SAMPLE_EYESEEISE_ISE_MO_ENABLE              "ise_mo_enable"
#define SAMPLE_EYESEEISE_ISE_MO_TYPE                "ise_mo_type"
#define SAMPLE_EYESEEISE_ISE_MO_MOUNT_TYPE          "ise_mo_mount_type"
#define SAMPLE_EYESEEISE_ISE_MO_IN_W                "ise_mo_in_w"
#define SAMPLE_EYESEEISE_ISE_MO_IN_H                "ise_mo_in_h"
#define SAMPLE_EYESEEISE_ISE_MO_IN_LP               "ise_mo_in_lp"
#define SAMPLE_EYESEEISE_ISE_MO_IN_CP               "ise_mo_in_cp"
#define SAMPLE_EYESEEISE_ISE_MO_IN_YUV_TYPE         "ise_mo_in_yuv_type"     //0-NV21

#define SAMPLE_EYESEEISE_ISE_MO_PAN                 "ise_mo_pan"
#define SAMPLE_EYESEEISE_ISE_MO_TILT                "ise_mo_tilt"
#define SAMPLE_EYESEEISE_ISE_MO_ZOOM                "ise_mo_zoom"
#define SAMPLE_EYESEEISE_ISE_MO_PANSUB0             "ise_mo_pan_sub0"
#define SAMPLE_EYESEEISE_ISE_MO_PANSUB1             "ise_mo_pan_sub1"
#define SAMPLE_EYESEEISE_ISE_MO_PANSUB2             "ise_mo_pan_sub2"
#define SAMPLE_EYESEEISE_ISE_MO_PANSUB3             "ise_mo_pan_sub3"
#define SAMPLE_EYESEEISE_ISE_MO_TILTSUB0            "ise_mo_tilt_sub0"
#define SAMPLE_EYESEEISE_ISE_MO_TILTSUB1            "ise_mo_tilt_sub1"
#define SAMPLE_EYESEEISE_ISE_MO_TILTSUB2            "ise_mo_tilt_sub2"
#define SAMPLE_EYESEEISE_ISE_MO_TILTSUB3            "ise_mo_tilt_sub3"
#define SAMPLE_EYESEEISE_ISE_MO_ZOOMSUB0            "ise_mo_zoom_sub0"
#define SAMPLE_EYESEEISE_ISE_MO_ZOOMSUB1            "ise_mo_zoom_sub1"
#define SAMPLE_EYESEEISE_ISE_MO_ZOOMSUB2            "ise_mo_zoom_sub2"
#define SAMPLE_EYESEEISE_ISE_MO_ZOOMSUB3            "ise_mo_zoom_sub3"
#define SAMPLE_EYESEEISE_ISE_MO_PTZ_SUB_CHG0        "ise_mo_ptz_sub_chg0"
#define SAMPLE_EYESEEISE_ISE_MO_PTZ_SUB_CHG1        "ise_mo_ptz_sub_chg1"
#define SAMPLE_EYESEEISE_ISE_MO_PTZ_SUB_CHG2        "ise_mo_ptz_sub_chg2"
#define SAMPLE_EYESEEISE_ISE_MO_PTZ_SUB_CHG3        "ise_mo_ptz_sub_chg3"

#define SAMPLE_EYESEEISE_ISE_MO_OUT_EN0             "ise_mo_out_en0"
#define SAMPLE_EYESEEISE_ISE_MO_OUT_EN1             "ise_mo_out_en1"
#define SAMPLE_EYESEEISE_ISE_MO_OUT_EN2             "ise_mo_out_en2"
#define SAMPLE_EYESEEISE_ISE_MO_OUT_EN3             "ise_mo_out_en3"

#define SAMPLE_EYESEEISE_ISE_MO_OUT_W0              "ise_mo_out_w0"
#define SAMPLE_EYESEEISE_ISE_MO_OUT_W1              "ise_mo_out_w1"
#define SAMPLE_EYESEEISE_ISE_MO_OUT_W2              "ise_mo_out_w2"
#define SAMPLE_EYESEEISE_ISE_MO_OUT_W3              "ise_mo_out_w3"
#define SAMPLE_EYESEEISE_ISE_MO_OUT_H0              "ise_mo_out_h0"
#define SAMPLE_EYESEEISE_ISE_MO_OUT_H1              "ise_mo_out_h1"
#define SAMPLE_EYESEEISE_ISE_MO_OUT_H2              "ise_mo_out_h2"
#define SAMPLE_EYESEEISE_ISE_MO_OUT_H3              "ise_mo_out_h3"

#define SAMPLE_EYESEEISE_ISE_MO_OUT_LP0             "ise_mo_out_lp0"
#define SAMPLE_EYESEEISE_ISE_MO_OUT_LP1             "ise_mo_out_lp1"
#define SAMPLE_EYESEEISE_ISE_MO_OUT_LP2             "ise_mo_out_lp2"
#define SAMPLE_EYESEEISE_ISE_MO_OUT_LP3             "ise_mo_out_lp3"
#define SAMPLE_EYESEEISE_ISE_MO_OUT_CP0             "ise_mo_out_cp0"
#define SAMPLE_EYESEEISE_ISE_MO_OUT_CP1             "ise_mo_out_cp1"
#define SAMPLE_EYESEEISE_ISE_MO_OUT_CP2             "ise_mo_out_cp2"
#define SAMPLE_EYESEEISE_ISE_MO_OUT_CP3             "ise_mo_out_cp3"

#define SAMPLE_EYESEEISE_ISE_MO_OUT_FLIP0           "ise_mo_out_flip0"
#define SAMPLE_EYESEEISE_ISE_MO_OUT_FLIP1           "ise_mo_out_flip1"
#define SAMPLE_EYESEEISE_ISE_MO_OUT_FLIP2           "ise_mo_out_flip2"
#define SAMPLE_EYESEEISE_ISE_MO_OUT_FLIP3           "ise_mo_out_flip3"
#define SAMPLE_EYESEEISE_ISE_MO_OUT_MIR0            "ise_mo_out_mir0"
#define SAMPLE_EYESEEISE_ISE_MO_OUT_MIR1            "ise_mo_out_mir1"
#define SAMPLE_EYESEEISE_ISE_MO_OUT_MIR2            "ise_mo_out_mir2"
#define SAMPLE_EYESEEISE_ISE_MO_OUT_MIR3            "ise_mo_out_mir3"

#define SAMPLE_EYESEEISE_ISE_MO_OUT_YUV_TYPE        "ise_mo_out_yuv_type"

#define SAMPLE_EYESEEISE_ISE_MO_P                   "ise_mo_p"
#define SAMPLE_EYESEEISE_ISE_MO_CX                  "ise_mo_cx"
#define SAMPLE_EYESEEISE_ISE_MO_CY                  "ise_mo_cy"
#define SAMPLE_EYESEEISE_ISE_MO_FX                  "ise_mo_fx"
#define SAMPLE_EYESEEISE_ISE_MO_FY                  "ise_mo_fy"
#define SAMPLE_EYESEEISE_ISE_MO_CXD                 "ise_mo_cxd"
#define SAMPLE_EYESEEISE_ISE_MO_CYD                 "ise_mo_cyd"
#define SAMPLE_EYESEEISE_ISE_MO_FXD                 "ise_mo_fxd"
#define SAMPLE_EYESEEISE_ISE_MO_FYD                 "ise_mo_fyd"

#define SAMPLE_EYESEEISE_ISE_MO_K                   "ise_mo_k"
#define SAMPLE_EYESEEISE_ISE_MO_PU0                 "ise_mo_pu0"
#define SAMPLE_EYESEEISE_ISE_MO_PU1                 "ise_mo_pu1"
#define SAMPLE_EYESEEISE_ISE_MO_CM                  "ise_mo_cm"
#define SAMPLE_EYESEEISE_ISE_MO_CMC                 "ise_mo_cmc"
#define SAMPLE_EYESEEISE_ISE_MO_DISTORT             "ise_mo_d"

#define SAMPLE_EYESEEISE_ISE_MO_REV                "ise_mo_reserve"

// ise-mo-preview param
#define SAMPLE_EYESEEISE_ISE_MO_DISP_ENABLE        "ise_mo_disp_enable"
#define SAMPLE_EYESEEISE_ISE_MO_DISP_RECT_X        "ise_mo_disp_rect_x"
#define SAMPLE_EYESEEISE_ISE_MO_DISP_RECT_Y        "ise_mo_disp_rect_y"
#define SAMPLE_EYESEEISE_ISE_MO_DISP_RECT_WIDTH    "ise_mo_disp_rect_w"
#define SAMPLE_EYESEEISE_ISE_MO_DISP_RECT_HEIGHT   "ise_mo_disp_rect_h"

// ise-mo-photo param
#define SAMPLE_EYESEEISE_ISE_MO_PHOTO_ENABLE          "ise_mo_photo_enable"
#define SAMPLE_EYESEEISE_ISE_MO_PHOTO_MODE            "ise_mo_photo_mode"
#define SAMPLE_EYESEEISE_ISE_MO_PHOTO_CHNID           "ise_mo_photo_chnid"
#define SAMPLE_EYESEEISE_ISE_MO_PHOTO_WIDTH           "ise_mo_photo_width"
#define SAMPLE_EYESEEISE_ISE_MO_PHOTO_HEIGHT          "ise_mo_photo_height"
#define SAMPLE_EYESEEISE_ISE_MO_PHOTO_PATH            "ise_mo_photo_path"
#define SAMPLE_EYESEEISE_ISE_MO_PHOTO_CONT_CNT        "ise_mo_photo_cont_cnt"
#define SAMPLE_EYESEEISE_ISE_MO_PHOTO_CONT_ITL        "ise_mo_photo_cont_itl"

// ise-mo-region param
#define SAMPLE_EYESEEISE_ISE_MO_RGN_ENABLE            "ise_mo_rgn_enable"
#define SAMPLE_EYESEEISE_ISE_MO_RGN_OVLY_ENABLE       "ise_mo_rgn_ovly_enable"
#define SAMPLE_EYESEEISE_ISE_MO_RGN_OVLY_X            "ise_mo_rgn_ovly_x"
#define SAMPLE_EYESEEISE_ISE_MO_RGN_OVLY_Y            "ise_mo_rgn_ovly_y"
#define SAMPLE_EYESEEISE_ISE_MO_RGN_OVLY_W            "ise_mo_rgn_ovly_w"
#define SAMPLE_EYESEEISE_ISE_MO_RGN_OVLY_H            "ise_mo_rgn_ovly_h"
#define SAMPLE_EYESEEISE_ISE_MO_RGN_OVLY_PIX          "ise_mo_rgn_ovly_pix"
#define SAMPLE_EYESEEISE_ISE_MO_RGN_OVLY_COR          "ise_mo_rgn_ovly_cor"
#define SAMPLE_EYESEEISE_ISE_MO_RGN_OVLY_INV_COR_EN   "ise_mo_rgn_ovly_inv_cor_en"
#define SAMPLE_EYESEEISE_ISE_MO_RGN_OVLY_PRI          "ise_mo_rgn_ovly_pri"
#define SAMPLE_EYESEEISE_ISE_MO_RGN_COV_ENABLE        "ise_mo_rgn_cov_enable"
#define SAMPLE_EYESEEISE_ISE_MO_RGN_COV_X             "ise_mo_rgn_cov_x"
#define SAMPLE_EYESEEISE_ISE_MO_RGN_COV_Y             "ise_mo_rgn_cov_y"
#define SAMPLE_EYESEEISE_ISE_MO_RGN_COV_W             "ise_mo_rgn_cov_w"
#define SAMPLE_EYESEEISE_ISE_MO_RGN_COV_H             "ise_mo_rgn_cov_h"
#define SAMPLE_EYESEEISE_ISE_MO_RGN_COV_COR           "ise_mo_rgn_cov_cor"
#define SAMPLE_EYESEEISE_ISE_MO_RGN_COV_PRI           "ise_mo_rgn_cov_pri"

// ise-mo-record0 param
#define SAMPLE_EYESEEISE_ISE_MO_REC0_ENABLE         "ise_mo_rec0_enable"
#define SAMPLE_EYESEEISE_ISE_MO_REC0_PATH           "ise_mo_rec0_path"
#define SAMPLE_EYESEEISE_ISE_MO_REC0_V_TYPE         "ise_mo_rec0_v_type"
#define SAMPLE_EYESEEISE_ISE_MO_REC0_V_WIDTH        "ise_mo_rec0_v_width"
#define SAMPLE_EYESEEISE_ISE_MO_REC0_V_HEIGHT       "ise_mo_rec0_v_height"
#define SAMPLE_EYESEEISE_ISE_MO_REC0_V_BITRATE      "ise_mo_rec0_v_bitrate"
#define SAMPLE_EYESEEISE_ISE_MO_REC0_V_FRAMERATE    "ise_mo_rec0_v_framerate"
#define SAMPLE_EYESEEISE_ISE_MO_REC0_A_TYPE         "ise_mo_rec0_a_type"
#define SAMPLE_EYESEEISE_ISE_MO_REC0_A_SR           "ise_mo_rec0_a_sr"
#define SAMPLE_EYESEEISE_ISE_MO_REC0_A_CHN          "ise_mo_rec0_a_chn"
#define SAMPLE_EYESEEISE_ISE_MO_REC0_A_BR           "ise_mo_rec0_a_br"
// ise-mo-record1 param
#define SAMPLE_EYESEEISE_ISE_MO_REC1_ENABLE         "ise_mo_rec1_enable"
#define SAMPLE_EYESEEISE_ISE_MO_REC1_PATH           "ise_mo_rec1_path"
#define SAMPLE_EYESEEISE_ISE_MO_REC1_V_TYPE         "ise_mo_rec1_v_type"
#define SAMPLE_EYESEEISE_ISE_MO_REC1_V_WIDTH        "ise_mo_rec1_v_width"
#define SAMPLE_EYESEEISE_ISE_MO_REC1_V_HEIGHT       "ise_mo_rec1_v_height"
#define SAMPLE_EYESEEISE_ISE_MO_REC1_V_BITRATE      "ise_mo_rec1_v_bitrate"
#define SAMPLE_EYESEEISE_ISE_MO_REC1_V_FRAMERATE    "ise_mo_rec1_v_framerate"
#define SAMPLE_EYESEEISE_ISE_MO_REC1_A_TYPE         "ise_mo_rec1_a_type"
#define SAMPLE_EYESEEISE_ISE_MO_REC1_A_SR           "ise_mo_rec1_a_sr"
#define SAMPLE_EYESEEISE_ISE_MO_REC1_A_CHN          "ise_mo_rec1_a_chn"
#define SAMPLE_EYESEEISE_ISE_MO_REC1_A_BR           "ise_mo_rec1_a_br"

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/

typedef struct SampleEyeseeIseCmdLineParam
{
    std::string mConfigFilePath;
}SampleEyeseeIseCmdLineParam;

class SampleEyeseeIseContext;
class EyeseeCameraListener
    : public EyeseeLinux::EyeseeCamera::PictureCallback
    , public EyeseeLinux::EyeseeCamera::InfoCallback
    , public EyeseeLinux::PictureRegionCallback
{
public:
    bool onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeLinux::EyeseeCamera *pCamera);
    void onPictureTaken(int chnId, const void *data, int size, EyeseeLinux::EyeseeCamera* pCamera);
    void addPictureRegion(std::list<PictureRegionType> &rPicRgnList);

    EyeseeCameraListener(SampleEyeseeIseContext *pContext);
    virtual ~EyeseeCameraListener(){}
private:
    SampleEyeseeIseContext *const mpContext;
};

class EyeseeIseListener
    : public EyeseeLinux::EyeseeISE::PictureCallback
    , public EyeseeLinux::EyeseeISE::ErrorCallback
    , public EyeseeLinux::EyeseeISE::InfoCallback
    , public EyeseeLinux::PictureRegionCallback
{
public:
    void onError(int chnId, int error, EyeseeLinux::EyeseeISE *pISE);
    bool onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeLinux::EyeseeISE *pISE);
    void onPictureTaken(int chnId, const void *data, int size, EyeseeLinux::EyeseeISE* pISE);
    void addPictureRegion(std::list<PictureRegionType> &rPicRgnList);

    EyeseeIseListener(SampleEyeseeIseContext *pContext);
    virtual ~EyeseeIseListener(){}
private:
    SampleEyeseeIseContext *const mpContext;
};

class EyeseeRecorderListener 
    : public EyeseeLinux::EyeseeRecorder::OnErrorListener
    , public EyeseeLinux::EyeseeRecorder::OnInfoListener
    , public EyeseeLinux::EyeseeRecorder::OnDataListener
{
public:
    void onError(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onInfo(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onData(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);

    EyeseeRecorderListener(SampleEyeseeIseContext *pOwner);
    virtual ~EyeseeRecorderListener(){}
private:
    SampleEyeseeIseContext *const mpContext;
};


typedef struct CamDeviceParam {
    int mCamCapWidth;
    int mCamCapHeight;
    int mCamPrevWidth;
    int mCamPrevHeight;
    int mCamFrameRate;
    int mCamBufNum;
    int mCamRotation;  // 0,90,180,270
    int mCamId;         // guarantee mCamId == mCamCsiChn
    int mCamCsiChn;
    int mCamIspDev;
    int mCamIspScale0;
    int mCamIspScale1;
} CamDeviceParam;

typedef struct DispParam {
    int mDispRectX;
    int mDispRectY;
    int mDispRectWidth;
    int mDispRectHeight;
} DispParam;

typedef struct PhotoParam{
    int mPhotoMode;     // 0-continuous; 1-fast
    int mCamChnId;
    int mPicNumId;
    int mPicWidth;
    int mPicHeight;
    char mPicPath[64];
//    char mThumbPath[64];
//    int mThumbWidth;
//    int mThumbHeight;
    // only for continuous
    int mContPicCnt;
    int mContPicItlMs;
} PhotoParam;

typedef struct RegionParam{
    // for overlay
    int mRgnOvlyEnable;
    int mRgnOvlyX;
    int mRgnOvlyY;
    int mRgnOvlyW;
    int mRgnOvlyH;
    int mRgnOvlyPixFmt;             // 0-ARGB8888; 1-ARGB1555
    int mRgnOvlyCorVal;             // 0xAARRGGBB
    int mRgnOvlyInvCorEn;           // invert color enable
    int mRgnOvlyPri;                // [0,VI_MAX_OVERLAY_NUM-1]
    // for cover
    int mRgnCovEnable;
    int mRgnCovX;
    int mRgnCovY;
    int mRgnCovW;
    int mRgnCovH;
    int mRgnCovCor;
    int mRgnCovPri;                 // [0,VI_MAX_COVER_NUM-1]
} RegionParam;

typedef struct RecParam {
    char mRecParamFileName[64];          // "/mnt/extsd/xyz.mp4"
    char mRecParamVideoEncoder[64];      // "H.264", "H.265"
    int mRecParamVideoSizeWidth;
    int mRecParamVideoSizeHeight;
    int mRecParamVideoBitRate;          // mbps
    int mRecParamVideoFrameRate;
    char mRecParamAudioEncoder[64];  // "AAC", "MP3"
    int mRecParamAudioSamplingRate;
    int mRecParamAudioChannels;
    int mRecParamAudioBitRate;
} RecParam;

typedef struct IseMoParam {
    //ref frome ISE_lib_mo.h
    int mIseMoType; //WARP_Type_MO: 1-WARP_PANO180; 2-WARP_PANO360; 3-WARP_NORMAL; 4-WARP_UNDISTORT; 5-WARP_180WITH2; 6-WARP_PTZ4IN1
    int mIseMoMountTpye;    //MOUNT_Type_MO: 1-MOUNT_TOP; 2-MOUNT_WALL; 3-MOUNT_BOTTOM
    int mIseMoInHeight;     //get from CamDeviceParam.mCamCapHeight
    int mIseMoInWidth;      //get from CamDeviceParam.mCamCapWidth
    int mIseMoInLumaPitch;
    int mIseMoInChromaPitch;
    int mIseMoInYuvType;    //0-NV21
    float mIseMoPan;
    float mIseMoTilt;
    float mIseMoZoom;

    //only for PTZ4IN1
    float mIseMoPanSub[4];
    float mIseMoTiltSub[4];
    float mIseMoZoomSub[4];

    //only for dynamic ptz
    float mIseMoPtzSubChg[4];

    int mIseMoOutEn[4];
    int mIseMoOutHeight[4];
    int mIseMoOutWidth[4];
    int mIseMoOutLumaPitch[4];
    int mIseMoOutChromaPitch[4];
    int mIseMoOutFlip[4];
    int mIseMoOutMirror[4];
    int mIseMoOutYuvType;    //0-NV21

    //for len param
    float mIseMoP;
    float mIseMoCx;
    float mIseMoCy;

    //only for undistort
    float mIseMoFx;
    float mIseMoFy;
    float mIseMoCxd;
    float mIseMoCyd;
    float mIseMoFxd;
    float mIseMoFyd;
    float mIseMoK[8];

    float mIseMoPUndis[2];
    double mIseMoCalibMatr[3][3];
    double mIseMoCalibMatrCv[3][3];
    double mIseMoDistort[8];
    char mIseMoReserved[32];
} IseMoParam;

typedef struct IseBiParam {


} IseBiParam;

typedef struct IseStiParam {


} IseStiParam;


typedef struct SampleEyeseeIseConfig {
    //camera
    BOOL mCamEnable[2];
    CamDeviceParam mCamDevParam[2];
    BOOL mCamDispEnable[2];
    DispParam mDispParam[2];
    BOOL mCamPhotoEnable[2];
    PhotoParam mCamPhotoParam[2];
    BOOL mCamRgnEnable[2];          // only for jpeg
    RegionParam mCamRgnParam[2];
    BOOL mCamRecEnable[2];
    RecParam mCamRecParam[2];

    //ise
    int mIseMode;   //0-none; 1-mo; 2-bi; 3-sti
    //ise-mo
    BOOL mIseMoEnable;
    IseMoParam mIseMoParam;
    BOOL mIseMoDispEnable[4];
    DispParam mIseMoDispParam[4];
    BOOL mIseMoPhotoEnable[4];
    PhotoParam mIseMoPhotoParam[4];
    BOOL mIseMoRgnEnable[4];          // only for jpeg
    RegionParam mIseMoRgnParam[4];
    BOOL mIseMoRecEnable[4];
    RecParam mIseMoRecParam[4];
    //ise-bi
    IseBiParam mIseBiParam;
    DispParam mIseBiDispParam;
    RecParam mIseBiRecParam;
    //ise-sti
    IseStiParam mIseStiParam;
    DispParam mIseStiDispParam;
    RecParam mIseStiRecParam;
} SampleEyeseeIseConfig;

class SampleEyeseeIseContext
{
public:
    SampleEyeseeIseContext();
    ~SampleEyeseeIseContext();

    EyeseeLinux::status_t ParseCmdLine(int argc, char *argv[]);
    EyeseeLinux::status_t loadConfig();
    EyeseeLinux::status_t checkConfig();

    SampleEyeseeIseCmdLineParam mCmdLineParam;
    SampleEyeseeIseConfig       mConfigParam;

    cdx_sem_t mSemExit;

    VO_DEV mVoDev;
    int mUILayer;

    MPP_SYS_CONF_S mSysConf;
    //VO_VIDEO_LAYER_ATTR_S mLayerAttr;
    VO_LAYER mCamVoLayer[2];
    int mCamDispChnId[2];

    ISE_CHN_ATTR_S mIseChnAttr[4];
    VO_LAYER mIseVoLayer[4];
    BOOL mIseVoLayerEnable[4];
    // for notify app to preview and check chn id.
    // fish:    ptz mode, [x][y] for preview; other mode, only use [0][x] for preview.
    // dfish:   only use [0]
    // sti:     only use [0]
    ISE_CHN mIseChnId[4][4];
    BOOL mIseChnEnable[4][4];

    CameraInfo mCameraInfo[2];
    EyeseeLinux::EyeseeCamera *mpCamera[2];
    EyeseeLinux::EyeseeISE *mpIse;
    EyeseeLinux::EyeseeRecorder *mpRecorder[4];
    EyeseeCameraListener mCameraListener;
    EyeseeIseListener mIseListener;
    EyeseeRecorderListener mRecorderListener;   // not use

    int mMuxerId[4];
    //int mFileNum;

    int mCamPicNum;
    int mIsePicNum;
};

#endif  /* _SAMPLE_EYESEEISE_H_ */

