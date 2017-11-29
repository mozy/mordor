# CMake utilities to help configuring mozy projects
# If this gets too large we can split it into more .cmake files
# These macros could be called from other projects like tethys


macro(setStandardDefines)
    #To be called once soon after the project() statement

    #Set C++ 11
    set(CMAKE_CXX_STANDARD 11)

    #By default there is no variable for linux
    if(UNIX AND NOT APPLE)
        set(LINUX TRUE)

        # Detect Linux distribution (if possible)
        execute_process(COMMAND "/usr/bin/lsb_release" "-is"
   	    TIMEOUT 4
    	    OUTPUT_VARIABLE LINUX_DISTRO
    	    ERROR_QUIET
   	    OUTPUT_STRIP_TRAILING_WHITESPACE)
        message(STATUS "Linux distro is: ${LINUX_DISTRO}")
    endif()
endmacro()


macro(setStandardConfigs)
    #By default 4 targets are defined but we only use Debug and Release
    if(CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_CONFIGURATION_TYPES Debug Release)
        set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING
            "Reset the configurations to what we need"
            FORCE)
    endif()
endmacro()

macro(getThirdPartyRoot)
    #Initialize the THIRDPARTY_LIB_ROOT cmake variable based on each platforms equivalent env variable

    if(MSVC)
        set(THIRDPARTY_LIB_ROOT $ENV{WINCLIENTLIB})

        #Use / slash because in some context '\' can get confused with escape character
        string(REPLACE "\\" "/" THIRDPARTY_LIB_ROOT ${THIRDPARTY_LIB_ROOT})
    elseif(CMAKE_HOST_APPLE)
        #Note: see build-cmake.sh which supports extraction of THIRDPARTY_LIB_ROOT variable out of xcode
        set(THIRDPARTY_LIB_ROOT $ENV{THIRDPARTY_LIB_ROOT})
    else()
        set(THIRDPARTY_LIB_ROOT $ENV{THIRDPARTY_LINUX})
    endif()
endmacro()

macro(configureOutput)

    #Although we set same settings the actual subdirectories will be
    #a bit difference as described below.

    #Windows:
    #By default the .lib and .exe files are output in Debug\Release subdirectories
    #of each source directory.
    #build32Or64.bat is responsible for setting the output directory based on the architecture
    #and here we configure cmake to put all .libs and .exe files into the same place.
    #Cmake automatically creates separate Debug and Release subdirectories
    #e.g. x64\Debug, x64\Release, Win32\Debug and Win32\Release

    #OSX:
    #build-cmake.sh will set up the binary directory, e.g. "build" and cmake
    #automatically outputs to "build/Debug" and "build/Release"
    #Set libs and executables to go directly to those directories

    #Linux:
    #By default it will compile based on the relative paths in the source tree.
    #E.g. /build/mordor/tests/run_tests and ./build/mordor/examples/echoserver
    #And because debug and release is a generate-time choice there is no Debug or Release
    #subdirectory (unless we choose force creation of Debug/Release subdirectories
    #in build-cmake.sh or add it here).
    #For backward compatibilty we could continue to compile "in source" for
    #linux by generating directly in the source tree instead of creating a binary
    #directory.  But getting consistency will help higher level projects and scripts be
    #cross platform more easily

    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
endmacro()

macro(setStandardCompilerSettings)
    #Settings that are common for all mozy client targets can be added here
    #Targets and source files may add additional non-global settings

    if(MSVC)
        #WIN32 _WINDOWS and NDEBUG or _DEBUG are added automatically
        add_definitions(-D_SCL_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS -DUNICODE -D_UNICODE -D_WIN32)
    endif()

    #Cmake fills in a lot of defaults in CMAKE_CXX_FLAG (and specific _DEBUG and _RELEASE lists)
    #but we need to add a few more
    if (MSVC)
        #Disable certain warnings
        #Only warnings that are never useful should be added here. In other cases they should be disabled
        #on a case by case basis on the settings for individual target or file
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -wd4345 -wd4503")

        # /Zi generates debug information in the .obj files.
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
        # Tell the linker to generate .pdb files for release builds (/DEBUG), but still remove unreferenced
        # functions (/OPT:REF), and merge functions with identical assembly instructions (/OPT:ICF).
        # Specifying /DEBUG automatically turns off the other (necessary) optimizations that are reenabled.
        set(CMAKE_MODULE_LINKER_FLAGS_RELEASE "${CMAKE_MODULE_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
        set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
    else()
        if(LINUX)
            #prepare for linking to pthread
            set(THREADS_PREFER_PTHREAD_FLAG ON)
            find_package(Threads REQUIRED)
        endif()

        #If code coverage option is enabled then debug build flags change
        if (BUILD_COVERAGE)
            set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fprofile-arcs -ftest-coverage")
            set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} --coverage")
        endif()

        if (CMAKE_HOST_APPLE)
            set(CMAKE_CXX_FLAGS "-stdlib=libc++ ${CMAKE_CXX_FLAGS}")
            set(CMAKE_EXE_LINKER_FLAGS "-stdlib=libc++ ${CMAKE_EXE_LINKER_FLAGS}")
        endif()

        #Note: linux build could potentially support valgrind flags
    endif()
