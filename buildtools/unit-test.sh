#!/bin/sh

mkdir -p test-results
rm -rf test-results/TEST-*.xml
TEST_ANTXML_DIRECTORY=test-results/ mordor/tests/run_tests $*
