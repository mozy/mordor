# Makefile for cmordor
#
# recursive make considered harmful
# see: http://aegis.sourceforge.net/auug97.pdf

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

PLATFORM := $(shell uname)
DPKG := $(shell which dpkg)
ifdef DPKG
    ARCH := $(shell dpkg --print-architecture 2>/dev/null)
endif
ifndef ARCH
    ARCH := $(shell uname -m)
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
    IOMANAGER := kqueue
    UNDERSCORE := _underscore
endif
ifeq ($(PLATFORM), FreeBSD)
    IOMANAGER := kqueue
endif
ifeq ($(shell uname), Linux)
    IOMANAGER := epoll
endif

# output directory for the build is prefixed with debug v.s. nondebug
ifdef GITVER
	OBJTOPDIR := obj-$(RELEASEVER)
else
	OBJTOPDIR := obj
endif

OBJDIR := $(OBJTOPDIR)/$(PLATFORMDIR)/$(BUILDTYPE)

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
            RLCODEGEN_FLAGS += -G2
        else
            OPT_FLAGS += -O
        endif
    endif
endif

ifdef ENABLE_STACKTRACE
	DBG_FLAGS += -DENABLE_STACKTRACE -rdynamic
endif

# example: 'make NDEBUG=1' will prevent -g (add debugging symbols) to the
# compilation options and not define DEBUG
ifndef NDEBUG
    DBG_FLAGS += -g
    DBG_FLAGS += -DDEBUG -fno-inline
endif

ifdef GPROF
	DBG_FLAGS += -pg
endif

# add current dir to include dir
INC_FLAGS := -I.

# run with 'make V=1' for verbose make output
ifeq ($(V),1)
    Q :=
else
    Q := @
endif

BIT64FLAGS = -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE

# Compiler options for c++ go here
#
# remove the no-unused-variable.  this was added when moving to gcc4 since
# it started complaining about our logger variables.  Same with
# no-strict-aliasing
CXXFLAGS += -Wall -Werror -Wno-unused-variable -fno-strict-aliasing -MD $(OPT_FLAGS) $(DBG_FLAGS) $(INC_FLAGS) $(BIT64FLAGS) $(GCOV_FLAGS)
CFLAGS += -Wall -Wno-unused-variable -fno-strict-aliasing -MD $(OPT_FLAGS) $(DBG_FLAGS) $(INC_FLAGS) $(BIT64FLAGS) $(GCOV_FLAGS)

RLCODEGEN	:= $(shell which rlcodegen rlgen-cd)
RAGEL   	:= ragel

RAGEL_MAJOR	:= $(shell ragel -v | sed -n 's/.*\([0-9]\)\.[0-9]\+.*/\1/p')

ifeq ($(RAGEL_MAJOR), 6)
    ifdef RLCODEGEN
        RAGEL_FLAGS += -x
    endif
endif

LIBS := -lboost_thread -lssl -lcrypto -lz

# compile and link a binary.  this *must* be defined using = and not :=
# because it uses target variables
COMPLINK = $(Q)$(CXX) $(CXXFLAGS) $(CPPFLAGS) $^ $(CXXLDFLAGS) $(LDFLAGS) $(LIBS) -o $@

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
$(OBJDIR)/%.o: %.cpp
ifeq ($(Q),@)
	@echo c++ $<
endif
	$(Q)mkdir -p $(@D)
	$(Q)$(CXX) -I $(OBJDIR)/$(dir $*) $(CXXFLAGS) -c -o $@ $<

$(OBJDIR)/%.o: %.c
ifeq ($(Q),@)
	@echo c++ $<
endif
	$(Q)mkdir -p $(@D)
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

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

$(OBJDIR)/%.o: %.s
ifeq ($(Q),@)
	@echo as $<
endif
	$(Q)mkdir -p $(@D)
	$(Q)$(AS) $(ASFLAGS) $(TARGET_MACH) -o $@ $<


#
# clean up all builds
#
#
# clean current build
#
.PHONY: clean
clean:
ifeq ($(Q),@)
	@echo rm $(OBJDIR)
