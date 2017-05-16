set(SRCS
    anymap.h
    assert.cpp
    assert.h
    atomic.h
    config.cpp
    config.h
    coroutine.h
    daemon.cpp
    daemon.h
    date_time.cpp
    date_time.h
    endian.h
    exception.cpp
    exception.h
    factory.h
    fiber.cpp
    fiber.h
    fibersynchronization.cpp
    fibersynchronization.h
    future.h
    iomanager.h
    json.h
    log.cpp
    log.h
    log_base.h
    main.cpp
    main.h
    openssl_lock.cpp
    openssl_lock.h
    parallel.cpp
    parallel.h
    predef.h
    ragel.cpp
    ragel.h
    runtime_linking.cpp
    runtime_linking.h
    scheduler.cpp
    scheduler.h
    semaphore.cpp
    semaphore.h
    sleep.cpp
    sleep.h
    socket.cpp
    socket.h
    socks.cpp
    socks.h
    statistics.cpp
    statistics.h
    string.cpp
    string.h
    tar.cpp
    tar.h
    thread.cpp
    thread.h
    thread_local_storage.h
    timer.cpp
    timer.h
    uri.h
    util.cpp
    util.h
    version.h
    workerpool.cpp
    workerpool.h
    zip.cpp
    zip.h
)

if(MSVC)
    set(SRCS_OSSPECIFIC
        iomanager_iocp.cpp
        iomanager_iocp.h
    )
else()
    set(SRCS_OSSPECIFIC
        pch.h
        cxa_exception.cpp
        cxa_exception.h
        eventloop.cpp
        eventloop.h
        streams/fd.cpp
        streams/fd.h
    )

    if(CMAKE_HOST_APPLE)
        list(APPEND SRCS_OSSPECIFIC
            iomanager_kqueue.cpp
            iomanager_kqueue.h
        )
    elseif(LINUX)
        set(SRCS_OSSPECIFIC
            iomanager_epoll.cpp
            iomanager_epoll.h
        )
    endif()
endif()

set(SRCS_HTTP
    http/auth.cpp
    http/auth.h
    http/basic.cpp
    http/basic.h
    http/broker.cpp
    http/broker.h
    http/chunked.cpp
    http/chunked.h
    http/client.cpp
    http/client.h
    http/connection.cpp
    http/connection.h
    http/digest.cpp
    http/digest.h
    http/http.cpp
    http/http.h
    http/multipart.cpp
    http/multipart.h
    http/negotiate.cpp
    http/negotiate.h
    http/oauth.cpp
    http/oauth.h
    http/oauth2.cpp
    http/oauth2.h
    http/parser.h
    http/proxy.cpp
    http/proxy.h
    http/server.cpp
    http/server.h
    http/servlet.cpp
    http/servlet.h
    http/servlets/config.cpp
    http/servlets/config.h
)

set(SRCS_STREAMS
    streams/buffer.cpp
    streams/buffer.h
    streams/buffered.cpp
    streams/buffered.h
    streams/cat.cpp
    streams/cat.h
    streams/counter.cpp
    streams/counter.h
    streams/crypto.cpp
    streams/crypto.h
    streams/deflate.h
    streams/delay.h
    streams/duplex.h
    streams/efs.cpp
    streams/efs.h
    streams/file.cpp
    streams/file.h
    streams/filter.cpp
    streams/filter.h
    streams/gzip.h
    streams/handle.cpp
    streams/handle.h
    streams/hash.cpp
    streams/hash.h
    streams/hashfwd.h
    streams/http.cpp
    streams/http.h
    streams/limited.cpp
    streams/limited.h
    streams/memory.cpp
    streams/memory.h
    streams/namedpipe.cpp
    streams/namedpipe.h
    streams/notify.h
    streams/null.cpp
    streams/null.h
    streams/pipe.cpp
    streams/pipe.h
    streams/progress.h
    streams/random.cpp
    streams/random.h
    streams/scheduler.h
    streams/singleplex.cpp
    streams/singleplex.h
    streams/socket.cpp
    streams/socket.h
    streams/ssl.cpp
    streams/ssl.h
    streams/std.cpp
    streams/std.h
    streams/stdcrypto.cpp
    streams/stdcrypto.h
    streams/stream.cpp
    streams/stream.h
    streams/temp.cpp
    streams/temp.h
    streams/test.cpp
    streams/test.h
    streams/throttle.cpp
    streams/throttle.h
    streams/timeout.cpp
    streams/timeout.h
    streams/transfer.cpp
    streams/transfer.h
    streams/zero.cpp
    streams/zero.h
    streams/zlib.cpp
    streams/zlib.h
)

#Would be a conditional based on presence of lzma (not needed currently on windows)
#set(SRC_LZMA
#    streams/lzma2.cpp
#    streams/lzma2.h
#)

set(SRCS_XML
    xml/dom_parser.cpp
    xml/dom_parser.h
    xml/parser.h
)

set(FULL_SRCS ${SRCS} ${SRCS_HTTP} ${SRCS_STREAMS} ${SRCS_XML} ${SRCS_OSSPECIFIC})

add_msvc_precompiled_header(mordor/pch.h pch.cpp FULL_SRCS)

#Often include statements are relative to root of mordor source tree
#e.g. #include <mordor/assert.h>
include_directories(AFTER
    ${mordor_SOURCE_DIR}
)

#Ragel is used to generate cpp files.
#the helper ragelmaker sets up the custom command and accumulates
#the list of output source files so we can add them to the mordor library

set(SRCS_RAGEL
    json.rl
    uri.rl
    xml/xml_parser.rl
)

foreach(SRC_RAGEL ${SRCS_RAGEL})
    ragelmaker(${SRC_RAGEL} SRCS_RAGEL_OUTPUT ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()

#http_parser.rl is special case because it needs its own directory as an include paths
#to force "../uri.rl" to resolve
ragelmaker( http/http_parser.rl SRCS_RAGEL_OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}\\http )

list(APPEND SRCS_RAGEL http/http_parser.rl)


add_library(mordor STATIC ${FULL_SRCS} ${SRCS_RAGEL} ${SRCS_RAGEL_OUTPUT})

source_group("Source Files" FILES ${SRCS} ${SRCS_OSSPECIFIC})
source_group("http" FILES ${SRCS_HTTP})
source_group("streams" FILES ${SRCS_STREAMS})
source_group("xml" FILES ${SRCS_XML})

#TODO    protobuf.cpp   protobuf.h
#todo - fix object file name overlap - in vcxproj the obj names are explicitly renamed -> seems relative paths used instead automatically
#there is also optional yaml library, based on HAVE_LIBYAML
#todo - There is also a pq and pqtests project

add_subdirectory(test)

add_subdirectory(tests)
