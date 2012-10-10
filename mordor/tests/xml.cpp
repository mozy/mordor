// Copyright (c) 2010 - Mozy, Inc.

#include <boost/bind.hpp>

#include "mordor/xml/dom_parser.h"
#include "mordor/test/test.h"
#include "mordor/string.h"

using namespace Mordor;

static void callback(std::string &value, int &called,
                     const std::string &string)
{
    value.append(string);
    ++called;
}

static void reference(std::string &value, int &called,
                              const std::string &string)
{
    // When processing inner text a special callback is invoked for xml
    // references
    if (string == "&amp;")
        value.append("&");
    else if (string == "&gt;")
        value.append(">");
    else if (string == "&lt;")
        value.append("<");
    else if (string == "&quot;")
        value.append("\"");
    else if (string == "&apos;")
        value.append("\'");
    else
        // Real code should also look for character references like "&#38;"
        MORDOR_NOTREACHED();

    ++called;
}

static void handlereferencecallback(std::string &value, int &called,
                              const std::string &string)
{
    // A full Attribute value is passed, which may contain xml references
    std::string rawstring(string);

    replace(rawstring, "&amp;", "&");
    replace(rawstring, "&gt;", ">" );
    replace(rawstring, "&lt;", "<" );
    replace(rawstring, "&quot;", "\"");
    replace(rawstring, "&apos;", "\'");
    // Should also look for character references like "&#38;"

    value.append(rawstring);
    ++called;
}


static void emptyTag(int &called)
{
    ++called;
}

MORDOR_UNITTEST(XMLParser, basic)
{
    std::string start, end;
    int calledStart = 0, calledEnd = 0;
    CallbackXMLParserEventHandler handler(
        boost::bind(&callback, boost::ref(start), boost::ref(calledStart), _1),
        boost::bind(&callback, boost::ref(end), boost::ref(calledEnd), _1));
    XMLParser parser(handler);
    parser.run("<body></body>");
    MORDOR_ASSERT(parser.final());
    MORDOR_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT_EQUAL(calledStart, 1);
    MORDOR_TEST_ASSERT_EQUAL(start, "body");
    MORDOR_TEST_ASSERT_EQUAL(calledEnd, 1);
    MORDOR_TEST_ASSERT_EQUAL(end, "body");
}

MORDOR_UNITTEST(XMLParser, emptyTag)
{
    std::string tag;
    int calledStart = 0, calledEmpty = 0;
    CallbackXMLParserEventHandler handler(
        boost::bind(&callback, boost::ref(tag), boost::ref(calledStart), _1),
        NULL,
        boost::bind(&emptyTag, boost::ref(calledEmpty)));
    XMLParser parser(handler);
    parser.run("<empty />");
    MORDOR_ASSERT(parser.final());
    MORDOR_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT_EQUAL(calledStart, 1);
    MORDOR_TEST_ASSERT_EQUAL(tag, "empty");
    MORDOR_TEST_ASSERT_EQUAL(calledEmpty, 1);
}

MORDOR_UNITTEST(XMLParser, references)
{
    std::string text;
    int called = 0;
    CallbackXMLParserEventHandler handler(NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        boost::bind(&callback, boost::ref(text), boost::ref(called), _1),
        boost::bind(&reference, boost::ref(text), boost::ref(called), _1));
    XMLParser parser(handler);
    parser.run("<root>sometext&amp;somemoretext</root>");
    MORDOR_ASSERT(parser.final());
    MORDOR_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT_EQUAL(called, 3);
    MORDOR_TEST_ASSERT_EQUAL(text, "sometext&somemoretext");

    text.clear();
    called = 0;
    parser.run("<path>/Users/test1/Public/&lt;C:\\abc\\n&apos;&amp;.txt1</path>");
    MORDOR_ASSERT(parser.final());
    MORDOR_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT_EQUAL(called, 6);
    MORDOR_TEST_ASSERT_EQUAL(text, "/Users/test1/Public/<C:\\abc\\n'&.txt1");

    text.clear();
    called = 0;
    parser.run("<p>&quot;&amp;</p>");
    MORDOR_ASSERT(parser.final());
    MORDOR_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT_EQUAL(called, 2);
    MORDOR_TEST_ASSERT_EQUAL(text, "\"&");
}

