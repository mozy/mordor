// Copyright (c) 2009 - Mozy, Inc.

// This example demos usage of zip streaming that compress and send files on the fly:
// [File Server] -- requested files --> [Compress Server:8000] -- compressed zip file --> [Download Server:8001]

#include "mordor/predef.h"

#include <iostream>
#include <string>
#include <vector>

#include <boost/bind.hpp>
#include <boost/function.hpp>

#include "mordor/assert.h"
#include "mordor/config.h"
#include "mordor/daemon.h"
#include "mordor/iomanager.h"
#include "mordor/main.h"
#include "mordor/socket.h"
#include "mordor/streams/file.h"
#include "mordor/streams/singleplex.h"
#include "mordor/streams/socket.h"
#include "mordor/streams/transfer.h"
#include "mordor/zip.h"

using namespace Mordor;
using namespace boost;

static const std::string COMPRESS_SRV_ADDR = "localhost:8000";
static const std::string DOWNLOAD_SRV_ADDR = "localhost:8001";
static const int SENDING_DELAY = 3;
static short FILE_SIG = 0xabcd;

static std::string g_downloadFileName;

struct Header
{
    short sig;
    char name[256];
    unsigned long long size;
};

void compress(IOManager& ioManager, Stream::ptr stream)
{
    try {
        std::cout << "start compressing" << std::endl;

        std::vector<Address::ptr> addresses = Address::lookup(DOWNLOAD_SRV_ADDR);
        MORDOR_ASSERT(!addresses.empty());
        Socket::ptr s(addresses[0]->createSocket(ioManager, SOCK_STREAM));
        s->connect(addresses[0]);

        Stream::ptr socketStream(new SocketStream(s));
        SingleplexStream::ptr single(new SingleplexStream(socketStream, SingleplexStream::WRITE));
        Zip zip(single);

        while (true) {
            Header header;
            size_t len = stream->read(&header, sizeof(header));
            if (len == 0) {
                break;
            }
            MORDOR_ASSERT(header.sig == FILE_SIG);

            std::cout << "compressing file " << header.name << std::endl;
            ZipEntry& entry = zip.addFile();
            entry.filename(header.name);
            transferStream(stream, entry.stream(), header.size);
        }

        zip.close();
        s->shutdown();
        std::cout << "compress done" << std::endl;
    } catch (...) {
        std::cerr << current_exception_diagnostic_information() << std::endl;
    }
    stream->close();
}

void download(Stream::ptr stream)
{
    try {
        std::cout << "start downloading" << std::endl;

        FileStream outputStream(g_downloadFileName, FileStream::WRITE, FileStream::OVERWRITE_OR_CREATE);
        transferStream(stream, outputStream);

        std::cout << "download done: " << g_downloadFileName << std::endl;
    } catch (...) {
        std::cerr << current_exception_diagnostic_information() << std::endl;
    }
    stream->close();
}

void serve(Socket::ptr listen, function<void (Stream::ptr stream)> handler)
{
    listen->listen();
    // only serve (accept) once since this is an one time task
    Socket::ptr socket = listen->accept();
    Stream::ptr stream(new SocketStream(socket));
    Scheduler::getThis()->schedule(bind(handler, stream));
}

void startServer(IOManager& ioManager, const std::string& addr, function<void (Stream::ptr stream)> handler)
{
    std::cout << "start server: " << addr << std::endl;
    std::vector<Address::ptr> addresses = Address::lookup(addr);
    MORDOR_ASSERT(!addresses.empty());
    Socket::ptr s(addresses[0]->createSocket(ioManager, SOCK_STREAM));
    s->bind(addresses[0]);
    Scheduler::getThis()->schedule(bind(&serve, s, handler));
}

void requestFiles(IOManager& ioManager, const std::vector<std::string>& files)
{
    try {
        std::vector<Address::ptr> addresses = Address::lookup(COMPRESS_SRV_ADDR);
        MORDOR_ASSERT(!addresses.empty());
        Socket::ptr s(addresses[0]->createSocket(ioManager, SOCK_STREAM));
        s->connect(addresses[0]);
        Stream::ptr socketStream(new SocketStream(s));

        for (std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++it) {
            FileStream inputStream(*it, FileStream::READ);
            std::cout << "sending file " << *it << std::endl;

            Header header;
            header.sig = FILE_SIG;
            strncpy(header.name, it->c_str(), 256);
            header.name[255] = 0;
            header.size = inputStream.size();
            socketStream->write(&header, sizeof(Header));
            transferStream(inputStream, socketStream);
        }

        s->shutdown();
    } catch (...) {
        std::cerr << current_exception_diagnostic_information() << std::endl;
    }
}

MORDOR_MAIN(int argc, char* argv[])
{
    try {
        Config::loadFromEnvironment();

        std::vector<std::string> files(argv + 1, argv + argc);
        if (!files.empty()) {
            g_downloadFileName = argv[0] + std::string("_example.zip");

            IOManager ioManager;
            startServer(ioManager, COMPRESS_SRV_ADDR, bind(&compress, ref(ioManager), _1));
            startServer(ioManager, DOWNLOAD_SRV_ADDR, &download);

            std::cout << "start sending " << files.size() << " files in "
                << SENDING_DELAY << " seconds" << std::endl;
            ioManager.registerTimer(SENDING_DELAY * 1000000, bind(&requestFiles, ref(ioManager), ref(files)));
            ioManager.dispatch();
        } else {
            std::cout << "no file provided, usage:\n"
                << argv[0] << " file1 [, file2 ...]" << std::endl;
        }
    } catch (...) {
        std::cerr << current_exception_diagnostic_information() << std::endl;
        return 1;
    }
    return 0;
}