endmacro()

macro(add_osspecific_linking targetname)
    if (CMAKE_HOST_APPLE)
        target_link_libraries(${targetname}
            "-framework CoreServices"
            "-framework CoreFoundation"
            "-framework SystemConfiguration"
            "-framework Security"
            )
    elseif(LINUX)
        target_link_libraries(${targetname} Threads::Threads rt dl z)

        #centos may need krb5 also when static linking openssl
        if(${LINUX_DISTRO} STREQUAL "CentOS")
            target_link_libraries(${targetname} krb5 k5crypto)
        endif()
    elseif(MSVC)
        set_property(TARGET ${targetname} APPEND PROPERTY LINK_FLAGS_RELEASE /DEBUG)
    endif()
endmacro()

macro(add_precompiled_header PrecompiledHeader PrecompiledSource SourcesVar)
    #Macro to enable precompiled headers (in mozy typically called pch.h and pch.cpp)
    #Note, this also forces the precompiled header as included in each .cpp
    #Note: really only intended for .cpp files but seems harmless for SourceVars to also include headers

    set(Sources ${${SourcesVar}})

    IF(MSVC)
        get_filename_component(PrecompiledBasename ${PrecompiledHeader} NAME_WE)
        set(PrecompiledBinary "${CMAKE_CURRENT_BINARY_DIR}/${PrecompiledBasename}.pch")

        set_source_files_properties(${PrecompiledSource}
                                    PROPERTIES COMPILE_FLAGS "/Yc\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\""
                                                OBJECT_OUTPUTS "${PrecompiledBinary}")
        set_source_files_properties(${Sources}
                                    PROPERTIES COMPILE_FLAGS "/Yu\"${PrecompiledHeader}\" /FI\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\""
                                                OBJECT_DEPENDS "${PrecompiledBinary}")
        # Add precompiled header to SourcesVar
        list(APPEND ${SourcesVar} ${PrecompiledSource})
    else()
        #On Xcode and Linux we should also enable precompiled headers to speed up build
        #and match existing autotools and xcode settings. But for the moment
        #just doing the minimal step of forcing pch.h to be included in each .cpp file
        set_source_files_properties(${Sources}
                                    PROPERTIES COMPILE_FLAGS "--include \"${PrecompiledHeader}\"")
    endif()
endmacro(add_precompiled_header)

function(ragelmaker src_rl outputlist includedir)
    #Create a custom build step that will call ragel on the provided src_rl file.
    #The output .cpp file will be appended to the variable name passed in outputlist.

    get_filename_component(src_file ${src_rl} NAME_WE)

    set(rl_out ${CMAKE_CURRENT_BINARY_DIR}/${src_file}.cpp)

    #adding to the list inside a function takes special care, we cannot use list(APPEND...)
    #because the results are local scope only
    set(${outputlist} ${${outputlist}} ${rl_out} PARENT_SCOPE)

    #Warning: The " -S -M -l -C -T0  --error-format=msvc" are added to match existing window invocation
    #we might want something different for mac and linux
    add_custom_command(
        OUTPUT ${rl_out}
        COMMAND cd ${includedir}
        COMMAND ${RAGEL_BIN} ${CMAKE_CURRENT_SOURCE_DIR}/${src_rl} -o ${rl_out} -I ${includedir} -l -C -T0  --error-format=msvc
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${src_rl}
        )
    set_source_files_properties(${rl_out} PROPERTIES GENERATED TRUE)
