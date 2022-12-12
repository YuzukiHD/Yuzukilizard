#define LOG_NDEBUG 0
#define LOG_TAG "TextEncApi"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tmessage.h>
#include <tsemaphore.h>

#include "TextEnc_debug.h"
#include "TextEncApi.h" 


#define TEXT_ENCRYPT(byte) (byte ^ 0xAA)

/* data position and length definition in packet */
#define DRIVER_ID_POS   0x19
#define DRIVER_ID_LEN   14      // 8 -> 14 in enc packet

#define DATE_TIME_POS   0x0a
#define DATE_TIME_LEN   14

#define GPS_GC_POS      0x28    // geographical coordinates(dir+val)
#define GPS_GC_LEN      19      // 1 + 8 + 1 + 9

#define GPS_SPEED_POS   0x40
#define GPS_SPEED_LEN   3

#define GSENSOR_POS     0xaf    // GSensor data
#define GSENSOR_LEN     12

#define EXTRA_GSENSOR_POS 0xc0  // extra gsensor data(cnt=video_frame_rate)

#define TEXTENC_END_FLAG_POS    (TEXTENC_PACKET_SIZE - 2)
//#define TEXTENC_END_FLAG_VAL    (0xFF<<8 | 0xFD)
#define TEXTENC_END_FLAG_VAL    ((0x100 - (TEXTENC_END_FLAG_POS>>8))<<8 | ((0x100 - TEXTENC_END_FLAG_POS)&0x00ff))


/* must clear TextEncDataManager after RecordDone */
int ClearDataMgr(void *handle)
{
    TextEncDataManager *pDataMgr = (TextEncDataManager *)(((TEXTENCCONTENT_t*)handle)->priv_data);

// clear input buf
    pthread_mutex_lock(&pDataMgr->in_buf_lock);
    memset(&pDataMgr->in_buf_mgr, 0, sizeof(InFrameManager_t));
    pthread_mutex_unlock(&pDataMgr->in_buf_lock);

// clear output buf
    pthread_mutex_lock(&pDataMgr->out_buf_lock);
    memset(&pDataMgr->out_buf_mgr, 0, sizeof(OutFrameManager_t));
    pthread_mutex_unlock(&pDataMgr->out_buf_lock);

//    pDataMgr->last_pts = 0;
//    pDataMgr->enc_start_pts = 0;

    return 0;
}

/* print info before get_packet, after encode */
static int ShowTextEncMgrData(void *handle)
{
    TextEncDataManager *pDataMgr = (TextEncDataManager *)(((TEXTENCCONTENT_t*)handle)->priv_data);
    InFrameManager_t *pInBufMgr = &pDataMgr->in_buf_mgr;
    OutFrameManager_t *pOutBufMgr = &pDataMgr->out_buf_mgr;

    int wid = pInBufMgr->write_id;
    wid = (wid - 1 + TEXTENC_PACKET_CNT) % TEXTENC_PACKET_CNT;
    GPS_t *gps_ptr = &pInBufMgr->in_pkt[wid].gps_data;
    GSENSOR_t *gsr_ptr = (GSENSOR_t*)&pInBufMgr->in_pkt[wid].gsensor_data;

    ALOGD("=================[show MgrData]====================\n");
    ALOGD("[in_frame ] gps(lat: %c%s, long: %c%s, speed: %s), gps(rid: %d, wid: %d, cnt: %d)\n",
        gps_ptr->lat_dir, gps_ptr->lat_val, gps_ptr->long_dir, gps_ptr->long_val, gps_ptr->speed,
        pInBufMgr->read_id, pInBufMgr->write_id, pInBufMgr->buf_used);
    ALOGD("[in_frame ] gsensor(xa: %f, ya: %f, za: %f), gsensor(rid: %d, wid: %d, cnt: %d)\n",
        gsr_ptr->xa, gsr_ptr->ya, gsr_ptr->za, pInBufMgr->read_id, pInBufMgr->write_id, pInBufMgr->buf_used);
    ALOGD("[in_frame ] driver_id(%s)\n", pInBufMgr->in_pkt[wid].driver_id_data.driver_id);

    ALOGD("[out_frame] packet(rid: %d, wid: %d, used_cnt: %d), total_packet_cnt: %d\n",
        pOutBufMgr->read_id, pOutBufMgr->write_id, pOutBufMgr->buf_used, pOutBufMgr->total_packet_cnt);
    wid = (pOutBufMgr->write_id  - 1 + TEXTENC_PACKET_CNT) % TEXTENC_PACKET_CNT;    // the pkt that be encoded just now
    ALOGD("[last_pkt ] last_encode_pkt info(wid: %d, buf_id: %d, pts: %lld)\n",
        wid, pOutBufMgr->out_pkt[wid].buf_id, pOutBufMgr->out_pkt[wid].pts);
    ALOGD("===================================================\n");

    return 0;
}

