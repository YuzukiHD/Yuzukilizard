/*
Copyright (c) 2020-2030 allwinnertech.com JetCui<cuiyuntao@allwinnertech.com>

*/
#include <sys/types.h>
#include <sys/stat.h>
#include <sys_device.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <memory.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdbool.h>
#include <dfs_posix.h>
#include "../trans_header.h"
#include "../sunxi_display2.h"
#include "de_generic.h"
#include "de201_trans.h"
#include <math.h>

#define PEAK_REG_SET 6
#define LTI_REG_SET 11

static int *trans_x_gamma(struct gamma_lut *lut, int cout)
{
	int *gamma_lut = NULL;
	int i = 0;

	gamma_lut = (int *)malloc(sizeof(int) * cout);
	if (gamma_lut == NULL) {
		PQ_Printf("malloc for gamma lut err...");
		return NULL;
	}
#if (PQ_DEBUG_LEVEL > 1)
	printf("%2x-%2x-%2x::\n", lut->value[0], lut->value[cout],
	       lut->value[cout * 2]);
#endif
	while (i < cout) {
		gamma_lut[i] = (((int)lut->value[i]) << 16) |
			       (((int)lut->value[i + cout]) << 8) |
			       (lut->value[i + cout * 2]);
#if (PQ_DEBUG_LEVEL > 1)
		printf("%d:%08x ", i, gamma_lut[i]);
#endif
		i++;
	}
	return gamma_lut;
}

static int gamma_x_trans(struct gamma_lut *lut, int cout, unsigned int *data)
{
	int i = 0;
	while (i < cout) {
		lut->value[i] = (char)(data[i] >> 16) & 0xff;
		lut->value[i + cout] = (char)(data[i] >> 8) & 0xff;
		lut->value[i + cout * 2] = (char)(data[i]) & 0xff;
#if (PQ_DEBUG_LEVEL > 1)
		printf("%d:%08x ", i, data[i]);
#endif
		i++;
	}
	return 0;
}

static int tans_to_peak(int disp, int de, struct peak_sharp *peak)
{
	unsigned long arg[4] = { 0 };
	struct Regiser_pair *peak_set = NULL;
	int ret;
	PQ_Printf("peak_set");
	peak_set = (struct Regiser_pair *)malloc(sizeof(struct Regiser_pair) *
						 PEAK_REG_SET);
	if (peak_set == NULL) {
		PQ_Printf("err: malloc Regiser_pair failed: %d", __LINE__);
		return -1;
	}
	PQ_Printf("memset");
	memset(peak_set, 0, sizeof(struct Regiser_pair) * PEAK_REG_SET);
	peak_set[0].offset = PEAK_OFST; /* enable */
	peak_set[1].offset =
		PEAK_OFST + 0x10; /*hp_ratio; bp0_ratio;  bp1_ratio;*/
	peak_set[2].offset = PEAK_OFST + 0x20; /* gain*/
	peak_set[3].offset = PEAK_OFST + 0x24; /*  dif_up   beta  */
	peak_set[4].offset = PEAK_OFST + 0x28; /* neg_gain */
	peak_set[5].offset = PEAK_OFST + 0x2c; /* corth */
	arg[0] = (unsigned long)PQ_GET_REG;
	arg[1] = (unsigned long)de;
	arg[2] = (unsigned long)peak_set;
	arg[3] = (unsigned long)PEAK_REG_SET;
	PQ_Printf("ioctl");
	ret = ioctl(disp, DISP_PQ_PROC, (unsigned long)arg);
	if (ret) {
		PQ_Printf("err: DISP_PQ_PROC single failed: %d", __LINE__);
		free(peak_set);
		return ret;
	}
	peak_set[0].value &= 0xfffffffe;
	peak_set[0].value |= !!peak->enable;
	peak_set[1].value &= 0xff000000;
	peak_set[1].value |= (peak->hp_ratio << 16) + (peak->bp0_ratio << 8) +
			     peak->bp1_ratio;
	peak_set[2].value &= 0xffffff00;
	peak_set[2].value |= peak->gain;
	peak_set[3].value &= 0xff000000;
	peak_set[3].value |= (peak->dif_up << 16) + peak->beta;
	peak_set[4].value &= 0xffffffa0;
	peak_set[4].value |= peak->neg_gain;
	peak_set[5].value &= 0xffffff00;
	peak_set[5].value |= peak->corth;
	arg[0] = (unsigned long)PQ_SET_REG;
	arg[1] = (unsigned long)de;
	arg[2] = (unsigned long)peak_set;
	arg[3] = (unsigned long)PEAK_REG_SET;
	ret = ioctl(disp, DISP_PQ_PROC, (unsigned long)arg);
	if (ret) {
		PQ_Printf("err: DISP_PQ_PROC failed: %d", __LINE__);
	}

	free(peak_set);
	return ret;
}

