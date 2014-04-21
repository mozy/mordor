// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include "mordor/http/parser.h"

#include <locale>
#include <sstream>
#include <string>

#include "mordor/version.h"

namespace Mordor {

// From uri.rl
std::string unescape(const std::string& str, bool spaceAsPlus = false);

namespace HTTP {

static
Version
parseVersion(const char *str)
{
    Version ver;
    ver.major = atoi(str + 5);
    ver.minor = atoi(strchr(str + 5, '.') + 1);
    return ver;
}

static boost::posix_time::time_input_facet rfc1123Facet_in("%a, %d %b %Y %H:%M:%S GMT",
        1 /* starting refcount, so this never gets deleted */);
static boost::posix_time::time_input_facet rfc850Facet_in("%A, %d-%b-%y %H:%M:%S GMT",
        1 /* starting refcount, so this never gets deleted */);
static boost::posix_time::time_input_facet ansiFacet_in("%a %b %e %H:%M:%S %Y",
        1 /* starting refcount, so this never gets deleted */);

boost::posix_time::ptime
parseHttpDate(const char *str, size_t size)
{
    boost::posix_time::ptime result;
    std::string val(str, size);

    #define ATTEMPT_WITH_FACET(facet)                              \
    {                                                              \
        std::istringstream is(val);                                \
        is.imbue(std::locale(is.getloc(), facet));                 \
        is >> result;                                              \
        if (!result.is_not_a_date_time())                          \
            return result;                                         \
    }

    ATTEMPT_WITH_FACET(&rfc1123Facet_in);
    ATTEMPT_WITH_FACET(&rfc850Facet_in);
    ATTEMPT_WITH_FACET(&ansiFacet_in);
    return result;
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

std::string
unquote(const char *str, size_t size)
{
    if (size == 0 || (str[0] != '"' && str[0] != '('))
        return std::string(str, size);
    MORDOR_ASSERT((str[size - 1] == '"' && str[0] == '"') ||
           (str[size - 1] == ')' && str[0] == '('));
    std::string result(str + 1, size - 2);
    char *p = const_cast<char *>(result.c_str());
    char *pe = p + result.size();
    char *pw = p;

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
        if (pw != p)
            *pw = *p;
        ++p; ++pw;
    }
    result.resize(pw - result.c_str());
    return result;
}

std::string
unquote(const std::string &str)
{
    if (str.empty() || (str[0] != '"' && str[0] != '('))
        return str;
    return unquote(str.c_str(), str.size());
}

%%{
    machine http_parser;

    # See RFC 2616: http://www.w3.org/Protocols/rfc2616/rfc2616.html

    action mark { mark = fpc; }
    action mark2 { mark2 = fpc; }
    action clearmark2 { mark2 = NULL; }
    action done { fbreak; }
    action bad_header { if (m_strict) m_isBadValue = true; }
    prepush { prepush(); }
    postpop { postpop(); }

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
    # token68 is from http://tools.ietf.org/html/draft-ietf-httpbis-p7-auth-26
    # token68 represents a set of 68 chars, we add ":" to support AWS and "@", "'" to support email address
    token68 = (ALPHA | DIGIT | "-" | "." | "_" | "~" | "+" | "/" | ":" | "@" | "'")+ "="*;
    quoted_pair = "\\" CHAR;
    ctext = TEXT -- ("(" | ")");
    comment = "(" @{fcall parse_comment;};
    parse_comment := (ctext | quoted_pair | '(' @{fcall parse_comment;} )* ")" @{fret;};
    qdtext = TEXT -- ("\"" | "\\");
    quoted_string = "\"" ( qdtext | quoted_pair )* "\"";

    base64char = ALPHA | DIGIT | '+' | '/';
    base64 = (base64char{4})+ ( (base64char{3} '=') | (base64char{2} '==') )?;

    action parse_HTTP_Version {
        MORDOR_ASSERT(m_ver);
        *m_ver = parseVersion(mark);
        mark = NULL;
    }

    HTTP_Version = ("HTTP/" DIGIT+ "." DIGIT+) >mark %parse_HTTP_Version;

    action save_date {
        MORDOR_ASSERT(m_date);
        *m_date = parseHttpDate(mark, fpc - mark);
        mark = NULL;
    }

    wkday = "Mon" | "Tue" | "Wed" | "Thu" | "Fri" | "Sat" | "Sun";
    weekday = "Monday" | "Tuesday" | "Wednesday" | "Thursday" | "Friday" | "Saturday" | "Sunday";
    month = "Jan" | "Feb" | "Mar" | "Apr" | "May" | "Jun" | "Jul" | "Aug" | "Sep" | "Oct" | "Nov" | "Dec";
    date1 = DIGIT{2} SP month SP DIGIT{4};
    date2 = DIGIT{2} "-" month "-" DIGIT{2};
    date3 = month SP ( DIGIT{2} | (SP DIGIT));
    time = DIGIT{2} ":" DIGIT{2} ":" DIGIT{2};
    rfc1123_date = wkday "," SP date1 SP time SP "GMT";
    rfc850_date = weekday "," SP date2 SP time SP "GMT";
    asctime_date = wkday SP date3 SP time SP DIGIT{4};
    HTTP_date = (rfc1123_date | rfc850_date | asctime_date) >mark %save_date;

    delta_seconds = DIGIT+;

    action save_product_name {
        m_product.product = std::string(mark, fpc - mark);
        mark = NULL;
    }
    action save_product_version {
        m_product.version = std::string(mark, fpc - mark);
        mark = NULL;
    }
    action save_product {
        MORDOR_ASSERT(m_productAndCommentList);
        m_productAndCommentList->push_back(m_product);
        m_product = Product();
    }
    action save_comment {
        MORDOR_ASSERT(m_productAndCommentList);
        m_productAndCommentList->push_back(unquote(mark, fpc - mark));
        mark = NULL;
    }

    product_version = token;
    product = token >mark %save_product_name ("/" product_version >mark %save_product_version)?;
    product_or_comment = (product %save_product) | (comment >mark %save_comment);
    product_and_comment_list = LWS* product_or_comment ( LWS+ product_or_comment)* LWS*;

    qvalue = ('0' ('.' DIGIT{0,3})?) | ('1' ('.' '0'{0,3})?);

    subtag = ALPHA{1,8};
    primary_tag = ALPHA{1,8};
    language_tag = primary_tag ("-" subtag);

    action start_etag {
        MORDOR_ASSERT(m_eTag);
        m_eTag->weak = false;
    }
    action save_weak {
        MORDOR_ASSERT(m_eTag);
        m_eTag->unspecified = false;
        m_eTag->weak = true;
    }
    action save_etag {
        MORDOR_ASSERT(m_eTag);
        m_eTag->unspecified = false;
        m_eTag->value = unquote(mark, fpc - mark);
    }
    action set_etag_list {
        m_eTag = &m_tempETag;
    }
    action save_unspecified {
        MORDOR_ASSERT(m_eTagSet);
        m_eTagSet->insert(ETag());
    }
    action save_etag_element {
        MORDOR_ASSERT(m_eTagSet);
        MORDOR_ASSERT(m_eTag == & m_tempETag);
        m_eTagSet->insert(m_tempETag);
    }

    weak = "W" "/" >save_weak;
    opaque_tag = quoted_string >mark %save_etag;
    entity_tag = ((weak)? opaque_tag) >start_etag;

    etag_list = LWS* ("*" % save_unspecified | (LWS* entity_tag %save_etag_element ( LWS* ',' LWS* entity_tag %save_etag_element)* LWS*) LWS* ) > set_etag_list;

    bytes_unit = "bytes";
    other_range_unit = token;
    range_unit = bytes_unit | other_range_unit;

    action save_field_name {
        m_genericHeaderName = std::string(mark2, fpc - mark2);
        mark2 = NULL;
    }
    action save_field_value {
        if (!mark2) break;

        std::string fieldValue = unfold((char*)mark2, (char*)fpc);

        if (m_isBadValue) {
            MORDOR_THROW_EXCEPTION(BadFieldValueException(m_genericHeaderName, fieldValue));
            m_isBadValue = false;
            fbreak;
        }

        StringMap::iterator it = m_entity->extension.find(m_genericHeaderName);
        if (it == m_entity->extension.end()) {
            m_entity->extension[m_genericHeaderName] = fieldValue;
        } else {
            it->second.append(", ");
            it->second.append(fieldValue);
        }
        mark2 = NULL;
    }

    field_chars = OCTET -- (CTL | CR LF SP HT);
    field_name = token >mark2 %save_field_name;
    field_value = TEXT* >mark2 %save_field_value;
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
        MORDOR_ASSERT(m_list || m_set);
        if (m_list)
            m_list->push_back(std::string(mark, fpc - mark));
        else
            m_set->insert(std::string(mark, fpc - mark));
        mark = NULL;
    }
    element = token >mark %save_element;
    list = (LWS* element ( LWS* ',' LWS* element)* LWS*);

