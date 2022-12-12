#include <iostream>
#include "XmlBuilder.h"
#include "SoapUtils.h"

#define TEST_SOAP_STR "<?xml version=\"1.0\" encoding=\"utf-8\"?>" \
"<Envelope xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" xmlns=\"http://www.w3.org/2003/05/soap-envelope\">" \
"<Header>" \
"<wsa:MessageID xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\">uuid:e3c0bc1f-075a-4777-9e01-3e8c7c8c6f98</wsa:MessageID>" \
"<wsa:To xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\">urn:schemas-xmlsoap-org:ws:2005:04:discovery</wsa:To>" \
"<wsa:Action xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\">http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe</wsa:Action>" \
"<keng></keng>" \
"</Header>" \
"<Body>" \
"<Probe xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\">" \
"<Types>tds:Device</Types>" \
"<Scopes />" \
"</Probe>" \
"</Body>" \
"</Envelope>"

using namespace onvif;
using namespace std;


int main(int argc, char **argv) {
    XmlBuilder builder("test");

//    XmlBuilder::nsMap = SoapUtils::getAllXmlNameSpace();
    builder.parse(TEST_SOAP_STR);
    builder.dump();

    cout << "test expression out [Envelope.Body.Probe.Types]: " << builder["Envelope.Body.Probe.Types"].stringValue() << endl;
    cout << "test expression attribute out [Envelope.Body.Probe.Types]: " << builder["Envelope.Body.Probe"].attrib("xmlns:xsi") << endl;
}
