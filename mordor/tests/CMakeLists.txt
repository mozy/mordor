set(SRCS
    buffer.cpp
    buffered_stream.cpp
    chunked_stream.cpp
    config.cpp
    coroutine.cpp
    counter_stream.cpp
    crypto.cpp
    efs_stream.cpp
    endian.cpp
    fibers.cpp
    fibersync.cpp
    fls.cpp
    future.cpp
    hash_stream.cpp
    hmac.cpp
    http_client.cpp
    http_parser.cpp
    http_proxy.cpp
    http_server.cpp
    http_servlet_dispatcher.cpp
    http_stream.cpp
    iomanager.cpp
    iomanager_iocp.cpp
    json.cpp
    log.cpp
    memory_stream.cpp
    oauth.cpp
    pipe_stream.cpp
    ragel.cpp
    run_tests.cpp
    scheduler.cpp
    socket.cpp
    ssl_stream.cpp
    statistics.cpp
    stream.cpp
    string.cpp
    temp_stream.cpp
    thread.cpp
    timeout_stream.cpp
    timer.cpp
    transfer_stream.cpp
    unicode.cpp
    uri.cpp
    util.cpp
    xml.cpp
    zip.cpp
    zlib.cpp
)

if (NOT MSVC)
    #These tests don't run on windows
    list(APPEND SRCS 
        notify_stream.cpp
        stdcrypto.cpp)

    if (LINUX)
        list(APPEND SRCS 
            atomic.cpp
            cat_stream.cpp
            file_stream.cpp
            tar.cpp
            )
    endif()
endif()

add_msvc_precompiled_header(pch.h pch.cpp SRCS)

add_executable(tests ${SRCS})

target_link_libraries(tests mordor mordortests ${OPENSSL_LIBRARIES} ${Boost_LIBRARIES} ${ZLIB_LIBRARIES})
