/*
 * Converter.cpp
 *
 *  Created on: 2017年1月4日
 *      Author: liu
 */

#include "XmlBuilder.h"
#include "tinyxml.h"
#include "utils.h"

#include <iostream>
#include <string.h>
#include <assert.h>

using namespace std;
using namespace onvif;

XmlBuilder::XmlBuilder() :
        _tag(""), _value(""), _parent(NULL) {
}

XmlBuilder::XmlBuilder(const string &tag) :
        _tag(tag), _value(""), _parent(NULL) {
}

XmlBuilder::XmlBuilder(const XmlBuilder &obj, const string &tag) :
        _tag(tag), _value(""), _parent(&obj) {
}

XmlBuilder::XmlBuilder(const string &tag, const string& value) :
        _tag(tag), _value(value), _parent(NULL) {
}

XmlBuilder::~XmlBuilder() {
    XmlBuilderList::iterator it = _childs.begin();
    while (it != _childs.end()) {
        delete (*it);
        it++;
    }
}

void XmlBuilder::operator =(int value) {
    _value = to_string(value);
}

void XmlBuilder::operator =(float value) {
    _value = to_string(value);
}

void XmlBuilder::operator =(const char* value) {
    _value = value;
}

void XmlBuilder::operator =(const string &value) {
    _value = value;
}

void XmlBuilder::operator =(const XmlBuilder &builder) {
    _value = builder._value;
}

void XmlBuilder::operator =(const bool b) {
    _value = b ? "true" : "false";
}

void XmlBuilder::setPathNamespace(const std::string& name, const std::string& url) {
    _attrib[name] = url;
}

bool XmlBuilder::noDefaultNamespace() {
    if (_attrib.find("xmlns") == _attrib.end()) {
        return true;
    } else {
        return false;
    }
}

string& XmlBuilder::tag() {
    return _tag;
}

string& XmlBuilder::attrib(const std::string& key) {
    return _attrib[key];
}

int XmlBuilder::intValue() {
    return atoi(_value.c_str());
}

string& XmlBuilder::stringValue() {
    return _value;
}

void XmlBuilder::copyAttrib(XmlBuilder *to, const TiXmlElement *from) {
    const TiXmlAttribute *attribIt = from->FirstAttribute();
    while (attribIt != NULL) {
        to->_attrib[attribIt->Name()] = attribIt->Value();
        attribIt = attribIt->Next();
    }
}

void XmlBuilder::elementToBuilder(XmlBuilder *to, const TiXmlElement *from) {
    assert(from != NULL);
//    cout << "to: " << to->_tag << " from: " << from->Value() << endl;
    copyAttrib(to, from);

    if (from->NoChildren())
        return;

    const char *text = from->GetText();
    if (NULL != text) {
        to->_value = text;
        return;
    }

    const TiXmlElement *child = from->FirstChildElement();
    while (NULL != child) {
        XmlBuilder *newBuilder = new XmlBuilder(child->Value());
        newBuilder->_parent = to;
        to->_childs.push_back(newBuilder);
        elementToBuilder(newBuilder, child);
        child = child->NextSiblingElement();
    }
}

int XmlBuilder::parse(const char *str) {
    // TODO: clear the content

    TiXmlDocument doc;
    doc.Parse(str, NULL, TIXML_ENCODING_UTF8);

    TiXmlElement *it = doc.RootElement();
    if (it == NULL) {
        cout << "root element is null!" << endl;
        return -1;
    }

    _tag = it->Value();
    elementToBuilder(this, it);

    return 0;
}

string XmlBuilder::parseTag(const string& path) {
    string::size_type offset = path.find('.');
    if (string::npos != offset) {
        return path.substr(0, offset);
    }

    return path;
}

string XmlBuilder::findNamespace(const string& key) {
    return findNamespace(key, *this);
}

string XmlBuilder::findNamespace(const string& key, XmlBuilder &builder) {
    string ns = "xmlns";
    if(ns != key) {
       ns = "xmlns:" + key;
    }

    const XmlBuilder *it = &builder;
    AttribList::const_iterator result;
    do {
        result = it->_attrib.find(ns);
        if (it->_attrib.end() != result) {
            return result->second;
        }

        it = it->_parent;
    } while (it != NULL);
    LOG("namespace %s is not found!", key.c_str());

    return string();
}

string XmlBuilder::expandNs(const string &tag, XmlBuilder& builder) {
    string::size_type index = tag.find(':');
    //default namespace
    if (string::npos == index) {
        const string& attrib = findNamespace("xmlns", builder);
        if (attrib == "") {
            return tag;
        } else {
            const string &expandTag = '{' + attrib + '}' + tag;
            return expandTag;
        }
    }

    const string &ns = tag.substr(0, index);

    if ("" == findNamespace(ns, builder)) {
//        throw "namespace not found!";
        return tag;
    }

    const string &expandNs = findNamespace(ns.c_str(), builder);

    const string &expandTag = '{' + expandNs + '}' + tag.substr(index + 1);
    return expandTag;
}

