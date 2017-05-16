#Main entry for the mordor cmake projects
#cmake would be invoked with the specific generator to create Windows/OSX/Linux output
#and generation also decides 32 versus 64 bit for windows case

#Minimum could probably be earlier, e.g. try with default cmake on our oldest linux distro
cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

#This determines name of the solution file etc.
#Warning until it is specified many variables are not available, such as MSVC
project(mordor)

include(cmake/utils.cmake)
include(cmake/configureThirdParty.cmake)

#Set C++ 11
set(CMAKE_CXX_STANDARD 11)

#By default 4 targets are defined but we only use Debug and Release
if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES Debug Release)
    set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING
        "Reset the configurations to what we need"
        FORCE)
endif()

message(STATUS "CMAKE_BINARY_DIR = ${CMAKE_BINARY_DIR}")

if(MSVC)
    #By default the .lib and .exe files are output in Debug\Release subdirectories
    #of each source directory.  But pointing them to the root (which is setup by build.bat)
    #we will get all the .libs and .exe created in x64\Debug, x64\Release, Win32\Debug, Win32\Release
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

    #Note we may also want to tweak the output location for other platforms

    #For consistency we should try to get all platforms with same env variable name and structure
    set(THIRDPARTY_LIB_ROOT $ENV{WINCLIENTLIB})

    #Use / slash because some \ can get confused with escape character
    string(REPLACE "\\" "/" THIRDPARTY_LIB_ROOT ${THIRDPARTY_LIB_ROOT})

elseif(CMAKE_HOST_APPLE)

    #Similar to WAF behavior but letting cmake add Debug/Release
    #e.g. ...kmipclient/build/lib/Debug
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

    #Note: see kmipclient/build.sh which extract THIRDPARTY_LIB_ROOT variable out of xcode
    #and puts it into an env variable if not already set
    set(THIRDPARTY_LIB_ROOT $ENV{THIRDPARTY_LIB_ROOT})
elseif(LINUX)
    set(THIRDPARTY_LIB_ROOT $ENV{THIRDPARTY_LIB_ROOT})
endif()


#
# Openssl configuration (not currently used by windows)
#


configure_openssl("1.0.1e")

#
# Boost configuration
#

#List all boost libraries that are needed (except for the header-only ones which require no linking)
#This will result in a populated Boost_LIBRARIES variable
set(boost_libs filesystem regex program_options date_time thread)
configure_boost("1.61.0" boost_libs)

config_lzma()
config_zlib()
config_ragel()

#
# Global visual studio settings
# Note: settings that are specific to a library or exe will be set directly on the target instead
# of globally
#

#REVIEW: WAF also set -DPROJECT_ROOT_PATH
if(MSVC)
    #WIN32 _WINDOWS and NDEBUG or _DEBUG are added automatically
    add_definitions(-D_SCL_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS -DUNICODE -D_UNICODE -D_WIN32)
elseif(LINUX)
    add_definitions(-DLINUX)
endif()


#By default cmake puts pretty reasonable defaults for CMAKE_CXX_FLAGS
#but add the profiling flags to match
#REVIEW: WAF had also added a /FS flag to windows CMAKE_CXX_FLAGS_DEBUG
if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -wd4345 -wd4503")
else()
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fprofile-arcs -ftest-coverage")

    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} --coverage")
endif()


add_subdirectory(mordor)
