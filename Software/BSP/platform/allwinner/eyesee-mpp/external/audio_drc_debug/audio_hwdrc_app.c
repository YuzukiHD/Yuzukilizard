/*
 * IPCLinuxPlatform/external/audio_drc_debug/audio_hwdrc_app.c
 *
 * Copyright (c) 2018 AllWinner Technology Co., Ltd. <www.allwinnertech.com>
 *
 * yumingfeng <yumingfeng@allwinnertech.com>
 *
 * This software used for debuging the drc function.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/select.h>
#include <signal.h>
#include <asm/types.h>
#include <errno.h>
#include <math.h>

#define ARRAY_SIZE_DIY(x) (sizeof(x)/sizeof(x[0]))
#define SUNXI_IO_MEMBASE (0x5096000)

struct sunxi_codec_drc {
	char *write_path;
	unsigned int base;
};

static struct sunxi_codec_drc *codec_drc;

struct reg_label {
	const char *name;
	unsigned int address;
	unsigned int value;
};

/* default value */
struct reg_label drc0_reglabel[] = {
	{"SUNXI_AC_DRC0_HOPL", 0x664, 0xFBD8},
	{"SUNXI_AC_DRC0_LOPL", 0x668, 0xFBA7},

	{"SUNXI_AC_DRC0_HOPC", 0x64C, 0xF95B},
	{"SUNXI_AC_DRC0_LOPC", 0x650, 0x2C3F},

	{"SUNXI_AC_DRC0_HOPE", 0x67C, 0xF45F},
	{"SUNXI_AC_DRC0_LOPE", 0x680, 0x8D6E},

	{"SUNXI_AC_DRC0_HLT", 0x654, 0x01A9},
	{"SUNXI_AC_DRC0_LLT", 0x658, 0x34F0},

	{"SUNXI_AC_DRC0_HCT", 0x63C, 0x06A4},
	{"SUNXI_AC_DRC0_LCT", 0x640, 0xD3C0},

	{"SUNXI_AC_DRC0_HET", 0x66C, 0x0BA0},
	{"SUNXI_AC_DRC0_LET", 0x670, 0x7291},

	{"SUNXI_AC_DRC0_HKI", 0x65C, 0x0005},
	{"SUNXI_AC_DRC0_LKI", 0x660, 0x1EB8},

	{"SUNXI_AC_DRC0_HKC", 0x644, 0x0080},
	{"SUNXI_AC_DRC0_LKC", 0x648, 0x0000},

	{"SUNXI_AC_DRC0_HKN", 0x684, 0x0100},
	{"SUNXI_AC_DRC0_LKN", 0x688, 0x0000},

	{"SUNXI_AC_DRC0_HKE", 0x674, 0x0500},
	{"SUNXI_AC_DRC0_LKE", 0x678, 0x0000},

	{"SUNXI_AC_DRC0_MXGHS", 0x69C, 0xFE56},
	{"SUNXI_AC_DRC0_MXGLS", 0x6A0, 0xCB0F},

	{"SUNXI_AC_DRC0_MNGHS", 0x6A4, 0xF95B},
	{"SUNXI_AC_DRC0_MNGLS", 0x6A8, 0x2C3F},
};

/*==============================================*/
/*               DRC caculator                  */
/*==============================================*/
/* CTin */
/* LTin */
/* ETin */
static double tdb_to_tin(int tdb)
{
	return tdb/6.0206;
}

/* AT */
static double threshold_parameter_caculator(double ts, double ta)
{
	return 1-(exp(-2.2*ts/ta));
}

/* RT */
static double time_parameter_caculator(double ts, double tr)
{
	return exp(-2.2*ts/tr);
}

/* KI: 50:1 */
/* Kc: 2:1 */
/* Ke: 1:5 */
static double slope_parameter_caculator(double n)
{
	return (double)1/n;
}

static int math_2_expo_caculator(int n)
{
	int val = 1;
	return (val << n);
}

/*
 * format:
 * (1)3.24 for AT & RT
 * (2)8.24 for CTin & ETin
 * (3)6.24 for Kc & Kn
 */
static unsigned int kx_to_conversion_uint(int integer, int decimal,
					double n)
{
	if (n < 0)
		return floor((slope_parameter_caculator(n) +
				math_2_expo_caculator(integer)) *
			math_2_expo_caculator(decimal));
	else
		return floor((slope_parameter_caculator(n)) *
			math_2_expo_caculator(decimal));
}

