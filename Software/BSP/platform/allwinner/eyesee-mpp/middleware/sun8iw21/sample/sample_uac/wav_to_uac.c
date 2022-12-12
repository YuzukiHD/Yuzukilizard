/*
 *作者：韦访
 *CSDN:https://blog.csdn.net/rookie_wei
 **/
#define ALSA_PCM_NEW_HW_PARAMS_API

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alsa/asoundlib.h>

#define u32 unsigned int
#define u8   unsigned char
#define u16 unsigned short


#pragma pack(push)
#pragma pack(1)     //1字节对齐

typedef  struct	{
	u32	dwSize;
	u16		wFormatTag;
	u16		wChannels;
	u32	dwSamplesPerSec;
	u32	dwAvgBytesPerSec;
	u16		wBlockAlign;
	u16		wBitsPerSample;
} WAVEFORMAT;

typedef  struct	{
	u8	RiffID [4];
	u32     RiffSize;
	u8	WaveID[4];
	u8	FmtID[4];
	u32     FmtSize;
	u16	wFormatTag;
	u16	nChannels;
	u32	nSamplesPerSec;  /*采样频率*/
	u32	nAvgBytesPerSec; /*每秒所需字节数*/
	u16		nBlockAlign; /*数据块对齐单位,每个采样需要的字节数*/
	u16		wBitsPerSample;/*每个采样需要的bit数*/
	u8		DataID[4];
	u32	nDataBytes;
} WAVE_HEADER;

#pragma pack(pop) /* 恢复先前的pack设置 */

WAVE_HEADER g_wave_header;
snd_pcm_t *gp_handle;  //调用snd_pcm_open打开PCM设备返回的文件句柄，后续的操作都使用是、这个句柄操作这个PCM设备
snd_pcm_hw_params_t *gp_params;  //设置流的硬件参数
snd_pcm_uframes_t g_frames;    //snd_pcm_uframes_t其实是unsigned long类型
char *gp_buffer;
u32 g_bufsize;

FILE * open_and_print_file_params(char *file_name)
{
	FILE * fp = fopen(file_name, "r");
	if (fp == NULL)
	{
		printf("can't open wav file\n");
		return NULL;
	}


	memset(&g_wave_header, 0, sizeof(g_wave_header));
	fread(&g_wave_header, 1, sizeof(g_wave_header), fp);

	printf("RiffID:%c%c%c%c\n", g_wave_header.RiffID[0], g_wave_header.RiffID[1], g_wave_header.RiffID[2], g_wave_header.RiffID[3]);
	printf("RiffSize:%d\n", g_wave_header.RiffSize);
	printf("WaveID:%c%c%c%c\n", g_wave_header.WaveID[0], g_wave_header.WaveID[1], g_wave_header.WaveID[2], g_wave_header.WaveID[3]);
	printf("FmtID:%c%c%c%c\n", g_wave_header.FmtID[0], g_wave_header.FmtID[1], g_wave_header.FmtID[2], g_wave_header.FmtID[3]);
	printf("FmtSize:%d\n", g_wave_header.FmtSize);
	printf("wFormatTag:%d\n", g_wave_header.wFormatTag);
	printf("nChannels:%d\n", g_wave_header.nChannels);
	printf("nSamplesPerSec:%d\n", g_wave_header.nSamplesPerSec);
	printf("nAvgBytesPerSec:%d\n", g_wave_header.nAvgBytesPerSec);
	printf("nBlockAlign:%d\n", g_wave_header.nBlockAlign);
	printf("wBitsPerSample:%d\n", g_wave_header.wBitsPerSample);
	printf("DataID:%c%c%c%c\n", g_wave_header.DataID[0], g_wave_header.DataID[1], g_wave_header.DataID[2], g_wave_header.DataID[3]);
	printf("nDataBytes:%d\n", g_wave_header.nDataBytes);

	return fp;
}

