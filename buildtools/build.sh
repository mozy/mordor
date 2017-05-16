#!/bin/bash

getThirdpartyRootFromXCode() {
    cmd=`/usr/libexec/PlistBuddy -c "Print :IDEApplicationwideBuildSettings:THIRDPARTY_LIB_ROOT" ~/Library/Preferences/com.apple.dt.Xcode.plist`
    read -a ARRAY <<< $cmd
    # echo ${#ARRAY[@]}

    returnPath=
    for strValue in ${ARRAY[@]}
    do
        if  [ $strValue = 'Array' ] || [ $strValue = '{' ] || [ $strValue = '}' ]; then
            continue
        fi

        returnPath=$strValue
        break
    done

    echo $returnPath
}

if [ $(uname) = 'Darwin' ]; then
    if [ -z "$THIRDPARTY_LIB_ROOT" ]; then
        echo "THIRDPARTY_LIB_ROOT is not set, try to get it from xcode settings"
        export THIRDPARTY_LIB_ROOT=$(getThirdpartyRootFromXCode)
    fi

    GENERATOR=Xcode
else
    GENERATOR=Linux
fi

echo "THIRDPARTY_LIB_ROOT: $THIRDPARTY_LIB_ROOT"

if [ `uname` = 'Darwin' ] ; then
	CMAKE_EXE=${THIRDPARTY_LIB_ROOT}/tools/cmake-3.7.1-Darwin-x86_64/CMake.app/Contents/bin/cmake
	
	${CMAKE_EXE} --help
	
	TARGETS="mordor mordorprotobuf mordortest run_tests"
	ARCH=${arch:-"x86_64"}
	CONFIG=Debug

	if [[ $ARCH == "amd64" ]] ; then
		ARCH=x86_64
	fi

	for t in $TARGETS
	do
		echo "build xcodebuild -target $t -configuration $CONFIG -arch $ARCH ..."
		xcodebuild -target $t -configuration $CONFIG -arch $ARCH
	done
else
	if [ ! -x configure ]; then
		autoreconf -i
	fi

	CXXFLAGS=

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
	else
		j=2
	fi
	make check -j$j
fi

