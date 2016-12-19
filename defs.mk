# This file contains definitions that are common to almost all of our makefiles.
# All makefiles should include this file at the top.

# These are used in debian packaging and .pc file distribution
PACKAGE_NAME=dlep
PACKAGE_VERSION=1.10

# These are used to make decisions later
PLATFORM=$(shell python -mplatform)
OS=$(shell uname)
ifneq (,$(shell ls /etc/debian_version 2>/dev/null))
    DEBIAN=yes
else
    DEBIAN=no
endif

# compiler selection and options
CC = g++
#CC = clang-3.5
#CC = clang
CDEBUGFLAGS = -ggdb3
CFLAGS = $(CDEBUGFLAGS) -pipe -fPIC -Wall -Werror -std=c++11

# Enabling _GLIBCXX_DEBUG should be safe within the Dlep build, but if
# enabled, any external code that uses the Dlep library will also have
# to be compiled with _GLIBCXX_DEBUG else mysterious crashes will
# occur due to structure size/layout differences in the client vs. the
# library.
#CFLAGS += -D_GLIBCXX_DEBUG

# If on Ubuntu 12.04, set CC to gcc 4.7 (assumed to be installed)
# to get C++11 support.  Also, define USE_BOOST_THREADS because we
# get compilation errors with standard threads.
ifneq (,$(findstring Ubuntu-12.04,$(PLATFORM)))
    ifeq ($(CC),g++)
        # this overrides setting CC on the make command line
        override CC = g++-4.7
    endif
    CFLAGS += -DUSE_BOOST_THREADS
else
    ifeq ($(OS),Darwin)
        CC = clang
    endif
endif


# Boost
ifeq ($(OS),Darwin)
    BOOST_INC = -I/opt/local/include
    # paper over warnings caused by Boost
    CFLAGS += -Wno-unused-local-typedef -Wno-redeclared-class-member
else
    BOOST_INC = -I/usr/include/boost -I/usr/local/include/boost
endif

# We need to define some preprocessor symbols to get more than the
# default maximum of 20 boost::variant types.
#
# ALL CODE THAT USES THE DLEP LIBRARY MUST DEFINE THESE PREPROCESSOR SYMBOLS.
#
# The safest way to define them is on the compiler command line to
# make sure they're defined from the very start, the approach taken
# here.  You can try putting them in a header or directly in your .cpp
# file, but this can fail if boost headers are included before the
# header that defines these symbols.
#
# Use `pkg-config --cflags libdlep` in your build to avoid hardcoding
# these defines.
BOOST_INCREASE_VARIANT_TYPES = -DBOOST_MPL_CFG_NO_PREPROCESSED_HEADERS \
                               -DBOOST_MPL_LIMIT_LIST_SIZE=30 \
                               -DBOOST_MPL_LIMIT_VECTOR_SIZE=30
CFLAGS += $(BOOST_INCREASE_VARIANT_TYPES)
ifeq ($(OS),Darwin)
    BOOST_LIB = -lboost_system-mt -lboost_thread-mt
else
    BOOST_LIB = -lboost_system -lboost_thread
endif


# pthread linker flags
ifeq ($(CC),clang)
    PTHREAD_LFLAGS=-lpthread
else
    PTHREAD_LFLAGS=-pthread -lpthread
endif


# XML2
XML2_LIB    = $(shell xml2-config --libs)
XML2_CFLAGS = $(shell xml2-config --cflags)


# Google Protocol Buffer
PBCFLAGS = $(shell pkg-config --cflags protobuf)
PBLFLAGS = $(shell pkg-config --libs protobuf)


# Installation directories

# set INSTALL_ROOT and INSTALL_LIB
ifeq ($(OS),Darwin)
    INSTALL_ROOT=/opt/local
    INSTALL_LIB=$(DESTDIR)$(INSTALL_ROOT)/lib/dlep
else
    # If we're on a Debian-based system, install under /usr
    # instead of /usr/local.
    ifeq ($(DEBIAN),yes)
        INSTALL_ROOT=/usr
    else
        INSTALL_ROOT=/usr/local
    endif
    INSTALL_LIB=$(DESTDIR)$(INSTALL_ROOT)/lib
endif

# set INSTALL_CONFIG
ifeq ($(DEBIAN),yes)
    INSTALL_CONFIG=$(DESTDIR)/etc/dlep
else
    INSTALL_CONFIG=$(DESTDIR)$(INSTALL_ROOT)/etc/dlep
endif

INSTALL_BIN=$(DESTDIR)$(INSTALL_ROOT)/bin
INSTALL_HEADER=$(DESTDIR)$(INSTALL_ROOT)/include/dlep
INSTALL_DOC=$(DESTDIR)$(INSTALL_ROOT)/share/dlep
INSTALL_PKGCONFIG=$(INSTALL_LIB)/pkgconfig

# Shared/dynamic lib options
ifeq ($(OS), Darwin)
    SHLIB_FLAGS = -dynamiclib
    # no ldconfig on Mac; make it a no-op
    LDCONFIG=/usr/bin/true
else
    SHLIB_FLAGS = -shared -Wl,-soname,$(DLEP_SONAME)
    LDCONFIG=ldconfig
endif


# Dependencies
# Follows this closely:
# http://make.mad-scientist.net/papers/advanced-auto-dependency-generation/
DEPDIR=deps
$(shell mkdir -p $(DEPDIR) >/dev/null)
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td
CFLAGS += $(DEPFLAGS)
POSTCOMPILE = mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d
# see epilog.mk for the last few pieces of the depend support
