/*
 * HttpCodec.cpp
 *
 *  Created on: 2017年1月12日
 *      Author: liu
 */

#include <stdlib.h>
#include <utils.h>
#include <sstream>
#include "HttpCodec.h"

using namespace onvif;
using namespace std;

HttpCodec::HttpCodec() {
}

HttpCodec::~HttpCodec() {
}

HttpCodec::ParseResult HttpCodec::parse(const string& str) {
    bool isContent = false;
    string::size_type currIndex = 0;

    do {
        string::size_type index = str.find("\r\n", currIndex);
        if (index == string::npos) {
            _request.content.append(str);
            break;
        } else if (index == currIndex) {
            currIndex += 2;
            isContent = true;
            index = string::npos;
        }

        const string& line = str.substr(currIndex, index - currIndex);
        int ret = HttpCodec::parseAndFillReq(line, isContent, _request);
        if (ret == -1)
            return HttpCodec::ParseFailed;
        currIndex = index + 2;
    } while (!isContent);

    unsigned int len = std::atoi(_request.contentLength.c_str());
    if (len != _request.content.size()) {
        return HttpCodec::ParseContinue;
    }

    return HttpCodec::ParseSuccess;
}

string HttpCodec::buildMessage(const HttpResponse& resp) {
    int length = resp.content.size();
    ostringstream out;
    if (resp.statusCode == 200) {
        out << "HTTP/1.1 200 OK\r\n";
    } else if (resp.statusCode == 500){
        out << "HTTP/1.1 500 Internal Server Error\r\n";
    } else {
        LOG("unknow status code! status code: %d", resp.statusCode);
        out << "HTTP/1.1 500 Internal Server Error\r\n";
    }
    out <<  "Server: AllWinnertech/zhuhai\r\n"
            "Content-Type: application/soap+xml; charset=utf-8\r\n"
            "Content-Length: " << length << "\r\n"
            "Connection: close\r\n\r\n" << resp.content;

    return out.str();
}

#define PARTTERN_POST "POST "
#define PARTTERN_GET  "GET"
#define PARTTERN_HOST "Host:"
#define PARTTERN_SPACE ' '
#define PARTTERN_USER_AGENT "User-Agent:"
#define PARTTERN_CONTENT_TYPE "Content-Type:"
#define PARTTERN_CONTENT_LEN "Content-Length:"
#define PARTTERN_CONNECTION "Connection:"

int HttpCodec::parseAndFillReq(const string& line, bool isContent, HttpRequest& req) {
    if (isContent) {
        int sizeField = atoi(req.contentLength.c_str());
        req.content = line.substr(0, sizeField);

    } else if (string::npos != line.find(PARTTERN_HOST)) {
        req.host = line.substr(sizeof(PARTTERN_HOST));

    } else if (string::npos != line.find(PARTTERN_GET)) {
        string::size_type offset = line.find(PARTTERN_SPACE, sizeof(PARTTERN_GET));
        //TODO parse parameter with url

    } else if (string::npos != line.find(PARTTERN_POST)) {
        string::size_type offset = line.find(PARTTERN_SPACE, sizeof(PARTTERN_POST));
        req.uri = line.substr(sizeof(PARTTERN_POST), offset - sizeof(PARTTERN_POST));

    } else if (string::npos != line.find(PARTTERN_USER_AGENT)) {
        req.userAgent = line.substr(sizeof(PARTTERN_USER_AGENT));

    } else if (string::npos != line.find(PARTTERN_CONTENT_TYPE)) {
        req.contentType = line.substr(sizeof(PARTTERN_CONTENT_TYPE));

    } else if (string::npos != line.find(PARTTERN_CONTENT_LEN)) {
        req.contentLength = line.substr(sizeof(PARTTERN_CONTENT_LEN));

    } else if (string::npos != line.find(PARTTERN_CONNECTION)) {
        req.connection = line.substr(sizeof(PARTTERN_CONNECTION));

    } else {
//        DBG("unknown message line: %s", line.c_str());
//        cout << "unknow message line! content:" << endl;
//        cout << line << endl;
    }

    return 0;
}

