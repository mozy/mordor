#ifndef __MORDOR_DOM_DOM_PARSER_H__
#define __MORDOR_DOM_DOM_PARSER_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/exception.h"
#include "mordor/xml/parser.h"

namespace Mordor {
    class Stream;
    struct Buffer;
/////////////////////////////////////////
// partial implementation of DOM spec  //
/////////////////////////////////////////
namespace DOM {

    // simplified DOM impl, doesn't support documentfragment, comment, cdata, etc
    // also doesn't support schema valiation, namespace, insert/remove/replace nodes
    enum NodeType {
        DOCUMENT,
        ELEMENT,
        TEXT,
    };

    class Node;
    class Document;
    class XMLParser;
    typedef std::vector<Node *> NodeList;

    class Node {
    public:
        virtual NodeList &childNodes() { return m_children; }
        virtual Node *firstChild() { return m_children.empty() ? NULL : m_children[0]; }
        virtual const std::string &nodeName() { return m_nodeName; }
        virtual NodeType nodeType() = 0;
        virtual void nodeValue(const std::string &value) { m_nodeValue = value; }
        virtual const std::string &nodeValue() { return m_nodeValue; }
        virtual Node *parentNode() { return m_parent; }
        virtual std::string text() {
            Node *child = firstChild();
            return (child && child->nodeType() == TEXT) ? child->nodeValue() : "";
        }
        virtual Node *appendChild(Node *node) {
            node->m_parent = this;
            m_children.push_back(node);
            return node;
        }

    protected:
        Node(const std::string &nodeName, const std::string &nodeValue) :
            m_nodeName(nodeName), m_nodeValue(nodeValue), m_parent(NULL) {}
        virtual ~Node() {
            for (size_t i = 0; i < m_children.size(); i++) {
                delete m_children[i];
            }
        }
    protected:
        std::string m_nodeName;
        std::string m_nodeValue;
        Node *m_parent;
        NodeList m_children;
    };


    class Element : public Node {
        friend class Document;
    public:
        void attribute(const std::string &attr, const std::string val) { m_attrs[attr] = val; }
        const std::string &attribute(const std::string &attr) { return m_attrs[attr]; }
        bool hasAttribute(const std::string &attr) { return m_attrs.find(attr) != m_attrs.end(); }
        NodeType nodeType() { return ELEMENT; }

        NodeList getElementsByTagName(const std::string &tagName);
        Element * getElementById(const std::string &id);
    protected:
        Element(const std::string &name) : Node(name, "") {}
    public:
        std::string id;
    private:
        std::map<std::string, std::string> m_attrs;
    };

    class Text : public Node {
        friend class Document;
    public:
        NodeType nodeType() { return TEXT; }
    protected:
        Text(const std::string &text) : Node("#text", text) {}
    };

    class Document : public Element {
        friend class XMLParser;
    public:
        typedef boost::shared_ptr<Document> ptr;
        NodeType nodeType() { return DOCUMENT; }

        Element *documentElement() { return m_docElement; }
        Element *createElement(const std::string &name) { return new Element(name); }
        Text *createTextNode(std::string value) { return new Text(value); }

    protected:
        Document() : Element("#document") { m_docElement = (Element*)appendChild(new Element("")); }

    private:
        Element *m_docElement;
    };

    class XMLParser : public XMLParserEventHandler {
    public:
        XMLParser() : m_element(NULL) {}
        template <class T>
        Document::ptr loadDocument(T &xml)
        {
            m_doc.reset(new Document());
            m_element = m_doc->documentElement();
            Mordor::XMLParser parser(*this);
            parser.run(xml);
            if (!parser.final() || parser.error()) {
                MORDOR_THROW_EXCEPTION(std::invalid_argument("failed to parse: Invalid xml"));
            }
            return m_doc;
        }

    protected:
        void onStartTag(const std::string &tag);
        void onEndTag(const std::string &tag);
        void onEmptyTag();
        void onAttributeName(const std::string &attribute);
        void onAttributeValue(const std::string &value);
        void onInnerText(const std::string &text);
        void onReference(const std::string &reference);
        void onCData(const std::string &text);
    private:
        Document::ptr m_doc;
        Element *m_element;
        std::string m_text, m_attrib;
    };
}

}
#endif
