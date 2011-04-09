// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include "mordor/xml/parser.h"

using namespace Mordor;

%%{
    machine xml_parser;

    action mark { mark = fpc;}
    action done { fbreak; }
    prepush {
        prepush();
    }
    postpop {
        postpop();
    }

    Char = '\t' | '\n' | '\r' | [' '-255];
    S = (' ' | '\t' | '\r' | '\n')+;

    NameStartChar = ':' | [A-Z] | '_' | [a-z] | 0xC0..0xD6 | 0xD8..0xF6 | 0xF8..0xFF;
    NameChar = NameStartChar | '-' | '.' | [0-9] | 0xB7;
    Name = NameStartChar NameChar*;
    Names = Name (' ' Name)*;
    Nmtoken = NameChar+;
    Nmtokens = Nmtoken (' ' Nmtoken)*;

    CharData = [^<&]* - ([^<&]* ']]>' [^<&]*);

    action reference
    {
        m_handler.onReference(std::string(mark, fpc-mark));
        mark = NULL;
    }

    CharRef = '&#' [0-9]+ ';' | '&#x' [0-9a-fA-F]+ ';';
    EntityRef = '&' Name ';';
    Reference = (EntityRef | CharRef) >mark %reference;
    PEReference = '%' Name ';';

    action attrib_value
    {
        m_handler.onAttributeValue(std::string(mark, fpc-mark));
        mark = NULL;
    }

    EntityValue = '"' ([^%&"] | PEReference | Reference)* '"' |
                  "'" ([^%&'] | PEReference | Reference)* '"';
    AttValue = '"' ([^<&"] | Reference)* >mark %attrib_value '"' |
               "'" ([^<&'] | Reference)* >mark %attrib_value "'";
    SystemLiteral = ('"' [^"]* '"') | ("'" [^']* "'");
    PubidChar = ' ' | '\r' | '\n' | [a-zA-Z0-9] | ['()+,./:=?;!*#@$_%] | '-';
    PubidLiteral = '"' PubidChar* '"' | "'" (PubidChar* -- "'") "'";

    Comment = '<!--' ((Char - '-') | ('-' (Char - '-')))* '-->';

    PITarget = Name - ([Xx][Mm][Ll]);
    PI = '<?' PITarget (S (Char* - (Char* '?>' Char*)))? '?>';

    Misc = Comment | PI | S;

    Eq = S? '=' S?;
    VersionNum = '1.' [0-9]+;
    VersionInfo = S 'version' Eq ("'" VersionNum "'" | '"' VersionNum '"');
    EncName = [A-Za-z] ([A-Za-z0-9._] | '-')*;
    EncodingDecl = S 'encoding' Eq ('"' EncName '"' | "'" EncName "'");
    SDDecl = S 'standalone' Eq (("'" ('yes' | 'no') "'") | ('"' ('yes' | 'no') '"'));
    XMLDecl = '<?xml' VersionInfo EncodingDecl? SDDecl? S? '?>';

    ExternalID = 'SYSTEM' S SystemLiteral | 'PUBLIC' S PubidLiteral S SystemLiteral;
    #markupdecl = elementdecl | AttlistDecl | EntityDecl | NotationDecl | PI | Comment;
    #intSubset = (markupdecl | DeclSep)*;
    intSubset = '';
    doctypedecl = '<!DOCTYPE' S Name (S ExternalID)? S? ('[' intSubset ']' S?)? '>';
    prolog = XMLDecl? Misc* (doctypedecl Misc*)?;

    action cdata
    {
        if (fpc != mark) {
            m_handler.onCData(std::string(mark, fpc - mark));
            mark = NULL;
        }
    }

    CDStart = '<![CDATA[';
    CData = (Char* - (Char* ']]>' Char*));
    CDEnd = ']]>';
    CDSect = CDStart CData >mark %cdata CDEnd;

    action start_tag {
        m_handler.onStartTag(std::string(mark, fpc-mark));
        mark = NULL;
    }
    action end_tag {
        m_handler.onEndTag(std::string(mark, fpc-mark));
        mark = NULL;
    }
    action empty_tag {
        m_handler.onEmptyTag();
    }

    action attrib_name
    {
        m_handler.onAttributeName(std::string(mark, fpc-mark));
        mark = NULL;
    }

    Attribute = Name >mark %attrib_name Eq AttValue;
    STag = '<' Name >mark %start_tag (S Attribute)* S? '>';
    ETag = '</' Name >mark %end_tag S? '>';
    EmptyElemTag = '<' Name >mark %start_tag (S Attribute)* S? '/>' %empty_tag;
    action call_parse_content {
        fcall *xml_parser_en_parse_content;
    }
    element = EmptyElemTag | STag @call_parse_content; #content ETag;

    action inner_text
    {
        if (fpc != mark) {
            m_handler.onInnerText(std::string(mark, fpc-mark));
            mark = NULL;
        }
    }

    content = CharData? >mark %inner_text ((element | Reference | CDSect | PI | Comment) CharData? >mark %inner_text)*;

    action element_finished {
        fret;
    }
    parse_content := parse_content_lbl: content ETag @element_finished;

    document = prolog element Misc*;

    main := document;
    write data;
}%%

void
XMLParser::init()
{
    RagelParserWithStack::init();
    %% write init;
}

void
XMLParser::exec()
{
#ifdef MSVC
#pragma warning(push)
#pragma warning(disable : 4244)
#endif
        %% write exec;
#ifdef MSVC
#pragma warning(pop)
#endif
}

bool
XMLParser::final() const
{
    return cs >= xml_parser_first_final;
}

bool
XMLParser::error() const
{
    return cs == xml_parser_error;
}
