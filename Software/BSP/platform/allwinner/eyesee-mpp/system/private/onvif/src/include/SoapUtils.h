/*
 * SoapUtils.h
 *
 *  Created on: 2017年1月12日
 *      Author: liu
 */

#ifndef SRC_SOAPUTILS_H_
#define SRC_SOAPUTILS_H_

#include "XmlBuilder.h"
#include <string>
#include <map>

namespace onvif {

typedef std::map<std::string, std::string> SoapXmlNsMap;

class SoapUtils {
public:
//    static XmlBuilder* createSoapTemplate(SoapXmlNsMap &xmlns);
    static XmlBuilder* createSoapTemplate(const std::string &expr, const SoapXmlNsMap &xmlns);
    static std::string soapFunctionName(XmlBuilder& obj);
    static SoapXmlNsMap& getAllXmlNameSpace();
    static void buildFailedMessage(XmlBuilder& obj);
//    static void addVisitNamespace(XmlBuilder& node, SoapXmlNsMap& map);
    static void addVisitNamespace(XmlBuilder& node, const std::string &expr, const SoapXmlNsMap& map);
};

} /* namespace onvif */

#endif /* SRC_SOAPUTILS_H_ */