static unsigned int tin_to_conversion_uint(int integer, int decimal,
					int tdb)
{
	if (tdb < 0)
		return floor((tdb_to_tin(tdb) + math_2_expo_caculator(integer)) *
			math_2_expo_caculator(decimal));
	else
		return floor((tdb_to_tin(tdb)) *
			math_2_expo_caculator(decimal));
}

static unsigned int at_to_conversion_uint(int integer, int decimal,
					int ts, int ta)
{
	return floor((threshold_parameter_caculator(ts, ta) +
				math_2_expo_caculator(integer)) *
			math_2_expo_caculator(decimal));
}

static unsigned int rt_to_conversion_uint(int integer, int decimal,
					int ts, int tr)
{
	return floor((time_parameter_caculator(ts, tr) +
				math_2_expo_caculator(integer)) *
			math_2_expo_caculator(decimal));
}

static int sunxi_dump_fwrite(char *path, unsigned int address,
			     unsigned int value)
{
	FILE * stream = NULL;
	char str_cmd[72] = {0};
	size_t len = 0;

	assert(path);
	stream = fopen(path, "w");
	if (!stream) {
		fprintf(stderr, "Cannot open:%s\n", path);
		return -EINVAL;
	}

	sprintf(str_cmd, "0x%x 0x%x", address, value);
	len = strlen(str_cmd);

	if (len != fwrite(str_cmd, 1, len, stream)) {
		fprintf(stderr, "[err] ------>fwrite size: %d\n", (int)len);
		fclose(stream);
		return -EINVAL;
	}

	fclose(stream);

	return 0;
}

static void sunxi_drc_reglabels_show(struct reg_label *reg_label, int size)
{
	int i = 0;
	for (i = 0; i < size; i++) {
		printf("[%s : 0x%x] ---> value:0x%x\n",
			reg_label[i].name, reg_label[i].address,
			reg_label[i].value);
	}
}

static int sunxi_drclabels_get_index_by_name(struct reg_label *reg_label,
					unsigned int size, char *name)
{
	int i = 0;

	for (i = 0; i < size; i++) {
		if (!strcmp(reg_label[i].name, name))
			break;
	}
	if (i >= size)
		return -1;

	return i;
}

static void sunxi_drclabels_update_by_name(struct reg_label *reg_label,
	unsigned int size, char *name, unsigned int value, unsigned int high)
{
	int index = 0;

	if (high > 1)
		return;

	index = sunxi_drclabels_get_index_by_name(reg_label, size, name);
	if (index < 0)
		fprintf(stderr, "[%s] cannot get the index\n", name);
	else if (high == 1)
		reg_label[index].value = (value >> 0x10) & 0xFFFF;
	else if (high == 0)
		drc0_reglabel[index].value = value & 0xFFFF;
}

static void sunxi_hwdrc_setup(struct sunxi_codec_drc *codec_drc)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE_DIY(drc0_reglabel); i++)
		sunxi_dump_fwrite(codec_drc->write_path,
				codec_drc->base + drc0_reglabel[i].address,
				drc0_reglabel[i].value);
}