static int peak_to_trans(int disp, int de, struct peak_sharp *peak)
{
	unsigned long arg[4] = { 0 };
	struct Regiser_pair *peak_set = NULL;
	int ret;

	peak_set = (struct Regiser_pair *)malloc(sizeof(struct Regiser_pair) *
						 PEAK_REG_SET);
	if (peak_set == NULL) {
		PQ_Printf("err: malloc Regiser_pair failed: %d", __LINE__);
		return -1;
	}
	memset(peak_set, 0, sizeof(struct Regiser_pair) * PEAK_REG_SET);
	peak_set[0].offset = PEAK_OFST; /*enble*/
	peak_set[1].offset =
		PEAK_OFST + 0x10; /*hp_ratio; bp0_ratio;  bp1_ratio;*/
	peak_set[2].offset = PEAK_OFST + 0x20; /* gain*/
	peak_set[3].offset = PEAK_OFST + 0x24; /*  dif_up   beta  */
	peak_set[4].offset = PEAK_OFST + 0x28; /* neg_gain */
	peak_set[5].offset = PEAK_OFST + 0x2c; /* corth */
	arg[0] = PQ_GET_REG;
	arg[1] = de;
	arg[2] = (unsigned long)peak_set;
	arg[3] = PEAK_REG_SET;
	ret = ioctl(disp, DISP_PQ_PROC, (unsigned long)arg);
	if (ret) {
		PQ_Printf("err: DISP_PQ_PROC single failed: %d", __LINE__);
		free(peak_set);
		return ret;
	}
	peak->enable = (peak_set[0].value) & 0x1;
	peak->hp_ratio = (peak_set[1].value >> 16) & 0x3f;
	peak->bp0_ratio = (peak_set[1].value >> 8) & 0x3f;
	peak->bp1_ratio = (peak_set[1].value) & 0x3f;
	peak->gain = peak_set[2].value & 0xff;
	peak->dif_up = (peak_set[3].value >> 16) & 0xff;
	peak->beta = peak_set[3].value & 0xff;
	peak->neg_gain = peak_set[4].value & 0x3f;
	peak->corth = peak_set[5].value & 0xff;

	free(peak_set);
	return 0;
}

