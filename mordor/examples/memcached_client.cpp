// Copyright (c) 2010 - Mozy, Inc.

#include "mordor/predef.h"

#include <iostream>

#include <openssl/sha.h>

#include "mordor/config.h"
#include "mordor/iomanager.h"
#include "mordor/memcached.h"
#include "mordor/socket.h"
#include "mordor/streams/buffered.h"
#include "mordor/streams/socket.h"

using namespace Mordor;

static Stream::ptr establishConn(const Address::ptr &address, IOManager &ioManager)
{
    Socket::ptr socket = address->createSocket(ioManager);
    socket->connect(address);
    Stream::ptr result(new SocketStream(socket));
    result.reset(new BufferedStream(result));
    return result;
}

struct SHA1Hasher
{
    unsigned long long operator()(const Address::ptr &address, size_t replica) const
    {
        std::ostringstream os;
        os << *address << '.' << replica;

        return (*this)(os.str());
    }

    unsigned long long operator()(const std::string &key) const
    {
        // Our hash function takes the first 8 bytes of a SHA1 hash
        SHA_CTX ctx;
        unsigned char raw[SHA_DIGEST_LENGTH];
        SHA1_Init(&ctx);
        SHA1_Update(&ctx, key.c_str(), key.size());
        SHA1_Final(raw, &ctx);

        return *(unsigned long long *)raw;
    }
};

int main(int argc, const char *argv[])
{
    try {
        int numberOfNodes = 1;
        if (argc > 2)
            numberOfNodes = boost::lexical_cast<int>(argv[1]);
        if (argc < numberOfNodes + 3 || argc > numberOfNodes + 4) {
            std::cerr << "Usage: " << argv[0] << " <numberofnodes> <nodes...> <key> [<value>]";
            return 1;
        }
        Config::loadFromEnvironment();
        IOManager ioManager;

        ConsistentHash<unsigned long long, Address::ptr, SHA1Hasher> consistentHash;
        for (int i = 2; i < numberOfNodes + 2; ++i) {
            std::vector<Address::ptr> addresses = Address::lookup(argv[i], AF_UNSPEC, SOCK_STREAM);
            consistentHash.insert(addresses.begin(), addresses.end());
        }

        MemcachedClient<Address::ptr> client(
            consistentHash,
            boost::bind(&establishConn, _1, boost::ref(ioManager)));

        if (argc == numberOfNodes + 4) {
            client.set(argv[numberOfNodes + 2], 0, argv[numberOfNodes + 3]);
        } else {
            MemcachedClient<Address::ptr>::Value value;
            if (client.get(argv[numberOfNodes + 2], value)) {
                std::cout << "Flags: " << value.flags
                    << " Value: " << value.value << std::endl;
            } else {
                std::cout << "NOT FOUND" << std::endl;
            }
        }
    } catch (...) {
        std::cerr << boost::current_exception_diagnostic_information() << std::endl;
        return 2;
    }
    return 0;
}
