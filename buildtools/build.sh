#!/bin/bash

#Trigger builds on linux or mac
#By default autoconf and xcode projects are used.

set -e -u
 
CONFIG=release
USE_CMAKE=0

while [[ $# -gt 0 ]] ; do
    key="$1"
    case "$1" in
         release|debug|coverage|valgrind)
            CONFIG=$1
            ;;
         cmake)
            USE_CMAKE=1
            ;;
         --)
            #Arguments after -- are forwarded to configure 
            shift
            break
            ;;
        *)
            echo "Unknown option $1"
            ;;
esac
shift
done
 
if [ "${USE_CMAKE}" = "1" ] ; then
    . buildtools/build-cmake.sh $CONFIG
elif [ `uname` = 'Darwin' ] ; then
    #To be phased out when building osx exclusively with cmake
    . buildtools/build-xcode.sh $CONFIG
else
    #Linux autotools base build
    if [ ! -x configure ]; then
        autoreconf -i
    fi

    CXXFLAGS=
    VALGRINDFLAGS=
    ASSERTFLAGS=
    POSTGRESFLAGS=

    if [ ! -f Makefile ]; then
        if [ "${CONFIG}" = "debug" ]; then
            CXXFLAGS+=' -g -O0'
        elif [ "${CONFIG}" = "coverage" ]; then
            CXXFLAGS+=' -g -O0 -fprofile-arcs -ftest-coverage'
        elif [ "${CONFIG}" = "valgrind" ]; then
            CXXFLAGS+=' -g -O0'
            VALGRINDFLAGS=--enable-valgrind=yes
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
    else
        j=2
    fi
    make check -j$j
fi

