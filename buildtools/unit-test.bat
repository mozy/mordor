@SETLOCAL

MKDIR test-results
DEL /F /Q test-results\TEST-*.xml
@SET TEST_ANTXML_DIRECTORY=test-results\
packages\tests.exe
