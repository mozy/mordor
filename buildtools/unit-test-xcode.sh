#!/bin/sh

mkdir -p test-results
rm -rf test-results/TEST-*.xml
CONFIG=DEBUG
TEST_ANTXML_DIRECTORY=test-results/ build/$CONFIG/run_tests
