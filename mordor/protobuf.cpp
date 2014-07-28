// Copyright (c) 2010 - Mozy, Inc.

#include "protobuf.h"

#include "mordor/assert.h"
#include "mordor/streams/buffer.h"

#ifdef MSVC
// Disable some warnings, but only while
// processing the google generated code
#pragma warning(push)
#pragma warning(disable : 4244)
#endif


#include <boost/algorithm/string/regex.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>

#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/message.h>

#ifdef MSVC
#pragma warning(pop)
#endif

using namespace google::protobuf;
using namespace Mordor::JSON;

#ifdef MSVC
#ifdef _DEBUG
#pragma comment(lib, "libprotobuf-d.lib")
#else
#pragma comment(lib, "libprotobuf.lib")
#endif
#endif

namespace Mordor {

class BufferZeroCopyInputStream : public io::ZeroCopyInputStream
{
public:
    BufferZeroCopyInputStream(const Buffer &buffer)
        : m_iovs(buffer.readBuffers()),
          m_currentIov(0),
          m_currentIovOffset(0),
          m_complete(0)
    {}

    bool Next(const void **data, int *size)
    {
        if (m_currentIov >= m_iovs.size())
            return false;
        MORDOR_ASSERT(m_currentIovOffset <= m_iovs[m_currentIov].iov_len);
        *data = (char *)m_iovs[m_currentIov].iov_base + m_currentIovOffset;
        *size = (int)(m_iovs[m_currentIov].iov_len - m_currentIovOffset);

        m_complete += *size;
        m_currentIovOffset = 0;
        ++m_currentIov;

        return true;
    }

    void BackUp(int count)
    {
        MORDOR_ASSERT(count >= 0);
        MORDOR_ASSERT(count <= m_complete);
        m_complete -= count;
        while (count) {
            if (m_currentIovOffset == 0) {
                MORDOR_ASSERT(m_currentIov > 0);
                m_currentIovOffset = m_iovs[--m_currentIov].iov_len;
            }
            size_t todo = (std::min)(m_currentIovOffset, (size_t)count);
            m_currentIovOffset -= todo;
            count -= (int)todo;
        }
    }

    bool Skip(int count) {
        MORDOR_ASSERT(count >= 0);
        while (count) {
            if (m_currentIov >= m_iovs.size())
                return false;
            size_t todo = (std::min)((size_t)m_iovs[m_currentIov].iov_len -
                m_currentIovOffset, (size_t)count);
            m_currentIovOffset += todo;
            count -= (int)todo;
            m_complete += todo;
            if (m_currentIovOffset == m_iovs[m_currentIov].iov_len) {
                m_currentIovOffset = 0;
                ++m_currentIov;
            }
        }
        return true;
    }

    int64 ByteCount() const { return m_complete; }

private:
    std::vector<iovec> m_iovs;
    size_t m_currentIov;
    size_t m_currentIovOffset;
    int64 m_complete;
};

class BufferZeroCopyOutputStream : public io::ZeroCopyOutputStream
{
public:
    BufferZeroCopyOutputStream(Buffer &buffer, size_t bufferSize = 1024)
        : m_buffer(buffer),
          m_bufferSize(bufferSize),
          m_pendingProduce(0),
          m_total(0)
    {}
    ~BufferZeroCopyOutputStream()
    {
        m_buffer.produce(m_pendingProduce);
    }

    bool Next(void **data, int *size)
    {
        m_buffer.produce(m_pendingProduce);
        m_pendingProduce = 0;

        // TODO: protect against std::bad_alloc?
        iovec iov = m_buffer.writeBuffer(m_bufferSize, false);
        *data = iov.iov_base;
        m_total += m_pendingProduce = iov.iov_len;
        *size = (int)m_pendingProduce;

        return true;
    }

    void BackUp(int count)
    {
        MORDOR_ASSERT(count <= (int)m_pendingProduce);
        m_pendingProduce -= count;
        m_total -= count;
    }

