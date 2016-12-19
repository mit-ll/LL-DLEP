# Dynamic Link Exchange Protocol (DLEP) tests
#
# Copyright (C) 2015, 2016 Massachusetts Institute of Technology

include ../defs.mk

INCLUDE = -I.. $(BOOST_INC) $(XML2_CFLAGS)
DLEP_LIB = ../libdlep.a
ifeq ($(OS),Darwin)
    BOOST_LIB += -lboost_unit_test_framework-mt -lboost_chrono-mt
else
    BOOST_LIB += -lboost_unit_test_framework -lboost_chrono
endif
LIB = $(BOOST_LIB) $(DLEP_LIB) $(XML2_LIB) $(PBLFLAGS) $(PTHREAD_LFLAGS) -lstdc++
CFLAGS += -DBOOST_TEST_DYN_LINK

#TEST_SRC = $(wildcard *.cpp)
TEST_SRC = dataitems.cpp messages.cpp lib_interface.cpp dest_advert.cpp LogFile.cpp TestClientImpl.cpp
TEST_OBJ = $(TEST_SRC:%.cpp=%.o)

all: lib_tests

lib_tests: $(TEST_OBJ) $(DLEP_LIB)
	$(CC) $(CDEBUGFLAGS) -o lib_tests $(TEST_OBJ) $(DLEP_SO) $(LIB) -Wfatal-errors

%.o: %.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c -o $@ $<
	$(POSTCOMPILE)

install:

clean:
	$(RM) $(TEST_OBJ) lib_tests

include ../epilog.mk
