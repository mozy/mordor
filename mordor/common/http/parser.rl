// Copyright (c) 2009 - Decho Corp.

#include "parser.h"

#include <string>

#include "mordor/common/version.h"

#ifdef ABSOLUTE
#undef ABSOLUTE
#endif
#ifdef RELATIVE
#undef RELATIVE
#endif

#ifdef WINDOWS
#define strtoull _strtoui64
#endif

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
        // Only copy if necessary
        if (pw != p) {
            *pw = *p;
        }
        ++p; ++pw;
    }
    // Remove trailing whitespace
    do {
        --pw;
    } while ((*pw == ' ' || *pw == '\t' || *pw == '\r' || *pw == '\n') && pw >= start);
    ++pw;
    return std::string(start, pw - start);
}

static
std::string
unquote(char *p, char *pe)
{
    if (pe == p || *p != '"')
        return std::string(p, pe - p);
    char *start = p;
    char *pw = p;

    assert(*p == '"');
    assert(*(pe - 1) == '"');
    ++p; ++pw; ++start;
    --pe;
    bool escaping = false;
    while (p < pe) {
        if (escaping) {
            escaping = false;
        } else if (*p == '\\') {
            escaping = true;
            ++p;
            continue;
        }
        // Only copy if necessary
        if (pw != p) {
            *pw = *p;
        }
        ++p; ++pw;
    }
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

    field_chars = OCTET -- (CTL | CR LF SP HT);
    field_name = token >mark %save_field_name;
    field_value = TEXT* >mark %save_field_value;
    message_header = field_name ":" field_value;
    
    action save_string {
        *m_string = std::string(mark, fpc - mark);
        mark = NULL;
    }
    action save_ulong {
        *m_ulong = strtoull(mark, NULL, 10);
        mark = NULL;
    }
    
    action save_element {
        assert(m_list || m_set);
        if (m_list)
            m_list->push_back(std::string(mark, fpc - mark));
        else
            m_set->insert(std::string(mark, fpc - mark));
        mark = NULL;
    }
    action end_list {
        m_list = NULL;
        m_set = NULL;
    }
    element = token >mark %save_element;
    list = LWS* element ( LWS* ',' LWS* element)* LWS* %end_list;
    
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
        (*m_parameters)[m_temp1] = unquote((char*)mark, (char*)fpc);
        mark = NULL;
    }

    attribute = token >mark %save_parameter_attribute;
    value = (token | quoted_string) >mark %save_parameter_value;
    parameter = attribute '=' value;
    parameterizedListElement = token >mark %save_parameterized_list_element (';' parameter)*;
    parameterizedList = LWS* parameterizedListElement ( LWS* ',' LWS* parameterizedListElement)* LWS*;
    
    action save_auth_scheme {
        if (m_parameterizedList && ((!m_parameterizedList->empty() && m_auth == &m_parameterizedList->back())
            || m_parameterizedList->empty())) {
            ValueWithParameters vp;
            m_parameterizedList->push_back(vp);
            m_auth = &m_parameterizedList->back();
        }
		m_auth->value = std::string(mark, fpc - mark);
		m_parameters = &m_auth->parameters;
		mark = NULL;
    }

    auth_param = attribute ('=' value)?;
    auth_scheme = token;
    challenge = auth_scheme >mark %save_auth_scheme SP LWS* auth_param ( LWS* ',' LWS* auth_param )* LWS*;
    credentials = auth_scheme >mark %save_auth_scheme (LWS+ auth_param)? (LWS* ',' LWS* auth_param )* LWS*;
    challengeList = LWS* challenge ( LWS* ',' LWS* challenge)* LWS*;
    
    action set_connection {
        m_set = &m_general->connection;
    }
    
    action set_trailer {
        m_set = &m_general->trailer;
    }
    
    action set_transfer_encoding {
        m_parameterizedList = &m_general->transferEncoding;
    }

    Connection = 'Connection:'i @set_connection list;
    Trailer = 'Trailer:'i @set_trailer list;
    Transfer_Encoding = 'Transfer-Encoding:'i @set_transfer_encoding parameterizedList;
    
    general_header = Connection | Trailer | Transfer_Encoding;
    general_header_names = 'Connection'i | 'Trailer'i | 'Transfer_Encoding'i;
    
    action set_content_encoding {
        m_list = &m_entity->contentEncoding;
    }

    action set_content_length {
        m_ulong = &m_entity->contentLength;
    }
    
    action save_cr_first_byte_pos {
        m_entity->contentRange.first = strtoull(mark, NULL, 10);
        mark = NULL;
    }

    action save_cr_last_byte_pos {
        m_entity->contentRange.last = strtoull(mark, NULL, 10);
        mark = NULL;
    }
    
    action save_blank_cr {
        m_entity->contentRange.last = 0;
    }

    action save_instance_length {
        m_entity->contentRange.instance = strtoull(mark, NULL, 10);
        mark = NULL;
    }
    
    action set_content_type
    {
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
    
    Content_Encoding = 'Content-Encoding:'i @set_content_encoding list;
    Content_Length = 'Content-Length:'i @set_content_length LWS* DIGIT+ >mark %save_ulong LWS*;
    
    byte_range_resp_spec = (DIGIT+ >mark %save_cr_first_byte_pos '-' DIGIT+ >mark %save_cr_last_byte_pos) | '*' %save_blank_cr;
    content_range_spec = bytes_unit SP byte_range_resp_spec '/' ( DIGIT+ >mark %save_instance_length | '*');
    Content_Range = 'Content-Range:'i LWS* content_range_spec LWS*;
    
    type = token >mark %save_type;
    subtype = token >mark %save_subtype;
    media_type = type'/' subtype (';' LWS* parameter)*;
    Content_Type = 'Content-Type:'i @set_content_type LWS* media_type LWS*;
    
    entity_header = Content_Encoding | Content_Length | Content_Range | Content_Type; # | extension_header;
    entity_header_names = 'Content-Encoding'i | 'Content-Length'i | 'Content-Range'i | 'Content-Type'i;

}%%