static void line_test_caculator(char *argv[])
{
	int sunxi_drc_debug = atoi(argv[1]);
	int OPC, OPL, OPE;
	int CT, LT, ET;
	int KIr, KE;
	int MXG, MNG;

	if (sunxi_drc_debug) {
		//OPL > OPC > OPE
		OPL = atoi(argv[2]);//-25;//atoi(argv[2])
		OPC = atoi(argv[3]);//-40;//atoi(argv[3])
		OPE = atoi(argv[4]);//-70;//atoi(argv[4])

		//LT > CT > ET;
		LT = atoi(argv[5]);//-10;//atoi(argv[5])
		CT = atoi(argv[6]);//-40;//atoi(argv[6])
		ET = atoi(argv[7]);//-70;//atoi(argv[7])

		KIr = atoi(argv[8]);//50;//atof(argv[8])
		KE = atoi(argv[9]);//5;//atof(argv[9])

		MXG = atoi(argv[10]);//-10;//-20 to 30; atoi(argv[10])
		MNG = atoi(argv[11]);//-40;//-60 to -40;	atoi(argv[11])
	} else {
		OPL = -25;//atoi(argv[2])
		OPC = -40;//atoi(argv[3])
		OPE = -70;//atoi(argv[4])

		//LT > CT > ET;
		LT = -10;//atoi(argv[5])
		CT = -40;//atoi(argv[6])
		ET = -70;//atoi(argv[7])

		KIr = 50;//atof(argv[8])
		KE = 5;//atof(argv[9])

		MXG = -10;//-20 to 30; atoi(argv[10])
		MNG = -40;//-60 to -40;	atoi(argv[11])
	}
	// Ke > Kn > Kc > Ki
	double KC = (double)(OPL-OPC)/(double)(LT-CT);//(-25-(-40))/(-10-(-40))=15/30;
	double KN = (double)(OPC-OPE)/(double)(CT-ET);//(-40 - (-70))/(-40-(-70)) = 30/30;

	double KCr = slope_parameter_caculator(KC);
	double KNr = slope_parameter_caculator(KN);

	double KI = slope_parameter_caculator(KIr);//0.02
	double KEr = slope_parameter_caculator(KE);//0.2

	fprintf(stdout, "=================================\n");
	fprintf(stdout, "OPL:%d, OPC:%d, OPE:%d\n", OPL, OPC, OPE);
	fprintf(stdout, "LT:%d, CT:%d, ET:%d\n", LT, CT, ET);
	fprintf(stdout, "KI:%lf, KIr:%d, KC:%lf, KCr:%lf\n", KI, KIr, KC, KCr);
	fprintf(stdout, "KN:%lf, KNr:%lf, KE:%d, KEr:%lf\n", KN, KNr, KE, KEr);
	fprintf(stdout, "MXG[-20 -> 30]:%d, MNG[-60 -> -40]:%d\n", MXG, MNG);

	/* default value */
	unsigned int temp_value = 0;
	int index = 0;
	unsigned int reglabel_size = ARRAY_SIZE_DIY(drc0_reglabel); 

	struct reg_label *drc_reglabel = drc0_reglabel;

	fprintf(stdout, "=================================\n");
	/* SUNXI_AC_DRC0_HOPL & SUNXI_AC_DRC0_LOPL*/
	temp_value = tin_to_conversion_uint(8, 24, OPL);
	fprintf(stdout, "OPL: 0x%x\n", temp_value);
	sunxi_drclabels_update_by_name(drc0_reglabel, reglabel_size,
				"SUNXI_AC_DRC0_HOPL", temp_value, 1);
	sunxi_drclabels_update_by_name(drc0_reglabel, reglabel_size,
				"SUNXI_AC_DRC0_LOPL", temp_value, 0);

	/* SUNXI_AC_DRC0_HOPC & SUNXI_AC_DRC0_LOPC*/
	temp_value = tin_to_conversion_uint(8, 24, OPC);
	fprintf(stdout, "OPC: 0x%x\n", temp_value);
	sunxi_drclabels_update_by_name(drc0_reglabel, reglabel_size,
				"SUNXI_AC_DRC0_HOPC", temp_value, 1);
	sunxi_drclabels_update_by_name(drc0_reglabel, reglabel_size,
				"SUNXI_AC_DRC0_LOPC", temp_value, 0);

	/* SUNXI_AC_DRC0_HOPE & SUNXI_AC_DRC0_LOPE*/
	temp_value = tin_to_conversion_uint(8, 24, OPE);
	fprintf(stdout, "OPE: 0x%x\n", temp_value);
	sunxi_drclabels_update_by_name(drc0_reglabel, reglabel_size,
				"SUNXI_AC_DRC0_HOPE", temp_value, 1);
	sunxi_drclabels_update_by_name(drc0_reglabel, reglabel_size,
				"SUNXI_AC_DRC0_LOPE", temp_value, 0);

	fprintf(stdout, "=================================\n");
	/* SUNXI_AC_DRC0_HLT & SUNXI_AC_DRC0_LLT*/
	temp_value = tin_to_conversion_uint(8, 24, -LT);
	fprintf(stdout, "LT: 0x%x\n", temp_value);
	sunxi_drclabels_update_by_name(drc0_reglabel, reglabel_size,
				"SUNXI_AC_DRC0_HLT", temp_value, 1);
	sunxi_drclabels_update_by_name(drc0_reglabel, reglabel_size,
				"SUNXI_AC_DRC0_LLT", temp_value, 0);
	/* SUNXI_AC_DRC0_HCT & SUNXI_AC_DRC0_LCT*/
	temp_value = tin_to_conversion_uint(8, 24,  -CT);
	fprintf(stdout, "CT: 0x%x\n", temp_value);
	sunxi_drclabels_update_by_name(drc0_reglabel, reglabel_size,
				"SUNXI_AC_DRC0_HCT", temp_value, 1);
	sunxi_drclabels_update_by_name(drc0_reglabel, reglabel_size,
				"SUNXI_AC_DRC0_LCT", temp_value, 0);

	/* SUNXI_AC_DRC0_HET & SUNXI_AC_DRC0_LET*/
	temp_value = tin_to_conversion_uint(8, 24, -ET);
	fprintf(stdout, "ET: 0x%x\n", temp_value);
	sunxi_drclabels_update_by_name(drc0_reglabel, reglabel_size,
				"SUNXI_AC_DRC0_HET", temp_value, 1);
	sunxi_drclabels_update_by_name(drc0_reglabel, reglabel_size,
				"SUNXI_AC_DRC0_LET", temp_value, 0);

	fprintf(stdout, "=================================\n");
	/* SUNXI_AC_DRC0_HKI & SUNXI_AC_DRC0_LKI */
	temp_value = kx_to_conversion_uint(6, 24, KIr);
	fprintf(stdout, "KI: 0x%x\n", temp_value);
	sunxi_drclabels_update_by_name(drc0_reglabel, reglabel_size,
				"SUNXI_AC_DRC0_HKI", temp_value, 1);
	sunxi_drclabels_update_by_name(drc0_reglabel, reglabel_size,
				"SUNXI_AC_DRC0_LKI", temp_value, 0);

	/* SUNXI_AC_DRC0_HKC & SUNXI_AC_DRC0_LKC */
	temp_value = kx_to_conversion_uint(6, 24, KCr);
	fprintf(stdout, "KC: 0x%x\n", temp_value);
	sunxi_drclabels_update_by_name(drc0_reglabel, reglabel_size,
				"SUNXI_AC_DRC0_HKC", temp_value, 1);
	sunxi_drclabels_update_by_name(drc0_reglabel, reglabel_size,
				"SUNXI_AC_DRC0_LKC", temp_value, 0);

	/* SUNXI_AC_DRC0_HKN & SUNXI_AC_DRC0_LKN */
	temp_value = kx_to_conversion_uint(6, 24, KNr);
	fprintf(stdout, "KN: 0x%x\n", temp_value);
	sunxi_drclabels_update_by_name(drc0_reglabel, reglabel_size,
				"SUNXI_AC_DRC0_HKN", temp_value, 1);
	sunxi_drclabels_update_by_name(drc0_reglabel, reglabel_size,
				"SUNXI_AC_DRC0_LKN", temp_value, 0);

	/* SUNXI_AC_DRC0_HKE & SUNXI_AC_DRC0_LKE */
	temp_value = kx_to_conversion_uint(6, 24, KEr);
	fprintf(stdout, "KE: 0x%x\n", temp_value);
	sunxi_drclabels_update_by_name(drc0_reglabel, reglabel_size,
				"SUNXI_AC_DRC0_HKE", temp_value, 1);
	sunxi_drclabels_update_by_name(drc0_reglabel, reglabel_size,
				"SUNXI_AC_DRC0_LKE", temp_value, 0);

	fprintf(stdout, "=================================\n");
	/* SUNXI_AC_DRC0_MXGHS & SUNXI_AC_DRC0_MXGLS*/
	temp_value = tin_to_conversion_uint(8, 24, MXG);
	fprintf(stdout, "MXG: 0x%x\n", temp_value);
	sunxi_drclabels_update_by_name(drc0_reglabel, reglabel_size,
				"SUNXI_AC_DRC0_MXGHS", temp_value, 1);
	sunxi_drclabels_update_by_name(drc0_reglabel, reglabel_size,
				"SUNXI_AC_DRC0_MXGLS", temp_value, 0);

	/* SUNXI_AC_DRC0_MNGHS & SUNXI_AC_DRC0_MNGLS*/
	temp_value = tin_to_conversion_uint(8, 24, MNG);
	fprintf(stdout, "MNG: 0x%x\n", temp_value);
	sunxi_drclabels_update_by_name(drc0_reglabel, reglabel_size,
				"SUNXI_AC_DRC0_MNGHS", temp_value, 1);
	sunxi_drclabels_update_by_name(drc0_reglabel, reglabel_size,
				"SUNXI_AC_DRC0_MNGLS", temp_value, 0);

	fprintf(stdout, "=================================\n");

}

