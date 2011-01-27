#!/bin/sh

if [ ! -x configure ]; then
    autoreconf -i
fi
if [ ! -f Makefile ]; then
    if [ "$1" = "debug" ]; then
        export CXXFLAGS='-g -O0 -DDEBUG'
    elif [ "$1" = "gcov" ]; then
        export CXXFLAGS='-g -O0 -DDEBUG -fprofile-arcs -ftest-coverage'
    fi
    which pg_config >/dev/null || POSTGRESFLAGS=--without-postgresql
    ./configure --disable-shared $POSTGRESFLAGS
fi
if [ $(uname) = 'Linux' ]; then
    j=$(grep processor /proc/cpuinfo | wc -l)
elif [ $(uname) = 'Darwin' ]; then
    j=$(sysctl -n hw.ncpu)
else
    j=2
fi
make mordor/tests/run_tests -j$j && make all -j$j