%%{
    machine http_request_parser;
    include http_parser;
    include uri_parser "../uri.rl";

    action parse_Method {
        m_request->requestLine.method = parseMethod(mark, fpc);
        mark = NULL;
    }
    
    action set_authorization {
        m_parameterizedList = NULL;
        m_auth = &m_request->request.authorization;
    }

    action set_host {
        m_string = &m_request->request.host;
    }
    
    action set_proxy_authorization {
        m_parameterizedList = NULL;
        m_auth = &m_request->request.proxyAuthorization;
    }

    action save_expectation {
        KeyValueWithParameters kvp;
        kvp.key = std::string(mark, fpc - mark);
        m_request->request.expect.push_back(kvp);
        mark = NULL;
    }
    action save_expectation_value {
        m_request->request.expect.back().value = unquote((char*)mark, (char*)fpc);
        mark = NULL;
    }
    action save_expectation_param {
		m_temp1 = std::string(mark, fpc - mark);
		m_request->request.expect.back().parameters[m_temp1] = "";
        mark = NULL;
    }
    action save_expectation_param_value {
        m_request->request.expect.back().parameters[m_temp1] = unquote((char*)mark, (char*)fpc);
        mark = NULL;
    }

    action save_first_byte_pos {
        m_request->request.range.push_back(RangeSet::value_type(
            strtoull(mark, NULL, 10), ~0ull));
        mark = NULL;
    }

    action save_last_byte_pos {
        if (mark != NULL) {
            m_request->request.range.back().second = strtoull(mark, NULL, 10);
        }
        mark = NULL;
    }

    action save_suffix_byte_pos {
        m_request->request.range.push_back(RangeSet::value_type(
            ~0ull, strtoull(mark, NULL, 10)));
        mark = NULL;
    }

    action set_te {
        m_parameterizedList = &m_request->request.te;
    }


    Authorization = 'Authorization:'i @set_authorization credentials;

    Host = 'Host:'i @set_host LWS* (host (':' port)?) >mark %save_string LWS*;
    
    expect_params = ';' token >mark %save_expectation_param ( '=' (token | quoted_string) >mark %save_expectation_param_value )?;
    expectation = token >mark %save_expectation ( '=' (token | quoted_string) >mark %save_expectation_value expect_params* )?;
    Expect = 'Expect:'i LWS* expectation ( LWS* ',' LWS* expectation )* LWS*;
    
    Proxy_Authorization = 'Proxy-Authorization:'i @set_proxy_authorization credentials;
    
    byte_range_spec = DIGIT+ >mark %save_first_byte_pos '-' (DIGIT+ >mark %save_last_byte_pos)?;
    suffix_byte_range_spec = '-' DIGIT+ > mark %save_suffix_byte_pos;
    byte_range_set = LWS* (byte_range_spec | suffix_byte_range_spec) ( LWS* ',' LWS* (byte_range_spec | suffix_byte_range_spec))* LWS*;
    ranges_specifier = bytes_unit '=' byte_range_set;
    Range = 'Range:'i LWS* ranges_specifier;
    TE = 'TE:'i @set_te parameterizedList;
    
    request_header = Authorization | Host | Expect | Proxy_Authorization | Range;
    request_header_names = 'Authorization'i | 'Host'i | 'Expect'i | 'Proxy-Authorization'i | 'Range'i;
    
    extension_header = (token - (general_header_names | request_header_names | entity_header_names)) >mark %save_field_name
        ':' field_value;

    Method = token >mark %parse_Method;
    Request_URI = ( "*" | absolute_URI | hier_part | authority);
    Request_Line = Method SP Request_URI SP HTTP_Version CRLF;
    Request = Request_Line ((general_header | request_header | entity_header | extension_header) CRLF)* CRLF @done;

    main := Request;
    write data;
}%%

