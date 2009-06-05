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
ARCH := $(shell dpkg --print-architecture 2>/dev/null)
ifndef ARCH
    ARCH := $(shell uname -m)
endif
DEBIANVER := $(shell cat /etc/debian_version 2>/dev/null)
ifeq ($(DEBIANVER), 3.1)
    PLATFORM := $(PLATFORM)-sarge
endif
ifeq ($(DEBIANVER), 4.0)
    PLATFORM := $(PLATFORM)-etch
endif
ifeq ($(DEBIANVER), 5.0)
    PLATFORM := $(PLATFORM)-lenny
endif

PLATFORMDIR := $(PLATFORM)/$(ARCH)

ifeq ($(PLATFORM), Darwin)
    IOMANAGER := kqueue
    UNDERSCORE := _underscore
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
            RLFLAGS += -G2
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

# add all the subdirs to the include path
INC_FLAGS := -Imordor

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

CC 		:= gcc-3.4
CXX		:= gcc-3.4
RLCODEGEN	:= $(shell which rlcodegen)
RAGEL   	:= ragel

LIBS := -lboost_thread-mt

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
ifeq ($(RLCODEGEN),)
	$(Q)$(RAGEL) $(RLFLAGS) -o $@ $<
else
	$(Q)$(RAGEL) $< | $(RLCODEGEN) $(RLFLAGS) -o $@
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

all: cat fibers simpleclient wget

.PHONY: cat
cat: $(OBJDIR)/bin/examples/cat

$(OBJDIR)/bin/examples/cat: $(OBJDIR)/mordor/common/examples/cat.o $(OBJDIR)/lib/libmordor.a
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

$(OBJDIR)/lib/libmordor.a:					\
	$(OBJDIR)/mordor/common/exception.o			\
	$(OBJDIR)/mordor/common/fiber.o				\
	$(OBJDIR)/mordor/common/fiber_$(ARCH)$(UNDERSCORE).o	\
	$(OBJDIR)/mordor/common/http/chunked.o			\
	$(OBJDIR)/mordor/common/http/client.o			\
	$(OBJDIR)/mordor/common/http/connection.o		\
	$(OBJDIR)/mordor/common/http/http.o			\
	$(OBJDIR)/mordor/common/http/parser.o			\
	$(OBJDIR)/mordor/common/iomanager_$(IOMANAGER).o	\
	$(OBJDIR)/mordor/common/ragel.o				\
	$(OBJDIR)/mordor/common/scheduler.o			\
	$(OBJDIR)/mordor/common/semaphore.o			\
	$(OBJDIR)/mordor/common/socket.o			\
	$(OBJDIR)/mordor/common/streams/buffer.o		\
	$(OBJDIR)/mordor/common/streams/buffered.o		\
        $(OBJDIR)/mordor/common/streams/fd.o			\
	$(OBJDIR)/mordor/common/streams/file.o			\
	$(OBJDIR)/mordor/common/streams/limited.o		\
	$(OBJDIR)/mordor/common/streams/null.o			\
	$(OBJDIR)/mordor/common/streams/socket.o		\
	$(OBJDIR)/mordor/common/streams/std.o			\
	$(OBJDIR)/mordor/common/streams/stream.o		\
	$(OBJDIR)/mordor/common/streams/transfer.o		\
	$(OBJDIR)/mordor/common/uri.o
ifeq ($(Q),@)
	@echo ar $@
endif
	$(Q)mkdir -p $(@D)
	$(Q)$(AR) r $@ $(filter %.o,$?)
