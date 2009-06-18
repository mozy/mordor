#ifndef __XML_PARSER_H__
#define __XML_PARSER_H__
// Copyright (c) 2009 - Decho Corp.

#include <boost/function.hpp>

#include "mordor/common/ragel.h"

class XMLParser : public RagelParserWithStack
{
public:
    XMLParser(boost::function<void (const std::string &)> startTag,
              boost::function<void (const std::string &)> endTag,
              boost::function<void (const std::string &)> attribName,
              boost::function<void (const std::string &)> attribValue,
              boost::function<void (const std::string &)> innerText)
    : m_startTag(startTag),
      m_endTag(endTag),
      m_attribName(attribName),
      m_attribValue(attribValue),
      m_innerText(innerText)
    {}

    void init();
    bool complete() const;
    bool error() const;

protected:
    void exec();

private:
    boost::function<void (const std::string &)> m_startTag, m_endTag,
        m_attribName, m_attribValue, m_innerText;
};

#endif