/* raw text data is sent to encoder's input-buf */
static int RequestWriteBuf(void *handle, void *pInBuf,int size)
{
    TextEncDataManager *pDataMgr = (TextEncDataManager *)(((TEXTENCCONTENT_t*)handle)->priv_data);
    InFrameManager_t *pInBufMgr = &pDataMgr->in_buf_mgr;
    TextPacket_t *cur_pkt = NULL;
    char *cur_buff = NULL;
    int wid = 0;

    pthread_mutex_lock(&pDataMgr->in_buf_lock);
    if (pInBufMgr->buf_used >= TEXTENC_PACKET_CNT) {
        ALOGE("(f:%s, l:%d) Low speed card! InBuf fifo full, discard this frame! InBuf_info(rid:%d, wid:%d, used_cnt:%d)",
            __FUNCTION__, __LINE__, pInBufMgr->read_id, pInBufMgr->write_id, pInBufMgr->buf_used);
        pthread_mutex_unlock(&pDataMgr->in_buf_lock);
        return -1;
    }

    
    wid = pInBufMgr->write_id;
    cur_pkt = &pInBufMgr->in_pkt[wid]; 
    cur_buff = (char *)pInBuf;
    if(0<size && NULL != cur_buff) // only gps info
    {
        memset(cur_pkt,0,sizeof(TextPacket_t));
        memcpy(&cur_pkt->gps_data,cur_buff,sizeof(RMCINFO));

    }

//    ALOGE("zjx_gps_data2:%x-%x-%x-%d-%d-%d-%x-%x-%x-%x",
//        cur_pkt->gps_data.Hour,cur_pkt->gps_data.Minute,cur_pkt->gps_data.Second,\
//        cur_pkt->gps_data.Year,cur_pkt->gps_data.Month,cur_pkt->gps_data.Day,\
//        cur_pkt->gps_data.Latitude,cur_pkt->gps_data.Longitude,cur_pkt->gps_data.Speed,cur_pkt->gps_data.Angle);

    
//    memcpy(, pInBuf, size/*sizeof(TextPacket_t)*/);
    pInBufMgr->buf_used++;
    pInBufMgr->write_id++;
    pInBufMgr->write_id %= TEXTENC_PACKET_CNT;
    pthread_mutex_unlock(&pDataMgr->in_buf_lock);

    return 0;
}

static int ShowInFrameData(void *handle)
{
    TextEncDataManager *pDataMgr = (TextEncDataManager *)(((TEXTENCCONTENT_t*)handle)->priv_data);
    InFrameManager_t *pInBufMgr = &pDataMgr->in_buf_mgr;
    int enable_flag = pDataMgr->info.enc_type_enable_flag;

    if (TEXTENC_PACK_TYPE_GPS == (enable_flag & (1<<TEXTENC_INPUT_TYPE_GPS))) {
        GPS_t *ptr = &pInBufMgr->in_pkt[pInBufMgr->read_id].gps_data;
        ALOGD("(f:%s, l:%d) gps: (%c%s, %c%s, %s)\n", __FUNCTION__, __LINE__,
            ptr->lat_dir, ptr->lat_val, ptr->long_dir, ptr->long_val, ptr->speed);
    }
    if (TEXTENC_PACK_TYPE_GSENSOR == (enable_flag & (1<<TEXTENC_INPUT_TYPE_GSENSOR))) {
        GSENSOR_t *ptr = &pInBufMgr->in_pkt[pInBufMgr->read_id].gsensor_data[0];
        ALOGD("(f:%s, l:%d) gsensor: (%f, %f, %f)\n", __FUNCTION__, __LINE__, ptr->xa, ptr->ya, ptr->za);
    }
    if (TEXTENC_PACK_TYPE_ADAS == (enable_flag & (1<<TEXTENC_INPUT_TYPE_ADAS))) {

    }
    if (TEXTENC_PACK_TYPE_DRIVER_ID == (enable_flag & (1<<TEXTENC_INPUT_TYPE_DRIVER_ID))) {
        char *ptr = pInBufMgr->in_pkt[pInBufMgr->read_id].driver_id_data.driver_id;
        ALOGD("(f:%s, l:%d) driver_id: (%s)\n", __FUNCTION__, __LINE__, ptr);
    }

    return 0;
}

