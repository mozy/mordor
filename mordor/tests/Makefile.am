AM_CPPFLAGS=$(OPENSSL_INCLUDES) $(BOOST_CPPFLAGS) -I$(top_srcdir)
AM_CXXFLAGS=-Wall -Werror -fno-strict-aliasing

check_PROGRAMS=run_tests
run_tests_SOURCES=		\
	run_tests.cpp		\
	buffer.cpp		\
	buffered_stream.cpp	\
	chunked_stream.cpp	\
	coroutine.cpp		\
	endian.cpp		\
	fibers.cpp		\
	fibersync.cpp		\
	file_stream.cpp		\
	fls.cpp			\
	future.cpp		\
	hmac.cpp		\
	http_client.cpp		\
	http_parser.cpp		\
	http_server.cpp		\
	iomanager.cpp		\
	json.cpp		\
	log.cpp			\
	memory_stream.cpp	\
	oauth.cpp		\
	pipe_stream.cpp		\
	scheduler.cpp		\
	socket.cpp		\
	ssl_stream.cpp		\
	stream.cpp		\
	string.cpp		\
	temp_stream.cpp		\
	thread.cpp		\
	timeout_stream.cpp	\
	timer.cpp		\
	transfer_stream.cpp	\
	uri.cpp			\
	xml.cpp			\
	zlib.cpp
run_tests_LDADD=../libmordor.la ../test/libmordortest.la

check:
	./run_tests