MORDOR_UNITTEST(XMLParser, attribute)
{
    std::string key, value;
    int calledKey = 0, calledVal = 0;
    CallbackXMLParserEventHandler handler(
        NULL,
        NULL,
        NULL,
        boost::bind(&callback, boost::ref(key), boost::ref(calledKey), _1),
        boost::bind(&handlereferencecallback, boost::ref(value), boost::ref(calledVal), _1));
    XMLParser parser(handler);

    parser.run("<mykey query=\"mymail\"/>");
    MORDOR_ASSERT(parser.final());
    MORDOR_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT_EQUAL(calledKey, 1);
    MORDOR_TEST_ASSERT_EQUAL(key, "query");
    MORDOR_TEST_ASSERT_EQUAL(calledVal, 1);
    MORDOR_TEST_ASSERT_EQUAL(value, "mymail");

    key.clear(); value.clear();
    calledKey = 0; calledVal = 0;
    parser.run("<mykey qry=\"&quot;mymail&apos;\"/>");
    MORDOR_ASSERT(parser.final());
    MORDOR_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT_EQUAL(calledKey, 1);
    MORDOR_TEST_ASSERT_EQUAL(key, "qry");
    MORDOR_TEST_ASSERT_EQUAL(calledVal, 1);
    MORDOR_TEST_ASSERT_EQUAL(value, "\"mymail\'");

    key.clear(); value.clear();
    calledKey = 0; calledVal = 0;
    parser.run("<mykey a=\'&quot;\"\' b=\"foo's\"/>");
    MORDOR_ASSERT(parser.final());
    MORDOR_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT_EQUAL(calledKey, 2);
    MORDOR_TEST_ASSERT_EQUAL(key, "ab");  // The test callback concatenates them together
    MORDOR_TEST_ASSERT_EQUAL(calledVal, 2);
    MORDOR_TEST_ASSERT_EQUAL(value, "\"\"foo's");

    // A more complex real life XML
    key.clear(); value.clear();
    parser.run("<folder id=\"3\" i4ms=\"692002\" ms=\"456229\" name=\"Trash\" n=\"38\" l=\"1\"><search id=\"384010\" sortBy=\"dateDesc\" query=\"(to:(foo) tag:&quot;mymail&quot;\" name=\"inbox my mail\" l=\"3\" /></folder>");
    MORDOR_ASSERT(parser.final());
    MORDOR_ASSERT(!parser.error());
    MORDOR_ASSERT(!key.empty());
    MORDOR_ASSERT(!value.empty());
}

const char * xml_typical =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\t"
"<NamedObject type='file' deleted='false'>\n"
"     <id>/sync/1/path/x.y.txt</id>"
"     <versionId>1310496040</versionId>"
"\t<Version deleted='false'>"
"\t\t<versionId>1310496040</versionId>"
"<size>8</size>"
"<stime>Tue, 12 Jul 2011 18:40:40 GMT</stime>\r\n"
"<fileAttributes archive='true' not_content_indexed='true' />"
"</Version>"
"</NamedObject>";

MORDOR_UNITTEST(XMLParser, parsedocument)
{
    // Confirm that a full xml with random white space and
    // typical elements can be parsed.
    XMLParserEventHandler defaulthandler;
    XMLParser parser(defaulthandler);
    parser.run(xml_typical);
    MORDOR_ASSERT(parser.final());
    MORDOR_ASSERT(!parser.error());
}