static int PacketFillDateTime(char *ptr)
{
    time_t timer;
    struct tm *tp;
    char buf[DATE_TIME_LEN+1];

    time(&timer);
    tp = localtime(&timer);
    sprintf(buf, "%04d%02d%02d%02d%02d%02d", tp->tm_year + 1900, tp->tm_mon + 1, tp->tm_mday, tp->tm_hour, tp->tm_min, tp->tm_sec);
    *(ptr-2) = 0xFE;
    *(ptr-1) = 0xFE;
    memcpy(ptr, buf, DATE_TIME_LEN);
//  ALOGD("(f:%s, l:%d) data&time: [%s]", __FUNCTION__, __LINE__, buf);

    return 0;
}

static int PacketFillDriverId(char *ptr, char *pDriverId)
{
    *(ptr - 1) = 0x0C;
    memcpy(ptr, pDriverId, DRIVER_ID_LEN);

    return 0;
}

static int PacketFillGPS(char *ptr, GPS_t *pGps)
{
    char buf[GPS_GC_LEN + 1] = {0};
    if (pGps->lat_dir == 0 || pGps->long_dir == 0) {
        return 0;
    }
    buf[0] = pGps->lat_dir;
    memcpy(buf+1, pGps->lat_val, 8);    // '.' has been deleted
    buf[9] = pGps->long_dir;
    memcpy(buf+10, pGps->long_val, 9);
    *(ptr + GPS_GC_POS - 1) = 0x03;
    memcpy(ptr + GPS_GC_POS, buf, GPS_GC_LEN);
    memcpy(ptr + GPS_SPEED_POS, pGps->speed, GPS_SPEED_LEN);

    return 0;
}

static int PacketFillGSensor(char *ptr, GSENSOR_t *pGSensor)
{
    char buf[8];

    sprintf(buf, "%+0.2f", pGSensor->xa);
    ptr[0] = buf[0];
    ptr[1] = buf[1];
    ptr[2] = buf[3];
    ptr[3] = buf[4];

    sprintf(buf, "%+0.2f", pGSensor->ya);
    ptr[4] = buf[0];
    ptr[5] = buf[1];
    ptr[6] = buf[3];
    ptr[7] = buf[4];

    sprintf(buf, "%+0.2f", pGSensor->za);
    ptr[8] = buf[0];
    ptr[9] = buf[1];
    ptr[10] = buf[3];
    ptr[11] = buf[4];

//  memcpy(pGSensor, 0, 12);    // do NOT clear it!
    return 0;
}

static int PacketFillExtraGSensor(char *ptr, GSENSOR_t *pGSensor, int gsr_cnt)
{
    memcpy(ptr, pGSensor, gsr_cnt * sizeof(GSENSOR_t));
    return 0;
}

