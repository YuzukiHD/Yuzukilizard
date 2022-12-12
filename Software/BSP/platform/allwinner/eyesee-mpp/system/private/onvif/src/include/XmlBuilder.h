/*
 * Converter.h
 *
 *  Created on: 2017年1月4日
 *      Author: liu
 */

#ifndef SRC_XMLBUILDER_H_
#define SRC_XMLBUILDER_H_

#include <string>
#include <vector>
#include <map>

class TiXmlElement;
class TiXmlNode;

namespace onvif {

class XmlBuilder {
public:
    typedef std::vector<XmlBuilder*> XmlBuilderList;
    typedef std::map<std::string, std::string> AttribList;
    typedef std::map<std::string, std::string> NamespaceMap;

public:
    XmlBuilder();
    XmlBuilder(const std::string &tag);
    XmlBuilder(const XmlBuilder &obj, const std::string &tag);
    XmlBuilder(const std::string &tag, const std::string& value);
    ~XmlBuilder();

    void setPathNamespace(const std::string &name, const std::string &url);
    bool noDefaultNamespace();
    std::string findNamespace(const std::string& key, XmlBuilder &builder);
    std::string findNamespace(const std::string& key);
    std::string& tag();
    std::string& attrib(const std::string& key);

    int intValue();
    std::string& stringValue();

    int parse(const char *str);

    void operator=(int value);
    void operator=(float value);
    void operator=(const char *value);
    void operator=(const std::string &value);
    void operator=(const XmlBuilder &builder);
    void operator=(const bool b);
    XmlBuilder& operator[](const char *path);
    XmlBuilder& getPath(const char *path);

    XmlBuilder& append(XmlBuilder *c);
    void addAttrib(const char *key, const char *value);
    void addAttrib(const char *key, const int value);
    void addAttrib(const char *key, const float value);
    void addAttrib(const std::string &key, const std::string &value);

    int childCount();
    XmlBuilder* getChild(int index);
    bool compareTag(XmlBuilder& node, const std::string& tag);
    bool compareTag(const std::string& tag);

    void dump(int indent = 0);
    const std::string toString();
    const std::string toFormatString();

private:
    std::string parseTag(const std::string &path);
    std::string expandNs(const std::string &tag, XmlBuilder &builder);
    void copyAttrib(XmlBuilder *to, const TiXmlElement *from);
    void elementToBuilder(XmlBuilder *to, const TiXmlElement *from);
    void builderToElement(bool isRoot, TiXmlElement *to, XmlBuilder *from);

private:
    std::string _tag;
    std::string _value;
    AttribList _attrib;

    const XmlBuilder *_parent;
    XmlBuilderList _childs;
};

} /* namespace onvif */

#endif /* SRC_XMLBUILDER_H_ */
