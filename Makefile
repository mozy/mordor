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
    GCC_ARCH := $(shell file `which gcc` | grep x86_64 -o | uniq)
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

ALLBINS = mordor/common/examples/cat						\
	mordor/common/examples/echoserver					\
	mordor/common/examples/simpleclient					\
	mordor/common/examples/tunnel						\
	mordor/common/examples/wget						\
	mordor/common/tests/run_tests


# clean current build
#
.PHONY: clean
clean:
	$(Q)find . -name '*.gcno' | xargs rm -f
	$(Q)find . -name '*.gcda' | xargs rm -f
	$(Q)find . -name '*.d' | xargs rm -f
	$(Q)find . -name '*.o' | xargs rm -f
	$(Q)find . -name '*.a' | xargs rm -f
	$(Q)rm -f mordor/common/pch.h.gch
	$(Q)rm -f mordor/common/uri.cpp mordor/common/http/parser.cpp mordor/common/xml/parser.cpp
	$(Q)rm -f $(ALLBINS) mordor/common/tests/run_tests
	$(Q)rm -rf lcov*

all: $(ALLBINS)

.PHONY: check
check: all
	$(Q)mordor/common/tests/run_tests

.PHONY: lcov
lcov:
	$(Q)lcov -d $(CURDIR) -z >/dev/null 2>&1
	$(Q)rm -rf lcov*
	$(Q)$(MAKE) -f $(SRCDIR)/Makefile --no-print-directory $(MAKEFLAGS) check GCOV=1
	$(Q)lcov -b $(SRCDIR) -d $(CURDIR) -c -i -o lcov_base.info >/dev/null
	$(Q)lcov -b $(SRCDIR) -d $(CURDIR) -c -o lcov.info >/dev/null 2>&1
	$(Q)lcov -a lcov.info -a lcov_base.info -o lcov.info >/dev/null
	$(Q)lcov -r lcov.info '/usr/*' -o lcov.info >/dev/null 2>&1
	$(Q)lcov -r lcov.info mordor/common/uri.cpp -o lcov.info >/dev/null 2>&1
	$(Q)lcov -r lcov.info mordor/common/http/parser.cpp -o lcov.info >/dev/null 2>&1
	$(Q)lcov -r lcov.info mordor/common/xml/parser.cpp -o lcov.info >/dev/null 2>&1
	$(Q)lcov -r lcov.info 'mordor/common/examples/*' -o lcov.info >/dev/null 2>&1
	$(Q)lcov -r lcov.info './*' -o lcov.info >/dev/null 2>&1
	$(Q)mkdir -p lcov && cd lcov && genhtml ../lcov.info >/dev/null && tar -czf lcov.tgz *