static int FillOneFrame(TextEncDataManager *pDataMgr, char *dst)    // dst:buffer addr of outpkt
{
    InFrameManager_t *pInBufMgr = &pDataMgr->in_buf_mgr;
    int enable_flag = pDataMgr->info.enc_type_enable_flag;
    int rid, cnt;

    pthread_mutex_lock(&pDataMgr->in_buf_lock);
    cnt = pInBufMgr->buf_used;
    rid = pInBufMgr->read_id;
    
    if (pDataMgr->base_pts == 0) {
        pDataMgr->base_pts = pInBufMgr->in_pkt[rid].pts;
    }
    pDataMgr->pts = pInBufMgr->in_pkt[rid].pts - pDataMgr->base_pts;
    if (TEXTENC_PACK_TYPE_GPS == (enable_flag & (1 << TEXTENC_INPUT_TYPE_GPS))) {
        RMCINFO *pGps = &pInBufMgr->in_pkt[rid].gps_data;
        memcpy(dst,pGps,sizeof(RMCINFO));
//        PacketFillGPS(dst, pGps);
    }
//    if (TEXTENC_PACK_TYPE_GSENSOR == (enable_flag & (1 << TEXTENC_INPUT_TYPE_GSENSOR))) {
//        GSENSOR_t *pGsr = (GSENSOR_t*)&pInBufMgr->in_pkt[rid].gsensor_data;
//        PacketFillGSensor(dst + GSENSOR_POS, pGsr);
//        if (gsr_cnt)
//            PacketFillExtraGSensor(dst + EXTRA_GSENSOR_POS, pGsr, gsr_cnt);
//    }
//  if (TEXTENC_PACK_TYPE_ADAS == (enable_flag & (1 << TEXTENC_INPUT_TYPE_ADAS))) {
//          PacketFillAdasData(dst + ADAS_POS, pAdas);      // fill adas
//  }
//    if (TEXTENC_PACK_TYPE_DRIVER_ID == (enable_flag & (1 << TEXTENC_INPUT_TYPE_DRIVER_ID))) {
//        char *pDriverId = pInBufMgr->in_pkt[rid].driver_id_data.driver_id;   // write driver_id only once, then keep it till destroy encoder
//        if (strlen(pDriverId) != 0)
//            PacketFillDriverId(dst + DRIVER_ID_POS, pDriverId);
//    }
    pInBufMgr->buf_used--;
    pInBufMgr->read_id++;
    pInBufMgr->read_id %= TEXTENC_PACKET_CNT;
    pthread_mutex_unlock(&pDataMgr->in_buf_lock);

//    PacketFillDateTime(dst + DATE_TIME_POS);    // fill time

//    *dst = TEXTENC_PACKET_SIZE>>8;
//    *(dst+1) = (TEXTENC_PACKET_SIZE-2)&0xFF;
//
//    *(dst+TEXTENC_END_FLAG_POS) = TEXTENC_END_FLAG_VAL>>8;      // 0xff
//    *(dst+TEXTENC_END_FLAG_POS+1) = TEXTENC_END_FLAG_VAL&0xff;  // 0xfd

    return 0;
}

// convert decimal system to senary system
static int ConvertNumSystem(char *ptr)
{
    char ret = 0;
    int val = atoi(ptr);

    ret = '0' + val*6/1000000;
    val = (val - (ret - '0') * 1000000 / 6) * 10;
    memset(ptr, 0, 8);
    sprintf(ptr, "%d", val);

    return ret;
}

static int PacketGCTransform(char *ptr)
{
    int i = 0;
    char dst[7] = {0};
    char buf[8] = {0};

    if (*(ptr-1) != 0x03) {     // GPS is NULL, NOT need to transform
        return 0;
    }
    memcpy(buf, ptr + 3, 6);
    for (i = 0; i < 6; i++)
        dst[i] = ConvertNumSystem(buf);
    memcpy(ptr + 3, dst, 6);
//  ALOGD("(f:%s, l:%d) after 10-6 transform, gps-latitude-decimal_part: [%s]", __FUNCTION__, __LINE__, dst);

    memset(buf, 0, 8);
    memcpy(buf, ptr + 13, 6);
    for (i = 0; i < 6; i++)
        dst[i] = ConvertNumSystem(buf);
    memcpy(ptr + 13, dst, 6);
//  ALOGD("(f:%s, l:%d) after 10-6 transform, gps-longitude-decimal_part: [%s]", __FUNCTION__, __LINE__, dst);

    return 0;
}

static int EncryptOneFrame(char *ptr)
{
    unsigned int i;

//    PacketGCTransform(ptr + GPS_GC_POS);
    for (i = 2; i < TEXTENC_PACKET_SIZE - 2; i++) {
        *(ptr + i) = TEXT_ENCRYPT(*(ptr + i));
    }
    *(ptr + 2) = 0;     // forbidden subtitle display in general player
    *(ptr + 3) = 0;

    return 0;
}

