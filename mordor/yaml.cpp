// Copyright (c) 2010 - Mozy, Inc.

#include "yaml.h"

#include <boost/exception_ptr.hpp>
#include <yaml.h>

#include "assert.h"
#include "streams/stream.h"

#ifdef _MSC_VER
#pragma comment(lib, "yaml.lib")
#endif

namespace Mordor {
namespace YAML {

static void convertNode(JSON::Value &value, yaml_node_t *node,
    yaml_document_t &document)
{
    switch (node->type) {
        case YAML_SCALAR_NODE:
            value = std::string((char *)node->data.scalar.value,
                node->data.scalar.length);
            break;
        case YAML_SEQUENCE_NODE:
        {
            value = JSON::Array();
            JSON::Array &array = value.get<JSON::Array>();
            yaml_node_item_t *item = node->data.sequence.items.start;
            array.resize(node->data.sequence.items.top - item);
            JSON::Array::iterator it = array.begin();
            while (item < node->data.sequence.items.top) {
                convertNode(*it, yaml_document_get_node(&document, *item),
                    document);
                ++it; ++item;
            }
            break;
        }
        case YAML_MAPPING_NODE:
        {
            value = JSON::Object();
            JSON::Object &object = value.get<JSON::Object>();
            yaml_node_pair_t *pair = node->data.mapping.pairs.start;
            while (pair < node->data.mapping.pairs.top) {
                yaml_node_t *keyNode = yaml_document_get_node(&document,
                    pair->key);
                yaml_node_t *valueNode = yaml_document_get_node(&document,
                    pair->value);
                if (keyNode->type != YAML_SCALAR_NODE)
                    MORDOR_THROW_EXCEPTION(std::runtime_error("Can't use a non-string as a key"));
                std::string key((char *)keyNode->data.scalar.value,
                    keyNode->data.scalar.length);
                convertNode(object.insert(std::make_pair(key,
                    JSON::Value()))->second, valueNode, document);
                ++pair;
            }
            break;
        }
        default:
            MORDOR_NOTREACHED();
    }
}

static JSON::Value parse(yaml_parser_t &parser)
{
    JSON::Value result;
    yaml_document_t document;
    if (!yaml_parser_load(&parser, &document)) {
        yaml_error_type_t error = parser.error;
        MORDOR_ASSERT(error != YAML_NO_ERROR);
        Exception exception(parser.problem, parser.context);
        yaml_parser_delete(&parser);
        switch (error) {
            case YAML_MEMORY_ERROR:
                MORDOR_THROW_EXCEPTION(std::bad_alloc());
            case YAML_READER_ERROR:
                MORDOR_THROW_EXCEPTION(InvalidUnicodeException());
            default:
                MORDOR_THROW_EXCEPTION(exception);
        }
    }

    try {
        convertNode(result, yaml_document_get_root_node(&document), document);
        yaml_document_delete(&document);
        yaml_parser_delete(&parser);
        return result;
    } catch (...) {
        yaml_document_delete(&document);
        yaml_parser_delete(&parser);
        throw;
    }
}

JSON::Value parse(const std::string &string)
{
    yaml_parser_t parser;
    if (!yaml_parser_initialize(&parser))
        MORDOR_THROW_EXCEPTION(std::bad_alloc());
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)string.c_str(), string.size());
    return parse(parser);
}

namespace {
struct GetDataContext
{
    Stream *stream;
    boost::exception_ptr exception;
};
}

static int getData(void *data, unsigned char *buffer,
    size_t size, size_t *size_read)
{
    GetDataContext *context = (GetDataContext *)data;
    try {
        *size_read = context->stream->read(buffer, size);
        return 1;
    } catch (...) {
        context->exception = boost::current_exception();
        return 0;
    }
}

JSON::Value parse(Stream &stream)
{
    yaml_parser_t parser;
    if (!yaml_parser_initialize(&parser))
        MORDOR_THROW_EXCEPTION(std::bad_alloc());
    GetDataContext context;
    context.stream = &stream;
    yaml_parser_set_input(&parser, &getData, &context);
    try {
        return parse(parser);
    } catch (...) {
        if (context.exception)
            ::Mordor::rethrow_exception(context.exception);
        throw;
    }
}

JSON::Value parse(Stream::ptr stream)
{
    return parse(*stream);
}

}}
