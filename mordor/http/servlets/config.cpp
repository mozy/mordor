// Copyright (c) 2010 - Mozy, Inc.

#include "mordor/http/servlets/config.h"

#include "mordor/config.h"
#include "mordor/http/server.h"
#include "mordor/json.h"
#include "mordor/streams/buffered.h"
#include "mordor/streams/limited.h"
#include "mordor/streams/memory.h"
#include "mordor/streams/transfer.h"

namespace Mordor {
namespace HTTP {
namespace Servlets {

static void eachConfigVarHTMLWrite(ConfigVarBase::ptr var, Stream::ptr stream)
{
    std::string name = var->name();
    stream->write("<tr><td align=\"right\">", 22);
    stream->write(name.c_str(), name.size());
    stream->write("=</td><td><form name=\"", 22);
    stream->write(name.c_str(), name.size());
    stream->write("\" method=\"post\"><input type=\"text\" name=\"", 41);
    stream->write(name.c_str(), name.size());
    stream->write("\" value=\"", 9);
    std::string value = var->toString();
    stream->write(value.c_str(), value.size());
    stream->write("\" /><input type=\"submit\" value=\"Change\" /></form></td></tr>\n", 60);
}

static void eachConfigVarHTML(ConfigVarBase::ptr var, Stream::ptr stream)
{
    std::string name = var->name();
    stream->write("<tr><td align=\"right\">", 22);
    stream->write(name.c_str(), name.size());
    stream->write("=</td><td>", 10);
    std::string value = var->toString();
    stream->write(value.c_str(), value.size());
    stream->write("</td></tr>\n", 11);
}

static void eachConfigVarJSON(ConfigVarBase::ptr var, JSON::Object &object)
{
    object.insert(std::make_pair(var->name(), var->toString()));
}

namespace {
enum Format {
    HTML,
    JSON
};
}

void Config::request(ServerRequest::ptr request, Access access)
{
    const std::string &method = request->request().requestLine.method;
    if (method == POST) {
        if (access != READWRITE) {
            respondError(request, FORBIDDEN);
            return;
        }
        if (request->request().entity.contentType.type != "application" ||
            request->request().entity.contentType.subtype != "x-www-form-urlencoded") {
            respondError(request, UNSUPPORTED_MEDIA_TYPE);
            return;
        }
        Stream::ptr requestStream = request->requestStream();
        requestStream.reset(new LimitedStream(requestStream, 65536));
        MemoryStream requestBody;
        transferStream(requestStream, requestBody);
        std::string queryString;
        queryString.resize(requestBody.buffer().readAvailable());
        requestBody.buffer().copyOut(&queryString[0], requestBody.buffer().readAvailable());

        bool failed = false;
        URI::QueryString qs(queryString);
        for (URI::QueryString::const_iterator it = qs.begin();
            it != qs.end();
            ++it) {
            ConfigVarBase::ptr var = Mordor::Config::lookup(it->first);
            if (var && !var->fromString(it->second))
                failed = true;
        }
        if (failed) {
            respondError(request, HTTP::FORBIDDEN,
                "One or more new values were not accepted");
            return;
        }
        // Fall through
    }
    if (method == GET || method == HEAD || method == POST) {
        Format format = HTML;
        URI::QueryString qs;
        if (request->request().requestLine.uri.queryDefined())
            qs = request->request().requestLine.uri.queryString();
        URI::QueryString::const_iterator it = qs.find("alt");
        if (it != qs.end() && it->second == "json")
            format = JSON;
        // TODO: use Accept to indicate JSON
        switch (format) {
            case HTML:
            {
                request->response().status.status = OK;
                request->response().entity.contentType = MediaType("text", "html");
                if (method == HEAD) {
                    if (request->request().requestLine.ver == Version(1, 1) &&
                        isAcceptable(request->request().request.te, "chunked", true)) {
                        request->response().general.transferEncoding.push_back("chunked");
                    }
                    return;
                }
                Stream::ptr response = request->responseStream();
                response.reset(new BufferedStream(response));
                response->write("<html><body><table>\n", 20);
                Mordor::Config::visit(boost::bind(access == READWRITE ?
                    &eachConfigVarHTMLWrite : &eachConfigVarHTML, _1,
                    response));
                response->write("</table></body></html>", 22);
                response->close();
                break;
            }
            case JSON:
            {
                JSON::Object root;
                Mordor::Config::visit(boost::bind(&eachConfigVarJSON, _1, boost::ref(root)));
                std::ostringstream os;
                os << root;
                std::string str = os.str();
                request->response().status.status = OK;
                request->response().entity.contentType = MediaType("application", "json");
                request->response().entity.contentLength = str.size();
                if (method != HEAD) {
                    request->responseStream()->write(str.c_str(), str.size());
                    request->responseStream()->close();
                }
                break;
            }
            default:
                MORDOR_NOTREACHED();
        }
    } else {
        respondError(request, METHOD_NOT_ALLOWED);
    }
}

}}}
