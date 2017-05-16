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

    if (NOT CMAKE_HOST_APPLE)
        list(APPEND SRCS
            atomic.cpp
            cat_stream.cpp
            file_stream.cpp
            tar.cpp
            )
    endif()
endif()

#So that paths like "pch.h" resolve (required on Linux)
include_directories(AFTER .)	

add_precompiled_header(pch.h pch.cpp SRCS)

#For the moment maintain inconsistency in the test executable name
#between platforms.  The naming could be made consistent if we also update
#buildtools scripts which trigger the tests
if (MSVC)
    set(MORDOR_TEST_EXE_NAME tests)
else()
    set(MORDOR_TEST_EXE_NAME run_tests)
endif()

add_executable(${MORDOR_TEST_EXE_NAME} ${SRCS})

target_link_libraries(${MORDOR_TEST_EXE_NAME} 
    mordor
    mordortest
    ${OPENSSL_LIBRARIES}
    ${Boost_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${LZMA_LIBRARIES})

add_osspecific_linking(${MORDOR_TEST_EXE_NAME})