XmlBuilder& XmlBuilder::operator [](const char* path) {
    assert((*path) != '\0');

    const string &tag = parseTag(path);
    const char *offset = path + tag.size();
    if ((*offset) == '.') {
        offset += 1;
    }

//    cout << "assert _tag: " << _tag << " tag: " << tag << " path: " << path << endl;
    assert(compareTag(*this, tag));

    if ((*offset) == '\0') {
        return *this;
    }

    return getPath(offset);
//    const string &peerTag = parseTag(offset);
//    if (peerTag == "") {
//        return *this;
//    }
//
//    XmlBuilderList::const_iterator it = _childs.begin();
//    while (it != _childs.end()) {
////        DBG("compare %s, %s", (*it)->tag().c_str(), peerTag.c_str());
//        if (compareTag((**it), peerTag)) {
////            DBG("equal!");
//            return (**it)[offset];
//        }
//        it++;
//    }
//
//    XmlBuilder *newNode = new XmlBuilder(peerTag);
//    newNode->_parent = this;
//    _childs.push_back(newNode);
//
//    return (*newNode)[offset];
}

XmlBuilder& XmlBuilder::getPath(const char* path) {
    assert((*path) != '\0');

    const string &tag = parseTag(path);
//    const char *offset = path + tag.size();
//    if ((*offset) == '.') {
//        offset += 1;
//    } else if ((*offset) == '\0') {
//        return *this;
//    }

    XmlBuilderList::const_iterator it = _childs.begin();
    while (it != _childs.end()) {
//        DBG("compare %s, %s", (*it)->tag().c_str(), peerTag.c_str());
        if ((*it)->compareTag(tag)) {
//            DBG("equal!");
            return (**it)[path];
        }
        it++;
    }

    XmlBuilder *newNode = new XmlBuilder(tag);
    newNode->_parent = this;
    _childs.push_back(newNode);

    return (*newNode)[path];
}

XmlBuilder& XmlBuilder::append(XmlBuilder* c) {
    _childs.push_back(c);
    c->_parent = this;

    return *this;
}

void XmlBuilder::addAttrib(const char *key, const char *value) {
    _attrib[key] = value;
}

void XmlBuilder::addAttrib(const char *key, const int value) {
    _attrib[key] = to_string(value);
}

void XmlBuilder::addAttrib(const char *key, const float value) {
    _attrib[key] = to_string(value);
}

void XmlBuilder::addAttrib(const string &key, const string &value) {
    _attrib[key] = value;
}

bool XmlBuilder::compareTag(const std::string& tag) {
    return compareTag(*this, tag);
}

bool XmlBuilder::compareTag(XmlBuilder& node, const std::string& tag) {
    const string &tag1 = expandNs(node._tag, node);
    const string &tag2 = expandNs(tag, node);
//    cout << "tag1: " << tag1 << endl;
//    cout << "tag2: " << tag2 << endl;

    if (tag1 == tag2) {
        return true;
    } else {
        return false;
    }
}

void XmlBuilder::builderToElement(bool isRoot, TiXmlElement *to, XmlBuilder *from) {
    // set attribute
    AttribList::iterator attribIt = from->_attrib.begin();
    for (; attribIt != from->_attrib.end();) {
        to->SetAttribute(attribIt->first, attribIt->second);
        attribIt++;
    }

    // set text
    if (from->_value != "") {
        to->LinkEndChild(new TiXmlText(from->_value));
        return;
    }

    XmlBuilderList::iterator builderIt = from->_childs.begin();

    for (; builderIt != from->_childs.end();) {
        XmlBuilder *currBuilder = *builderIt;
        //set tag
        TiXmlElement *newElement = new TiXmlElement(currBuilder->_tag);
        to->LinkEndChild(newElement);

        builderToElement(false, newElement, currBuilder);
        builderIt++;
    }
}

int XmlBuilder::childCount() {
    return _childs.size();
}

XmlBuilder* XmlBuilder::getChild(int index) {
    return _childs[index];
}

void XmlBuilder::dump(int indent) {
    const string &xml = toFormatString();

    cout << xml << endl;
}

const string XmlBuilder::toString() {
    TiXmlDocument doc;
    doc.LinkEndChild(new TiXmlDeclaration("1.0", "utf-8", "yes"));
    doc.LinkEndChild(new TiXmlElement(_tag));

    builderToElement(true, doc.RootElement(), this);

    TiXmlPrinter printer;
    printer.SetIndent(NULL);
    printer.SetLineBreak(NULL);
    doc.Accept(&printer);
    return printer.Str();
}

const string XmlBuilder::toFormatString() {
    TiXmlDocument doc;
    doc.LinkEndChild(new TiXmlDeclaration("1.0", "utf-8", "yes"));
    doc.LinkEndChild(new TiXmlElement(_tag));

    builderToElement(true, doc.RootElement(), this);

    TiXmlPrinter printer;
    doc.Accept(&printer);
    return printer.Str();
}

#if 0
void XmlBuilder::dump(int indent) {
    for (int i = 0; i < indent; i++) {
        cout << ' ';
    }
    cout << "<" << _tag;
    {
        map<string, string>::iterator it = _attrib.begin();
        while (it != _attrib.end()) {
            cout << " " << it->first << "=\"" << it->second << '"';
            it++;
        }
    }
    cout << ">" << endl;

    if (_value != "") {
        for (int i = 0; i < (indent + 4); i++) {
            cout << ' ';
        }
        cout << _value << endl;
    }

    {
        XmlBuilderList::iterator it = _childs.begin();
        while (it != _childs.end()) {
            (*it)->dump(indent + 4);
            it++;
        }
    }

    for (int i = 0; i < indent; i++) {
        cout << ' ';
    }
    cout << "</" << _tag << ">" << endl;
}
#endif