static int tans_to_lti(int disp, int de, struct lti_sharp *lti)
{
	unsigned long arg[4] = { 0 };
	struct Regiser_pair *lti_set = NULL;
	int ret;

	lti_set = (struct Regiser_pair *)malloc(sizeof(struct Regiser_pair) *
						LTI_REG_SET);
	if (lti_set == NULL) {
		PQ_Printf("err: malloc Regiser_pair failed: %d", __LINE__);
		return -1;
	}
	memset(lti_set, 0, sizeof(struct Regiser_pair) * LTI_REG_SET);
	lti_set[0].offset =
		LTI_OFST; /* bit 0: enable,  bit 8: sel,  bit 16: nolinear_limmit_en */
	lti_set[1].offset =
		LTI_OFST +
		0x10; /* bit 0~7:cstm_coeff0, bit 16~23:cstm_coeff1 */
	lti_set[2].offset =
		LTI_OFST +
		0x14; /* bit 0~7:cstm_coeff2, bit 16~23:cstm_coeff3 */
	lti_set[3].offset = LTI_OFST + 0x18; /*  bit 0~7:cstm_coeff4  */
	lti_set[4].offset = LTI_OFST + 0x1c; /* bit 0~3: fir_gain */
	lti_set[5].offset = LTI_OFST + 0x20; /* bit 0~9:cor_th */
	lti_set[6].offset =
		LTI_OFST + 0x24; /* bit 0~7:diff_offset, bit 16~20:diff_slope */
	lti_set[7].offset = LTI_OFST + 0x28; /* bit 0~4:edge_gain */
	lti_set[8].offset =
		LTI_OFST +
		0x2c; /* bit 0~7:core_x, bit 16~23:clip_y, bit 28~30:peak_limmit */
	lti_set[9].offset = LTI_OFST + 0x30; /* bit 0~7:win_expansion */
	lti_set[10].offset = LTI_OFST + 0x34; /* bit 0~7:edge_level_th */
	arg[0] = (unsigned long)PQ_GET_REG;
	arg[1] = (unsigned long)de;
	arg[2] = (unsigned long)lti_set;
	arg[3] = (unsigned long)LTI_REG_SET;
	ret = ioctl(disp, DISP_PQ_PROC, (unsigned long)arg);
	if (ret) {
		PQ_Printf("err: DISP_PQ_PROC single failed: %d", __LINE__);
		free(lti_set);
		return ret;
	}
	lti_set[0].value &= 0xfffefefe;
	lti_set[0].value |= (!!lti->enable) |
			    (((unsigned int)(!!lti->sel)) << 8) |
			    ((unsigned int)!!lti->nolinear_limmit_en) << 16;
	lti_set[1].value &= 0xff00ff00;
	lti_set[1].value |= (((int)lti->cstm_coeff1) << 16) + lti->cstm_coeff0;
	lti_set[2].value &= 0xff00ff00;
	lti_set[2].value |= (((int)lti->cstm_coeff3) << 16) + lti->cstm_coeff2;
	lti_set[3].value &= 0xffffff00;
	lti_set[3].value |= lti->cstm_coeff4;
	lti_set[4].value &= 0xfffffff0;
	lti_set[4].value |= lti->fir_gain;
	lti_set[5].value &= 0xfffffc00;
	lti_set[5].value |= lti->cor_th;
	lti_set[6].value &= 0xff00ff00;
	lti_set[6].value |= ((int)lti->diff_slope) << 16 | lti->diff_offset;
	lti_set[7].value &= 0xfffffff0;
	lti_set[7].value |= lti->edge_gain;
	lti_set[8].value &= 0x8f00ff00;
	lti_set[8].value |= lti->core_x | (((unsigned int)lti->clip_y) << 16) |
			    (((unsigned int)lti->peak_limmit) << 28);
	lti_set[9].value &= 0xffffff00;
	lti_set[9].value |= lti->win_expansion;
	lti_set[10].value &= 0xffffff00;
	lti_set[10].value |= lti->edge_level_th;

	arg[0] = (unsigned long)PQ_SET_REG;
	arg[1] = (unsigned long)de;
	arg[2] = (unsigned long)lti_set;
	arg[3] = (unsigned long)LTI_REG_SET;
	ret = ioctl(disp, DISP_PQ_PROC, (unsigned long)arg);
	if (ret) {
		PQ_Printf("err: DISP_PQ_PROC failed: %d", __LINE__);
	}
	free(lti_set);
	return ret;
}

