ifndef SRCDIR
    SRCDIR=.
endif

VPATH=$(SRCDIR)

# make sure the 'all' target is listed first
all:

# determine build type
ifdef NDEBUG
    BUILDTYPE := nondebug
    ifndef OPT
    	OPT := 3
    endif
else
    ifdef GCOV
        BUILDTYPE := gcov
    else
        BUILDTYPE := debug
    endif
endif

ifdef INSURE
	INSURE_CC := insure
endif

PLATFORM := $(shell uname -o 2>/dev/null)
ifneq ($(PLATFORM), Cygwin)
    PLATFORM := $(shell uname)
endif

DPKG := $(shell which dpkg 2>/dev/null)
ifdef DPKG
    ARCH := $(shell dpkg --print-architecture 2>/dev/null)
endif
ifneq ($(PLATFORM), Darwin)
    ifndef ARCH
        ARCH := $(shell uname -m)
    endif
endif
DEBIANVER := $(shell cat /etc/debian_version 2>/dev/null)
UBUNTUCODENAME := $(shell sed -n 's/DISTRIB_CODENAME\=\(.*\)/\1/p' /etc/lsb_release 2>/dev/null)
ifdef UBUNTUCODENAME
    PLATFORM := $(PLATFORM)-$(UBUNTUCODENAME)
else
    ifeq ($(DEBIANVER), 3.1)
        PLATFORM := $(PLATFORM)-sarge
    endif
    ifeq ($(DEBIANVER), 4.0)
        PLATFORM := $(PLATFORM)-etch
    endif
    ifeq ($(DEBIANVER), 5.0)
        PLATFORM := $(PLATFORM)-lenny
    endif
    ifeq ($(DEBIANVER), testing)
        PLATFORM := $(PLATFORM)-squeeze
    endif
endif

PLATFORMDIR := $(PLATFORM)/$(ARCH)

ifeq ($(PLATFORM), Darwin)
    DARWIN := 1
    IOMANAGER := kqueue
    UNDERSCORE := _underscore
    GCC_ARCH := $(shell file -L `which gcc` | grep x86_64 -o | uniq)
    ifndef GCC_ARCH
        GCC_ARCH := i386
    endif
    ifndef ARCH
        ARCH := $(GCC_ARCH)
    endif
    ifeq ($(ARCH), x86_64)
        ARCH := amd64
    endif
    ifeq ($(ARCH), amd64)
        ifneq ($(GCC_ARCH), x86_64)
            MACH_TARGET := -arch x86_64
        endif
    endif
    ifeq ($(ARCH), i386)
        ifneq ($(GCC_ARCH), i386)
            MACH_TARGET := -arch i386
        endif
    endif
endif
ifeq ($(PLATFORM), FreeBSD)
    IOMANAGER := kqueue
endif
ifeq ($(shell uname), Linux)
    IOMANAGER := epoll
endif


# set optimization level (disable for gcov builds)
# example: 'make OPT=2' will add -O2 to the compilation options
ifdef GCOV
    OPT_FLAGS :=
    GCOV_FLAGS += -fprofile-arcs -ftest-coverage
else
    ifdef INSURE
		OPT_FLAGS :=
    else
        ifdef OPT
            OPT_FLAGS += -O$(OPT)
        else
            OPT_FLAGS += -O0
        endif
    endif
endif

ifdef ENABLE_VALGRIND
	DBG_FLAGS += -DVALGRIND
endif

ifdef ENABLE_STACKTRACE
	DBG_FLAGS += -DENABLE_STACKTRACE -rdynamic
endif

# example: 'make NDEBUG=1' will prevent -g (add debugging symbols) to the
# compilation options and not define DEBUG
DBG_FLAGS += -g
ifndef NDEBUG
    DBG_FLAGS += -DDEBUG -fno-inline
endif

ifdef GPROF
	DBG_FLAGS += -pg
endif

# add current dir to include dir
INC_FLAGS := -I$(SRCDIR)

# run with 'make V=1' for verbose make output
ifeq ($(V),1)
    Q :=
else
    Q := @
endif

BIT64FLAGS = -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE

# Compiler options for c++ go here
CXXFLAGS += -Wall -Werror -MD $(OPT_FLAGS) $(DBG_FLAGS) $(INC_FLAGS) $(BIT64FLAGS) $(GCOV_FLAGS) $(MACH_TARGET)
CFLAGS += -Wall -MD $(OPT_FLAGS) $(DBG_FLAGS) $(INC_FLAGS) $(BIT64FLAGS) $(GCOV_FLAGS) $(MACH_TARGET)

RLCODEGEN	:= $(shell which rlcodegen rlgen-cd 2>/dev/null)
RAGEL   	:= ragel

RAGEL_MAJOR	:= $(shell ragel -v | sed -n 's/.*\([0-9]\)\.[0-9].*/\1/p')

ifeq ($(RAGEL_MAJOR), 6)
    RLCODEGEN :=
endif

LIBS := -lboost_thread -lboost_regex -lboost_date_time -lssl -lcrypto -lz

ifeq ($(PLATFORM), FreeBSD)
    LIBS += -lexecinfo
