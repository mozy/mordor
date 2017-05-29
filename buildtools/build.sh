#!/bin/bash

if [ ! -x configure ]; then
    autoreconf -i
fi

CXXFLAGS=
if [ `uname` = 'Darwin' ] ; then
    CXXFLAGS="-Wno-deprecated-declarations"
fi

if [ ! -f Makefile ]; then
    if [ "$1" = "debug" ]; then
        CXXFLAGS+=' -g -O0'
        shift
    elif [ "$1" = "coverage" ]; then
        CXXFLAGS+=' -g -O0 -fprofile-arcs -ftest-coverage'
        shift
    elif [ "$1" = "valgrind" ]; then
        CXXFLAGS+=' -g -O0'
        VALGRINDFLAGS=--enable-valgrind=yes
        shift
    else
        ASSERTFLAGS=--disable-assert
    fi
    if [[ -n "$CXXFLAGS" ]] ; then
        export CXXFLAGS
    fi
    which pg_config >/dev/null || POSTGRESFLAGS=--without-postgresql
    ./configure --disable-shared $ASSERTFLAGS $POSTGRESFLAGS $VALGRINDFLAGS "$@"
fi
if [ $(uname) = 'Linux' ]; then
    j=$(grep processor /proc/cpuinfo | wc -l)
elif [ $(uname) = 'Darwin' ]; then
    j=$(sysctl -n hw.ncpu)
else
    j=2
fi
make check -j$j