static int lti_to_trans(int disp, int de, struct lti_sharp *lti)
{
	unsigned long arg[4] = { 0 };
	struct Regiser_pair *lti_set = NULL;
	int ret;

	lti_set = (struct Regiser_pair *)malloc(sizeof(struct Regiser_pair) *
						LTI_REG_SET);
	if (lti_set == NULL) {
		PQ_Printf("err: malloc Regiser_pair failed: %d", __LINE__);
		return -1;
	}
	memset(lti_set, 0, sizeof(struct Regiser_pair) * LTI_REG_SET);
	lti_set[0].offset =
		LTI_OFST; /* bit 0: enable,  bit 8: sel,  bit 16: nolinear_limmit_en */
	lti_set[1].offset =
		LTI_OFST +
		0x10; /* bit 0~7:cstm_coeff0, bit 16~23:cstm_coeff1 */
	lti_set[2].offset =
		LTI_OFST +
		0x14; /* bit 0~7:cstm_coeff2, bit 16~23:cstm_coeff3 */
	lti_set[3].offset = LTI_OFST + 0x18; /*  bit 0~7:cstm_coeff4  */
	lti_set[4].offset = LTI_OFST + 0x1c; /* bit 0~3: fir_gain */
	lti_set[5].offset = LTI_OFST + 0x20; /* bit 0~9:cor_th */
	lti_set[6].offset =
		LTI_OFST + 0x24; /* bit 0~7:diff_offset, bit 16~20:diff_slope */
	lti_set[7].offset = LTI_OFST + 0x28; /* bit 0~4:edge_gain */
	lti_set[8].offset =
		LTI_OFST +
		0x2c; /* bit 0~7:core_x, bit 16~23:clip_y, bit 28~30:peak_limmit */
	lti_set[9].offset = LTI_OFST + 0x30; /* bit 0~7:win_expansion */
	lti_set[10].offset = LTI_OFST + 0x34; /* bit 0~7:edge_level_th */

	arg[0] = PQ_GET_REG;
	arg[1] = de;
	arg[2] = (unsigned long)lti_set;
	arg[3] = LTI_REG_SET;
	ret = ioctl(disp, DISP_PQ_PROC, (unsigned long)arg);
	if (ret) {
		PQ_Printf("err: DISP_PQ_PROC single failed: %d", __LINE__);
		free(lti_set);
		return ret;
	}
	lti->enable = (lti_set[0].value) & 0x1;
	lti->sel = !!(lti_set[0].value & 0x100);
	lti->nolinear_limmit_en = !!(lti_set[0].value & 0x10000);
	lti->cstm_coeff0 = lti_set[1].value & 0xff;
	lti->cstm_coeff1 = (lti_set[1].value >> 16) & 0xff;
	lti->cstm_coeff2 = lti_set[2].value & 0xff;
	lti->cstm_coeff3 = (lti_set[2].value >> 16) & 0xff;
	lti->cstm_coeff4 = lti_set[3].value & 0xff;
	lti->fir_gain = lti_set[4].value & 0xf;
	lti->cor_th = lti_set[5].value & 0x3ff;
	lti->diff_offset = lti_set[6].value & 0xff;
	lti->diff_slope = (lti_set[6].value >> 16) & 0x1f;
	lti->edge_gain = lti_set[7].value & 0x1f;
	lti->core_x = lti_set[8].value & 0xff;
	lti->clip_y = (lti_set[8].value >> 16) & 0xff;
	lti->peak_limmit = (lti_set[8].value >> 28) & 0x07;
	lti->win_expansion = lti_set[9].value & 0xff;
	lti->edge_level_th = lti_set[10].value & 0xff;
#if (PQ_DEBUG_LEVEL > 1)
	printf("Read:\n");
	printf("[%d, %d, %d]:%08x\n", lti->enable, lti->sel,
	       lti->nolinear_limmit_en, lti_set[0].value);
	printf("[%d, %d, %d, %d, %d]\n", lti->cstm_coeff0, lti->cstm_coeff1,
	       lti->cstm_coeff2, lti->cstm_coeff3, lti->cstm_coeff4);
	printf("[%d, %d, %d, %d]\n", lti->fir_gain, lti->cor_th,
	       lti->diff_offset, lti->diff_slope);
	printf("[%d, %d, %d]\n", lti->edge_gain, lti->core_x, lti->clip_y);
	printf("[%d, %d, %d]\n", lti->peak_limmit, lti->win_expansion,
	       lti->edge_level_th);
#endif
	free(lti_set);

	return 0;
}

static inline void matix_to_trans(struct matrix_user *us,
				  struct matrix_in_out *matrix)
{
	matrix->c00 = us->matrix.x00 / 4096.0;
	matrix->c01 = us->matrix.x01 / 4096.0;
	matrix->c02 = us->matrix.x02 / 4096.0;
	matrix->c10 = us->matrix.x10 / 4096.0;
	matrix->c11 = us->matrix.x11 / 4096.0;
	matrix->c12 = us->matrix.x12 / 4096.0;
	matrix->c20 = us->matrix.x20 / 4096.0;
	matrix->c21 = us->matrix.x21 / 4096.0;
	matrix->c22 = us->matrix.x22 / 4096.0;
	matrix->offset0 = us->matrix.x03 / 4096.0;
	matrix->offset1 = us->matrix.x13 / 4096.0;
	matrix->offset2 = us->matrix.x23 / 4096.0;
#if (PQ_DEBUG_LEVEL > 1)
	printf("matix_to_trans:\n");
	printf("[%f, %f, %f]\n", matrix->c00, matrix->c01, matrix->c02);
	printf("[%f, %f, %f]\n", matrix->c10, matrix->c11, matrix->c12);
	printf("[%f, %f, %f]\n", matrix->c20, matrix->c21, matrix->c22);
	printf("[%f, %f, %f]\n", matrix->offset0, matrix->offset1,
	       matrix->offset2);
#endif
}

struct matrix_user *trans_to_matix(struct matrix_in_out *matrix)
{
	struct matrix_user* mama = (struct matrix_user*)malloc(sizeof(struct matrix_user));

