SUBDIRS=. test tests examples

AUTOMAKE_OPTIONS=nostdinc subdir-objects
AM_CPPFLAGS=$(OPENSSL_INCLUDES) $(BOOST_CPPFLAGS)
AM_CXXFLAGS=-Wall -Werror -fno-strict-aliasing

lib_LTLIBRARIES=libmordor.la
libmordor_la_SOURCES=			\
	assert.cpp			\
	config.cpp			\
	daemon.cpp			\
	date_time.cpp			\
	exception.cpp			\
	fiber.cpp			\
	fibersynchronization.cpp	\
	http/auth.cpp			\
	http/basic.cpp			\
	http/broker.cpp			\
	http/chunked.cpp		\
	http/client.cpp			\
	http/connection.cpp		\
	http/digest.cpp			\
	http/http.cpp			\
	http/http_parser.cpp		\
	http/multipart.cpp		\
	http/oauth.cpp			\
	http/proxy.cpp			\
	http/server.cpp			\
	iomanager_epoll.cpp		\
	iomanager_kqueue.cpp		\
	json.cpp			\
	log.cpp				\
	parallel.cpp			\
	ragel.cpp			\
	scheduler.cpp			\
	semaphore.cpp			\
	sleep.cpp			\
	socket.cpp			\
	socks.cpp			\
	statistics.cpp			\
	streams/buffer.cpp		\
	streams/buffered.cpp		\
	streams/cat.cpp			\
	streams/fd.cpp			\
	streams/file.cpp		\
	streams/filter.cpp		\
	streams/hash.cpp		\
	streams/http.cpp		\
	streams/limited.cpp		\
	streams/memory.cpp		\
	streams/null.cpp		\
	streams/pipe.cpp		\
	streams/random.cpp		\
	streams/singleplex.cpp		\
	streams/socket.cpp		\
	streams/ssl.cpp			\
	streams/std.cpp			\
	streams/stream.cpp		\
	streams/temp.cpp		\
	streams/timeout.cpp		\
	streams/test.cpp		\
	streams/throttle.cpp		\
	streams/transfer.cpp		\
	streams/zlib.cpp		\
	string.cpp			\
	thread.cpp			\
	timer.cpp			\
	uri.cpp				\
	workerpool.cpp			\
	xml/xml_parser.cpp		\
	zip.cpp
libmordor_la_CPPFLAGS=-I$(top_srcdir) -include mordor/pch.h

SUFFIXES=.rl

.rl.cpp:
	$(RAGEL) $(RAGELFLAGS) -C $< -o $@

# Objects specifically depend on the PCH
$(libmordor_la_OBJECTS): pch.h.gch

MOSTLYCLEANFILES=pch.h.gch
CLEANFILES=http/http_parser.cpp json.cpp uri.cpp xml/xml_parser.cpp
EXTRA_DIST=http/http_parser.rl json.rl uri.rl xml/xml_parser.rl

pch.h.gch: pch.h
	$(CXXCOMPILE) $(pic_flag) -x c++-header $< -o $@
