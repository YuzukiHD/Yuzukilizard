//
// Created by A on 2016/6/22.
//

#include "IPCSource.h"
#include <linux/videodev2.h>
#include <sys/time.h>
#include <iostream>

extern "C" {
#include "encode.h"
};

IPCSource::IPCSource(UsageEnvironment &env, const char *dev)
    : FramedSource(env), _callback(NULL), _encode((Encode *)calloc(1, sizeof(Encode))), _camera(new BYL::CameraV4l2())
{

    int width = 320;
    int height = 240;

    _encode->in_w = width;
    _encode->in_h = height;  //宽高

    ffmpeg_init(_encode);

    _camera->setCaptureSize(width, height);
    _camera->setCaptureFormat(V4L2_PIX_FMT_YUYV);
    _camera->openDevice(dev);
    //	_camera->saveFrameData(std::string("test.jpg"));
}

IPCSource::~IPCSource()
{
    ffmpeg_encode_end(_encode);
    free(_encode);
    delete _camera;
}

IPCSource *IPCSource::createNew(UsageEnvironment &env, const char *dev) { return new IPCSource(env, dev); }

void IPCSource::doGetNextFrame() { encode_data(); }

void IPCSource::encode_data()
{
    static int pts = 0;

    do {
        int dummy;
        av_init_packet(&_encode->pkt);
        _encode->pkt.data = NULL;  // packet data will be allocated by the _encoder
        _encode->pkt.size = 0;
        while (_camera->read_frame(_encode->src_data[0], dummy)) usleep(30);  //util the data is prepared
        sws_scale(_encode->sws_ctx, (const uint8_t *const *)_encode->src_data, _encode->src_linesize, 0,
                  _encode->pCodecCtx->height, _encode->picture->data, _encode->picture->linesize);
        //PTS
        _encode->picture->pts = pts++;
        //编码
        int got_picture = 0;
        int ret = avcodec_encode_video2(_encode->pCodecCtx, &_encode->pkt, _encode->picture, &got_picture);

        if (ret < 0) {
            printf("编码错误！\n");
            handleClosure();
            return;
        }

        if (got_picture == 1) {
            if (_callback != NULL) {
                uint8_t *buff = NULL;
                int buffSize = 0;
                int a = av_bitstream_filter_filter(_encode->bsfc, _encode->pCodecCtx, NULL, &buff, &buffSize,
                                                   _encode->pkt.data, _encode->pkt.size, 0);
                if (a < 0) {
                    fprintf(stderr, "bitstream_filter failed!\n");
                } else {
                    _callback(buff, buffSize);
                }
            }
            av_free_packet(&_encode->pkt);
            break;
        }
        av_free_packet(&_encode->pkt);
    } while (1);
}

void IPCSource::setCompleteCallback(DidCompleteEncode function) { _callback = function; }