	if (mama == NULL) {
		PQ_Printf("err: malloc matrix4x4 failed: %d", __LINE__);
		return mama;
	}
	struct matrix4x4 *ma = &mama->matrix;
	mama->choice = matrix->space;
	ma->x00 = ceil((double)(matrix->c00) * 4096);
	ma->x01 = ceil((double)(matrix->c01) * 4096);
	ma->x02 = ceil((double)(matrix->c02) * 4096);
	ma->x03 = ceil((double)(matrix->offset0) * 4096);

	ma->x10 = ceil((double)(matrix->c10) * 4096);
	ma->x11 = ceil((double)(matrix->c11) * 4096);
	ma->x12 = ceil((double)(matrix->c12) * 4096);
	ma->x13 = ceil((double)(matrix->offset1) * 4096);

	ma->x20 = ceil((double)(matrix->c20) * 4096);
	ma->x21 = ceil((double)(matrix->c21) * 4096);
	ma->x22 = ceil((double)(matrix->c22) * 4096);
	ma->x23 = ceil((double)(matrix->offset2) * 4096);

	ma->x30 = 0;
	ma->x31 = 0;
	ma->x32 = 0;
	ma->x33 = 0x00001000;
#if (PQ_DEBUG_LEVEL > 1)
	printf("trans_to_matix:\n");
	printf("[%f, %f, %f]\n", matrix->c00, matrix->c01, matrix->c02);
	printf("[%f, %f, %f]\n", matrix->c10, matrix->c11, matrix->c12);
	printf("[%f, %f, %f]\n", matrix->c20, matrix->c21, matrix->c22);
	printf("[%f, %f, %f]\n", matrix->offset0, matrix->offset1,
	       matrix->offset2);
	printf("[%lld, %lld, %lld, %lld]\n", ma->x00, ma->x01, ma->x02,
	       ma->x03);
#endif
	return mama;
}

/*id same to /etc/sunxi_pq.cfg*/
int trans_to_spec(int disp_fd, int de, void *data, int cmd)
{
	int ret;
	struct trans_base *base_data = (struct trans_base *)(data);
	unsigned long arg[4] = { 0 };
	PQ_Printf("de:%d : d:%d  cmd:%d", de, base_data->id, cmd);

	switch (base_data->id) {
	case 11: { /*gamma LUT*/
		struct gamma_lut *lut;
		lut = (struct gamma_lut *)data;
		if (cmd == WRITE_CMD) {
			int *lut_d = trans_x_gamma(data, 256);
			ret = ioctl(disp_fd, DISP_LCD_GAMMA_CORRECTION_ENABLE,
				    arg);
			if (ret) {
				PQ_Printf(
					"err: DISP_LCD_GAMMA_CORRECTION_ENABLE failed: %d  %d",
					ret, __LINE__);
			}
			arg[0] = (unsigned long)de;
			arg[1] = (unsigned long)lut_d;
			arg[2] = (unsigned long)256 * 4;
			ret = ioctl(disp_fd, DISP_LCD_SET_GAMMA_TABLE,
				    (unsigned long)arg);
			if (ret) {
				PQ_Printf("err: %d set gamma failed: %d  %d",
					  disp_fd, ret, __LINE__);
			}
			free(lut_d);
		} else {
			unsigned int *lut_d =
				(unsigned int *)malloc(sizeof(int) * 256);
			if (lut_d == NULL) {
				PQ_Printf("malloc for gamma lut err...\n");
				return -1;
			}
			arg[0] = (unsigned long)de;
			arg[1] = (unsigned long)lut_d;
			arg[2] = (unsigned long)256 * 4;
			ret = ioctl(disp_fd, DISP_LCD_GET_GAMMA_TABLE,
				    (unsigned long)arg);
			if (ret) {
				PQ_Printf("err: %d set gamma failed: %d  %d",
					  disp_fd, ret, __LINE__);
			}
			gamma_x_trans(lut, 256, lut_d);
			free(lut_d);
		}

	} break;
	case 12: { /*peak*/
		PQ_Printf("start to test peak");
		struct peak_sharp *peak = (struct peak_sharp *)data;

		if (cmd == WRITE_CMD) {
			PQ_Printf("start to write");
			tans_to_peak(disp_fd, de, peak);
		} else {
			PQ_Printf("start to write");
			peak_to_trans(disp_fd, de, peak);
		}
		PQ_Printf("write peak success");
	} break;
	case 13: { /*lti*/
		struct lti_sharp *lti = (struct lti_sharp *)data;

		if (cmd == WRITE_CMD) {
			tans_to_lti(disp_fd, de, lti);
		} else {
			lti_to_trans(disp_fd, de, lti);
		}
	} break;
	case 14: { /*enhance*/
		struct color_enhance *color_enh = (struct color_enhance *)data;
		struct color_enhanc cm;
		cm.brightness = color_enh->brightness;
		cm.hue = color_enh->hue;
		cm.saturation = color_enh->saturation;
		cm.contrast = color_enh->contrast;
		cm.cmd = 0;
		cm.read = (cmd != WRITE_CMD);
		arg[0] = (unsigned long)PQ_COLOR_MATRIX;
		arg[1] = (unsigned long)de;
		arg[2] = (unsigned long)&cm;
		arg[3] = 0;
		ret = ioctl(disp_fd, DISP_PQ_PROC, (unsigned long)arg);
		if (ret) {
			PQ_Printf("err: %d set enhance failed: %d  %d", disp_fd,
				  ret, __LINE__);
		}
		if (cmd != WRITE_CMD) {
			color_enh->brightness = cm.brightness;
			color_enh->hue = cm.hue;
			color_enh->saturation = cm.saturation;
			color_enh->contrast = cm.contrast;
		}
	} break;
	case 15:
	case 16:
	{	
        /*matrix_in*/
		struct matrix_in_out *matrix_in_out = (struct matrix_in_out *)data;
		struct matrix_user* ma_us = NULL;

		ma_us = trans_to_matix(matrix_in_out);
		if (ma_us == NULL) {
			return -1;
		}
		if (ma_us->choice >= NUM_SUM) {
			PQ_Printf("err: %d get a err choice space: %d",
				  __LINE__, ma_us->choice);
			free(ma_us);
			return -1;
		}
		ma_us->cmd = matrix_in_out->id - 14; // 1 is in 2 is out
		ma_us->read = (cmd != WRITE_CMD);
		arg[0] = (unsigned long)PQ_COLOR_MATRIX;
		arg[1] = (unsigned long)de;
		arg[2] = (unsigned long)ma_us;
		arg[3] = 0;
		ret = ioctl(disp_fd, DISP_PQ_PROC, (unsigned long)arg);
		if (ret) {
			PQ_Printf("err: %d set matrix failed: %d  %d", disp_fd,
				  ret, __LINE__);
		}
		if (cmd != WRITE_CMD) {
			matix_to_trans(ma_us, matrix_in_out);
		}
		free(ma_us);
	} break;
	default:
		PQ_Printf("de20x got a wronge id:%d...", base_data->id);
	}
	return 0;
}