MORDOR_UNITTEST(XMLParser, domParser)
{
    DOM::XMLParser parser;
    DOM::Document::ptr doc = parser.loadDocument(
        "<?xml version='1.0' encoding='UTF-8'?>\n"
        "<root><children>\n"
        "<child>  this is child1\n</child>\n"
        "<child>this is child2</child>\n"
        "<empty id=\"1\" attr1=\"one\" attr2='two' />\n"
        "<tag>tag text<subtag>  &quot;some &amp; some&quot;  </subtag></tag>\n"
        "</children></root>");

    DOM::Element *root = (DOM::Element *)doc->getElementsByTagName("root")[0];

    MORDOR_TEST_ASSERT_EQUAL("root", root->nodeName());
    DOM::NodeList l = root->getElementsByTagName("child");
    MORDOR_TEST_ASSERT_EQUAL(2u, l.size());
    MORDOR_TEST_ASSERT_EQUAL("this is child1", l[0]->text());
    MORDOR_TEST_ASSERT_EQUAL("this is child1", l[0]->firstChild()->nodeValue());
    MORDOR_TEST_ASSERT_EQUAL("this is child2", l[1]->text());

    DOM::Node *children = root->firstChild();
    MORDOR_TEST_ASSERT_EQUAL(DOM::ELEMENT, children->nodeType());
    MORDOR_TEST_ASSERT_EQUAL("children", children->nodeName());
    MORDOR_TEST_ASSERT_EQUAL(4u, children->childNodes().size());

    DOM::Element *empty = (DOM::Element *)children->childNodes()[2];
    MORDOR_TEST_ASSERT_EQUAL("empty", empty->nodeName());
    MORDOR_TEST_ASSERT_EQUAL(DOM::ELEMENT, empty->nodeType());
    MORDOR_TEST_ASSERT_EQUAL("", empty->text());
    MORDOR_TEST_ASSERT_EQUAL("1", empty->id);
    MORDOR_TEST_ASSERT_EQUAL("one", empty->attribute("attr1"));
    MORDOR_TEST_ASSERT_EQUAL("two", empty->attribute("attr2"));
    MORDOR_TEST_ASSERT_EQUAL(true, empty->hasAttribute("attr2"));
    MORDOR_TEST_ASSERT_EQUAL(false, empty->hasAttribute("attr3"));

    DOM::Element *node = doc->getElementById("1");
    MORDOR_TEST_ASSERT_EQUAL(empty, node);
    MORDOR_TEST_ASSERT_EQUAL((DOM::Element *)NULL, doc->getElementById("haha"));

    DOM::Node *tag = children->childNodes()[3];
    MORDOR_TEST_ASSERT_EQUAL(DOM::ELEMENT, tag->nodeType());
    MORDOR_TEST_ASSERT_EQUAL("tag", tag->nodeName());
    MORDOR_TEST_ASSERT_EQUAL("tag text", tag->text());
    MORDOR_TEST_ASSERT_EQUAL("tag text", tag->firstChild()->nodeValue());

    DOM::NodeList subtags = ((DOM::Element *)tag)->getElementsByTagName("subtag");
    MORDOR_TEST_ASSERT_EQUAL(1u, subtags.size());
    MORDOR_TEST_ASSERT_EQUAL("\"some & some\"", subtags[0]->text());
}

MORDOR_UNITTEST(XMLParser, domParserInvalid)
{
    DOM::XMLParser parser;
    MORDOR_TEST_ASSERT_EXCEPTION(parser.loadDocument(
        "<?xml version='1.0' encoding='UTF-8'?>\n"
        "<root<children>\"somesome\n"),
        std::invalid_argument);
}


#if 0
// Disabled until comment support fixed
MORDOR_UNITTEST(XMLParser, parsecomment)
{
    XMLParserEventHandler defaulthandler;
    XMLParser parser(defaulthandler);

    parser.run("<!-- a comment -->");
    MORDOR_ASSERT(parser.final());
    MORDOR_ASSERT(!parser.error());

    parser.run("foo <!-- bar --> baz <!-- qox --> blurb");
    MORDOR_ASSERT(parser.final());
    MORDOR_ASSERT(!parser.error());

    parser.run("<![CDATA[ <!-- OMGWTFBBQ ]]>Shoulda used a <!-- real parser -->");
    MORDOR_ASSERT(parser.final());
    MORDOR_ASSERT(!parser.error());
}
#endif

