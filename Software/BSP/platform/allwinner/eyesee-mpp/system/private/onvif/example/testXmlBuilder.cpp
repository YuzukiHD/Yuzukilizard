/*
 * xmltest.cpp
 *
 *  Created on: 2017年1月4日
 *      Author: liu
 */
#include <iostream>
#include "XmlBuilder.h"
#include "SoapUtils.h"

using namespace std;
using namespace onvif;

int main(int argc, char **argv) {
    onvif::XmlBuilder            *c    = SoapUtils::createSoapTemplate("", SoapUtils::getAllXmlNameSpace());
    c->dump();
    cout << endl;

    onvif::XmlBuilder            &test = *c;
    test["Envelope.Header.wsa:MessageID"]       = "why";
    test["Envelope.Body.Probe.Types"]           = "tds:Device";
    onvif::XmlBuilder                     *hehe = new onvif::XmlBuilder("hehe");
    test["Envelope.Body.Probe"].append(hehe);
    (*hehe)["hehe.test"] = "successful";

    test["Envelope.Body.Probe"].addAttrib("hehe", "keng");
    test["Envelope.Body.Probe"].addAttrib("xmlns:hehe", "http://only.for.test");

    test.dump();

    return 0;
}

