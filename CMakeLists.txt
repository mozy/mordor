#Main entry for the mordor cmake projects
#cmake would be invoked with the specific generator to create Windows/OSX/Linux output
#and generation also decides 32 versus 64 bit for windows case

#Minimum could probably be earlier, e.g. try with default cmake on our oldest linux distro
cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

#
# Define options to customize the build
# Caller can override default at generation by using "-D" argument or using cmake-gui.
# The values are cached, so to change after first generation a clean cache is required.
#

#The examples are optional - this can be disabled when using cmake-gui or passing argument
#on the command lines
option(BUILD_EXAMPLES "Build mordor example programs" ON)
option(BUILD_MORDOR_TESTS "Build mordor tests" ON)

option(BUILD_COVERAGE "Build for code coverage" OFF)

#
# Mac SDK
# This must be set as cache variable before project set.  It is ignored by other platforms
#

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.7" CACHE STRING "Minimum OS X deployment version")

#This determines name of the solution file etc.
#Warning until it is specified many variables are not available, such as MSVC
project(mordor)


include(cmake/utils.cmake)
include(cmake/configureThirdParty.cmake)

setStandardDefines()

setStandardConfigs()

#
# Configuration of the output location
#


getThirdPartyRoot()
message(STATUS "Thirdparty THIRDPARTY_LIB_ROOT = ${THIRDPARTY_LIB_ROOT}")

configureOutput()
message(STATUS "Output     CMAKE_BINARY_DIR = ${CMAKE_BINARY_DIR}")

#
# Global 3rd party configuration
# These functions append to the global include directory list
# and generate variables for use a link time
#


configure_openssl("1.0.1e")

#List all boost libraries that are needed (except for the header-only ones which require no linking)
#This will result in a populated Boost_LIBRARIES variable
set(boost_libs filesystem regex program_options date_time thread)
configure_boost("1.61.0" boost_libs)

config_lzma()
config_zlib()
config_ragel()

#configure header file autoconfig.h on Linux
configure_headers(${CMAKE_CURRENT_SOURCE_DIR})

#Note: not calling config_protobuf() here because that is only needed by one library

setStandardCompilerSettings()

#recursive into the source directory
add_subdirectory(mordor)
