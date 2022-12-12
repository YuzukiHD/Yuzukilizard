/*
 * H264FrameParser.cpp
 *
 *  Created on: 2016年9月24日
 *      Author: liu
 */

#include "H264FrameParser.h"
#include <stdio.h>
#include <iostream>
#include "stdlib.h"

using namespace std;

namespace Byl {

H264FrameParser::H264FrameParser(uint8_t *buffer, int32_t size)
    : _type(ManagerBuffer),
      _buffer(buffer),
      _bufferSize(size),
      _currStartPtr(buffer),
      _nextStartPtr(buffer),
      _currStartcodeSize(0),
      _nextStartcodeSize(0),
      _isLastNalu(false)
{
}

H264FrameParser::H264FrameParser(H264FrameParserType type)
    : _type(type),
      _buffer(NULL),
      _bufferSize(0),
      _currStartPtr(NULL),
      _nextStartPtr(NULL),
      _currStartcodeSize(0),
      _nextStartcodeSize(0),
      _isLastNalu(false)
{
}

H264FrameParser::~H264FrameParser()
{
    if (_type == ManagerBuffer) {
        free(_buffer);
    }
}

void H264FrameParser::setBuffer(uint8_t *buffer, uint32_t size)
{
    if (_type == ManagerBuffer) {
        free(_buffer);
    }
    _buffer = buffer;
    _bufferSize = size;
    _currStartPtr = buffer;
    _nextStartPtr = buffer;
    _currStartcodeSize = 0;
    _nextStartcodeSize = 0;
    _isLastNalu = false;
    startcodeOffsets.clear();
}

void H264FrameParser::resetParser()
{
    _currStartPtr = _buffer;
    _currStartcodeSize = 0;
    _nextStartPtr = _buffer;
    _nextStartcodeSize = 0;
    _isLastNalu = false;
    startcodeOffsets.clear();
}

bool H264FrameParser::findNalu()
{
    if (_currStartPtr == NULL) {
        return false;
    }

    uint8_t *bufferTail = _buffer + _bufferSize;
    if (_currStartPtr == _nextStartPtr) {
        findStartcode(_currStartPtr, bufferTail, _nextStartPtr, _nextStartcodeSize);
        if (_nextStartPtr != _currStartPtr) {
            cout << "[rtsp-service] warning: the buffer is not start with nalu start code!" << endl;
        }
    }
    _currStartPtr = _nextStartPtr;
    _currStartcodeSize = _nextStartcodeSize;
    uint8_t *start = _nextStartPtr + _nextStartcodeSize;

    bool reslut = findStartcode(start, bufferTail, _nextStartPtr, _nextStartcodeSize);

    // last nalu case
    if ((reslut == false) && (_currStartPtr != bufferTail)) {
        _nextStartPtr = bufferTail;
        _nextStartcodeSize = 0;
        _isLastNalu = true;
        reslut = true;
    }

    return reslut;
}

bool H264FrameParser::getNextNaluBuffer(uint8_t *&buffer, uint32_t &size, bool &isLastNalu)
{
    bool result = true;

    // for statistics
    startcodeOffsets.push_back(_currStartPtr);

    buffer = _currStartPtr + _currStartcodeSize;
    size = _nextStartPtr - _currStartPtr - _currStartcodeSize;
    isLastNalu = _isLastNalu;

    return result;
}

void H264FrameParser::dumpStartcode()
{
    cout << endl << "current parse state:" << endl << "buffer start address: " << (void *)_buffer << endl;
    int size = 0;
    uint8_t *tailPtr = NULL;
    for (uint32_t i = 0; i < startcodeOffsets.size(); i++) {
        if (i == (startcodeOffsets.size() - 1)) {
            tailPtr = _buffer + _bufferSize;
        } else {
            tailPtr = startcodeOffsets[i + 1];
        }
        size = tailPtr - startcodeOffsets[i];
        cout << "output Nalu number: " << i << " size: " << size << " address: " << (void *)(startcodeOffsets[i])
             << endl;
    }
    cout << "buffer total size: " << _bufferSize << endl << "parser state dump over!" << endl;
}

bool H264FrameParser::findStartcode(uint8_t *start, uint8_t *end, uint8_t *&startcode, uint32_t &startcodeSize)
{
    uint8_t *readPtr = start;
    bool result = false;
    while (readPtr <= end) {
        if ((readPtr[0] == 0) && (readPtr[1] == 0) && ((readPtr[2] == 1) || ((readPtr[2] == 0) && (readPtr[3] == 1)))) {
            startcodeSize = (readPtr[2] == 0) ? 4 : 3;
            result = true;
            startcode = readPtr;
            break;
        } else {
            readPtr++;
        }
    }
    return result;
}

} /* namespace Byl */

/**
 *  only for test parser
 */
//int main(int args, char** argv)
//{
//	Byl::H264FrameParser parser(Byl::H264FrameParser::NotManagerBuffer);
//
//	unsigned char buffer[] = {
////  format data
////			0x00, 0x00, 0x00, 0x01, 0x12, 0x13, 0x14, 0x15,
////			0x00, 0x00, 0x01, 0x12, 0x13,
////			0x00, 0x00, 0x01, 0x12, 0x13, 0x14,
////			0x00, 0x00, 0x01, 0x12, 0x13, 0x14, 0x15,
////			0x00, 0x00, 0x01, 0x12, 0x13, 0x14, 0x15, 0x16
//			0x00, 0x00, 0x00, 0x01, 0x12, 0x13, 0x14, 0x15, 0x00, 0x00, 0x01, 0x12, 0x13, 0x00, 0x00, 0x01, 0x12, 0x13, 0x14, 0x00, 0x00, 0x01, 0x12, 0x13,
//			0x14, 0x15, 0x00, 0x00, 0x01, 0x12, 0x13, 0x14, 0x15, 0x16 };
//
//	parser.setBuffer(buffer, sizeof(buffer));
//	while (parser.findNalu()) {
//		uint8_t *data = NULL;
//		uint32_t size = 0;
//		bool isLast = false;
//		parser.getNextNaluBuffer(data, size, isLast);
//		if (isLast)
//			parser.dumpStartcode();
//	}
//
//	return 0;
//}