int updata_firm_spec(char* buf, int length, struct trans_base *data, int de)
{
	int i = 0, next = 0;
	switch(data->id) {
	case 11:
	{	/*gamma LUT*/
		struct gamma_lut *lut = (struct gamma_lut *)data;
        char *buff1 = (char *)malloc(4096);
        memset(buff1, 0, 4096);
        char buff2[5];
        printf("%d", (int)lut->value[i]);
        while ( i < 256*3 ) {
            sprintf(buff2, "%d,", (int)lut->value[i]);
			if (i == (256*3-1)) {
				sprintf(buff2, "%d", (int)lut->value[i]);
			}
            strcat(buff1, buff2);
            i++;
        }
		sprintf(buf, "[%d,%d]\n{%s}", data->id, de, buff1);
        free(buff1);
    }
	break;
	case 12:
	{	/*peak*/
		struct peak_sharp *peak = (struct peak_sharp *)data;
        char *buff1 = (char *)malloc(40);
        char *buff2 = (char *)malloc(40);
        sprintf(buff1, "%d,%d,%d,%d", peak->hp_ratio, peak->bp0_ratio, peak->bp1_ratio, peak->gain);
        sprintf(buff2, "%d,%d,%d,%d", peak->dif_up, peak->beta, peak->neg_gain, peak->corth);
        sprintf(buf, "[%d,%d]\n{%s,%s}", data->id, de, buff1, buff2);
        free(buff1);
        free(buff2);

	}
	break;
    case 13:
	{
        struct lti_sharp *lti = (struct lti_sharp *)data;
        char *buff1 = (char *)malloc(40);
        char *buff2 = (char *)malloc(40);
        char *buff3 = (char *)malloc(40);
        char *buff4 = (char *)malloc(40);
        sprintf(buff1, "%d,%d,%d,%d,%d", lti->enable, lti->cstm_coeff0, lti->cstm_coeff1, lti->cstm_coeff2, lti->cstm_coeff3);
        sprintf(buff2, "%d,%d,%d,%d", lti->cstm_coeff4, lti->fir_gain, lti->cor_th, lti->diff_slope);
        sprintf(buff3, "%d,%d,%d,%d,%d", lti->diff_offset, lti->edge_gain, lti->sel, lti->win_expansion, lti->nolinear_limmit_en);
        sprintf(buff4, "%d,%d,%d,%d", lti->peak_limmit, lti->core_x, lti->clip_y, lti->edge_level_th);
        sprintf(buf, "[%d,%d]\n{%s,%s,%s,%s}", data->id, de, buff1, buff2, buff3, buff4);
        free(buff1);
        free(buff2);
        free(buff3);
        free(buff4);
	}
	break;
    case 14:
	{
		/* get the de and ic version */
        struct color_enhance *color = (struct color_enhance *)data;
        sprintf(buf, "[%d,%d]\n{%d,%d,%d,%d}", data->id, de, color->contrast, color->brightness, color->saturation, color->hue);
		/* get the de and ic version */
	}
	break;
    case 15:
    case 16:
	{
        struct matrix_in_out *matrix = (struct matrix_in_out *)data;
        char *buff1 = (char *)malloc(100);
        char *buff2 = (char *)malloc(100);
        char *buff3 = (char *)malloc(100);
        char *buff4 = (char *)malloc(100);
        sprintf(buff1, "%d,%.3f,%.3f,%.3f", matrix->space, matrix->c00, matrix->c01, matrix->c02);
        sprintf(buff2, "%.3f,%.3f,%.3f", matrix->c10, matrix->c11, matrix->c12);
        sprintf(buff3, "%.3f,%.3f,%.3f", matrix->c20, matrix->c21, matrix->c22);
        sprintf(buff4, "%.3f,%.3f,%.3f", matrix->offset0, matrix->offset1, matrix->offset2);
        sprintf(buf,"[%d,%d]\n{%s,%s,%s,%s}", data->id, de, buff1, buff2, buff3, buff4);
        free(buff1);
        free(buff2);
        free(buff3);
        free(buff4);
	}
    break;
	default:
		PQ_Printf("de20x updata_firm_spec a wronge id:%d...", data->id);
	}
	length = strlen(buf);
	return length;

}

