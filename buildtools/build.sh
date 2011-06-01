#!/bin/sh

if [ ! -x configure ]; then
    autoreconf -i
fi
if [ ! -f Makefile ]; then
    if [ "$1" = "debug" ]; then
        export CXXFLAGS='-g -O0'
    elif [ "$1" = "coverage" ]; then
        export CXXFLAGS='-g -O0 -fprofile-arcs -ftest-coverage'
    elif [ "$1" = "valgrind" ]; then
        export CXXFLAGS='-g -O0'
        VALGRINDFLAGS=--enable-valgrind=yes
    else
        ASSERTFLAGS=--disable-assert
    fi
    which pg_config >/dev/null || POSTGRESFLAGS=--without-postgresql
    ./configure --disable-shared $ASSERTFLAGS $POSTGRESFLAGS $VALGRINDFLAGS
fi
if [ $(uname) = 'Linux' ]; then
    j=$(grep processor /proc/cpuinfo | wc -l)
elif [ $(uname) = 'Darwin' ]; then
    j=$(sysctl -n hw.ncpu)
else
    j=2
fi
make check -j$j
