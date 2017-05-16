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
    #in build-cmake.sh or add it here)
    #For backward compatibilty we could continue to compile "in source" for
    #linux by generating directly in the source tree instead of creating a binary
    #directory.  But getting consistency will help higher level projects and scripts be
    #cross platform more easily

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
        #centos may need krb5 also when static linking openssl
        target_link_libraries(${targetname} Threads::Threads rt dl)
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