static int TextEncodePro(void *handle)
{
    TextEncDataManager *pDataMgr = (TextEncDataManager *)(((TEXTENCCONTENT_t*)handle)->priv_data);
    InFrameManager_t *pInBufMgr = &pDataMgr->in_buf_mgr;
    OutFrameManager_t *pOutBufMgr = &pDataMgr->out_buf_mgr;

    char *dst;
    int enable_flag = pDataMgr->info.enc_type_enable_flag;
    int ret = 0, overflow_flag = 0;

    pthread_mutex_lock(&pDataMgr->in_buf_lock);
    if (pInBufMgr->buf_used == 0) {
        pthread_mutex_unlock(&pDataMgr->in_buf_lock);
        return ERR_TEXT_ENC_INPUT_UNDERFLOW;
    }
    pthread_mutex_unlock(&pDataMgr->in_buf_lock);

    pthread_mutex_lock(&pDataMgr->out_buf_lock);
    if (pOutBufMgr->buf_used >= TEXTENC_PACKET_CNT) {
        pthread_mutex_unlock(&pDataMgr->out_buf_lock);
        return ERR_TEXT_ENC_OUTFRM_UNDERFLOW;
    }
    pthread_mutex_unlock(&pDataMgr->out_buf_lock);

    int wid = pOutBufMgr->write_id;

StartTextEncode:
    dst = pOutBufMgr->out_pkt[wid].out_data;
    memset(dst, 0, TEXTENC_PACKET_SIZE);
    FillOneFrame(pDataMgr, dst);
//    EncryptOneFrame(dst);

    pthread_mutex_lock(&pDataMgr->out_buf_lock);
    pOutBufMgr->out_pkt[wid].buf_id= wid;//pOutBufMgr->total_packet_cnt++;
    pOutBufMgr->out_pkt[wid].pts = pDataMgr->pts;
    pOutBufMgr->write_id++;
    pOutBufMgr->write_id %= TEXTENC_PACKET_CNT;
    pOutBufMgr->buf_used++;
    pOutBufMgr->prefetch_cnt++;
    pthread_mutex_unlock(&pDataMgr->out_buf_lock);

    return ERR_TEXT_ENC_NONE;
}

static int GetValidEncPktCnt(void *handle)
{
    TextEncDataManager *pDataMgr = (TextEncDataManager *)(((TEXTENCCONTENT_t*)handle)->priv_data);
    OutFrameManager_t *pOutBufMgr = &pDataMgr->out_buf_mgr;
    int ret = 0;

    pthread_mutex_lock(&pDataMgr->out_buf_lock);
    ret = pOutBufMgr->prefetch_cnt;
    if (ret != 0) {
        ALOGW("(f:%s, l:%d) Be careful! InBuf_info(rid:%d, wid:%d, used:%d, prefetch_cnt:%d)", __FUNCTION__, __LINE__,
            pOutBufMgr->read_id, pOutBufMgr->write_id, pOutBufMgr->buf_used, pOutBufMgr->prefetch_cnt);
    }
    pthread_mutex_unlock(&pDataMgr->out_buf_lock);

    return ret;
}

static int GetValidInputPktCnt(void *handle)
{
    TextEncDataManager *pDataMgr = (TextEncDataManager *)(((TEXTENCCONTENT_t*)handle)->priv_data);
    int ret = 0;

    pthread_mutex_lock(&pDataMgr->in_buf_lock);
    ret = pDataMgr->in_buf_mgr.buf_used;
    pthread_mutex_unlock(&pDataMgr->in_buf_lock);

    return ret;
}

static int GetEmptyOutFrameCnt(void *handle)
{
    TextEncDataManager *pDataMgr = (TextEncDataManager *)(((TEXTENCCONTENT_t*)handle)->priv_data);
    int ret = 0;

    pthread_mutex_lock(&pDataMgr->out_buf_lock);
    ret = TEXTENC_PACKET_CNT - pDataMgr->out_buf_mgr.buf_used;
    pthread_mutex_unlock(&pDataMgr->out_buf_lock);

    return ret;
}

/* print enced-packet data after encode, before getencbuf */
int ShowOutFrameData(void *handle, int cur_rid, int pkt_id)
{
    TextEncDataManager *pDataMgr = (TextEncDataManager *)(((TEXTENCCONTENT_t*)handle)->priv_data);
    OutFrameManager_t *pOutBufMgr = &pDataMgr->out_buf_mgr;

    unsigned int i;
    char *ptr;

    ALOGD("============[show OutFrame]============\n");
    ALOGD("PacketInfo -> (cnt, rid, wid): (%d, %d, %d), enced_pkt_cnt: %d\n", pOutBufMgr->buf_used,
        pOutBufMgr->read_id, pOutBufMgr->write_id, pOutBufMgr->total_packet_cnt);
    if (cur_rid == 1) {     // 1: show packet[out_buf_packet_rid]; 0: show packet[pkt_id]
        ptr = pOutBufMgr->out_pkt[pOutBufMgr->read_id].out_data;
        for (i = 0; i < TEXTENC_PACKET_SIZE; i++) {
            if (i%16 == 0)
                ALOGD("%#5x: ", i);
            ALOGD("%2x%c", (unsigned char)ptr[i], i%16 == 15 ? '\n':' ');
        }
    } else if (cur_rid == 0 && pkt_id < TEXTENC_PACKET_CNT) {
        ptr = pOutBufMgr->out_pkt[pkt_id].out_data;
        for (i = 0; i < TEXTENC_PACKET_SIZE; i++) {
            if (i%16 == 0)
                ALOGD("%#5x: ", i);
            ALOGD("%2x%c", (unsigned char)ptr[i], i%16 == 15 ? '\n':' ');
        }
    } else {
        ALOGE("(%s) param error! cur_rid: [%d], pkt_id: [%d]\n", __FUNCTION__, cur_rid, pkt_id);
    }
    ALOGD("\n=======================================\n");

    return 0;
}

