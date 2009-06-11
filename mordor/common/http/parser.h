#ifndef __HTTP_PARSER_H__
#define __HTTP_PARSER_H__
// Copyright (c) 2009 - Decho Corp.

#include "common/ragel.h"
#include "http.h"

namespace HTTP
{
    class HttpParser : public RagelParser
    {
    protected:
        // Pointers to current headers
        std::string *m_string;
        StringSet *m_list;
        ParameterizedList *m_parameterizedList;
        StringMap *m_parameters;
        ValueWithParameters *m_auth;
        unsigned long long *m_ulong;

        // Temp storage
        std::string m_temp1, m_temp2;
        bool m_headerHandled;
    };

    class RequestParser : public HttpParser
    {
    public:
        RequestParser(Request& request);

        void init();
        bool complete() const;
        bool error() const;
    protected:
        void exec();
    private:
        Request *m_request;
        Version *m_ver;
        URI *m_uri;
        URI::Path *m_path;
        GeneralHeaders *m_general;
        EntityHeaders *m_entity;
    };

    class ResponseParser : public HttpParser
    {
    public:
        ResponseParser(Response& response);

        void init();
        bool complete() const;
        bool error() const;
    protected:
        void exec();
    private:
        Response *m_response;
        Version *m_ver;
        URI *m_uri;
        URI::Path *m_path;
        GeneralHeaders *m_general;
        EntityHeaders *m_entity;
    };

    class TrailerParser : public HttpParser
    {
    public:
        TrailerParser(EntityHeaders& entity);

        void init();
        bool complete() const;
        bool error() const;
    protected:
        void exec();
    private:
        EntityHeaders *m_entity;
    };
};

#endif