int download_firmware_spec(int disp_fd, int id, int de, char *buffer,
			   trans_firm_data_t func)
{
	int i = 0, j = 0, ret = -1, *data2 = NULL;
	long *data = NULL;
	unsigned long arg[4] = { 0 };
#if PQ_DEBUG_LEVEL
	PQ_Printf("init config id:%d", id);
#endif
	switch (id) {
	case 11: { /*gamma LUT*/
		struct gamma_lut *lut;
		data = func(buffer, &i);
		if (data == NULL) {
			PQ_Printf("de20x line[%d] Trans err..", __LINE__);
			return -1;
		}
		if (i != 256 * 3) {
			PQ_Printf("de20x give us a wrong number %d.", i);
		}

		lut = (struct gamma_lut *)malloc(sizeof(struct gamma_lut) +
						 sizeof(char) * i);

		lut->id = 11;
		while (j < i) {
			lut->value[j] = (char)data[j];
			j++;
		}
		data2 = trans_x_gamma(lut, i / 3);
		ret = ioctl(disp_fd, DISP_LCD_GAMMA_CORRECTION_ENABLE, arg);
		arg[0] = (unsigned long)de;
		arg[1] = (unsigned long)data2;
		arg[2] = (unsigned long)i / 3 * 4;
		ret = ioctl(disp_fd, DISP_LCD_SET_GAMMA_TABLE,
			    (unsigned long)arg);
		if (ret) {
			PQ_Printf(
				"err: DISP_LCD_SET_GAMMA_TABLE failed: %d  %d",
				ret, __LINE__);
		}

		free(data);
		free(data2);
		free(lut);
	} break;
	case 12: { /*peak*/
		struct peak_sharp *peak = NULL;
		data = func(buffer, &i);
		if (data == NULL) {
			PQ_Printf("de20x line[%d] Trans err..", __LINE__);
			return -1;
		}
		if (i != 8) {
			PQ_Printf("de20x give us a wrong peak %d.", i);
			return -1;
		}

		peak = (struct peak_sharp *)malloc(sizeof(struct peak_sharp));
		if (peak == NULL) {
			PQ_Printf("de20x line[%d] malloc mem err.", __LINE__);
			free(data);
			return -1;
		}
		peak->id = 12;
		peak->gain = (char)data[0];
		peak->neg_gain = (char)data[1];
		peak->hp_ratio = (char)data[2];
		peak->bp0_ratio = (char)data[3];
		peak->bp1_ratio = (char)data[4];
		peak->dif_up = (char)data[5];
		peak->corth = (char)data[6];
		peak->beta = (char)data[7];
		tans_to_peak(disp_fd, de, peak);
		free(peak);
		free(data);
	} break;
	case 13: { /*lti*/
		struct lti_sharp *lti = NULL;
		data = func(buffer, &i);
		if (data == NULL) {
			PQ_Printf("de20x line[%d] Trans err..", __LINE__);
			return -1;
		}
		if (i != 18) {
			PQ_Printf("de20x give us a wrong lti %d.", i);
			return -1;
		}

		lti = (struct lti_sharp *)malloc(sizeof(struct lti_sharp));
		if (lti == NULL) {
			PQ_Printf("de20x line[%d] malloc mem err.", __LINE__);
			free(data);
			return -1;
		}
		lti->enable = data[0];
		lti->cstm_coeff0 = data[1];
		lti->cstm_coeff1 = data[2];
		lti->cstm_coeff2 = data[3];
		lti->cstm_coeff3 = data[4];
		lti->cstm_coeff4 = data[5];
		lti->fir_gain = data[6];
		lti->cor_th = data[7];
		lti->diff_slope = data[8];
		lti->diff_offset = data[9];
		lti->edge_gain = data[10];
		lti->sel = data[11];
		lti->win_expansion = data[12];
		lti->nolinear_limmit_en = data[13];
		lti->peak_limmit = data[14];
		lti->core_x = data[15];
		lti->clip_y = data[16];
		lti->edge_level_th = data[17];
		tans_to_lti(disp_fd, de, lti);
		free(lti);
		free(data);
	} break;
	case 14: { /*color_enhance*/
		struct color_enhance *ce = NULL;
		data = func(buffer, &i);
		if (data == NULL) {
			PQ_Printf("de20x line[%d] Trans err..", __LINE__);
			return -1;
		}
		if (i != 4) {
			PQ_Printf("de20x give us a wrong peak %d.", i);
			return -1;
		}
		ce = (struct color_enhance *)malloc(
			sizeof(struct color_enhance));
		if (ce == NULL) {
			PQ_Printf("de20x line[%d] malloc mem err.", __LINE__);
			free(data);
			return -1;
		}
		ce->id = id;
		ce->contrast = (char)data[0];
		ce->brightness = (char)data[1];
		ce->saturation = (char)data[2];
		ce->hue = (char)data[3];
		trans_to_spec(disp_fd, de, ce, WRITE_CMD);
		free(ce);
		free(data);
	} break;
	case 15:
	case 16: { /*matrix*/
		struct matrix_in_out *ma = NULL;
		int ne[13];
		i = 0, j = 0;
		while (buffer[i] != '}') {
			if (buffer[i] == ',') {
				ne[j++] = i + 1;
			}
			i++;
		}
		if (j != 12) {
			PQ_Printf("de20x line[%d] give us a err config..",
				  __LINE__);
			return -1;
		}
		ma = (struct matrix_in_out *)malloc(
			sizeof(struct matrix_in_out));
		if (ma == NULL) {
			PQ_Printf("de20x line[%d] malloc mem err.", __LINE__);
			return -1;
		}
		ma->id = id;
		ma->space = atoi(&buffer[1]);
		ma->c00 = atof(&buffer[ne[0]]);
		ma->c01 = atof(&buffer[ne[1]]);
		ma->c02 = atof(&buffer[ne[2]]);
		ma->c10 = atof(&buffer[ne[3]]);
		ma->c11 = atof(&buffer[ne[4]]);
		ma->c12 = atof(&buffer[ne[5]]);
		ma->c20 = atof(&buffer[ne[6]]);
		ma->c21 = atof(&buffer[ne[7]]);
		ma->c22 = atof(&buffer[ne[8]]);
		ma->offset0 = atof(&buffer[ne[9]]);
		ma->offset1 = atof(&buffer[ne[10]]);
		ma->offset2 = atof(&buffer[ne[11]]);
		trans_to_spec(disp_fd, de, ma, WRITE_CMD);
		free(ma);
	} break;
	default:
		PQ_Printf("de20x got a wronge id:%d...", id);
	}
	return 0;
}
