#!/bin/bash

if [ ! -x configure ]; then
    autoreconf -i
fi

CXXFLAGS=
TCMALLOC=
if [ `uname` = 'Darwin' ] ; then
    CXXFLAGS="-Wno-deprecated-declarations"
fi

if [ ! -f Makefile ]; then
    if [ "$1" = "debug" ]; then
        CXXFLAGS+=' -g -O0'
    elif [ "$1" = "coverage" ]; then
        CXXFLAGS+=' -g -O0 -fprofile-arcs -ftest-coverage'
    elif [ "$1" = "valgrind" ]; then
        CXXFLAGS+=' -g -O0'
        VALGRINDFLAGS=--enable-valgrind=yes
    else
        ASSERTFLAGS=--disable-assert
        CXXFLAGS+=' -O3'
	TCMALLOC=--with-tcmalloc=yes
    fi
    export CXXFLAGS
    which pg_config >/dev/null || POSTGRESFLAGS=--without-postgresql
    ./configure --disable-shared $ASSERTFLAGS $POSTGRESFLAGS $VALGRINDFLAGS $TCMALLOC
fi
if [ $(uname) = 'Linux' ]; then
    j=$(grep processor /proc/cpuinfo | wc -l)
elif [ $(uname) = 'Darwin' ]; then
    j=$(sysctl -n hw.ncpu)
else
    j=2
fi
make check -j$j
