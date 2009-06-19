#ifndef __HTTP_PARSER_H__
#define __HTTP_PARSER_H__
// Copyright (c) 2009 - Decho Corp.

#include "http.h"
#include "mordor/common/ragel.h"

namespace HTTP
{
    class HTTPParser : public RagelParser
    {
    public:
        void init();
    protected:
        HTTPParser() { init(); }
        // Pointers to current headers
        std::string *m_string;
        StringSet *m_set;
        std::vector<std::string> *m_list;
        ParameterizedList *m_parameterizedList;
        StringMap *m_parameters;
        ValueWithParameters *m_auth;
        unsigned long long *m_ulong;

        // Temp storage
        std::string m_temp1, m_temp2;
        bool m_headerHandled;
    };

    class RequestParser : public HTTPParser
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

    class ResponseParser : public HTTPParser
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

    class TrailerParser : public HTTPParser
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