endfunction(ragelmaker)

# A wrapper around several target-specific executable building
# options.  This is useful for avoiding a bunch of global definitions
# for the various options.  It is also more convenient.
#
# Valid options:
#   NAME <name> -- (REQUIRED) the name of the executable (no suffix)
#   SRCS <source list> -- (REQUIRED) the list of sources for the executable
#   LIBS <lib list> -- (OPTIONAL) the list of libraries to link against
#   INCLUDES <include list> -- (OPTIONAL) the list of include directories for the target
#   PCH_HEADER <pch.h> -- (OPTIONAL) specify a header for precompiled headers
#   PCH_SOURCE <pch.cpp> -- (OPTIONAL) specify a source file for precompiled headers
# SRCS and LIBS can have prefixes for WIN, NOT_WIN, LINUX, NOT_LINUX, OSX, NOT_OSX.
#
# Examples:
#   add_binary(NAME foo TYPE EXE SRCS foo.cpp)
#   add_binary(NAME bar TYPE EXE SRCS bar.cpp INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/.. LIBS x11)
#   add_binary(NAME baz TYPE LIB LINKAGE STATIC SRCS baz.cpp NOT_WIN_SOURCES quux.cpp)
function(add_binary)
  cmake_parse_arguments(
    PARAMS # prefix for local variables
    ""     # No boolean arguments
    "NAME;TYPE;LINKAGE;PCH_HEADER;PCH_SOURCE"
    "SRCS;LIBS;INCLUDES;WIN_SRCS;WIN_LIBS;NOT_WIN_SRCS;NOT_WIN_LIBS;LINUX_SRCS;LINUX_LIBS;NOT_LINUX_SRCS;NOT_LINUX_LIBS;OSX_SRCS;OSX_LIBS;NOT_OSX_SRCS;NOT_OSX_LIBS" # Arguments the user will (usually optionally) provide.
    ${ARGN}
    )

  if(NOT PARAMS_NAME)
    message(FATAL_ERROR "No name provided")
  endif(NOT PARAMS_NAME)

  # It is legal to have all platform-specific sources but no general sources
  # included.  Create an empty variable for them to be added below.
  if(NOT PARAMS_SRCS)
    set(PARAMS_SRCS)
  endif()

  message("${PARAMS_NAME}: will be a binary of type ${PARAMS_TYPE}")

  # Combine all of the possible source listings into one list of sources.
  # Platform-specific:
  if(MSVC AND DEFINED PARAMS_WIN_SRCS AND PARAMS_WIN_SRCS)
    list(APPEND PARAMS_SRCS ${PARAMS_WIN_SRCS})
  elseif(LINUX AND DEFINED PARAMS_LINUX_SRCS AND PARAMS_LINUX_SRCS)
    list(APPEND PARAMS_SRCS ${PARAMS_LINUX_SRCS})
  elseif(APPLE AND DEFINED PARAMS_OSX_SRCS AND PARAMS_OSX_SRCS)
    list(APPEND PARAMS_SRCS ${PARAMS_OSX_SRCS})
  endif()
  # Inverted logic for platforms:
  if((NOT MSVC) AND (DEFINED PARAMS_NOT_WIN_SRCS))
    list(APPEND PARAMS_SRCS ${PARAMS_NOT_WIN_SRCS})
  endif()
  if((NOT LINUX) AND (DEFINED PARAMS_NOT_LINUX_SRCS))
    list(APPEND PARAMS_SRCS ${PARAMS_NOT_LINUX_SRCS})
  endif()
  if((NOT APPLE) AND (DEFINED PARAMS_NOT_OSX_SRCS))
    list(APPEND PARAMS_SRCS ${PARAMS_NOT_OSX_SRCS})
  endif()

  # Must have source files.
  if(NOT PARAMS_SRCS)
    message(FATAL_ERROR "No source files provided with ((NOT_)?(WIN_|LINUX_|OSX))?SRCS parameters")
  endif(NOT PARAMS_SRCS)

  # Optionally have precompiled header files
  if((DEFINED PARAMS_PCH_HEADER) AND (DEFINED PARAMS_PCH_SOURCE))
    add_precompiled_header(${PARAMS_PCH_HEADER} ${PARAMS_PCH_SOURCE} PARAMS_SRCS)
  endif()

  # if cmake supported calling functions by name, this would be much half as much code.
  if(DEFINED PARAMS_TYPE)
    if(${PARAMS_TYPE} STREQUAL "EXE")
      if(DEFINED PARAMS_LINKAGE)
        message(ERROR, "Linkage options not yet implemented for type EXE")
      endif()
      add_executable(${PARAMS_NAME} ${PARAMS_SRCS})
    elseif(${PARAMS_TYPE} STREQUAL "LIB")
      if(DEFINED PARAMS_LINKAGE)
        add_library(${PARAMS_NAME} ${PARAMS_LINKAGE} ${PARAMS_SRCS})
      else()
        add_library(${PARAMS_NAME} ${PARAMS_SRCS})
      endif()
    endif()
  else()
    message(FATAL_ERROR "No TYPE provided for target ${PARAMS_NAME}")
  endif()

  # Target-specific include directories
  if((DEFINED PARAMS_INCLUDES) AND PARAMS_INCLUDES)
    target_include_directories(${PARAMS_NAME} PUBLIC ${PARAMS_INCLUDES})
  endif()

  # User-specified libraries.  Note that target_link_libraries is additive, so
  # they don't need to all be included on one call
  if(DEFINED PARAMS_LIBS)
    target_link_libraries(${PARAMS_NAME} ${PARAMS_LIBS})
  endif(DEFINED PARAMS_LIBS)
  if(MSVC AND (DEFINED PARAMS_WIN_LIBS) AND PARAMS_WIN_LIBS)
    target_link_libraries(${PARAMS_NAME} ${PARAMS_WIN_LIBS})
  elseif(LINUX AND (DEFINED PARAMS_LINUX_LIBS) AND PARAMS_LINUX_LIBS)
    target_link_libraries(${PARAMS_NAME} ${PARAMS_LINUX_LIBS})
  elseif(APPLE AND (DEFINED PARAMS_OSX_LIBS) AND PARAMS_OSX_LIBS)
    target_link_libraries(${PARAMS_NAME} ${PARAMS_OSX_LIBS})
  endif()
  if((NOT MSVC) AND (DEFINED PARAMS_NOT_WIN_LIBS))
    target_link_libraries(${PARAMS_NAME} ${PARAMS_NOT_WIN_LIBS})
  endif()
  if((NOT LINUX) AND (DEFINED PARAMS_NOT_LINUX_LIBS))
    target_link_libraries(${PARAMS_NAME} ${PARAMS_NOT_LINUX_LIBS})
  endif()
  if((NOT APPLE) AND (DEFINED PARAMS_NOT_OSX_LIBS))
    target_link_libraries(${PARAMS_NAME} ${PARAMS_NOT_OSX_LIBS})
  endif()

  add_osspecific_linking(${PARAMS_NAME})