static void test_default_val(void)
{
	/* default value */
	fprintf(stdout, "=================================\n");
	/* SUNXI_AC_DRC0_HOPC & SUNXI_AC_DRC0_LOPC*/
	fprintf(stdout, "OPC: 0x%x\n", tin_to_conversion_uint(8, 24, -40));
	/* SUNXI_AC_DRC0_HOPL & SUNXI_AC_DRC0_LOPL*/
	fprintf(stdout, "OPL: 0x%x\n", tin_to_conversion_uint(8, 24, -25));
	/* SUNXI_AC_DRC0_HOPE & SUNXI_AC_DRC0_LOPE*/
	fprintf(stdout, "OPE: 0x%x\n", tin_to_conversion_uint(8, 24, -70));

	fprintf(stdout, "=================================\n");
	/* SUNXI_AC_DRC0_HCT & SUNXI_AC_DRC0_LCT*/
	fprintf(stdout, "CT:0x%x\n", tin_to_conversion_uint(8, 24, -(-40)));
	/* SUNXI_AC_DRC0_HLT & SUNXI_AC_DRC0_LLT*/
	fprintf(stdout, "LT: 0x%x\n", tin_to_conversion_uint(8, 24, -(-10)));
	/* SUNXI_AC_DRC0_HET & SUNXI_AC_DRC0_LET*/
	fprintf(stdout, "ET: 0x%x\n", tin_to_conversion_uint(8, 24, -(-70)));
	fprintf(stdout, "=================================\n");

	/* SUNXI_AC_DRC0_HKC & SUNXI_AC_DRC0_LKC */
	fprintf(stdout, "KC: 0x%x\n", kx_to_conversion_uint(6, 24, 2));
	/* SUNXI_AC_DRC0_HKI & SUNXI_AC_DRC0_LKI */
	fprintf(stdout, "KI: 0x%x\n", kx_to_conversion_uint(6, 24, 50));
	/* SUNXI_AC_DRC0_HKN & SUNXI_AC_DRC0_LKN */
	fprintf(stdout, "KN: 0x%x\n", kx_to_conversion_uint(6, 24, 1));

	fprintf(stdout, "=================================\n");
	/* SUNXI_AC_DRC0_MXGHS & SUNXI_AC_DRC0_MXGLS*/
	fprintf(stdout, "MXG: 0x%x\n", tin_to_conversion_uint(8, 24, -10));
	/* SUNXI_AC_DRC0_MNGHS & SUNXI_AC_DRC0_MNGLS*/
	fprintf(stdout, "MNG: 0x%x\n", tin_to_conversion_uint(8, 24, -40));
	fprintf(stdout, "=================================\n");
	/* SUNXI_AC_DRC0_HKE & SUNXI_AC_DRC0_LKE */
	fprintf(stdout, "0x%x\n", kx_to_conversion_uint(6, 24, 0.2));
	fprintf(stdout, "=================================\n\n");

}

