#!/bin/sh

mkdir -p test-results
rm -rf test-results/TEST-*.xml
export TEST_ANTXML_DIRECTORY=test-results/ 

#cmake builds generate binaries out-of-source
if [ -e build/run_tests ] ; then
    build/run_tests $*
else
    mordor/tests/run_tests $*
fi
