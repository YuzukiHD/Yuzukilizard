/*
 * SoapUtils.cpp
 *
 *  Created on: 2017年1月12日
 *      Author: liu
 */

#include "SoapUtils.h"
#include "utils.h"
#include <iostream>

using namespace std;
using namespace onvif;

XmlBuilder* SoapUtils::createSoapTemplate(const string &expr, const SoapXmlNsMap &xmlns) {
    XmlBuilder *c = new onvif::XmlBuilder("s:Envelope");
    XmlBuilder &builderRef = *c;

    SoapXmlNsMap& ns = SoapUtils::getAllXmlNameSpace();
    SoapUtils::addVisitNamespace(builderRef, expr, ns);
    builderRef["s:Envelope.s:Header"];
    builderRef["s:Envelope.s:Body"];

    return c;
}

void SoapUtils::addVisitNamespace(XmlBuilder& node, const string &expr, const SoapXmlNsMap& map) {
    node.addAttrib("xmlns", map.find("xmlns")->second);
    node.addAttrib("xmlns:s", map.find("xmlns")->second);

    if (expr == "") return;
    SoapXmlNsMap::const_iterator it;
    string::size_type start = 0;
    string::size_type end = 0;

    do {
        end = expr.find(':', start);
        if (string::npos == end)
            end = expr.length();
        const string &ns = expr.substr(start, end - start);
        it = map.find(ns);
        if (it == map.end()) {
            LOG("namespace %s is not found!", ns.c_str());
            break;
        }
        node["Envelope"].addAttrib("xmlns:" + it->first, it->second);

        start = end + 1;
        if (start >= expr.length()) {
            break;
        }
    } while(true);
}

string SoapUtils::soapFunctionName(XmlBuilder& obj) {
    XmlBuilder &body = obj["Envelope.Body"];
    if (body.childCount() < 1) {
        cout << "there is not valid soap function!\n";
        return "Fault";
    }

    XmlBuilder *funcNode = body.getChild(0);
    string::size_type offset = funcNode->tag().find(':');
    if (string::npos != offset) {
        return funcNode->tag().substr(offset + 1);
    }

    return funcNode->tag();
}

void SoapUtils::buildFailedMessage(XmlBuilder &obj) {
// prototype:
//    HTTP/1.1 500 Internal Server Error
//    Server: gSOAP/2.8
//    Content-Type: application/soap+xml; charset=utf-8
//    Content-Length: 1502
//    Connection: close
//
//    <?xml version="1.0" encoding="UTF-8"?>
//    <SOAP-ENV:Envelope xmlns:SOAP-ENV="http://www.w3.org/2003/05/soap-envelope" xmlns:SOAP-ENC="http://www.w3.org/2003/05/soap-encoding" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:wsa="http://schemas.xmlsoap.org/ws/2004/08/addressing" xmlns:wsdd="http://schemas.xmlsoap.org/ws/2005/04/discovery" xmlns:chan="http://schemas.microsoft.com/ws/2005/02/duplex" xmlns:wsa5="http://www.w3.org/2005/08/addressing" xmlns:tmd="http://www.onvif.org/ver10/deviceIO/wsdl" xmlns:tt="http://www.onvif.org/ver10/schema" xmlns:wsrfbf="http://docs.oasis-open.org/wsrf/bf-2" xmlns:wstop="http://docs.oasis-open.org/wsn/t-1" xmlns:wsrfr="http://docs.oasis-open.org/wsrf/r-2" xmlns:tad="http://www.onvif.org/ver10/analyticsdevice/wsdl" xmlns:tan="http://www.onvif.org/ver20/analytics/wsdl" xmlns:tdn="http://www.onvif.org/ver10/network/wsdl" xmlns:tds="http://www.onvif.org/ver10/device/wsdl" xmlns:wsnt="http://docs.oasis-open.org/wsn/b-2" xmlns:tev="http://www.onvif.org/ver10/events/wsdl" xmlns:timg="http://www.onvif.org/ver20/imaging/wsdl" xmlns:trt="http://www.onvif.org/ver10/media/wsdl">
//        <SOAP-ENV:Body>
//            <SOAP-ENV:Fault>
//                <SOAP-ENV:Code>
//                    <SOAP-ENV:Value>SOAP-ENV:MustUnderstand</SOAP-ENV:Value>
//                </SOAP-ENV:Code>
//                <SOAP-ENV:Reason>
//                    <SOAP-ENV:Text xml:lang="en">The data in element 'wsse:Security' must be understood but cannot be processed</SOAP-ENV:Text>
//                </SOAP-ENV:Reason>
//            </SOAP-ENV:Fault>
//        </SOAP-ENV:Body>
//    </SOAP-ENV:Envelope>

    obj["Envelope.Body.Fault.Code.Value"] = "s:MustUnderstand";
    obj["Envelope.Body.Fault.Reason.Text"].addAttrib("xml:lang", "en");
    obj["Envelope.Body.Fault.Reason.Text"] = "method not implement!";
}

SoapXmlNsMap& SoapUtils::getAllXmlNameSpace() {
    static SoapXmlNsMap nsmap;
    static bool first = true;

    if (first) {
        nsmap["xmlns"] = "http://www.w3.org/2003/05/soap-envelope";
        nsmap["s"] = "http://www.w3.org/2003/05/soap-envelope";
        nsmap["sc"] = "http://www.w3.org/2003/05/soap-encoding";
        nsmap["tds"] = "http://www.onvif.org/ver10/device/wsdl";
        nsmap["tt"] = "http://www.onvif.org/ver10/schema";
        nsmap["wsa"] = "http://schemas.xmlsoap.org/ws/2004/08/addressing";
        nsmap["wsa5"] = "http://www.w3.org/2005/08/addressing";
        nsmap["wsnt"] = "http://docs.oasis-open.org/wsn/b-2";
        nsmap["wsrf-bf"] = "http://docs.oasis-open.org/wsrf/bf-2";
        nsmap["wsse"] = "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd";
        nsmap["wstop"] = "http://docs.oasis-open.org/wsn/t-1";
        nsmap["wsu"] = "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd";
        nsmap["xmime"] = "http://tempuri.org/xmime.xsd";
        nsmap["xop"] = "http://www.w3.org/2004/08/xop/include";
        nsmap["xsd"] = "http://www.w3.org/2001/XMLSchema";
        nsmap["xsi"] = "http://www.w3.org/2001/XMLSchema-instance";
        nsmap["wsdd"] = "http://schemas.xmlsoap.org/ws/2005/04/discovery";
        nsmap["tdn"] = "http://www.onvif.org/ver10/network/wsdl";
        nsmap["trt"] = "http://www.onvif.org/ver10/media/wsdl";
        nsmap["timg"] = "http://www.onvif.org/ver20/imaging/wsdl";
        nsmap["tan"] = "http://www.onvif.org/ver20/analytics/wsdl";
        nsmap["tev"] = "http://www.onvif.org/ver10/events/wsdl";

        first = false;
    }

    return nsmap;
}