    action save_parameterized_list_element {
        ValueWithParameters vp;
        vp.value = std::string(mark, fpc - mark);
        m_parameterizedList->push_back(vp);
        m_parameters = &m_parameterizedList->back().parameters;
        mark = NULL;
    }

    action save_parameter_attribute {
        m_temp1 = std::string(mark, fpc - mark);
        // Don't NULL out here; could be base64 later
    }

    action save_parameter_attribute_unquote {
        m_temp1 = unquote(mark, fpc - mark);
        // Don't NULL out here; could be base64 later
    }

    action save_parameter_value {
        (*m_parameters)[m_temp1] = unquote(mark, fpc - mark);
        mark = NULL;
    }

    attribute = (token - 'q'i) >mark %save_parameter_attribute; # q separates params from accept-params
    value = (token | quoted_string) >mark %save_parameter_value;
    parameter = attribute '=' value;
    parameterizedListElement = token >mark %save_parameterized_list_element (';' parameter)*;
    parameterizedList = LWS* parameterizedListElement ( LWS* ',' LWS* parameterizedListElement)* LWS*;

    action save_auth_scheme {
        if (m_challengeList && ((!m_challengeList->empty() && m_auth == &m_challengeList->back())
            || m_challengeList->empty())) {
            AuthParams ap;
            m_challengeList->push_back(ap);
            m_auth = &m_challengeList->back();
        }
        m_auth->scheme = std::string(mark, fpc - mark);
        m_parameters = &m_auth->parameters;
        mark = NULL;
    }