int set_hardware_params()
{
	int rc;
	/* Open PCM device for playback */
	rc = snd_pcm_open(&gp_handle, "hw:UAC1Gadget", SND_PCM_STREAM_PLAYBACK, 0);
	if (rc < 0)
	{
		printf("unable to open pcm device\n");
		return -1;
	}


	/* Allocate a hardware parameters object */
	snd_pcm_hw_params_alloca(&gp_params);

	/* Fill it in with default values. */
	rc = snd_pcm_hw_params_any(gp_handle, gp_params);
	if (rc < 0)
	{
		printf("unable to Fill it in with default values.\n");
		goto err1;
	}


	/* Interleaved mode */
	rc = snd_pcm_hw_params_set_access(gp_handle, gp_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (rc < 0)
	{
		printf("unable to Interleaved mode.\n");
		goto err1;
	}

	snd_pcm_format_t format;

	if (8 == g_wave_header.FmtSize)
	{
		format = SND_PCM_FORMAT_U8;
	}
	else if (16 == g_wave_header.FmtSize)
	{
		format = SND_PCM_FORMAT_S16_LE;
	}
	else if (24 == g_wave_header.FmtSize)
	{
		format = SND_PCM_FORMAT_U24_LE;
	}
	else if (32 == g_wave_header.FmtSize)
	{
		format = SND_PCM_FORMAT_U32_LE;
	}
	else
	{
		printf("SND_PCM_FORMAT_UNKNOWN.\n");
		format = SND_PCM_FORMAT_UNKNOWN;
		goto err1;
	}


	/* set format */
	rc = snd_pcm_hw_params_set_format(gp_handle, gp_params, format);
	if (rc < 0)
	{
		printf("unable to set format.\n");
		goto err1;
	}

	/* set channels (stero) */
	snd_pcm_hw_params_set_channels(gp_handle, gp_params, g_wave_header.nChannels);
	if (rc < 0)
	{
		printf("unable to set channels (stero).\n");
		goto err1;
	}

	/* set sampling rate */
	u32 dir, rate = g_wave_header.nSamplesPerSec;
	rc = snd_pcm_hw_params_set_rate_near(gp_handle, gp_params, &rate, &dir);
	if (rc < 0)
	{
		printf("unable to set sampling rate.\n");
		goto err1;
	}

	/* Write the parameters to the dirver */
	rc = snd_pcm_hw_params(gp_handle, gp_params);
	if (rc < 0) {
		printf("unable to set hw parameters: %s\n", snd_strerror(rc));
		goto err1;
	}

	snd_pcm_hw_params_get_period_size(gp_params, &g_frames, &dir);
	g_bufsize = g_frames * 4;
	gp_buffer = (u8 *)malloc(g_bufsize);
	if (gp_buffer == NULL)
	{
		printf("malloc failed\n");
		goto err1;
	}

	return 0;

err1:
	snd_pcm_close(gp_handle);
	return -1;
}


int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		printf("usage: %s filename.wav\n", argv[0]);
		return -1;
	}

	FILE * fp = open_and_print_file_params(argv[1]);
	int fd = open(argv[1], O_RDONLY);
	if (fp == NULL)
	{
		printf("open_and_print_file_params error\n");
		return -1;
	}

	int ret = set_hardware_params();
	if (ret < 0)
	{
		printf("set_hardware_params error\n");
		return -1;
	}

	size_t rc;
	while (1)
	{
		rc = fread(gp_buffer, g_bufsize, 1, fp);
		if (rc <1)
		{
			break;
		}

		ret = snd_pcm_writei(gp_handle, gp_buffer, g_frames);
		if (ret == -EPIPE) {
			printf("underrun occured\n");
			break;
		}
		else if (ret < 0) {
			printf("error from writei: %s\n", snd_strerror(ret));
			break;
		}
	}


	snd_pcm_drain(gp_handle);
	snd_pcm_close(gp_handle);
	free(gp_buffer);
	fclose(fp);
	return 0;
}
