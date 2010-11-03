AM_CPPFLAGS=$(OPENSSL_INCLUDES) $(BOOST_CPPFLAGS) -I$(top_srcdir)
AM_LDFLAGS=					\
	$(OPENSSL_LDFLAGS)			\
	$(BOOST_LDFLAGS)			\
	$(CORESERVICES_FRAMEWORK_LIBS)		\
	$(COREFOUNDATION_FRAMEWORK_LIBS)	\
	$(SECURITY_FRAMEWORK_LIBS)		\
	$(SYSTEMCONFIGURATION_FRAMEWORK_LIBS)
AM_CXXFLAGS=-Wall -Werror

noinst_PROGRAMS=cat echoserver iombench tunnel udpstats wget

cat_SOURCES=cat.cpp
cat_LDADD=../libmordor.la $(OPENSSL_LIBS) $(BOOST_REGEX_LIB) $(BOOST_THREAD_LIB)

echoserver_SOURCES=echoserver.cpp
echoserver_LDADD=../libmordor.la $(OPENSSL_LIBS) $(BOOST_REGEX_LIB) $(BOOST_THREAD_LIB)

iombench_SOURCES=iombench.cpp netbench.cpp
iombench_LDADD=../libmordor.la $(OPENSSL_LIBS) $(BOOST_REGEX_LIB) $(BOOST_THREAD_LIB)

tunnel_SOURCES=tunnel.cpp
tunnel_LDADD=../libmordor.la $(OPENSSL_LIBS) $(BOOST_REGEX_LIB) $(BOOST_THREAD_LIB)

udpstats_SOURCES=udpstats.cpp
udpstats_LDADD=../libmordor.la $(OPENSSL_LIBS) $(BOOST_REGEX_LIB) $(BOOST_THREAD_LIB)

wget_SOURCES=wget.cpp
wget_LDADD=../libmordor.la $(OPENSSL_LIBS) $(BOOST_REGEX_LIB) $(BOOST_THREAD_LIB) $(BOOST_PROGRAM_OPTIONS_LIB)

