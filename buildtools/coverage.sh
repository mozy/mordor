#!/bin/sh

mkdir -p test-results
gcovr -r . -e mordor/uri.cpp -e mordor/xml/xml_parser.cpp -e mordor/json.cpp -e mordor/http/http_parser.cpp -e 'mordor/test.*' -x > test-results/coverage.xml