static int RequestTextEncBuf(void *handle, void ** pOutbuf, unsigned int * outSize, long long * timeStamp, int* pBufId)
{
    TextEncDataManager *pDataMgr = (TextEncDataManager *)(((TEXTENCCONTENT_t*)handle)->priv_data);
    OutFrameManager_t *pOutBufMgr = &pDataMgr->out_buf_mgr;

    pthread_mutex_lock(&pDataMgr->out_buf_lock);
    if (pOutBufMgr->prefetch_cnt <= 0) {
        ALOGE("(f:%s, l:%d) get EncBuf failed! prefetch_cnt = %d!\n", __FUNCTION__, __LINE__, pOutBufMgr->prefetch_cnt);
        pthread_mutex_unlock(&pDataMgr->out_buf_lock);
        return -1;
    }
    int rid = pOutBufMgr->read_id++;
    pOutBufMgr->read_id %= TEXTENC_PACKET_CNT;

    *pOutbuf = pOutBufMgr->out_pkt[rid].out_data;
    *outSize = sizeof(RMCINFO);//TEXTENC_PACKET_SIZE;       // Now only support gps data
    *timeStamp = pOutBufMgr->out_pkt[rid].pts;
    *pBufId = pOutBufMgr->out_pkt[rid].buf_id;
    pOutBufMgr->prefetch_cnt--;
//    ALOGE("zjx_rq_text:%d-%d-%lld",pOutBufMgr->out_pkt[rid].buf_id,rid,pOutBufMgr->out_pkt[rid].pts);
    pthread_mutex_unlock(&pDataMgr->out_buf_lock);

    return 0;
}

/* return the last buf that get by RequestTextEncBuf just now, return last's last pkt will fail */
static int ReturnTextEncBuf(void *handle, void* pOutbuf, unsigned int outSize, long long timeStamp, int nBufId)
{
    TextEncDataManager *pDataMgr = (TextEncDataManager *)(((TEXTENCCONTENT_t*)handle)->priv_data);
    OutFrameManager_t *pOutBufMgr = &pDataMgr->out_buf_mgr;

    pthread_mutex_lock(&pDataMgr->out_buf_lock);
    int rid = pOutBufMgr->read_id;
    rid = (rid - 1 + TEXTENC_PACKET_CNT) % TEXTENC_PACKET_CNT;
    EncPacket_t *pEncData = &pOutBufMgr->out_pkt[rid];
    if (nBufId != pOutBufMgr->out_pkt[rid].buf_id || timeStamp != pOutBufMgr->out_pkt[rid].pts) {
        ALOGE("(f:%s, l:%d) return param error! OutBuf_info(pts:%lld, BufId:%d), Return_info(size:%d, pts:%lld, BufId:%d)!\n",
            __FUNCTION__, __LINE__, pEncData->pts, pEncData->buf_id, outSize, timeStamp, nBufId);
        pthread_mutex_unlock(&pDataMgr->out_buf_lock);
        return -1;
    }
    pOutBufMgr->read_id = rid;
    pthread_mutex_unlock(&pDataMgr->out_buf_lock);

    return 0;
}

