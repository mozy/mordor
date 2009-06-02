// Copyright (c) 2009 - Decho Corp.
/* To compile to .cpp:
   ragel parser.rl -G2 -o parser.cpp
*/

#include "parser.h"

// From uri.rl
std::string unescape(const std::string& str);

static
HTTP::Version
parseVersion(const char *str)
{
    HTTP::Version ver;
    ver.major = atoi(str + 5);
    ver.minor = atoi(strchr(str + 5, '.') + 1);
    return ver;
}

static
HTTP::Method
parseMethod(const char *str, const char *end)
{
	*(char*)end = '\0';
    for(size_t i = 0; i < 8; ++i) {
		if (stricmp(str, HTTP::methods[i]) == 0) {
			return (HTTP::Method)i;
		}
    }
    throw std::invalid_argument("Unrecognized method");
}

static
std::string
unfold(char *p, char *pe)
{
    char *start = p;
    char *pw = p;

    while (p < pe) {
        // Skip leading whitespace
        if (pw == start) {
            if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
                ++p; ++pw; ++start;
                continue;
            }
        }
        // Remove interior line breaks
        if (*p == '\r' || *p == '\n') {
            ++p;
            continue;
        }
        // Only copy if necessary
        if (pw != p) {
            *pw = *p;
        }
        ++p; ++pw;
    }
    // Remove trailing whitespace (\r and \n already removed)
    do {
        --pw;
    } while ((*pw == ' ' || *pw == '\t') && pw >= start);
    ++pw;
    return std::string(start, pw - start);
}

%%{
    machine http_parser;

    action mark { mark = fpc; }
    action done { fbreak; }

    # basic character types
    OCTET = any;
    CHAR = ascii;
    UPALPHA = "A".."Z";
    LOALPHA = "a".."z";
    ALPHA = alpha;
    DIGIT = digit;
    CTL = cntrl | 127;
    CR = "\r";
    LF = "\n";
    SP = " ";
    HT = "\t";

    # almost-basic character types
    # note that we allow a single LF for a CR LF
    CRLF = CR LF | LF;
    LWS = CRLF? ( SP | HT )+;
    TEXT = LWS | (OCTET -- CTL);
    HEX = xdigit;

    # some basic tokens
    separators = "(" | ")" | "<" | ">" | "@" | "," | ";" | ":" | "\\" | "\"" | "/" | "[" | "]" | "?" | "=" | "{" | "}" | SP | HT;
    token = (CHAR -- (separators | CTL))+;
    quoted_pair = "\\" CHAR;
    ctext = TEXT -- ("(" | ")");
    base_comment = "(" ( ctext | quoted_pair )* ")";
    comment = "(" ( ctext | quoted_pair | base_comment )* ")";
    qdtext = TEXT -- "\"";
    quoted_string = "\"" ( qdtext | quoted_pair )* "\"";

    action parse_HTTP_Version {
        *m_ver = parseVersion(mark);
        mark = NULL;
    }

    HTTP_Version = ("HTTP/" DIGIT+ "." DIGIT+) >mark %parse_HTTP_Version;

    delta_seconds = DIGIT+;

    product_version = token;
    product = token ("/" product_version)?;

    qvalue = ("0" ("." DIGIT{0,3})) | ("1" ("." "0"{0,3}));

    subtag = ALPHA{1,8};
    primary_tag = ALPHA{1,8};
    language_tag = primary_tag ("-" subtag);

    weak = "W/";
    opaque_tag = quoted_string;
    entity_tag = (weak)? opaque_tag;  

    bytes_unit = "bytes";
    other_range_unit = token;
    range_unit = bytes_unit | other_range_unit;

    action save_field_name {
        m_temp1 = std::string(mark, fpc - mark);
        mark = NULL;
    }
    action save_field_value {
        if (m_headerHandled) {
            m_headerHandled = false;
        } else {
            std::string fieldValue = unfold((char*)mark, (char*)fpc);
            
            StringMap::iterator it = m_entity->extension.find(m_temp1);
            if (it == m_entity->extension.end()) {
                m_entity->extension[m_temp1] = fieldValue;
            } else {
                it->second.append(", ");
                it->second.append(fieldValue);
            }
            mark = NULL;
        }
    }

    field_chars = OCTET -- (CTL | CR LF SP HT);
    field_name = token >mark %save_field_name;
    field_value = TEXT* >mark %save_field_value;
    message_header = field_name ":" field_value;
    
    action save_string {
        *m_string = std::string(mark, fpc - mark);
        mark = NULL;
    }
    action save_ulong {
        *m_ulong = _atoi64(mark);
        mark = NULL;
    }
    
    action save_element {
        m_list->insert(std::string(mark, fpc - mark));
        mark = NULL;
    }
    action save_element_eof {
		m_list->insert(std::string(mark, pe - mark));
        mark = NULL;
    }
    element = token >mark %save_element %/save_element_eof;
    list = LWS* element ( LWS* ',' LWS* element)* LWS*;
    
    action save_parameterized_list_element {
        ValueWithParameters vp;
        vp.value = std::string(mark, fpc - mark);
        m_parameterizedList->push_back(vp);
        m_parameters = &m_parameterizedList->back().parameters;
        mark = NULL;
    }
    
    action save_parameter_attribute {
        m_temp1 = std::string(mark, fpc - mark);
        mark = NULL;
    }
    
    action save_parameter_value {
        (*m_parameters)[m_temp1] = std::string(mark, fpc - mark);
        mark = NULL;
    }
    
    attribute = token >mark %save_parameter_attribute;
    value = token | quoted_string >mark %save_parameter_value;
    parameter = attribute '=' value;
    parameterizedListElement = token >mark %save_parameterized_list_element (';' parameter)*;
    parameterizedList = LWS* parameterizedListElement ( LWS* ',' LWS* parameterizedListElement)* LWS*;
    
    action set_connection {
        m_headerHandled = true;
        m_list = &m_general->connection;
    }
    
    action set_transfer_encoding {
        m_headerHandled = true;
        m_parameterizedList = &m_general->transferEncoding;
    }

    Connection = 'Connection:' @set_connection list;
    Transfer_Encoding = 'Transfer-Encoding:' @set_transfer_encoding parameterizedList;
    
    general_header = Connection | Transfer_Encoding;
    
    action set_content_length {
        m_headerHandled = true;
        m_ulong = &m_entity->contentLength;
    }
    
    Content_Length = 'Content-Length:' @set_content_length LWS* DIGIT+ >mark %save_ulong LWS*;
    
    action set_content_type
    {
		m_headerHandled = true;
		m_parameters = &m_entity->contentType.parameters;
    }
    action save_type
    {
		m_entity->contentType.type = std::string(mark, fpc - mark);
		mark = NULL;
    }
    action save_subtype
    {
		m_entity->contentType.subtype = std::string(mark, fpc - mark);
		mark = NULL;
    }
    
    type = token >mark %save_type;
    subtype = token >mark %save_subtype;
    media_type = type'/' subtype (';' parameter)*;
    Content_Type = 'Content-Type:' @set_content_type LWS* media_type LWS*;
    
    extension_header = message_header;

    entity_header = Content_Length | Content_Type | extension_header;

}%%

