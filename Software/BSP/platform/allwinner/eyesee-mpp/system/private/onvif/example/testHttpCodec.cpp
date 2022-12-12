/*
 * testHttpCodec.cpp
 *
 *  Created on: 2017年1月12日
 *      Author: liu
 */
#include <utils.h>
#include "HttpCodec.h"
#include "XmlBuilder.h"
#include "SoapUtils.h"

using namespace std;
using namespace onvif;

#define TEST_REQ \
    "Host: 192.168.205.12\r\n" \
    "User-Agent: Dahua SOAP Client\r\n" \
    "Content-Type: application/soap+xml; charset=utf-8\r\n" \
    "Content-Length: 1470\r\n" \
    "Connection: close\r\n" \
    "\r\n" \
    "<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"yes\" ?><Envelope xmlns:sc=\"http://www.w3.org/2003/05/soap-encoding\" xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:tt=\"http://www.onvif.org/ver10/schema\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:wsa5=\"http://www.w3.org/2005/08/addressing\" xmlns:wsu=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd\" xmlns:wsse=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:xmime=\"http://tempuri.org/xmime.xsd\" xmlns:xop=\"http://www.w3.org/2004/08/xop/include\" xmlns:wsrf-bf=\"http://docs.oasis-open.org/wsrf/bf-2\" xmlns:wstop=\"http://docs.oasis-open.org/wsn/t-1\" xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" xmlns:wsnt=\"http://docs.oasis-open.org/wsn/b-2\"><Header><wsse:Security><wsu:Timestamp><wsu:Created>2016-12-30T10:38:09Z</wsu:Created><wsu:Expires>2016-12-30T10:38:09Z</wsu:Expires></wsu:Timestamp><wsse:UsernameToken><wsse:Username>admin</wsse:Username><wsse:Password Type=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest\">oEpIoYwhTL3sm6cTHEIgwdZsuj8=</wsse:Password><wsse:Nonce>tM4dP10OYGykPGoa+lp4CCsSgDY=</wsse:Nonce><wsu:Created>2016-12-30T10:38:09Z</wsu:Created></wsse:UsernameToken></wsse:Security></Header><Body><tds:GetCapabilities><tds:Category>All</tds:Category></tds:GetCapabilities></Body></Envelope>"

#define TEST_SOAP_REQ_STR \
    "POST /onvif/device_service HTTP/1.1\r\n" \
    "Host: 192.168.205.12\r\n" \
    "User-Agent: Dahua SOAP Client\r\n" \
    "Content-Type: application/soap+xml; charset=utf-8\r\n" \
    "Content-Length: 963\r\n" \
    "Connection: close\r\n" \
    "\r\n" \
    "<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"yes\" ?>" \
    "<s:Envelope xmlns:sc=\"http://www.w3.org/2003/05/soap-encoding\" xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:tt=\"http://www.onvif.org/ver10/schema\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:wsa5=\"http://www.w3.org/2005/08/addressing\" xmlns:wsu=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd\" xmlns:wsse=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:xmime=\"http://tempuri.org/xmime.xsd\" xmlns:xop=\"http://www.w3.org/2004/08/xop/include\" xmlns:wsrf-bf=\"http://docs.oasis-open.org/wsrf/bf-2\" xmlns:wstop=\"http://docs.oasis-open.org/wsn/t-1\" xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" xmlns:wsnt=\"http://docs.oasis-open.org/wsn/b-2\"><s:Header/><s:Body><tds:GetCapabilities><tds:Category>All</tds:Category></tds:GetCapabilities></s:Body></s:Envelope>\r\n"



int main(int argc, char **argv) {
    HttpCodec decode;
    HttpRequest req;
    int ret = decode.parse(TEST_SOAP_REQ_STR, req);
    if (ret == -1) {
       LOG("parsing request failed!");
       return -1;
    }

//    req.dump();

    XmlBuilder builder;
    ret = builder.parse(req.content.c_str());
    if (ret == -1) {
        cout << "xml parsing failed!" << endl;
        return -1;
    }

    if (builder.noDefaultNamespace()) {
        builder.setPathNamespace("xmlns", SoapUtils::getAllXmlNameSpace()["xmlns"]);
    }
    builder.dump();

    cout << endl;
    cout << "function name: " << SoapUtils::soapFunctionName(builder) << endl;

    /**
     * only for test soaputils
     */
    cout << endl;
    XmlBuilder test1("Envelope");
    SoapUtils::addVisitNamespace(test1, "wsnt:tdn:trt", SoapUtils::getAllXmlNameSpace());
    test1["Envelope.Body.Test"];
    test1.dump();

    return 0;
}




