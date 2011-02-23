#ifndef __MORDOR_PROTOBUF_H__
#define __MORDOR_PROTOBUF_H__
// Copyright (c) 2010 - Mozy, Inc.

namespace google {
namespace protobuf {
class Message;
}}

namespace Mordor {
struct Buffer;

void serializeToBuffer(const google::protobuf::Message &proto,
    Mordor::Buffer &buffer);
void parseFromBuffer(google::protobuf::Message &proto,
    const Mordor::Buffer &buffer);
}

#endif
