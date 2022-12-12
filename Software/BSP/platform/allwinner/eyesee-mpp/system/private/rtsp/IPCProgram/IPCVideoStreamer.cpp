/**********
 This library is free software; you can redistribute it and/or modify it under
 the terms of the GNU Lesser General Public License as published by the
 Free Software Foundation; either version 2.1 of the License, or (at your
 option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

 This library is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 more details.

 You should have received a copy of the GNU Lesser General Public License
 along with this library; if not, write to the Free Software Foundation, Inc.,
 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 **********/
// Copyright (c) 1996-2016, Live Networks, Inc.  All rights reserved
// A test program that reads a H.264 Elementary Stream video file
// and streams it using RTP
// main program
//
// NOTE: For this application to work, the H.264 Elementary Stream video file *must* contain SPS and PPS NAL units,
// ideally at or near the start of the file.  These SPS and PPS NAL units are used to specify 'configuration' information
// that is set in the output stream's SDP description (by the RTSP server that is built in to this application).
// Note also that - unlike some other "*Streamer" demo applications - the resulting stream can be received only using a
// RTSP client (such as "openRTSP")
#include <pthread.h>
#include <signal.h>
#include <BasicUsageEnvironment.hh>
#include <fstream>
#include <iostream>

#include "IPCSource.h"
#include "MediaStream.h"
#include "TinyServer.h"
#include "UsageEnvironment.hh"

TinyServer* tinyServer = NULL;
MediaStream* stream1 = NULL;
//MediaStream *stream2 = NULL;
IPCSource* source1 = NULL;
//IPCSource *source2 = NULL;

char* ip;

bool running = true;

using namespace std;

ofstream* fin;

void testCallback1(unsigned char* data, unsigned int size)
{
    stream1->appendVideoData(data, size);
    // fin->write((const char*)data, size);
}

void testCallback2(unsigned char* data, unsigned int size)
{
    //	stream2->appendH264data(data+4, size-4);
}

void* captureLoop(void* source)
{
    for (; running;) {
        ((IPCSource*)source)->doGetNextFrame();
    }
    fprintf(stderr, "the capture loop is over!\n");
}

void* testDelayQueueBug(void* args) { ((MediaStream*)args)->appendVideoData(NULL, 0); }

void* testStop(void* args)
{
    tinyServer->stop();
    running = false;
    fprintf(stderr, "operation : try to stop the server!");
    fin->close();
}

void sig_callback(int signum)
{
    switch (signum) {
        case SIGINT:
            testStop(NULL);
            break;
        default:
            printf("Unknown signal %d. \r\n", signum);
            break;
    }

    return;
}

void* doServerLoop(void*)
{
    stream1 = tinyServer->createMediaStream(
      "testStream1", {
                       MediaStream::MediaStreamAttr::VIDEO_TYPE_H264, MediaStream::MediaStreamAttr::AUDIO_TYPE_AAC,
                       MediaStream::MediaStreamAttr::STREAM_TYPE_UNICAST,
                     });
    tinyServer->run();
    fprintf(stderr, "==================================\n");
}

void runOnce()
{
    fin = new ofstream("capture.h264");
    tinyServer = TinyServer::createServer(ip, 8554);
    //    tinyServer = TinyServer::createServer();

    int i = 1;
    while (i--) {
        running = true;
        pthread_t tids[3];
        int ret[3];
        ret[0] = pthread_create(&tids[0], NULL, doServerLoop, NULL);
        if (ret[0] != 0) {
            cout << "pthread_create doServerLoop error: error_code=" << ret << "\n";
        }
        sleep(1);
        ret[1] = pthread_create(&tids[1], NULL, captureLoop, source1);
        if (ret[1] != 0) {
            cout << "pthread_create captureLoop error: error_code=" << ret << "\n";
        }

        cout << "test url interface: " << stream1->streamURL() << endl;
        pthread_join(tids[0], NULL);
        pthread_join(tids[1], NULL);

        delete stream1;
    }

    delete tinyServer;

    cout << "stop over!" << endl;
}

int main(int argc, char** argv)
{
    ip = argv[1];

    signal(SIGINT, sig_callback);
    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    UsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler);
    source1 = IPCSource::createNew(*env, "/dev/video0");
    source1->setCompleteCallback(testCallback1);

    do {
        runOnce();
    } while (0);

    Medium::close(source1);
    delete scheduler;
    if (!env->reclaim()) {
        cout << "env reclaim failed!" << endl;
    }

    return 0;  // only to prevent compiler warning
}
