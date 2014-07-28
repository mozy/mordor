#ifndef __MORDOR_PROTOBUF_H__
#define __MORDOR_PROTOBUF_H__
// Copyright (c) 2010 - Mozy, Inc.

#include "mordor/json.h"

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

// reflection utils
void serializeToJsonObject(const google::protobuf::Message &proto,
    Mordor::JSON::Object &object, bool validate=false);

void parseFromJsonObject(google::protobuf::Message *proto,
    const Mordor::JSON::Object &object, bool validate=false);

google::protobuf::Message* forName(const std::string& typeName);

std::string toJson(const google::protobuf::Message &proto, bool validate=false);

google::protobuf::Message * fromJson(const Mordor::Buffer &buffer,
    bool validate=false);

void setField(google::protobuf::Message *proto, const std::string &fieldName,
    const Mordor::JSON::Value &value);

Mordor::JSON::Value getField(google::protobuf::Message *proto,
    const std::string &fieldName);

}

/* sample code

    // test reflection
    Message *msg = forName("restpb.PartialObject");
    setField(msg, "store_dat.len", 100);
    Value val = getField(msg, "store_dat.len");

    std::cout << "set/getField store_dat.len = " << val << std::endl;


    try {
        Buffer json =
            "{\n"
            "    \"restpb.PartialObject\" : {\n"
            "        \"container_id\": 12345,\n"
            "        \"filename\": \"/tmp/ahaha.txt\",\n"
            "        \"store_dat\": {\n"
            "            \"len\": 100,\n"
            "            \"m_factor\": 100,\n"
            "            \"frag\": [\n"
            "                {\n"
            "                    \"frag_id\": 1001,\n"
            "                    \"tdn_id\": 45,\n"
            "                    \"tmp_file\": \"/frag-tmp/ajfajfaf.dat\"\n"
            "                },\n"
            "                {\n"
            "                    \"frag_id\": 1001,\n"
            "                    \"tmp_file\": \"/frag-tmp/ajfafafafjfaf.dat\"\n"
            "                }\n"
            "            ]\n"
            "        }\n"
            "    }\n"
            "}\n";

        Message * msg = fromJson(json);
        std::cout << "from Json\n" << json.toString() << std::endl << "debug string: " << msg->DebugString() << std::endl;

        // set indexed field
        setField(msg, "store_dat.frag[1].tdn_id", 46);

        std::string output = toJson(*msg);
        std::cout << "back to Json\n" << output << std::endl;

        std::cout << "store_dat.frag[0].tmp_file = " << getField(msg, "store_dat.frag[0].tmp_file") << std::endl;
    } catch (std::runtime_error &e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
    }

    // validating prototype
    msg = forName("restpb.StoreDat");
    Object obj;
    try {
        parseFromJsonObject(msg, obj, true);
    } catch (std::exception &e) {
        std::cerr << "validating error : " << e.what() << std::endl;
        obj["m_factor"] = 10;
        obj["len"] = 100;
        try {
            parseFromJsonObject(msg, obj, true);
        } catch (std::exception &e) {
            std::cerr << "validating error : " << e.what() << std::endl;
        }
    }
}
*/

#endif