    int64 ByteCount() const
    {
        return m_total;
    }

private:
    Buffer &m_buffer;
    size_t m_bufferSize, m_pendingProduce;
    int64 m_total;
};

void serializeToBuffer(const Message &proto, Buffer &buffer)
{
    BufferZeroCopyOutputStream stream(buffer);
    if (!proto.SerializeToZeroCopyStream(&stream))
        MORDOR_THROW_EXCEPTION(std::invalid_argument("proto"));
}

void parseFromBuffer(Message &proto, const Buffer &buffer)
{
    BufferZeroCopyInputStream stream(buffer);
    if (!proto.ParseFromZeroCopyStream(&stream))
        MORDOR_THROW_EXCEPTION(std::invalid_argument("buffer"));
}


// begin anonymous namespace for reflection stuff
namespace {

void setFieldValue(Message *message, const FieldDescriptor *descriptor, const Value &fieldValue, int index=-1)
{

    const Reflection* reflection = message->GetReflection();

#define SET_FIELD(setter, type, proto_type)\
    /*BOOST_STATIC_ASSERT(\
        (#setter == "Int32" && #type == "long long") ||\
        (#setter == "UInt32" && #type == "long long") ||\
        (#setter == "Int64" && #type == "long long") ||\
        (#setter == "UInt64" && #type == "long long") ||\
        (#setter == "Float" && #type == "double") ||\
        (#setter == "Double" && #type == "double") ||\
        (#setter == "Bool" && #type == "bool") ||\
        (#setter == "String" && #type == "std::string"));*/\
    if (descriptor->label() != FieldDescriptor::LABEL_REPEATED) {\
        type value = boost::get<type>(fieldValue);\
        reflection->Set##setter(message, descriptor, (proto_type)value);\
    } else if (index >= 0) {\
        type value = boost::get<type>(fieldValue);\
        reflection->SetRepeated##setter(message, descriptor, index, (proto_type)value);\
    } else {\
        const Array &array = boost::get<Array>(fieldValue);\
        BOOST_FOREACH(Value v, array) {\
            type value = boost::get<type>(v);\
            reflection->Add##setter(message, descriptor, (proto_type)value);\
        }\
    }


    try {
        switch (descriptor->cpp_type()) {
            case FieldDescriptor::CPPTYPE_INT32:
                SET_FIELD(Int32, long long, int32_t)
                break;
            case FieldDescriptor::CPPTYPE_INT64:
                SET_FIELD(Int64, long long, int64_t)
                break;
            case FieldDescriptor::CPPTYPE_UINT32:
                SET_FIELD(UInt32, long long, uint32_t)
                break;
            case FieldDescriptor::CPPTYPE_UINT64:
                SET_FIELD(UInt64, long long, uint64_t)
                break;
            case FieldDescriptor::CPPTYPE_FLOAT:
                SET_FIELD(Float, double, float)
                break;
            case FieldDescriptor::CPPTYPE_DOUBLE:
                SET_FIELD(Double, double, double)
                break;
            case FieldDescriptor::CPPTYPE_BOOL:
                SET_FIELD(Bool, bool, bool)
                break;
            case FieldDescriptor::CPPTYPE_STRING:
                SET_FIELD(String, std::string, std::string)
                break;
            case FieldDescriptor::CPPTYPE_ENUM:
                if (descriptor->label() != FieldDescriptor::LABEL_REPEATED) {
                    std::string value = boost::get<std::string>(fieldValue);
                    const EnumValueDescriptor * val = message->GetDescriptor()->FindEnumValueByName(value);
                    reflection->SetEnum(message, descriptor, val);
                } else {
                    const Array &array = boost::get<Array>(fieldValue);
                    BOOST_FOREACH(Value v, array) {
                        std::string value = boost::get<std::string>(v);
                        const EnumValueDescriptor * val = message->GetDescriptor()->FindEnumValueByName(value);
                        reflection->AddEnum(message, descriptor, val);
                    }
                }
                break;
            default:
                MORDOR_NOTREACHED();
        }
    } catch (boost::bad_get &) {
        throw std::runtime_error(message->GetDescriptor()->name() + "." +
            descriptor->name() + " is invalid");
    }
#undef SET_FIELD
}

Value getFieldValue(const Message &message, const FieldDescriptor *descriptor, int index=-1)
{
    const Reflection *reflection = message.GetReflection();

#define GET_FIELD(getter_type, type)\
    if (descriptor->label() != FieldDescriptor::LABEL_REPEATED) {\
        if (reflection->HasField(message, descriptor)) {\
            type value = reflection->Get##getter_type(message, descriptor);\
            return value;\
        } else {\
            return boost::blank();\
        }\
    } else if (index >= 0) {\
        type value = reflection->GetRepeated##getter_type(message, descriptor, index);\
        return value;\
    } else {\
        Array array;\
        int field_size = reflection->FieldSize(message, descriptor);\
        for (int i = 0; i < field_size; i++) {\
            type value = reflection->GetRepeated##getter_type(message, descriptor, i);\
            array.push_back(value);\
        }\
        return array;\
    }

    try {
        switch (descriptor->cpp_type()) {
            case FieldDescriptor::CPPTYPE_INT32:
                GET_FIELD(Int32, long long)
            case FieldDescriptor::CPPTYPE_INT64:
                GET_FIELD(Int64, long long)
            case FieldDescriptor::CPPTYPE_UINT32:
                GET_FIELD(UInt32, long long)
            case FieldDescriptor::CPPTYPE_UINT64:
                GET_FIELD(UInt64, long long)
            case FieldDescriptor::CPPTYPE_FLOAT:
                GET_FIELD(Float, double)
            case FieldDescriptor::CPPTYPE_DOUBLE:
                GET_FIELD(Double, double)
            case FieldDescriptor::CPPTYPE_BOOL:
                GET_FIELD(Bool, bool)
            case FieldDescriptor::CPPTYPE_STRING:
                GET_FIELD(String, std::string)
            case FieldDescriptor::CPPTYPE_ENUM:
                if (descriptor->label() != FieldDescriptor::LABEL_REPEATED) {\
                    if (reflection->HasField(message, descriptor)) {
                        const EnumValueDescriptor *value = reflection->GetEnum(message, descriptor);
                        MORDOR_ASSERT(value);
                        return value->name();
                    }else {
                        return boost::blank();
                    }
                } else {
                    Array array;
                    int field_size = reflection->FieldSize(message, descriptor);
                    for (int i = 0; i < field_size; i++) {
                        const EnumValueDescriptor *value = reflection->GetRepeatedEnum(message, descriptor, i);
                        array.push_back(value->name());
                    }
                    return array;
                }
            default:
                MORDOR_NOTREACHED();
        }
    } catch (boost::bad_get &) {
        throw std::runtime_error(message.GetDescriptor()->name() + "." + descriptor->name() + " is invalid");
    }
#undef GET_FIELD
}


const FieldDescriptor *getFieldDescription(Message *msg, const std::string &fieldName, int &index)
{
    static boost::regex index_regex("^([^ ]*)\\[([0-9]+)\\]$");
    const Reflection* reflection = msg->GetReflection();


    index = -1;
    size_t pos = fieldName.find('.');

    std::string field = fieldName.substr(0, pos);

    // check indexed field
    if (boost::regex_match(field, index_regex)) {
        std::string idx = field;
        boost::replace_all_regex(idx, index_regex, std::string("$2"));
        index = boost::lexical_cast<int>(idx);
        field = field.substr(0, field.find('['));
    }

    const FieldDescriptor *desc = msg->GetDescriptor()->FindFieldByName(field);
    if (desc == NULL) {
        throw std::runtime_error(field + " field cannot be found");
    }
    if (desc->label() != FieldDescriptor::LABEL_REPEATED && index != -1) {
        throw std::runtime_error(field + " is not repeated");
    }
    if (desc->label() == FieldDescriptor::LABEL_REPEATED && index == -1) {
        throw std::runtime_error(field + " should be repeated");
    }

    if (index >= 0 && index >= reflection->FieldSize(*msg, desc)) {
        throw std::runtime_error(field + " out of bound");
    }

    if (pos != std::string::npos && desc->type() != FieldDescriptor::TYPE_MESSAGE) {
        throw std::runtime_error(field + " is not be a sub-message");
    }
    if (pos == std::string::npos && desc->type() == FieldDescriptor::TYPE_MESSAGE) {
        throw std::runtime_error(field + " should be a sub-message");
    }

    return desc;
}


} // end of anonymous ns


void serializeToJsonObject(const Message &message, Object &object, bool validate)
{

    const Reflection* reflection = message.GetReflection();
    for (int i = 0; i < message.GetDescriptor()->field_count(); i++) {
        const FieldDescriptor* descriptor = message.GetDescriptor()->field(i);
        std::string fieldName = descriptor->name();

        if (descriptor->type() == FieldDescriptor::TYPE_MESSAGE) {
            if (descriptor->label() != FieldDescriptor::LABEL_REPEATED) {
                Object msgObj;
                if (reflection->HasField(message, descriptor)) {
                    const Message &subMessage = reflection->GetMessage(message, descriptor,
                            MessageFactory::generated_factory());
                    serializeToJsonObject(subMessage, msgObj, validate);
                }
                else if (validate && descriptor->is_required()) {
                    throw std::runtime_error(message.GetDescriptor()->name() + "." + descriptor->name() + " is required");
                }
                object[fieldName] = msgObj;
            } else {
                Array array;
                int field_size = reflection->FieldSize(message, descriptor);
                for (int i = 0; i < field_size; i++) {
                    Object msgObj;
                    const Message &subMessage = reflection->GetRepeatedMessage(message, descriptor, i);
                    serializeToJsonObject(subMessage, msgObj, validate);
                    array.push_back(msgObj);
                }
                object[fieldName] = array;
            }
            continue;
        }

        // other value types
        Value value = getFieldValue(message, descriptor);
        object[fieldName] = value;
    }
}

void parseFromJsonObject(Message *message, const Object &object, bool validate)
{
    const Reflection* reflection = message->GetReflection();
    MORDOR_ASSERT(reflection);
    for (int i = 0; i < message->GetDescriptor()->field_count(); i++) {
        const FieldDescriptor* descriptor = message->GetDescriptor()->field(i);
        std::string field_name = descriptor->name();

        MORDOR_ASSERT(descriptor);

        if (descriptor->type() == FieldDescriptor::TYPE_MESSAGE) {
            Object::const_iterator it = object.find(descriptor->name());
            if (it != object.end()) {
                if (descriptor->label() != FieldDescriptor::LABEL_REPEATED) {
                    Message *subMessage = reflection->MutableMessage(message, descriptor,
                        MessageFactory::generated_factory());
                    const Object &value = boost::get<Object>(it->second);
                    parseFromJsonObject(subMessage, value, validate);
                } else {
                    const Array &array = boost::get<Array>(it->second);
                    BOOST_FOREACH(Value v, array) {
                        const Object &childObject = boost::get<Object>(v);
                        Message *subMessage = reflection->AddMessage(message, descriptor,
                            MessageFactory::generated_factory());
                        parseFromJsonObject(subMessage, childObject, validate);
                    }
                }
            }
        } else {
            Object::const_iterator it = object.find(field_name);
            if (it == object.end() || it->second.isBlank()) {
                if (validate && descriptor->is_required()) {
                    throw std::runtime_error(message->GetDescriptor()->name() + "." + field_name + " is required");
                }
                continue; // skip optional field
            }
            setFieldValue(message, descriptor, it->second);
        }
    }
}

Message* forName(const std::string& typeName)
{
    Message* message = NULL;
    const Descriptor* descriptor = DescriptorPool::generated_pool()->FindMessageTypeByName(typeName);
    if (descriptor)
    {
        const Message* prototype = MessageFactory::generated_factory()->GetPrototype(descriptor);
        if (prototype)
        {
            message = prototype->New();
        }
    }
    return message;
}

std::string toJson(const Message &message, bool validate)
{
    Object root, msgObj;

    serializeToJsonObject(message, msgObj, validate);
    root[message.GetDescriptor()->full_name()] = msgObj;

    std::ostringstream os;
    os << root;
    return os.str();
}


Message * fromJson(const Buffer &buffer, bool validate)
{
    Value root;
    Parser parser(root);
    parser.run(buffer);
    if (!parser.final() || parser.error()) {
        return NULL;
    }

    Object &rootObject = boost::get<Object>(root);
    std::string messageType = rootObject.begin()->first;

    Message *message = forName(messageType);
    if (!message) {
        return NULL;
    }

    Object &messageObject = boost::get<Object>(rootObject.begin()->second);
    parseFromJsonObject(message, messageObject, validate);

    return message;
}



// setField(partialObject, "store_dat.len", 100);
// setField(namedObject, "fs_type", "DIRECTORY");
void setField(Message *msg, const std::string &fieldName, const Value &value)
{
    const Reflection* reflection = msg->GetReflection();

    int index;
    // get child property desc
    const FieldDescriptor *desc = getFieldDescription(msg, fieldName, index);

    MORDOR_ASSERT(desc);
    if (desc->type() == FieldDescriptor::TYPE_MESSAGE) {
        Message *subMsg = (index >=0) ?
            reflection->MutableRepeatedMessage(msg, desc, index) :
            reflection->MutableMessage(msg, desc);

        size_t pos = fieldName.find('.');
        MORDOR_ASSERT(pos != std::string::npos);

        std::string remaining = fieldName.substr(pos + 1);

        return setField(subMsg, remaining, value);
    }
    setFieldValue(msg, desc, value, index);
}

// getField(partialObject, "store_dat.frag[1].tdn_id")
Value getField(Message *msg, const std::string &fieldName)
{
    const Reflection* reflection = msg->GetReflection();

    int index;
    // get child property desc
    const FieldDescriptor *desc = getFieldDescription(msg, fieldName, index);

    MORDOR_ASSERT(desc);
    if (desc->type() == FieldDescriptor::TYPE_MESSAGE) {
        Message *subMsg = (index >=0) ?
            reflection->MutableRepeatedMessage(msg, desc, index) :
            reflection->MutableMessage(msg, desc);

        size_t pos = fieldName.find('.');
        MORDOR_ASSERT(pos != std::string::npos);

        std::string remaining = fieldName.substr(pos + 1);

        return getField(subMsg, remaining);
    }
    return getFieldValue(*msg, desc, index);
}



}
