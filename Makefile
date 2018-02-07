# Dynamic Link Exchange Protocol (DLEP)
#
# Copyright (C) 2015, 2016 Massachusetts Institute of Technology

include defs.mk

DLEP_HEADERS = $(wildcard *.h)
DLEP_PUBLIC_HEADERS = DlepInit.h DlepClient.h DlepService.h DlepCommon.h \
	DataItem.h ProtocolConfig.h CreditWindow.h IdTypes.h DlepMac.h Serialize.h

INCLUDE = -I. $(BOOST_INC) -I/usr/local/include -I/opt/local/include \
	  $(XML2_CFLAGS) -I/opt/local/lib

# Google protocol buffer files
PBSRCS     := $(wildcard *.proto)
PBOBJS     := $(PBSRCS:.proto=.pb.o)
PBGENS     := $(PBSRCS:.proto=.pb.cc) $(PBSRCS:.proto=.pb.h)

.PRECIOUS: $(PBGENS)

# Objects that go in the dlep shared library
DLEP_OBJ = $(PBOBJS) DestAdvert.o DlepInit.o DlepLogger.o Dlep.o Peer.o \
           InfoBaseMgr.o PeerDiscovery.o DlepServiceImpl.o \
           DlepMac.o PeriodicMcastSendRcv.o NetUtils.o \
           ProtocolMessage.o DataItem.o ProtocolConfigImpl.o

# Objects that comprise the example client (library user)
EXAMPLE_CLIENT_OBJ = ExampleMain.o  ExampleDlepClientImpl.o Table.o

# Unadorned library name.
DLEP_LIBNAME = libdlep

# Shared object name.  Should only change when the library changes in a way
# that makes it incompatible with previous versions.
DLEP_SONAME = $(DLEP_LIBNAME).so.1

# Shared object version.  Changes whenever the shared object changes, even
# in compatible ways.
DLEP_SOVERSION = 0

# Full shared object filename.
DLEP_SO = $(DLEP_SONAME).$(DLEP_SOVERSION)

LIB = $(BOOST_LIB) $(XML2_LIB) $(PBLFLAGS) -lreadline $(PTHREAD_LFLAGS) \
      -lstdc++

# Subdirectories to run make in.  Make will run in these subdirs
# *after* it is run in this directory, enabling the subdirs to use
# just-built libraries from this directory.
SUBDIRS = tests config

# These are derived from SUBDIRS, so you shouldn't have to change them.
BUILDDIRS = $(SUBDIRS)
CLEANDIRS = $(SUBDIRS:%=clean-%)
INSTALLDIRS = $(SUBDIRS:%=install-%)

# above this line are variable settings
#----------------------------------------------------------------------
# below this line are rules

all : build-defs.h libdlep.pc Dlep Dlep-static $(BUILDDIRS)

# Make a header file that contains some of the interesting build
# definitions from defs.mk.
build-defs.h : defs.mk
	@echo '/* auto-generated file, do not edit */'  >> $@
	@printf "#define INSTALL_CONFIG \"%s\"\n" $(INSTALL_CONFIG) >> $@

# recursive build
.PHONY: $(BUILDDIRS)
$(BUILDDIRS):
	    $(MAKE) -C $@

%.o: %.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c -o $@ $<
	$(POSTCOMPILE)

%.pb.cc: %.proto
	protoc --cpp_out=. $<

%.pb.o : %.pb.cc
	$(CC) $(CFLAGS) $(PBCFLAGS) -c -o $@ $<
	$(POSTCOMPILE)

%.pc : %.pc.in
	sed -e 's:\@prefix\@:$(INSTALL_ROOT):' \
			-e 's:\@sysconfdir\@:$(INSTALL_CONFIG):' \
			-e 's:\@PACKAGE_NAME\@:$(PACKAGE_NAME):' \
			-e 's:\@PACKAGE_VERSION\@:$(PACKAGE_VERSION):' \
			$< > $@

# Build the dlep shared library.
$(DLEP_SO): $(DLEP_OBJ)
	$(CC) $(CDEBUGFLAGS) $(SHLIB_FLAGS) -o $(DLEP_SO) $(DLEP_OBJ) $(LIB)
	ln -s -f $(DLEP_SO)  $(DLEP_LIBNAME).so

$(DLEP_LIBNAME).a: $(DLEP_OBJ)
	$(AR) -crv $@ $(DLEP_OBJ)

Dlep:	$(DLEP_SO) $(EXAMPLE_CLIENT_OBJ)
	$(CC) $(CDEBUGFLAGS) -o Dlep $(EXAMPLE_CLIENT_OBJ) $(DLEP_SO) $(LIB) -Wfatal-errors

# Dlep linked with static (.a) Dlep library
Dlep-static: $(DLEP_LIBNAME).a $(EXAMPLE_CLIENT_OBJ)
	$(CC) $(CDEBUGFLAGS) -o Dlep-static $(EXAMPLE_CLIENT_OBJ) $(DLEP_LIBNAME).a  $(LIB) -Wfatal-errors

clean: $(CLEANDIRS)
	$(RM) *.o $(DLEP_SO) $(DLEP_LIBNAME).a Dlep Dlep-static build-defs.h libdlep.pc
	$(RM) -r doc/doxygen/html
	$(RM) -rf debian/dlep
	$(RM) $(PBOBJS) $(PBGENS)

# recursive clean
.PHONY: $(CLEANDIRS)
$(CLEANDIRS):
	$(MAKE) -C $(@:clean-%=%) clean

doxygen::
	doxygen doc/doxygen.config

install: all $(INSTALLDIRS)
	install -d -v $(INSTALL_BIN) $(INSTALL_LIB) $(INSTALL_HEADER)
	install Dlep $(INSTALL_BIN)
	install -m 0644 $(DLEP_PUBLIC_HEADERS) $(INSTALL_HEADER)
	install $(DLEP_LIBNAME).a $(INSTALL_LIB)
	install $(DLEP_SO) $(INSTALL_LIB)
	install -d $(INSTALL_PKGCONFIG)
	install -m 644 libdlep.pc $(INSTALL_PKGCONFIG)
	ln -s -f $(DLEP_SO) $(INSTALL_LIB)/$(DLEP_SONAME)
	ln -s -f $(DLEP_SO) $(INSTALL_LIB)/$(DLEP_LIBNAME).so
	install -d $(INSTALL_DOC)
	rsync --exclude='*.md5' -av doc/doxygen/* $(INSTALL_DOC)
	if test -z "$(DESTDIR)" ; then \
	    sudo $(LDCONFIG) $(INSTALL_LIB) ; \
	fi

# recursive install
.PHONY: $(INSTALLDIRS)
$(INSTALLDIRS):
	$(MAKE) -C $(@:install-%=%) install

deb: all
	debuild -i -I -us -uc

include epilog.mk
