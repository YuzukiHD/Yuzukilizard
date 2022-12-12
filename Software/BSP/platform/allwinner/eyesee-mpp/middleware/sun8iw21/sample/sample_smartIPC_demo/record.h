#ifndef __RECORD_H__
#define __RECORD_H__

#define MAX_RECORD_NUM		4
#define MAX_RECORD_FILE_PATH_LEN	256

#include <stdint.h>

typedef enum RECORD_FRAME_TYPE_E
{
	RECORD_FRAME_TYPE_I = 0,
	RECORD_FRAME_TYPE_P,
}RECORD_FRAME_TYPE_E;

typedef struct RECORD_FRAME_S
{
	unsigned char *mpFrm;
	unsigned int mFrmSize;
	RECORD_FRAME_TYPE_E mFrameType;
	uint64_t mPts;
}RECORD_FRAME_S;

typedef struct RECORD_CONFIG_S
{
	int mVeChn;
	int mRecordHandler; //start from 0
	int mMaxFileCnt;
	uint64_t mMaxFileDuration;  //unit: us
	char mFilePath[MAX_RECORD_FILE_PATH_LEN];
	void (*requestKeyFrame)(int nVeChn);
}RECORD_CONFIG_S;

int record_init(void);

int record_set_config(RECORD_CONFIG_S *pConfig);

int record_send_data(int nRecordHandler, RECORD_FRAME_S *pFrm);

int record_exit(void);

#endif
