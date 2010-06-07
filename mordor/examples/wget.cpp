// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/predef.h"

#include <iostream>

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/program_options.hpp>

#include "mordor/config.h"
#include "mordor/exception.h"
#include "mordor/http/broker.h"
#include "mordor/http/client.h"
#include "mordor/http/multipart.h"
#include "mordor/http/proxy.h"
#include "mordor/iomanager.h"
#include "mordor/sleep.h"
#include "mordor/socket.h"
#include "mordor/streams/socket.h"
#include "mordor/streams/ssl.h"
#include "mordor/streams/std.h"
#include "mordor/streams/transfer.h"

using namespace Mordor;
namespace po = boost::program_options;

static bool getCredentials(HTTP::ClientRequest::ptr priorRequest,
    std::string &scheme, std::string &username, std::string &password,
    const std::string &user, const std::string &pass,
    size_t attempts, bool forProxy)
{
    if (!priorRequest)
        return false;
    if (attempts > 1)
        return false;
    username = user;
    password = pass;
    const HTTP::ChallengeList &challengeList = forProxy ?
        priorRequest->response().response.proxyAuthenticate :
        priorRequest->response().response.wwwAuthenticate;
#ifdef WINDOWS
    if (HTTP::isAcceptable(challengeList, "Negotiate")) {
        scheme = "Negotiate";
        return true;
    }
    if (HTTP::isAcceptable(challengeList, "NTLM")) {
        scheme = "NTLM";
        return true;
    }
#endif
    if (HTTP::isAcceptable(challengeList, "Digest")) {
        scheme = "Digest";
        return true;
    }
    if (HTTP::isAcceptable(challengeList, "Basic")) {
        scheme = "Basic";
        return true;
    }
    return false;
}

int main(int argc, char *argv[])
{
    Config::loadFromEnvironment();
    StdoutStream stdoutStream;
    IOManager ioManager;
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "print this help message")
        ("username", po::value<std::string>(), "username")
        ("password", po::value<std::string>(), "password")
        ("proxyusername", po::value<std::string>(), "proxyusername")
        ("proxypassword", po::value<std::string>(), "proxypassword")
        ("uri", po::value<std::string>(), "uri to download");
    po::positional_options_description positions;
    positions.add("uri", -1);
    po::variables_map vm;
    try {
        po::store(po::command_line_parser(argc, argv).options(desc).positional(positions).run(), vm);
    } catch (...) {
        std::cout << desc << "\n";
        return 1;
    }
    if (vm.count("help") || !vm.count("uri")) {
        std::cout << desc << "\n";
        return 1;
    }

    try {
        URI uri = vm["uri"].as<std::string>();
        MORDOR_ASSERT(uri.authority.hostDefined());
        MORDOR_ASSERT(!uri.schemeDefined() || uri.scheme() == "http" || uri.scheme() == "https");

        HTTP::RequestBrokerOptions options;
        options.ioManager = &ioManager;
        std::string username, password, proxyusername, proxypassword;
        if (vm.count("username")) username = vm["username"].as<std::string>();
        if (vm.count("password")) password = vm["password"].as<std::string>();
        if (vm.count("proxyusername")) proxyusername = vm["proxyusername"].as<std::string>();
        if (vm.count("proxypassword")) proxypassword = vm["proxypassword"].as<std::string>();
        if (vm.count("proxyusername"))
            options.getProxyCredentialsDg = boost::bind(&getCredentials, _2, _3, _5, _6,
                proxyusername, proxypassword, _7, true);
        HTTP::RequestBroker::ptr proxyBroker =
            HTTP::createRequestBroker(options).first;
        if (vm.count("username"))
            options.getCredentialsDg = boost::bind(&getCredentials, _2, _3, _5, _6,
                username, password, _7, false);
        options.proxyRequestBroker = proxyBroker;
#ifdef WINDOWS
        HTTP::ProxyCache proxyCache;
        options.proxyForURIDg = boost::bind(
            &HTTP::ProxyCache::proxyFromUserSettings, &proxyCache, _1);
#elif defined (OSX)
        HTTP::ProxyCache proxyCache(proxyBroker);
        options.proxyForURIDg = boost::bind(
            &HTTP::ProxyCache::proxyFromSystemConfiguration, &proxyCache, _1);
#else
        options.proxyForURIDg = &HTTP::proxyFromConfig;
#endif
        HTTP::RequestBroker::ptr requestBroker =
            HTTP::createRequestBroker(options).first;

        HTTP::Request requestHeaders;
        requestHeaders.requestLine.uri = uri;
        requestHeaders.request.host = uri.authority.host();
        HTTP::ClientRequest::ptr request = requestBroker->request(requestHeaders);
        if (request->hasResponseBody()) {
            if (request->response().entity.contentType.type != "multipart") {
                transferStream(request->responseStream(), stdoutStream);
            } else {
                Multipart::ptr responseMultipart = request->responseMultipart();
                for (BodyPart::ptr bodyPart = responseMultipart->nextPart(); bodyPart;
                    bodyPart = responseMultipart->nextPart()) {
                    transferStream(bodyPart->stream(), stdoutStream);
                }
            }
        }
        return request->response().status.status == HTTP::OK ? 0 : request->response().status.status;
    } catch (...) {
        std::cerr << boost::current_exception_diagnostic_information() << std::endl;
        return 2;
    }
}