void
HTTP::HTTPParser::init()
{
    m_string = NULL;
    m_set = NULL;
    m_list = NULL;
    m_parameterizedList = NULL;
    m_parameters = NULL;
    m_auth = NULL;
    m_ulong = NULL;
    RagelParser::init();
}

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
    HTTPParser::init();
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
#ifdef MSVC
#pragma warning(push)
#pragma warning(disable : 4244)
#endif
    %% write exec;
#ifdef MSVC
#pragma warning(pop)
#endif
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
    
    action set_accept_ranges
    {
        m_set = &m_response->response.acceptRanges;
    }
    action set_proxy_authenticate {
        m_parameterizedList = &m_response->response.proxyAuthenticate;
    }
    action set_www_authenticate {
        m_parameterizedList = &m_response->response.wwwAuthenticate;
    }
    
    Accept_Ranges = 'Accept-Ranges:'i @set_accept_ranges list;
    Location = 'Location:'i LWS* absolute_URI LWS*;
    Proxy_Authenticate = 'Proxy-Authenticate:'i @set_proxy_authenticate challengeList;
    WWW_Authenticate = 'WWW-Authenticate:'i @set_www_authenticate challengeList;
    
    response_header = Accept_Ranges | Location | Proxy_Authenticate | WWW_Authenticate;
    response_header_names = 'Accept-Ranges'i | 'Location'i | 'Proxy-Authenticate'i | 'WWW-Authenticate'i;
    
    extension_header = (token - (general_header_names | response_header_names | entity_header_names)) >mark %save_field_name
        ':' field_value;

    Status_Code = DIGIT{3} > mark %parse_Status_Code;
    Reason_Phrase = (TEXT -- (CR | LF))* >mark %parse_Reason_Phrase;
    Status_Line = HTTP_Version SP Status_Code SP Reason_Phrase CRLF;
    Response = Status_Line ((general_header | response_header | entity_header | extension_header) CRLF)* CRLF @done;

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
    HTTPParser::init();
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
#ifdef MSVC
#pragma warning(push)
#pragma warning(disable : 4244)
#endif
    %% write exec;
#ifdef MSVC
#pragma warning(pop)
#endif
}

%%{
    machine http_trailer_parser;
    include http_parser;

    extension_header = (token - (entity_header_names)) >mark %save_field_name ':' field_value;

    trailer = (entity_header | extension_header CRLF)*;

    main := trailer CRLF @done;

    write data;
}%%

HTTP::TrailerParser::TrailerParser(EntityHeaders& entity)
: m_entity(&entity)
{}

void
HTTP::TrailerParser::init()
{
    HTTPParser::init();
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
#ifdef MSVC
#pragma warning(push)
#pragma warning(disable : 4244)
#endif
    %% write exec;
#ifdef MSVC
#pragma warning(pop)
#endif
}
