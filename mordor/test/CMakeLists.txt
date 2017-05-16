#This library offers mordor unit test framework
#Note: for new test projects boost unit test framework is recommended insted

set(SRCS
antxmllistener.cpp
antxmllistener.h
compoundlistener.cpp
compoundlistener.h
stdoutlistener.cpp
stdoutlistener.h
test.cpp
test.h
)

include_directories(AFTER
    ../..
)

add_library(mordortests STATIC ${SRCS})
