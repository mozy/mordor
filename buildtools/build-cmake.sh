#!/bin/bash

#Build mordor using cmake.  By default release build is generated and compiled
#Working directory should be the root of the git repro.
#Output goes into "build" directory
#
#usage:
#buildtools/build-cmake.sh [debug|coverage]

set -e


#code reused from tethys kmipclient
getThirdpartyRootFromXCode() {
    cmd=`/usr/libexec/PlistBuddy -c "Print :IDEApplicationwideBuildSettings:THIRDPARTY_LIB_ROOT" ~/Library/Preferences/com.apple.dt.Xcode.plist`
    read -a ARRAY <<< $cmd

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

COVERAGE_SETTING=OFF
CONFIG=Release

while [ $# -gt 0 ]; do
    case "$1" in
        debug)
            CONFIG=Debug
            ;;
        release)
            CONFIG=Release
            ;;
        coverage)
            CONFIG=Debug
            COVERAGE_SETTING=ON
            ;;
        --)
            shift
            break
            ;;
    esac
    shift
done

parallel_flag=

if [ $(uname) = 'Darwin' ]; then
    if [ -z ${THIRDPARTY_LIB_ROOT+x} ]; then
        echo "THIRDPARTY_LIB_ROOT is not set, try to get it from xcode settings"
        export THIRDPARTY_LIB_ROOT=$(getThirdpartyRootFromXCode)
    fi

    #A consistent version of Cmake is part of thirdparty-mac so we don't have to install separately on each slave
    CMAKE_EXE=${THIRDPARTY_LIB_ROOT}/tools/cmake-3.7.1-Darwin-x86_64/CMake.app/Contents/bin/cmake

    echo "THIRDPARTY_LIB_ROOT: ${THIRDPARTY_LIB_ROOT:?this must be set}"

    GENERATOR=Xcode

    #Debug or release is a build time decision on Mac
    CONFIG_GEN_ARG=
    BUILDTIME_CONFIG_ARG="--config ${CONFIG}"

    #OSX be default is very verbose
    VERBOSE_CMAKE_ARG=
else
    : ${THIRDPARTY_LINUX=~/thirdparty-linux}
	
    CMAKE_EXE=${THIRDPARTY_LINUX:?this must be set}/tools/cmake-3.7.1-Linux-x86_64/bin/cmake

    GENERATOR="Unix Makefiles"

    #With makefiles the debug or release choice is make at generation time
    #while on OSX and Windows the project fies are multi-config making it a build time
    #decision
    CONFIG_GEN_ARG="-DCMAKE_BUILD_TYPE=${CONFIG}"
    BUILDTIME_CONFIG_ARG=

    #By default the output is not verbose. Specifying this will show the compiler and
    #linking flags which is helpful for debugging build issues
    VERBOSE_CMAKE_ARG="-DCMAKE_RULE_MESSAGES:BOOL=OFF -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON"

    parallel_flag=-j$(grep processor /proc/cpuinfo | wc -l)
fi

mkdir -p build
pushd build

#Removing cache forces a full generation.  If this slows down build
#it can be made optional or just let devs flush workspace when doing big changes.
rm -f CMakeCache.txt


echo Generating ${GENERATOR} project files using cmake
${CMAKE_EXE} -G "${GENERATOR}" \
   -DBUILD_COVERAGE=${COVERAGE_SETTING} \
   -DBUILD_EXAMPLES=ON \
   ${CONFIG_GEN_ARG} \
   ${VERBOSE_CMAKE_ARG} \
   ..

popd

#Equivalent on linux is to run "cd build ; make"
${CMAKE_EXE} --build build ${BUILDTIME_CONFIG_ARG} -- ${parallel_flag} "$@"