    action save_token_param {
        m_auth->param = std::string(mark, fpc - mark);
        mark = NULL;
    }

    auth_param = attribute '=' value;
    auth_scheme = token;
    auth_params = auth_param (LWS* ',' LWS* auth_param)*;
    auth_token68 = token68 >mark %save_token_param;
    credentials = auth_scheme >mark %save_auth_scheme (SP+ (auth_token68 | auth_params))?;
    challenge = credentials;
    challengeList = LWS* challenge ( LWS* ',' LWS* challenge)* LWS*;

    action set_connection {
        m_set = &m_general->connection;
        m_list = NULL;
    }

    action set_date {
        m_date = &m_general->date;
    }

    action set_proxy_connection {
        m_set = &m_general->proxyConnection;
        m_list = NULL;
    }

    action set_trailer {
        m_set = &m_general->trailer;
        m_list = NULL;
    }

    action set_transfer_encoding {
        m_parameterizedList = &m_general->transferEncoding;
    }

    action save_upgrade_product {
        m_general->upgrade.push_back(m_product);
        m_product = Product();
    }

    Connection = 'Connection:'i @set_connection list;
    Date = 'Date:'i @set_date LWS* HTTP_date $lerr(bad_header) LWS*;
    # NON-STANDARD!!!
    Proxy_Connection = 'Proxy-Connection:'i @set_proxy_connection list;
    Trailer = 'Trailer:'i @set_trailer list;
    Transfer_Encoding = 'Transfer-Encoding:'i @set_transfer_encoding parameterizedList;
    Upgrade = 'Upgrade:'i LWS* product %save_upgrade_product ( LWS* ',' LWS* product %save_upgrade_product)* LWS*;

