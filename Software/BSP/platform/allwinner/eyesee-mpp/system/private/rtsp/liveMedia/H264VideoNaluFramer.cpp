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
 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 **********/
// "liveMedia"
// Copyright (c) 1996-2016 Live Networks, Inc.  All rights reserved.
// A simplified version of "H264VideoStreamFramer" that takes only complete,
// discrete frames (rather than an arbitrary byte stream) as input.
// This avoids the parsing and data copying overhead of the full
// "H264VideoStreamFramer".
// Implementation
#include "include/H264VideoNaluFramer.hh"

#include <iostream>
#include "../IPCProgram/interface/H264FrameParser.h"
#include "string.h"

using namespace std;
using namespace Byl;

H264VideoNaluFramer* H264VideoNaluFramer::createNew(UsageEnvironment& env, FramedSource* inputSource)
{
    return new H264VideoNaluFramer(env, inputSource);
}

H264VideoNaluFramer::H264VideoNaluFramer(UsageEnvironment& env, FramedSource* inputSource)
    : H264or5VideoStreamDiscreteFramer(264, env, inputSource),
      _parser(new H264FrameParser(H264FrameParser::NotManagerBuffer))
{
}

H264VideoNaluFramer::~H264VideoNaluFramer() { delete _parser; }

Boolean H264VideoNaluFramer::isH264VideoStreamFramer() const { return True; }

void H264VideoNaluFramer::doGetNextFrame()
{
    if (_parser->findNalu()) {
        fillData();
    } else {
        fInputSource->getNextFrame(fTo, fMaxSize, H264VideoNaluFramer::afterGettingFrame, this,
                                   FramedSource::handleClosure, this);
    }
}

void H264VideoNaluFramer::afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
                                            struct timeval presentationTime, unsigned durationInMicroseconds)
{
    H264VideoNaluFramer* source = (H264VideoNaluFramer*)clientData;
    source->afterGettingFrame1(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

void H264VideoNaluFramer::afterGettingFrame1(unsigned frameSize, unsigned numTruncatedBytes,
                                             struct timeval presentationTime, unsigned durationInMicroseconds)
{
    _parser->setBuffer(fTo, frameSize);
    fPresentationTime = presentationTime;
    if (_parser->findNalu()) {
        fillData();
    }
}

void H264VideoNaluFramer::fillData()
{
    uint8_t* data = NULL;
    uint32_t size = 0;
    bool isLast = false;
    _parser->getNextNaluBuffer(data, size, isLast);
    memmove(fTo, data, size);
    fFrameSize = size;
    fPictureEndMarker = isLast;
    if (isLast) _parser->dumpStartcode();
    envir().taskScheduler().scheduleDelayedTask(0, (TaskFunc*)afterGetting, this);
}