endif
	$(Q)rm -rf $(OBJDIR)
	$(Q)rm -f mordor/common/uri.cpp mordor/common/http/parser.cpp

#
# Include the dependency information generated during the previous compile
# phase (note that since the .d is generated during the compile, editing
# the .cpp will cause it to be regenerated for the next build.)
#
DEPS := $(shell test -d $(OBJDIR) && find $(OBJDIR) -name "*.d")
-include $(DEPS)

all: cat echoserver fibers simpleclient wget list

.PHONY: check
check: $(OBJDIR)/mordor/common/run_tests all
	$(Q)$(OBJDIR)/mordor/common/run_tests


$(OBJDIR)/mordor/common/run_tests:						\
	$(patsubst %.cpp,$(OBJDIR)/%.o,$(wildcard mordor/common/tests/*.cpp))	\
	$(OBJDIR)/lib/libmordortest.a						\
	$(OBJDIR)/lib/libmordor.a
ifeq ($(Q),@)
	@echo ld $@
endif
	$(Q)mkdir -p $(@D)
	$(COMPLINK)

.PHONY: cat
cat: $(OBJDIR)/bin/examples/cat

$(OBJDIR)/bin/examples/cat: $(OBJDIR)/mordor/common/examples/cat.o $(OBJDIR)/lib/libmordor.a
ifeq ($(Q),@)
	@echo ld $@
endif
	$(Q)mkdir -p $(@D)
	$(COMPLINK)


.PHONY: echoserver
echoserver: $(OBJDIR)/bin/examples/echoserver

$(OBJDIR)/bin/examples/echoserver: $(OBJDIR)/mordor/common/examples/echoserver.o $(OBJDIR)/lib/libmordor.a
ifeq ($(Q),@)
	@echo ld $@
endif
	$(Q)mkdir -p $(@D)
	$(COMPLINK)

.PHONY: fibers
fibers: $(OBJDIR)/bin/examples/fibers

$(OBJDIR)/bin/examples/fibers: $(OBJDIR)/mordor/common/examples/fibers.o $(OBJDIR)/lib/libmordor.a
ifeq ($(Q),@)
	@echo ld $@
endif
	$(Q)mkdir -p $(@D)
	$(COMPLINK)

.PHONY: simpleclient
fibers: $(OBJDIR)/bin/examples/simpleclient

$(OBJDIR)/bin/examples/simpleclient: $(OBJDIR)/mordor/common/examples/simpleclient.o $(OBJDIR)/lib/libmordor.a
ifeq ($(Q),@)
	@echo ld $@ 
endif
	$(Q)mkdir -p $(@D)
	$(COMPLINK)

.PHONY: wget
wget: $(OBJDIR)/bin/examples/wget

$(OBJDIR)/bin/examples/wget: $(OBJDIR)/mordor/common/examples/wget.o $(OBJDIR)/lib/libmordor.a
ifeq ($(Q),@)
	@echo ld $@
endif
	$(Q)mkdir -p $(@D)
	$(COMPLINK)

.PHONY: list
list: $(OBJDIR)/bin/triton/list

$(OBJDIR)/bin/triton/list: $(OBJDIR)/mordor/triton/client/list_main.o $(OBJDIR)/lib/libtritonclient.a $(OBJDIR)/lib/libmordor.a
ifeq ($(Q),@)
	@echo ld $@
endif
	$(Q)mkdir -p $(@D)
	$(COMPLINK)

$(OBJDIR)/mordor/common/http/http_parser.o: mordor/common/http/parser.cpp
ifeq ($(Q),@)
	@echo c++ $<
endif
	$(Q)mkdir -p $(@D)
	$(Q)$(CXX) -I $(OBJDIR)/$(dir $*) $(CXXFLAGS) -c -o $@ $<
   
$(OBJDIR)/mordor/common/streams/socket_stream.o: mordor/common/streams/socket.cpp
ifeq ($(Q),@)
	@echo c++ $<
endif
	$(Q)mkdir -p $(@D)
	$(Q)$(CXX) -I $(OBJDIR)/$(dir $*) $(CXXFLAGS) -c -o $@ $<
   
$(OBJDIR)/mordor/common/xml/xml_parser.o: mordor/common/xml/parser.cpp
ifeq ($(Q),@)
	@echo c++ $<
endif
	$(Q)mkdir -p $(@D)
	$(Q)$(CXX) -I $(OBJDIR)/$(dir $*) $(CXXFLAGS) -c -o $@ $<


$(OBJDIR)/lib/libmordor.a:					\
	$(OBJDIR)/mordor/common/exception.o			\
	$(OBJDIR)/mordor/common/fiber.o				\
	$(OBJDIR)/mordor/common/fiber_$(ARCH)$(UNDERSCORE).o	\
	$(OBJDIR)/mordor/common/http/basic.o			\
	$(OBJDIR)/mordor/common/http/chunked.o			\
	$(OBJDIR)/mordor/common/http/client.o			\
	$(OBJDIR)/mordor/common/http/connection.o		\
	$(OBJDIR)/mordor/common/http/http.o			\
	$(OBJDIR)/mordor/common/http/multipart.o		\
	$(OBJDIR)/mordor/common/http/http_parser.o		\
	$(OBJDIR)/mordor/common/http/server.o			\
	$(OBJDIR)/mordor/common/iomanager_$(IOMANAGER).o	\
	$(OBJDIR)/mordor/common/log.o				\
	$(OBJDIR)/mordor/common/ragel.o				\
	$(OBJDIR)/mordor/common/scheduler.o			\
	$(OBJDIR)/mordor/common/semaphore.o			\
	$(OBJDIR)/mordor/common/socket.o			\
	$(OBJDIR)/mordor/common/streams/buffer.o		\
	$(OBJDIR)/mordor/common/streams/buffered.o		\
	$(OBJDIR)/mordor/common/streams/fd.o			\
	$(OBJDIR)/mordor/common/streams/file.o			\
	$(OBJDIR)/mordor/common/streams/limited.o		\
	$(OBJDIR)/mordor/common/streams/memory.o		\
	$(OBJDIR)/mordor/common/streams/null.o			\
	$(OBJDIR)/mordor/common/streams/openssl.o		\
	$(OBJDIR)/mordor/common/streams/socket_stream.o		\
	$(OBJDIR)/mordor/common/streams/ssl.o			\
	$(OBJDIR)/mordor/common/streams/std.o			\
	$(OBJDIR)/mordor/common/streams/stream.o		\
	$(OBJDIR)/mordor/common/streams/transfer.o		\
	$(OBJDIR)/mordor/common/streams/zlib.o			\
	$(OBJDIR)/mordor/common/string.o			\
	$(OBJDIR)/mordor/common/timer.o				\
	$(OBJDIR)/mordor/common/uri.o				\
	$(OBJDIR)/mordor/common/xml/xml_parser.o
ifeq ($(Q),@)
	@echo ar $@
endif
	$(Q)mkdir -p $(@D)
	$(Q)$(AR) ruc $@ $(filter %.o,$?)

$(OBJDIR)/lib/libtritonclient.a:				\
	$(OBJDIR)/mordor/triton/client/client.o			\
	$(OBJDIR)/mordor/triton/client/get.o			\
	$(OBJDIR)/mordor/triton/client/list.o			\
	$(OBJDIR)/mordor/triton/client/put.o
ifeq ($(Q),@)
	@echo ar $@
endif
	$(Q)mkdir -p $(@D)
	$(Q)$(AR) ruc $@ $(filter %.o,$?)

$(OBJDIR)/lib/libmordortest.a:					\
	$(OBJDIR)/mordor/test/test.o				\
	$(OBJDIR)/mordor/test/stdoutlistener.o
ifeq ($(Q),@)
	@echo ar $@
endif
	$(Q)mkdir -p $(@D)
	$(Q)$(AR) ruc $@ $(filter %.o,$?)
