/*
Copyright (c) 2020-2030 allwinnertech.com JetCui<cuiyuntao@allwinnertech.com>

*/
#include "ntfs/types.h"

#define PEAK_OFST 0xA6000	/* PEAKING offset based on RTMX */
#define LTI_OFST  0xA4000	/* LTI offset based on RTMX */
#define FCE_OFST  0xA0000	/* FCE offset based on RTMX */
#define ASE_OFST  0xA8000	/* ASE offset based on RTMX */
#define FCC_OFST  0xAA000	/* FCC offset based on RTMX */


struct gamma_global {
	int id;
	float gamma;
}__attribute__ ((packed));

struct gamma_pair {
	int id;
	char channel;
	char original;
	char value;
}__attribute__ ((packed));

struct gamma_lut {
	int id;
	char value[0];
}__attribute__ ((packed));

struct peak_sharp {
	int id;
	char enable;
	char gain;
	char neg_gain;
	char hp_ratio;
	char bp0_ratio;
	char bp1_ratio;
	char dif_up;
	char corth;
	char beta;
}__attribute__ ((packed));

struct lti_sharp {
	int id;
	char enable;
	char cstm_coeff0;
	char cstm_coeff1;
	char cstm_coeff2;
	char cstm_coeff3;
	char cstm_coeff4;
	char fir_gain;
	short cor_th;
	char diff_slope;
	char diff_offset;
	char edge_gain;
	char sel;
	char win_expansion;
	char nolinear_limmit_en;
	char peak_limmit;
	char core_x;
	char clip_y;
	char edge_level_th;
}__attribute__ ((packed));

struct matrix_in_out {
	int id;
	char space;
	float c00;
	float c01;
	float c02;
	float c10;
	float c11;
	float c12;
	float c20;
	float c21;
	float c22;
	float offset0;
	float offset1;
	float offset2;
}__attribute__ ((packed));

struct color_enhance {
	int id;
	char contrast;
	char brightness;
	char saturation;
	char hue;
}__attribute__ ((packed));

struct matrix4x4 {
	__s64 x00;
	__s64 x01;
	__s64 x02;
	__s64 x03;
	__s64 x10;
	__s64 x11;
	__s64 x12;
	__s64 x13;
	__s64 x20;
	__s64 x21;
	__s64 x22;
	__s64 x23;
	__s64 x30;
	__s64 x31;
	__s64 x32;
	__s64 x33;
};

enum choice{
	BT601_F2F,
	BT709_F2F,
	YCC,
	ENHANCE,
	NUM_SUM,
};

struct matrix_user {
	int cmd;
	int read;
	int choice;
	struct matrix4x4 matrix;
};

struct color_enhanc {
	int cmd;
	int read;
	int contrast;
	int brightness;
	int saturation;
	int hue;
};

struct color_matrix {
	int cmd;
	int read;
};


