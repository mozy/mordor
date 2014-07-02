// Copyright (c) 2009 - Mozy, Inc.

#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>
#include "mordor/assert.h"
#include "mordor/xml/dom_parser.h"

namespace Mordor {
namespace DOM {

#define PARSE_DOC(xml) \
    Document *doc = new Document(); \
    m_element = doc->documentElement(); \
    CallbackXMLParserEventHandler handler( \
        boost::bind(&XMLParser::onStartTag, this, _1, doc), \
        boost::bind(&XMLParser::onEndTag, this, _1, doc), \
        boost::bind(&XMLParser::onEmptyTag, this), \
        boost::bind(&XMLParser::onAttributeName, this, _1), \
        boost::bind(&XMLParser::onAttributeValue, this, _1), \
        boost::bind(&XMLParser::onInnerText, this, _1), \
        boost::bind(&XMLParser::onReference, this, _1)); \
        Mordor::XMLParser parser(handler); \
    parser.run(xml); \
    if (!parser.final() || parser.error()) { \
        delete doc; \
        MORDOR_THROW_EXCEPTION(std::invalid_argument("failed to parse: Invalid xml")); \
    } \
    return Document::ptr(doc);

NodeList Element::getElementsByTagName(const std::string &tagName) {
    NodeList l;
    for (size_t i = 0; i < m_children.size(); i++) {
        if (m_children[i]->nodeType() == ELEMENT) {
            Element *e = (Element *)m_children[i];
            if (e->nodeName() == tagName) {
                l.push_back(e);
            }
            NodeList nodes = e->getElementsByTagName(tagName);
            std::copy(nodes.begin(), nodes.end(), std::back_inserter(l));
        }
    }
    return l;
}

Element * Element::getElementById(const std::string &id) {
    if (id.empty()) return NULL;
    Element *e = NULL;
    for (size_t i = 0; i < m_children.size(); i++) {
        if (m_children[i]->nodeType() == ELEMENT) {
            e = (Element *)m_children[i];
            if (e->id == id) {
                return e;
            }
            e = e->getElementById(id);
        }
    }
    return e;
}

Document::ptr XMLParser::loadDocument(const std::string& str) {
    PARSE_DOC(str)
}
Document::ptr XMLParser::loadDocument(const char *str) {
    PARSE_DOC(str)
}
Document::ptr XMLParser::loadDocument(const Buffer& buffer) {
    PARSE_DOC(buffer)
}
Document::ptr XMLParser::loadDocument(Stream& stream) {
    PARSE_DOC(stream)
}
Document::ptr XMLParser::loadDocument(boost::shared_ptr<Stream> stream) {
    PARSE_DOC(*stream)
}

void XMLParser::onStartTag(const std::string &tag, Document *doc) {
    Element *parent = m_element;

    boost::trim(m_text);
    if (!m_text.empty()) {
        Text *text = doc->createTextNode(m_text);
        parent->appendChild(text);
    }
    m_element = doc->createElement(tag);
    parent->appendChild(m_element);
    m_text.clear();
}

void XMLParser::onEndTag(const std::string &tag, Document *doc) {
    // choke on unmatched end tag
    if (tag != m_element->nodeName()) {
        MORDOR_THROW_EXCEPTION(
            std::invalid_argument(
                std::string("failed to parse: unmatched end tag: ") + tag));
    }
    // ignore white space by default
    boost::trim(m_text);
    if (!m_text.empty()) {
        Text *text = doc->createTextNode(m_text);
        m_element->appendChild(text);
    }
    m_element = (Element *)m_element->parentNode();
    m_text.clear();
}

void XMLParser::onEmptyTag() {
    m_element = (Element *)m_element->parentNode();
}

void XMLParser::onAttributeName(const std::string &attribute) {
    m_attrib = attribute;
}

void XMLParser::onAttributeValue(const std::string &value) {
    if (m_attrib == "id") {
        m_element->id = value;
    }
    m_element->attribute(m_attrib, value);
    m_attrib.clear();
}

void XMLParser::onInnerText(const std::string &text) {
    m_text.append(text);
}

void XMLParser::onReference(const std::string &reference) {
    if (reference == "&amp;")
        m_text.append("&");
    else if (reference == "&gt;")
        m_text.append(">");
    else if (reference == "&lt;")
        m_text.append("<");
    else if (reference == "&quot;")
        m_text.append("\"");
    else if (reference == "&apos;")
        m_text.append("\'");
    else
        // TODO: Real code should convert escaped characters of specific encodings to UTF-8.
        // Preserve original text for now, and consider converting following escaped
        // characters to UTF-8 later:
        // 1. UTF-* encoding character, such as: &#x30C6
        // 2. Code point number, such as: &#931
        m_text.append(reference);
}
}
}
