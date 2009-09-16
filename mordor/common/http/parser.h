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
        AcceptList *m_acceptList;
        StringMap *m_parameters;
        AuthParams *m_auth;
        ChallengeList *m_challengeList;
        unsigned long long *m_ulong;
        ETag *m_eTag;
        Product m_product;
        ETagSet *m_eTagSet;
        ProductAndCommentList *m_productAndCommentList;
        boost::posix_time::ptime *m_date;

        // Temp storage
        std::string m_temp1, m_temp2;
        ETag m_tempETag;
    };

    class RequestParser : public HTTPParser
    {
    public:
        RequestParser(Request& request);

        void init();
        bool final() const;
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
        bool final() const;
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
        bool final() const;
        bool error() const;
    protected:
        void exec();
    private:
        EntityHeaders *m_entity;
    };

    class ListParser : public RagelParser
    {
    public:
        ListParser(StringSet &stringSet);

        void init();
        bool final() const;
        bool error() const;
    protected:
        void exec();
    private:
        StringSet *m_set;
        std::vector<std::string> *m_list;
    };
};

#endif