static int ReleaseTextEncBuf(void *handle, void* pOutbuf, unsigned int outSize, long long timeStamp, int nBufId)
{
    TextEncDataManager *pDataMgr = (TextEncDataManager *)(((TEXTENCCONTENT_t*)handle)->priv_data);
    OutFrameManager_t *pOutBufMgr = &pDataMgr->out_buf_mgr;
    int ret = 0;

    pthread_mutex_lock(&pDataMgr->out_buf_lock);
    if (pOutBufMgr->buf_used == 0) {
        pthread_mutex_unlock(&pDataMgr->out_buf_lock);
        ALOGE("(f:%s, l:%d) fatal error! no valid fifo to release!", __FUNCTION__, __LINE__);
        return -1;
    }
    int pos = ((char*)pOutbuf - (char*)pOutBufMgr->out_pkt) / sizeof(EncPacket_t);
    EncPacket_t *pEncData = &pOutBufMgr->out_pkt[pos];

//    ALOGE("zjx_rls_text:%d-%d-%lld-%d-%lld",pOutBufMgr->out_pkt[pos].buf_id,pos,pOutBufMgr->out_pkt[pos].pts,
//        nBufId,timeStamp);

    
    if (pEncData->buf_id != nBufId || pEncData->pts != timeStamp) {
        ALOGE("(f:%s, l:%d) release pkt verify failed! OutBuf_info(pts:%lld, bufid:%d), Release_info(size:%d, pts:%lld, bufid:%d)",
            __FUNCTION__, __LINE__, pEncData->pts, pEncData->buf_id, outSize, timeStamp, nBufId);
//        ret = -1;
    }
    pOutBufMgr->buf_used--;
    pthread_mutex_unlock(&pDataMgr->out_buf_lock);

    return ret;
}

TEXTENCCONTENT_t *TextEncInit(__text_enc_info_t *pTEncInfo)
{
    TEXTENCCONTENT_t* htext_enc = (TEXTENCCONTENT_t*)malloc(sizeof(TEXTENCCONTENT_t));
    if (htext_enc == NULL) {
        ALOGE("malloc TEXTENCCONTENT_t failed!");
        return NULL;
    }
    memset(htext_enc, 0, sizeof(TEXTENCCONTENT_t));

    TextEncDataManager *pDataMgr = (TextEncDataManager *)malloc(sizeof(TextEncDataManager));
    if (pDataMgr == NULL) {
        ALOGE("malloc TextEncDataManager failed!");
        goto EXIT;
    }
    htext_enc->priv_data = pDataMgr;
    memset(pDataMgr, 0, sizeof(TextEncDataManager));
    memcpy(&pDataMgr->info, pTEncInfo, sizeof(__text_enc_info_t));
    pthread_mutex_init(&pDataMgr->in_buf_lock, NULL);
    pthread_mutex_init(&pDataMgr->out_buf_lock, NULL);

//  htext_enc->ClearDataMgr = ClearDataMgr;
//  htext_enc->ShowTextEncMgrData = ShowTextEncMgrData;
    htext_enc->RequestWriteBuf = RequestWriteBuf;           // src_out_port -> enc_in_port
//  htext_enc->ShowInFrameData = ShowInFrameData;
    htext_enc->TextEncodePro = TextEncodePro;               // enc_in_port -> enc_out_port
    htext_enc->GetValidEncPktCnt = GetValidEncPktCnt;
    htext_enc->GetValidInputPktCnt = GetValidInputPktCnt;   // get cnt which causes InputUnderflow
    htext_enc->GetEmptyOutFrameCnt = GetEmptyOutFrameCnt;
//  htext_enc->ShowOutFrameData = ShowOutFrameData;
    htext_enc->RequestTextEncBuf = RequestTextEncBuf;       // enc_out_port -> recrender_in_port
    htext_enc->ReturnTextEncBuf = ReturnTextEncBuf;         // return the pkt just now get
    htext_enc->ReleaseTextEncBuf = ReleaseTextEncBuf;       // verify for release in OutBuf
    return htext_enc;

EXIT:
    free(htext_enc);
    return NULL;
}

void TextEncExit(void *handle)
{
    TEXTENCCONTENT_t* htext_enc = (TEXTENCCONTENT_t*)handle;
    LOGV("(f:%s, l:%d) free TextEncoder!\n", __FUNCTION__, __LINE__);
    if (htext_enc->priv_data != NULL) {
        TextEncDataManager *pDataMgr = (TextEncDataManager *)(((TEXTENCCONTENT_t*)handle)->priv_data);
        pthread_mutex_destroy(&pDataMgr->in_buf_lock);
        pthread_mutex_destroy(&pDataMgr->out_buf_lock);
        free(pDataMgr);
    }
    free(htext_enc);
}
