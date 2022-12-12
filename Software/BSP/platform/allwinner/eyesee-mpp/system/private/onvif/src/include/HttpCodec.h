/*
 * HttpCodec.h
 *
 *  Created on: 2017年1月12日
 *      Author: liu
 */

#ifndef SRC_HTTPCODEC_H_
#define SRC_HTTPCODEC_H_

#include <string>
#include <iostream>

using namespace std;

namespace onvif {

struct HttpRequest {
    string host;
    string method;
    string uri;
    string userAgent;
    string contentType;
    string contentLength;
    string connection;
    string content;

    void dump() {
        cout << "dump the req:" << endl;
        cout << "Method: " << method << endl;
        cout << "Host: " << host << endl;
        cout << "Uri: " << uri << endl;
        cout << "user agent: " << userAgent << endl;
        cout << "content type: " << contentType << endl;
        cout << "content length: " << contentLength << endl;
        cout << "connection: " << connection << endl;
        cout << "content: " << content << endl;
    }
};

struct HttpResponse {
    int statusCode;
    string server;
    string contentType;
    string connection;
    string content;
};


class HttpCodec {
public:
    enum ParseResult {
       ParseFailed,
       ParseContinue,
       ParseSuccess,
    };

    HttpCodec();
    ~HttpCodec();

    static std::string buildMessage(const HttpResponse& resp);
    ParseResult parse(const std::string &str);
    HttpRequest req() {
        return _request;
    };

private:
    static int parseAndFillReq(const std::string &line, bool isContent, HttpRequest& req);

    HttpRequest _request;
};

} /* namespace onvif */

#endif /* SRC_HTTPCODEC_H_ */