    general_header = Connection | Date | Proxy_Connection | Trailer | Transfer_Encoding | Upgrade;

    action set_allow {
        m_list = &m_entity->allow;
        m_set = NULL;
    }

    action set_content_encoding {
        m_list = &m_entity->contentEncoding;
        m_set = NULL;
    }

    action set_content_length {
        m_ulong = &m_entity->contentLength;
    }

    action set_content_md5 {
        m_string = &m_entity->contentMD5;
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

    action set_expires
    {
        m_date = &m_entity->expires;
    }
    action set_last_modified
    {
        m_date = &m_entity->lastModified;
    }

    Allow = 'Allow:'i @set_allow list;
    Content_Encoding = 'Content-Encoding:'i @set_content_encoding list;
    Content_Length = 'Content-Length:'i @set_content_length LWS* DIGIT+ >mark $lerr(bad_header) %save_ulong $lerr(bad_header) LWS*;
    Content_MD5 = 'Content-MD5:'i @set_content_md5 LWS* base64 >mark $lerr(bad_header) %save_string LWS*;

    byte_range_resp_spec = (DIGIT+ >mark %save_cr_first_byte_pos '-' DIGIT+ >mark %save_cr_last_byte_pos) | '*' %save_blank_cr;
    content_range_spec = bytes_unit SP byte_range_resp_spec '/' ( DIGIT+ >mark %save_instance_length | '*');
    Content_Range = 'Content-Range:'i LWS* content_range_spec $lerr(bad_header) LWS*;

    type = token >mark %save_type;
    subtype = token >mark %save_subtype;
    media_type = type '/' subtype (';' LWS* parameter)*;
    Content_Type = 'Content-Type:'i @set_content_type LWS* media_type LWS*;

    Expires = 'Expires:'i @set_expires LWS* HTTP_date $lerr(bad_header) LWS*;
    Last_Modified = 'Last-Modified:'i @set_last_modified LWS* HTTP_date $lerr(bad_header) LWS*;

    entity_header = Allow | Content_Encoding | Content_Length | Content_MD5 | Content_Range | Content_Type | Expires | Last_Modified; # | message_header;

}%%

%%{
    machine http_request_parser;
    include http_parser;
    include uri_parser "../uri.rl";

    action save_Method {
        m_request->requestLine.method = std::string(mark, fpc - mark);
        mark = NULL;
    }

    action set_request_uri {
        m_uri = &m_request->requestLine.uri;
        m_segments = &m_uri->path.segments;
        m_authority = &m_uri->authority;
    }

    action save_accept_list_element {
        if (m_acceptList) {
            AcceptValue av;
            if (fpc - mark != 1 || *mark != '*')
                av.value = std::string(mark, fpc - mark);
            m_acceptList->push_back(av);
            mark = NULL;
        } else {
            MORDOR_ASSERT(m_acceptListWithParams);
            AcceptValueWithParameters avp;
            avp.value = std::string(mark, fpc - mark);
            m_acceptListWithParams->push_back(avp);
            m_parameters = &m_acceptListWithParams->back().parameters;
            mark = NULL;
        }
    }

    action save_qvalue {
        unsigned int *qvalue = NULL;
        if (m_acceptList) {
            qvalue = &m_acceptList->back().qvalue;
        } else {
            MORDOR_ASSERT(m_acceptListWithParams);
            qvalue = &m_acceptListWithParams->back().qvalue;
        }
        *qvalue = 0;
        size_t i = 0;
        unsigned int curPlace = 1000;
        for (; i < 5 && mark < fpc; ++i, ++mark) {
            if (i == 1)
                continue;
            unsigned int cur = *mark - '0';
            *qvalue += cur * curPlace;
            curPlace /= 10;
        }
        mark = NULL;
    }

    action set_accept_charset {
        m_acceptList = &m_request->request.acceptCharset;
        m_acceptListWithParams = NULL;
    }

    action set_accept_encoding {
        m_acceptList = &m_request->request.acceptEncoding;
        m_acceptListWithParams = NULL;
    }

    action set_authorization {
        m_challengeList = NULL;
        m_auth = &m_request->request.authorization;
    }

    action save_expectation {
        KeyValueWithParameters kvp;
        kvp.key = std::string(mark, fpc - mark);
        m_request->request.expect.push_back(kvp);
        mark = NULL;
    }
    action save_expectation_value {
        m_request->request.expect.back().value = unquote(mark, fpc - mark);
        mark = NULL;
    }
    action save_expectation_param {
        m_temp1 = std::string(mark, fpc - mark);
        m_request->request.expect.back().parameters[m_temp1] = "";
        mark = NULL;
    }
    action save_expectation_param_value {
        m_request->request.expect.back().parameters[m_temp1] = unquote(mark, fpc - mark);
        mark = NULL;
    }

    action set_host {
        m_string = &m_request->request.host;
    }

    action set_if_match {
        m_eTagSet = &m_request->request.ifMatch;
    }

    action set_if_modified_since {
        m_date = &m_request->request.ifModifiedSince;
    }

    action set_if_none_match {
        m_eTagSet = &m_request->request.ifNoneMatch;
    }

    action clear_if_range_entity_tag {
        m_eTag = NULL;
    }

    action set_if_range_entity_tag {
        if (!m_eTag) {
            m_request->request.ifRange = ETag();
            m_eTag = boost::get<ETag>(&m_request->request.ifRange);
        }
    }

    action set_if_range_http_date {
        m_request->request.ifRange = boost::posix_time::ptime();
        m_date = boost::get<boost::posix_time::ptime>(&m_request->request.ifRange);
    }

    action set_if_unmodified_since {
        m_date = &m_request->request.ifModifiedSince;
    }

    action set_proxy_authorization {
        m_challengeList = NULL;
        m_auth = &m_request->request.proxyAuthorization;
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

    action set_referer {
        m_uri = &m_request->request.referer;
        m_segments = &m_uri->path.segments;
        m_authority = &m_uri->authority;
    }

    action save_accept_attribute {
        m_temp1 = std::string(mark, fpc - mark);
        m_acceptListWithParams->back().acceptParams[m_temp1] = "";
        mark = NULL;
    }

    action save_accept_value {
        m_acceptListWithParams->back().acceptParams[m_temp1] = unquote(mark, fpc - mark);
        mark = NULL;
    }

    action set_te {
        m_acceptList = NULL;
        m_acceptListWithParams = &m_request->request.te;
    }

    action set_user_agent {
        m_productAndCommentList = &m_request->request.userAgent;
    }

    acceptListElement = ( token | '*' ) >mark %save_accept_list_element (';q='i qvalue >mark %save_qvalue)?;
    acceptList = LWS* acceptListElement ( LWS* ',' LWS* acceptListElement)* LWS*;
    Accept_Charset = 'Accept-Charset:'i @set_accept_charset acceptList;

    Accept_Encoding = 'Accept-Encoding:'i @set_accept_encoding acceptList;

    Authorization = 'Authorization:'i @set_authorization LWS* credentials;

    expect_params = ';' token >mark %save_expectation_param ( '=' (token | quoted_string) >mark %save_expectation_param_value )?;
    expectation = token >mark %save_expectation ( '=' (token | quoted_string) >mark %save_expectation_value expect_params* )?;
    Expect = 'Expect:'i LWS* expectation ( LWS* ',' LWS* expectation )* LWS*;

    Host = 'Host:'i @set_host LWS* (host (':' port)?) >mark %save_string LWS*;

    If_Match = 'If-Match:'i @set_if_match etag_list;
    If_Modified_Since = 'If-Modified-Since:'i @set_if_modified_since LWS* HTTP_date LWS*;
    If_None_Match = 'If-None-Match:'i @set_if_none_match etag_list;

    weak_for_if_range = "W" "/" >set_if_range_entity_tag >save_weak;
    entity_tag_for_if_range = ((weak_for_if_range)? opaque_tag >set_if_range_entity_tag) >clear_if_range_entity_tag;

    If_Range = 'If-Range:'i LWS* (entity_tag_for_if_range | HTTP_date >set_if_range_http_date) LWS*;
    If_Unmodified_Since = 'If-Unmodified-Since:'i @set_if_unmodified_since LWS* HTTP_date LWS*;

    Proxy_Authorization = 'Proxy-Authorization:'i @set_proxy_authorization LWS* credentials;

    byte_range_spec = DIGIT+ >mark %save_first_byte_pos '-' (DIGIT+ >mark %save_last_byte_pos)?;
    suffix_byte_range_spec = '-' DIGIT+ > mark %save_suffix_byte_pos;
    byte_range_set = LWS* (byte_range_spec | suffix_byte_range_spec) ( LWS* ',' LWS* (byte_range_spec | suffix_byte_range_spec))* LWS*;
    ranges_specifier = bytes_unit '=' byte_range_set;
    Range = 'Range:'i LWS* ranges_specifier $lerr(bad_header);

    Referer = 'Referer:'i @set_referer LWS* (absolute_URI | relative_URI);

    accept_extension = ';' token >mark %save_accept_attribute ('=' (token | quoted_string) >mark %save_accept_value)?;
    accept_params = ';q='i qvalue >mark %save_qvalue (accept_extension)*;
    acceptListWithParamsElement = token >mark %save_accept_list_element (';' parameter)* (accept_params)?;
    acceptListWithParams = LWS* acceptListWithParamsElement ( LWS* ',' LWS* acceptListWithParamsElement)* LWS*;
    TE = 'TE:'i @set_te acceptListWithParams;

    User_Agent = 'User-Agent:'i @set_user_agent product_and_comment_list;

    request_header = Accept_Charset | Accept_Encoding | Authorization | Expect | Host | If_Match | If_Modified_Since | If_None_Match | If_Range | If_Unmodified_Since | Proxy_Authorization | Range | Referer | TE | User_Agent;

    Method = token >mark %save_Method;

    # we explicitly add query to path_absolute, because the URI spec changed from RFC 2396 to RFC 3986
    # with the query not being part of hier_part
    Request_URI = ( "*" | absolute_URI | (path_absolute ( "?" query )?));
    # HTTP specifies that a Request_URI may be an authority, but only for the
    # CONNECT method; enforce that, and by so doing remove the ambiguity that
    # an authority might be a scheme
    Connect_Line = 'CONNECT' %save_Method SP authority >set_request_uri SP HTTP_Version CRLF;
    Request_Line = (Method - 'CONNECT') SP Request_URI >set_request_uri SP HTTP_Version CRLF;
    Request = (Request_Line | Connect_Line) (((general_header | request_header | entity_header) %clearmark2 | message_header) CRLF)* CRLF @done;

    main := Request;
    write data;
}%%

void
Parser::init()
{
    m_string = NULL;
    m_set = NULL;
    m_list = NULL;
    m_parameterizedList = NULL;
    m_acceptList = NULL;
    m_acceptListWithParams = NULL;
    m_parameters = NULL;
    m_auth = NULL;
    m_challengeList = NULL;
    m_ulong = NULL;
    m_eTag = NULL;
    m_eTagSet = NULL;
    m_productAndCommentList = NULL;
    mark2 = NULL;
    m_isBadValue = false;
    RagelParser::init();
}

const char *
Parser::earliestPointer() const
{
    const char *parent = RagelParser::earliestPointer();
    if (mark2 && parent)
        return (std::min)(mark2, parent);
    if (mark2)
        return mark2;
    return parent;
}

void
Parser::adjustPointers(ptrdiff_t offset)
{
    if (mark2)
        mark2 += offset;
    RagelParser::adjustPointers(offset);
}

RequestParser::RequestParser(Request& request, bool strict)
: Parser(strict),
  m_request(&request),
  m_ver(&request.requestLine.ver),
  m_segments(&request.requestLine.uri.path.segments),
  m_authority(&request.requestLine.uri.authority),
  m_general(&request.general),
  m_entity(&request.entity)
{}

void
RequestParser::init()
{
    Parser::init();
    %% write init;
}

bool
RequestParser::final() const
{
    return cs >= http_request_parser_first_final;
}

bool
RequestParser::error() const
{
    return cs == http_request_parser_error;
}

void
RequestParser::exec()
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
        m_response->status.status = (Status)atoi(mark);
        mark = NULL;
    }

