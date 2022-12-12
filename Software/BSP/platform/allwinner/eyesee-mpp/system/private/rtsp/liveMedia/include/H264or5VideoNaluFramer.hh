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
// C++ header
#ifndef _H264_VIDEO_STREAM_DISCRETE_FRAMER_HH
#define _H264_VIDEO_STREAM_DISCRETE_FRAMER_HH

#ifndef _H264_OR_5_VIDEO_STREAM_DISCRETE_FRAMER_HH
#include "H264or5VideoStreamDiscreteFramer.hh"
#endif

namespace Byl {
class FrameNaluParser;
}

class H264or5VideoNaluFramer : public H264or5VideoStreamDiscreteFramer {
  public:
    static H264or5VideoNaluFramer* createNew(UsageEnvironment& env, FramedSource* inputSource, int hNum);

  protected:
    H264or5VideoNaluFramer(UsageEnvironment& env, FramedSource* inputSource, int hNum);
    // called only by createNew()
    virtual ~H264or5VideoNaluFramer();
    // redefine virtual function
    virtual void doGetNextFrame();
    static void afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
                                  struct timeval presentationTime, unsigned durationInMicroseconds);
    void afterGettingFrame1(unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime,
                            unsigned durationInMicroseconds);

  private:
    // redefined virtual functions:
    virtual Boolean isH264VideoStreamFramer() const;
    virtual Boolean isH265VideoStreamFramer() const;

    void fillData();

    Byl::FrameNaluParser* _parser;
};

#endif
