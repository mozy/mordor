// Copyright (c) 2010 - Mozy, Inc.

#include "mordor/pch.h"

#include "mordor/xml/parser.h"
#include "mordor/test/test.h"

using namespace Mordor;

static void callback(std::string &value, int &called,
                     const std::string &string)
{
    value.append(string);
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
        boost::bind(&callback, boost::ref(text), boost::ref(called), _1));
    XMLParser parser(handler);
    parser.run("<root>sometext&amp;somemoretext</root>");
    MORDOR_ASSERT(parser.final());
    MORDOR_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT_EQUAL(called, 3);
    MORDOR_TEST_ASSERT_EQUAL(text, "sometext&amp;somemoretext");

    text.clear();
    called = 0;
    parser.run("<path>/Users/test1/Public/C:\\abc\\n&apos;&amp;.txt1</path>");
    MORDOR_ASSERT(parser.final());
    MORDOR_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT_EQUAL(called, 4);
    MORDOR_TEST_ASSERT_EQUAL(text, "/Users/test1/Public/C:\\abc\\n&apos;&amp;.txt1");
}
