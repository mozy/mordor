# CMake utilities to help configuring mozy projects
# If this gets too large we can split it into more .cmake files

#Macro to enable precompiled headers on windows (in mozy typically called pch.h and pch.cpp)
#Note, this also forces the precompiled header as included in each .cpp
#Note: really only intended for .cpp files but seems harmless for SourceVars to also include headers

macro(add_msvc_precompiled_header PrecompiledHeader PrecompiledSource SourcesVar)
    IF(MSVC)
        get_filename_component(PrecompiledBasename ${PrecompiledHeader} NAME_WE)
        set(PrecompiledBinary "${CMAKE_CURRENT_BINARY_DIR}/${PrecompiledBasename}.pch")
        set(Sources ${${SourcesVar}})

        set_source_files_properties(${PrecompiledSource}
                                    PROPERTIES COMPILE_FLAGS "/Yc\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\""
                                                OBJECT_OUTPUTS "${PrecompiledBinary}")
        set_source_files_properties(${Sources}
                                    PROPERTIES COMPILE_FLAGS "/Yu\"${PrecompiledHeader}\" /FI\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\""
                                                OBJECT_DEPENDS "${PrecompiledBinary}")
        # Add precompiled header to SourcesVar
        list(APPEND ${SourcesVar} ${PrecompiledSource})
    endif()

    #todo: on linux there were precompiled header instructions in the Makefile.am
endmacro(add_msvc_precompiled_header)

#Create a custom build step that will call ragel on the provided src_rl file.
#The output .cpp file will be appended to the variable name passed in outputlist.
function(ragelmaker src_rl outputlist includedir)
    get_filename_component(src_file ${src_rl} NAME_WE)

    set(rl_out ${CMAKE_CURRENT_BINARY_DIR}/${src_file}.cpp)

    #adding to the list inside a function takes special care, we cannot use list(APPEND...)
    #because the results are local scope only
    set(${outputlist} ${${outputlist}} ${rl_out} PARENT_SCOPE)

    #Warning: The " -S -M -l -C -T0  --error-format=msvc" are added to match existing window invocation
    #we might want something different for mac and linux
    add_custom_command(
        OUTPUT ${rl_out}
        COMMAND CD ${includedir}
        COMMAND ${RAGEL_BIN} ${CMAKE_CURRENT_SOURCE_DIR}/${src_rl} -o ${rl_out} -I ${includedir} -l -C -T0  --error-format=msvc
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${src_rl}
        )
    set_source_files_properties(${rl_out} PROPERTIES GENERATED TRUE)
endfunction(ragelmaker)