int main(int argc, char *argv[])
{
	int ret = 0;

	printf("argc:%d, argv[0]:%s\n", argc, argv[0]);

	if ((argc != 2) && (argc != 12)) {
		printf("argc:%d, argv[0]:%s, the argc != 2 && argc != 12 !!!\n",
			argc, argv[0]);
		printf("argv[0]:%s\n", argv[0]);
		printf("argv[1]: debug(1 or 0)\n");
		printf("argv[2]: OPL\n");
		printf("argv[3]: OPC\n");
		printf("argv[4]: OPE\n");
		printf("argv[5]: LT\n");
		printf("argv[6]: CT\n");
		printf("argv[7]: ET\n");
		printf("argv[8]: KIr\n");
		printf("argv[9]: KE\n");
		printf("argv[10]: MXG\n");
		printf("argv[11]: MNG\n");
		return -1;
	} else if ((argc == 2) && (atoi(argv[1]) == 1)) {
		return 0;
	}
	codec_drc = malloc(sizeof(struct sunxi_codec_drc));
	memset(codec_drc, 0, sizeof(struct sunxi_codec_drc));

	/* by sunxi_dump interface */
	codec_drc->write_path = "/sys/class/sunxi_dump/write";
	codec_drc->base = SUNXI_IO_MEMBASE;

//	test_default_val();
	line_test_caculator(argv);

	sunxi_hwdrc_setup(codec_drc);
	sunxi_drc_reglabels_show(drc0_reglabel, ARRAY_SIZE_DIY(drc0_reglabel));

	free(codec_drc);

	return 0;
}