    action parse_Reason_Phrase {
        m_response->status.reason = std::string(mark, fpc - mark);
        mark = NULL;
    }

    action set_accept_ranges
    {
        m_set = &m_response->response.acceptRanges;
        m_list = NULL;
    }
    action set_etag
    {
        m_eTag = &m_response->response.eTag;
    }
    action set_proxy_authenticate {
        m_challengeList = &m_response->response.proxyAuthenticate;
    }
    action set_retry_after_http_date {
        m_response->response.retryAfter = boost::posix_time::ptime();
        m_date = boost::get<boost::posix_time::ptime>(&m_response->response.retryAfter);
    }
    action set_retry_after_delta_seconds {
        m_response->response.retryAfter = ~0ull;
        m_ulong = boost::get<unsigned long long>(&m_response->response.retryAfter);
    }
    action set_server {
        m_productAndCommentList = &m_response->response.server;
    }
    action set_www_authenticate {
        m_challengeList = &m_response->response.wwwAuthenticate;
    }

    Accept_Ranges = 'Accept-Ranges:'i @set_accept_ranges list;
    ETag = 'ETag:'i @set_etag LWS* entity_tag;
    # This *should* be absolute_URI, but we're generous
    Location = 'Location:'i LWS* URI_reference LWS*;
    Proxy_Authenticate = 'Proxy-Authenticate:'i @set_proxy_authenticate challengeList;
    Retry_After = 'Retry-After:'i LWS* (HTTP_date %set_retry_after_http_date | delta_seconds >mark %set_retry_after_delta_seconds %save_ulong) LWS*;
    Server = 'Server:'i @set_server product_and_comment_list;
    WWW_Authenticate = 'WWW-Authenticate:'i @set_www_authenticate challengeList;

