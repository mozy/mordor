#ifndef __MORDOR_XML_PARSER_H__
#define __MORDOR_XML_PARSER_H__
// Copyright (c) 2009 - Decho Corporation

#include <boost/function.hpp>

#include "mordor/ragel.h"

namespace Mordor {

class XMLParserEventHandler
{
public:
    virtual ~XMLParserEventHandler() {}

    virtual void onStartTag(const std::string &tag) {}
    virtual void onEndTag(const std::string &tag) {}
    virtual void onEmptyTag() {}
    virtual void onAttributeName(const std::string &attribute) {}
    virtual void onAttributeValue(const std::string &value) {}
    virtual void onInnerText(const std::string &text) {}
    virtual void onReference(const std::string &reference) {}
};

class CallbackXMLParserEventHandler : public XMLParserEventHandler
{
public:
    CallbackXMLParserEventHandler(
        boost::function<void (const std::string &)> startTag,
        boost::function<void (const std::string &)> endTag = NULL,
        boost::function<void ()> emptyTag = NULL,
        boost::function<void (const std::string &)> attribName = NULL,
        boost::function<void (const std::string &)> attribValue = NULL,
        boost::function<void (const std::string &)> innerText = NULL,
        boost::function<void (const std::string &)> reference = NULL)
      : m_startTag(startTag),
        m_endTag(endTag),
        m_attribName(attribName),
        m_attribValue(attribValue),
        m_innerText(innerText),
        m_reference(reference),
        m_emptyTag(emptyTag)
    {}

    void onStartTag(const std::string &tag)
    { if (m_startTag) m_startTag(tag); }
    void onEndTag(const std::string &tag)
    { if (m_endTag) m_endTag(tag); }
    void onEmptyTag()
    { if (m_emptyTag) m_emptyTag(); }
    void onAttributeName(const std::string &attribute)
    { if (m_attribName) m_attribName(attribute); }
    void onAttributeValue(const std::string &value)
    { if (m_attribValue) m_attribValue(value); }
    void onInnerText(const std::string &text)
    { if (m_innerText) m_innerText(text); }
    void onReference(const std::string &reference)
    { if (m_reference) m_reference(reference); }

private:
    boost::function<void (const std::string &)> m_startTag, m_endTag,
        m_attribName, m_attribValue, m_innerText, m_reference;
    boost::function<void ()> m_emptyTag;
};

class XMLParser : public RagelParserWithStack
{
public:
    XMLParser(XMLParserEventHandler &handler)
    : m_handler(handler)
    {}

    void init();
    bool complete() const { return false; }
    bool final() const;
    bool error() const;

protected:
    void exec();

private:
    XMLParserEventHandler &m_handler;
};

}

#endif