TESTDATA_COMMON := $(patsubst $(SRCDIR)/%,$(CURDIR)/%,$(wildcard $(SRCDIR)/mordor/common/tests/data/*))

COMMONTESTSOBJECTS := $(patsubst $(SRCDIR)/%.cpp,%.o,$(wildcard $(SRCDIR)/mordor/common/tests/*.cpp))

$(TESTDATA_COMMON): $(CURDIR)/%: $(SRCDIR)/%
	$(Q)mkdir -p $(@D)
	$(Q)cp -f $< $@

$(COMMONTESTSOBJECTS): mordor/common/pch.h.gch

mordor/common/tests/run_tests:							\
	$(COMMONTESTSOBJECTS)							\
	mordor/test/libmordortest.a						\
        mordor/common/libmordor.a						\
	$(TESTDATA_COMMON)							\
	mordor/common/pch.h.gch
ifeq ($(Q),@)
	@echo ld $@
endif
	$(COMPLINK)


EXAMPLEOBJECTS :=								\
	mordor/common/examples/cat.o						\
	mordor/common/examples/echoserver.o					\
	mordor/common/examples/simpleclient.o					\
	mordor/common/examples/tunnel.o						\
	mordor/common/examples/wget.o

$(EXAMPLEOBJECTS): mordor/common/pch.h.gch

mordor/common/examples/cat: mordor/common/examples/cat.o			\
	mordor/common/libmordor.a
ifeq ($(Q),@)
	@echo ld $@
endif
	$(COMPLINK)


mordor/common/examples/echoserver: mordor/common/examples/echoserver.o		\
	mordor/common/libmordor.a
ifeq ($(Q),@)
	@echo ld $@
endif
	$(COMPLINK)

mordor/common/examples/simpleclient: mordor/common/examples/simpleclient.o	\
	mordor/common/libmordor.a
ifeq ($(Q),@)
	@echo ld $@ 
endif
	$(COMPLINK)

mordor/common/examples/tunnel: mordor/common/examples/tunnel.o			\
	mordor/common/libmordor.a
ifeq ($(Q),@)
	@echo ld $@ 
endif
	$(COMPLINK)

mordor/common/examples/wget: mordor/common/examples/wget.o			\
	mordor/common/libmordor.a
ifeq ($(Q),@)
	@echo ld $@
endif
	$(COMPLINK)

mordor/common/http/http_parser.o: mordor/common/http/parser.cpp
ifeq ($(Q),@)
	@echo c++ $<
endif
	$(Q)mkdir -p $(@D)
	$(Q)$(CXX) $(CXXFLAGS) -c -o $@ $<
   
mordor/common/streams/socket_stream.o: mordor/common/streams/socket.cpp
ifeq ($(Q),@)
	@echo c++ $<
endif
	$(Q)mkdir -p $(@D)
	$(Q)$(CXX) $(CXXFLAGS) -c -o $@ $<
   
mordor/common/xml/xml_parser.o: mordor/common/xml/parser.cpp
ifeq ($(Q),@)
	@echo c++ $<
endif
	$(Q)mkdir -p $(@D)
	$(Q)$(CXX) $(CXXFLAGS) -c -o $@ $<


LIBMORDOROBJECTS := 								\
	mordor/common/config.o							\
	mordor/common/exception.o						\
	mordor/common/fiber.o							\
	mordor/common/fiber_$(ARCH)$(UNDERSCORE).o				\
	mordor/common/http/auth.o						\
	mordor/common/http/basic.o						\
	mordor/common/http/chunked.o						\
	mordor/common/http/client.o						\
	mordor/common/http/connection.o						\
	mordor/common/http/digest.o						\
	mordor/common/http/http.o						\
	mordor/common/http/multipart.o						\
	mordor/common/http/oauth.o						\
	mordor/common/http/http_parser.o					\
	mordor/common/http/server.o						\
	mordor/common/iomanager_$(IOMANAGER).o					\
	mordor/common/log.o							\
	mordor/common/ragel.o							\
	mordor/common/scheduler.o						\
	mordor/common/semaphore.o						\
	mordor/common/sleep.o							\
	mordor/common/socket.o							\
	mordor/common/streams/buffer.o						\
	mordor/common/streams/buffered.o					\
	mordor/common/streams/crypto.o						\
	mordor/common/streams/fd.o						\
	mordor/common/streams/file.o						\
	mordor/common/streams/hash.o						\
	mordor/common/streams/limited.o						\
	mordor/common/streams/memory.o						\
	mordor/common/streams/null.o						\
	mordor/common/streams/pipe.o						\
	mordor/common/streams/socket_stream.o					\
	mordor/common/streams/ssl.o						\
	mordor/common/streams/std.o						\
	mordor/common/streams/stream.o						\
	mordor/common/streams/test.o						\
	mordor/common/streams/throttle.o					\
	mordor/common/streams/transfer.o					\
	mordor/common/streams/zlib.o						\
	mordor/common/string.o							\
	mordor/common/timer.o							\
	mordor/common/uri.o							\
	mordor/common/xml/xml_parser.o

$(LIBMORDOROBJECTS): mordor/common/pch.h.gch

ARFLAGS := ruc
ifdef DARWIN
	ARFLAGS := -rucs
endif

mordor/common/libmordor.a: $(LIBMORDOROBJECTS)
ifeq ($(Q),@)
	@echo ar $@
endif
	$(Q)$(AR) $(ARFLAGS) $@ $(filter %.o,$?)

LIBMORDORTESTOBJECTS :=								\
 	mordor/test/test.o							\
	mordor/test/stdoutlistener.o

$(LIBMORDORTESTOBJECTS): mordor/common/pch.h.gch

mordor/test/libmordortest.a: $(LIBMORDORTESTOBJECTS)
ifeq ($(Q),@)
	@echo ar $@
endif
	$(Q)$(AR) $(ARFLAGS) $@ $(filter %.o,$?)