endif

# compile and link a binary.  this *must* be defined using = and not :=
# because it uses target variables
COMPLINK = $(Q)$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(filter %.o %.a, $^) $(CXXLDFLAGS) $(LDFLAGS) $(LIBS) -o $@

# Eliminate default suffix rules
.SUFFIXES:

# Delete target files if there is an error
.DELETE_ON_ERROR:

# Don't delete intermediate files
.SECONDARY:

#
# Default build rules
#

# cpp rules
%.o: %.cpp
ifeq ($(Q),@)
	@echo c++ $<
endif
	$(Q)mkdir -p $(@D)
	$(Q)$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

%.o: %.c
ifeq ($(Q),@)
	@echo c++ $<
endif
	$(Q)mkdir -p $(@D)
	$(Q)$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

%.cpp: %.rl
ifeq ($(Q),@)
	@echo ragel $<
endif
	$(Q)mkdir -p $(@D)
ifndef RLCODEGEN
	$(Q)$(RAGEL) $(RAGEL_FLAGS) $(RLCODEGEN_FLAGS) -o $@ $<
else
	$(Q)$(RAGEL) $(RAGEL_FLAGS) $< | $(RLCODEGEN) $(RLCODEGEN_FLAGS) -o $@
endif

%.o: %.s
ifeq ($(Q),@)
	@echo as $<
endif
	$(Q)mkdir -p $(@D)
	$(Q)$(AS) $(ASFLAGS) $(MACH_TARGET) -o $@ $<

%.gch: %
ifeq ($(Q),@)
	@echo c++ $<
endif
	$(Q)mkdir -p $(@D)
	$(Q)$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

#
# Include the dependency information generated during the previous compile
# phase (note that since the .d is generated during the compile, editing
# the .cpp will cause it to be regenerated for the next build.)
#
DEPS := $(shell find $(CURDIR) -name '*.d')
-include $(DEPS)

ALLBINS = mordor/examples/cat						\
	mordor/examples/echoserver					\
	mordor/examples/simpleclient					\
	mordor/examples/tunnel						\
	mordor/examples/udpstats					\
	mordor/examples/wget						\
	mordor/tests/run_tests


# clean current build
#
.PHONY: clean
clean:
	$(Q)find . -name '*.gcno' | xargs rm -f
	$(Q)find . -name '*.gcda' | xargs rm -f
	$(Q)find . -name '*.d' | xargs rm -f
	$(Q)find . -name '*.o' | xargs rm -f
	$(Q)find . -name '*.a' | xargs rm -f
	$(Q)rm -f mordor/pch.h.gch
	$(Q)rm -f mordor/uri.cpp mordor/http/parser.cpp mordor/xml/parser.cpp
	$(Q)rm -f $(ALLBINS) mordor/tests/run_tests
	$(Q)rm -rf lcov* html

all: $(ALLBINS)

.PHONY: check
check: all
	$(Q)mordor/tests/run_tests

.PHONY: lcov
lcov:
	$(Q)lcov -d $(CURDIR) -z >/dev/null 2>&1
	$(Q)rm -rf lcov*
	$(Q)$(MAKE) -f $(SRCDIR)/Makefile --no-print-directory $(MAKEFLAGS) check GCOV=1
	$(Q)lcov -b $(SRCDIR) -d $(CURDIR) -c -i -o lcov_base.info >/dev/null
	$(Q)lcov -b $(SRCDIR) -d $(CURDIR) -c -o lcov.info >/dev/null 2>&1
	$(Q)lcov -a lcov.info -a lcov_base.info -o lcov.info >/dev/null
	$(Q)lcov -r lcov.info '/usr/*' -o lcov.info >/dev/null 2>&1
	$(Q)lcov -r lcov.info mordor/uri.cpp -o lcov.info >/dev/null 2>&1
	$(Q)lcov -r lcov.info mordor/http/parser.cpp -o lcov.info >/dev/null 2>&1
	$(Q)lcov -r lcov.info mordor/xml/parser.cpp -o lcov.info >/dev/null 2>&1
	$(Q)lcov -r lcov.info 'mordor/examples/*' -o lcov.info >/dev/null 2>&1
	$(Q)lcov -r lcov.info './*' -o lcov.info >/dev/null 2>&1
	$(Q)mkdir -p lcov && cd lcov && genhtml ../lcov.info >/dev/null && tar -czf lcov.tgz *

