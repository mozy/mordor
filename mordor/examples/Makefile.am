AM_CPPFLAGS=$(OPENSSL_INCLUDES) $(BOOST_CPPFLAGS) -I$(top_srcdir)
AM_CXXFLAGS=-Wall -Werror

noinst_PROGRAMS=cat echoserver iombench tunnel udpstats wget

cat_SOURCES=cat.cpp
cat_LDADD=../libmordor.la

echoserver_SOURCES=echoserver.cpp
echoserver_LDADD=../libmordor.la

iombench_SOURCES=iombench.cpp netbench.cpp
iombench_LDADD=../libmordor.la

EXTRA_DIST=netbench.h

tunnel_SOURCES=tunnel.cpp
tunnel_LDADD=../libmordor.la

udpstats_SOURCES=udpstats.cpp
udpstats_LDADD=../libmordor.la

wget_SOURCES=wget.cpp
wget_LDADD=../libmordor.la $(BOOST_PROGRAM_OPTIONS_LIB)