    response_header = Accept_Ranges | ETag | Location | Proxy_Authenticate | Retry_After | Server | WWW_Authenticate;

    Status_Code = DIGIT{3} > mark %parse_Status_Code;
    Reason_Phrase = (TEXT -- (CR | LF))* >mark %parse_Reason_Phrase;
    Status_Line = HTTP_Version SP Status_Code SP Reason_Phrase CRLF;
    Response = Status_Line (((general_header | response_header | entity_header) %clearmark2 | message_header) CRLF)* CRLF @done;

    main := Response;

    write data;
}%%

ResponseParser::ResponseParser(Response& response, bool strict)
: Parser(strict),
  m_response(&response),
  m_ver(&response.status.ver),
  m_uri(&response.response.location),
  m_segments(&response.response.location.path.segments),
  m_authority(&response.response.location.authority),
  m_general(&response.general),
  m_entity(&response.entity)
{}

void
ResponseParser::init()
{
    Parser::init();
    %% write init;
}

bool
ResponseParser::final() const
{
    return cs >= http_response_parser_first_final;
}

bool
ResponseParser::error() const
{
    return cs == http_response_parser_error;
}

void
ResponseParser::exec()
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

    trailer = ((entity_header %clearmark2 | message_header) CRLF)*;

    main := trailer CRLF @done;

    write data;
}%%

TrailerParser::TrailerParser(EntityHeaders& entity, bool strict)
: Parser(strict), m_entity(&entity)
{}

void
TrailerParser::init()
{
    Parser::init();
    %% write init;
}

bool
TrailerParser::final() const
{
    return cs >= http_trailer_parser_first_final;
}

bool
TrailerParser::error() const
{
    return cs == http_trailer_parser_error;
}

void
TrailerParser::exec()
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
    machine http_list_parser;
    include http_parser;

    main := list;

    write data;
}%%

ListParser::ListParser(StringSet& stringSet)
: m_set(&stringSet),
  m_list(NULL)
{}

void
ListParser::init()
{
    RagelParser::init();
    %% write init;
}

bool
ListParser::final() const
{
    return cs >= http_list_parser_first_final;
}

bool
ListParser::error() const
{
    return cs == http_list_parser_error;
}

void
ListParser::exec()
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

}}
