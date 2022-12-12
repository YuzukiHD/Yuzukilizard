/*
 * H264FrameParser.h
 *
 *  Created on: 2016年9月24日
 *      Author: liu
 */

#ifndef LIVE_IPCPROGRAM_INTERFACE_H264FRAMEPARSER_H_
#define LIVE_IPCPROGRAM_INTERFACE_H264FRAMEPARSER_H_

#include <stdint.h>
#include <vector>

namespace Byl {

class H264FrameParser {
  public:
    enum H264FrameParserType { ManagerBuffer, NotManagerBuffer };

    H264FrameParser(H264FrameParserType type);
    H264FrameParser(uint8_t *buffer, int32_t size);
    ~H264FrameParser();

    void setBuffer(uint8_t *buffer, uint32_t size);
    void resetParser();
    // note: 先调用findNalu判断有没有，再使用getNextNaluBuffer去读取
    bool findNalu();
    bool getNextNaluBuffer(uint8_t *&buffer, uint32_t &size, bool &isLastNalu);
    void dumpStartcode();

  private:
    bool findStartcode(uint8_t *start, uint8_t *end, uint8_t *&startcode, uint32_t &startcodeSize);

  private:
    H264FrameParserType _type;
    uint8_t *_buffer;
    uint32_t _bufferSize;
    uint8_t *_currStartPtr;
    uint8_t *_nextStartPtr;
    uint32_t _currStartcodeSize;
    uint32_t _nextStartcodeSize;
    bool _isLastNalu;
    std::vector<uint8_t *> startcodeOffsets;
};

} /* namespace Byl */

#endif /* LIVE_IPCPROGRAM_INTERFACE_H264FRAMEPARSER_H_ */