endfunction(add_binary)

function(add_static_lib)
  add_binary(${ARGN} TYPE LIB LINKAGE STATIC)
endfunction()

function(add_lib)
  add_binary(${ARGN} TYPE LIB)
endfunction()

function(add_exe)
  add_binary(${ARGN} TYPE EXE)
endfunction()

# This function will overwrite the standard predefined macro "__FILE__".
# "__FILE__" expands to the name of the current input file, but cmake
# input the absolute path of source file, any code using the macro 
# would expose sensitive information, such as MORDOR_THROW_EXCEPTION(x),
# so we'd better overwirte it with filename.
function(force_redefine_file_macro_for_sources targetname)
    get_target_property(source_files "${targetname}" SOURCES)
    foreach(sourcefile ${source_files})
        # Get source file's current list of compile definitions.
        get_property(defs SOURCE "${sourcefile}"
            PROPERTY COMPILE_DEFINITIONS)
        # Get the relative path of the source file in project directory
        get_filename_component(filepath "${sourcefile}" ABSOLUTE)
        string(REPLACE ${PROJECT_SOURCE_DIR}/ "" relpath ${filepath})
        list(APPEND defs "__FILE__=\"${relpath}\"")
        # Set the updated compile definitions on the source file.
        set_property(
            SOURCE "${sourcefile}"
            PROPERTY COMPILE_DEFINITIONS ${defs}
            )
    endforeach()
endfunction()
