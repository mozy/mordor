#Compile Mordor examples

function(set_mordor_example_linking target)
    target_link_libraries(${target} mordor ${OPENSSL_LIBRARIES} ${Boost_LIBRARIES} ${ZLIB_LIBRARIES})

    add_osspecific_linking(${target}) 
endfunction(set_mordor_example_linking)

#Syntax helper because most examples only have a single .cpp file
#whose name matches the test output
function(compile_example name)
    add_executable(${name} ${name}.cpp)
    set_mordor_example_linking(${name})
endfunction()

compile_example("cat")
compile_example("wget")
compile_example("echoserver")
compile_example("tunnel")

if (MSVC)
    compile_example("decodebacktrace")
    compile_example("simpleclient")
else()
    compile_example("udpstats")
    compile_example("zipstreaming")
    compile_example("simpleappserver")
endif()

#This example has two source files
add_executable(iombench iombench.cpp netbench.cpp)
set_mordor_example_linking(iombench)