%%{
    machine http_request_parser;
    include http_parser;
    include uri_parser "../uri.rl";

    action parse_Method {
        m_request->requestLine.method = parseMethod(mark, fpc);
        mark = NULL;
    }
    
    action set_host {
        m_headerHandled = true;
        m_string = &m_request->request.host;
    }

    Host = 'Host:' @set_host LWS* host (':' port)? >mark %save_string LWS*;
    
    request_header = Host;

    Method = token >mark %parse_Method;
    Request_URI = ( "*" | absolute_URI | hier_part | authority);
    Request_Line = Method SP Request_URI SP HTTP_Version CRLF;
    Request = Request_Line ((general_header | request_header | entity_header) CRLF)* CRLF @done;

    main := Request;
    write data;
}%%

HTTP::RequestParser::RequestParser(Request& request)
: m_request(&request),
  m_ver(&request.requestLine.ver),
  m_uri(&request.requestLine.uri),
  m_path(&request.requestLine.uri.path),
  m_general(&request.general),
  m_entity(&request.entity)
{}

void
HTTP::RequestParser::init()
{
    RagelParser::init();
    %% write init;
}

bool
HTTP::RequestParser::complete() const
{
    return cs >= http_request_parser_first_final;
}

bool
HTTP::RequestParser::error() const
{
    return cs == http_request_parser_error;
}

void
HTTP::RequestParser::exec()
{
    %% write exec;
}

%%{
    machine http_response_parser;
    include http_parser;
    include uri_parser "../uri.rl";
    
    action parse_Status_Code {
        m_response->status.status = (HTTP::Status)atoi(mark);
        mark = NULL;
    }

    action parse_Reason_Phrase {
        m_response->status.reason = std::string(mark, fpc - mark);
        mark = NULL;
    }
    
    action set_location {
        m_headerHandled = true;
    }
    
    Location = 'Location:' @set_location LWS* absolute_URI LWS*;
    
    response_header = Location;

    Status_Code = DIGIT{3} > mark %parse_Status_Code;
    Reason_Phrase = (TEXT -- (CR | LF))* >mark %parse_Reason_Phrase;
    Status_Line = HTTP_Version SP Status_Code SP Reason_Phrase CRLF;
    Response = Status_Line ((general_header | response_header | entity_header) CRLF)* CRLF @done;

    main := Response;

    write data;
}%%

HTTP::ResponseParser::ResponseParser(Response& response)
: m_response(&response),
  m_ver(&response.status.ver),
  m_uri(&response.response.location),
  m_path(&response.response.location.path),
  m_general(&response.general),
  m_entity(&response.entity)
{}

void
HTTP::ResponseParser::init()
{
    RagelParser::init();
    %% write init;
}

bool
HTTP::ResponseParser::complete() const
{
    return cs >= http_response_parser_first_final;
}

bool
HTTP::ResponseParser::error() const
{
    return cs == http_response_parser_error;
}

void
HTTP::ResponseParser::exec()
{
    %% write exec;
}

%%{
    machine http_trailer_parser;
    include http_parser;

    trailer = (entity_header CRLF)*;

    main := trailer CRLF @done;

    write data;
}%%

HTTP::TrailerParser::TrailerParser(EntityHeaders& entity)
: m_entity(&entity)
{}

void
HTTP::TrailerParser::init()
{
    RagelParser::init();
    %% write init;
}

bool
HTTP::TrailerParser::complete() const
{
    return cs >= http_trailer_parser_first_final;
}

bool
HTTP::TrailerParser::error() const
{
    return cs == http_trailer_parser_error;
}

void
HTTP::TrailerParser::exec()
{
    %% write exec;
}