TESTDATA := $(patsubst $(SRCDIR)/%,$(CURDIR)/%,$(wildcard $(SRCDIR)/mordor/tests/data/*))

TESTSOBJECTS := $(patsubst $(SRCDIR)/%.cpp,%.o,$(wildcard $(SRCDIR)/mordor/tests/*.cpp))

$(TESTDATA): $(CURDIR)/%: $(SRCDIR)/%
	$(Q)mkdir -p $(@D)
	$(Q)cp -f $< $@

$(TESTSOBJECTS): mordor/pch.h.gch

mordor/tests/run_tests:							\
	$(TESTSOBJECTS)							\
	mordor/test/libmordortest.a					\
        mordor/libmordor.a						\
	$(TESTDATA)							\
	mordor/pch.h.gch
ifeq ($(Q),@)
	@echo ld $@
endif
	$(COMPLINK)


EXAMPLEOBJECTS :=							\
	mordor/examples/cat.o						\
	mordor/examples/echoserver.o					\
	mordor/examples/simpleclient.o					\
	mordor/examples/tunnel.o					\
	mordor/examples/udpstats.o					\
	mordor/examples/wget.o

$(EXAMPLEOBJECTS): mordor/pch.h.gch

mordor/examples/cat: mordor/examples/cat.o				\
	mordor/libmordor.a
ifeq ($(Q),@)
	@echo ld $@
endif
	$(COMPLINK)

mordor/examples/echoserver: mordor/examples/echoserver.o		\
	mordor/libmordor.a
ifeq ($(Q),@)
	@echo ld $@
endif
	$(COMPLINK)

mordor/examples/simpleclient: mordor/examples/simpleclient.o		\
	mordor/libmordor.a
ifeq ($(Q),@)
	@echo ld $@ 
endif
	$(COMPLINK)

mordor/examples/tunnel: mordor/examples/tunnel.o			\
	mordor/libmordor.a
ifeq ($(Q),@)
	@echo ld $@ 
endif
	$(COMPLINK)

mordor/examples/udpstats: mordor/examples/udpstats.o			\
	mordor/libmordor.a
ifeq ($(Q),@)
	@echo ld $@
endif
	$(COMPLINK)

mordor/examples/wget: mordor/examples/wget.o				\
	mordor/libmordor.a
ifeq ($(Q),@)
	@echo ld $@
endif
	$(COMPLINK)

mordor/http/http_parser.o: mordor/http/parser.cpp
ifeq ($(Q),@)
	@echo c++ $<
endif
	$(Q)mkdir -p $(@D)
	$(Q)$(CXX) $(CXXFLAGS) -c -o $@ $<
   
mordor/streams/socket_stream.o: mordor/streams/socket.cpp
ifeq ($(Q),@)
	@echo c++ $<
endif
	$(Q)mkdir -p $(@D)
	$(Q)$(CXX) $(CXXFLAGS) -c -o $@ $<
   
mordor/xml/xml_parser.o: mordor/xml/parser.cpp
ifeq ($(Q),@)
	@echo c++ $<
endif
	$(Q)mkdir -p $(@D)
	$(Q)$(CXX) $(CXXFLAGS) -c -o $@ $<


LIBMORDOROBJECTS := 							\
	mordor/config.o							\
	mordor/exception.o						\
	mordor/fiber.o							\
	mordor/fiber_$(ARCH)$(UNDERSCORE).o				\
	mordor/http/auth.o						\
	mordor/http/basic.o						\
	mordor/http/broker.o						\
	mordor/http/chunked.o						\
	mordor/http/client.o						\
	mordor/http/connection.o					\
	mordor/http/digest.o						\
	mordor/http/http.o						\
	mordor/http/mockserver.o					\
	mordor/http/multipart.o						\
	mordor/http/oauth.o						\
	mordor/http/http_parser.o					\
	mordor/http/server.o						\
	mordor/iomanager_$(IOMANAGER).o					\
	mordor/json.o							\
	mordor/log.o							\
	mordor/ragel.o							\
	mordor/scheduler.o						\
	mordor/semaphore.o						\
	mordor/sleep.o							\
	mordor/socket.o							\
	mordor/statistics.o						\
	mordor/streams/buffer.o						\
	mordor/streams/buffered.o					\
	mordor/streams/crypto.o						\
	mordor/streams/fd.o						\
	mordor/streams/file.o						\
	mordor/streams/hash.o						\
	mordor/streams/http.o						\
	mordor/streams/limited.o					\
	mordor/streams/memory.o						\
	mordor/streams/null.o						\
	mordor/streams/pipe.o						\
	mordor/streams/socket_stream.o					\
	mordor/streams/ssl.o						\
	mordor/streams/std.o						\
	mordor/streams/stream.o						\
	mordor/streams/test.o						\
	mordor/streams/throttle.o					\
	mordor/streams/transfer.o					\
	mordor/streams/zlib.o						\
	mordor/string.o							\
	mordor/timer.o							\
	mordor/uri.o							\
	mordor/xml/xml_parser.o

$(LIBMORDOROBJECTS): mordor/pch.h.gch

ARFLAGS := ruc
ifdef DARWIN
	ARFLAGS := -rucs
endif

mordor/libmordor.a: $(LIBMORDOROBJECTS)
ifeq ($(Q),@)
	@echo ar $@
endif
	$(Q)$(AR) $(ARFLAGS) $@ $(filter %.o,$?)

LIBMORDORTESTOBJECTS :=							\
 	mordor/test/test.o						\
	mordor/test/stdoutlistener.o

$(LIBMORDORTESTOBJECTS): mordor/pch.h.gch

mordor/test/libmordortest.a: $(LIBMORDORTESTOBJECTS)
ifeq ($(Q),@)
	@echo ar $@
endif
	$(Q)$(AR) $(ARFLAGS) $@ $(filter %.o,$?)
