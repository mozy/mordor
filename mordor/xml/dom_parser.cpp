// Copyright (c) 2009 - Mozy, Inc.

#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>
#include "mordor/assert.h"
#include "mordor/xml/dom_parser.h"

namespace Mordor {
namespace DOM {

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

void XMLParser::onStartTag(const std::string &tag) {
    Element *parent = m_element;

    boost::trim(m_text);
    if (!m_text.empty()) {
        Text *text = m_doc->createTextNode(m_text);
        parent->appendChild(text);
    }
    m_element = m_doc->createElement(tag);
    parent->appendChild(m_element);
    m_text.clear();
}

void XMLParser::onEndTag(const std::string &tag) {
    // ignore white space by default
    boost::trim(m_text);
    if (!m_text.empty()) {
        Text *text = m_doc->createTextNode(m_text);
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
        // Real code should also look for character references like ~S&#38;~T
        MORDOR_NOTREACHED();
}

void XMLParser::onCData(const std::string &text) {
    m_text.append(text);
}

}
}
