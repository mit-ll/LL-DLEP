The MIT Lincoln Laboratory DLEP (LL-DLEP) is an implementation of the
Dynamic Link Exchange Protocol, an IETF standards-track protocol
(https://tools.ietf.org/wg/manet/draft-ietf-manet-dlep/) that
specifies a radio-to-router interface.  It is an IP network protocol
that runs between a radio/modem and a router over a direct (single
hop) connection. Its primary purpose is to enable the modem to tell
the router about destinations (other computers/networks) that the
modem can reach, and to provide metrics associated with those
destinations. The router can then make routing decisions based on that
information.

This README describes how to build and run the Dlep implementation
developed at MIT Lincoln Laboratory.  See RELEASENOTES.md for what has
changed in this release.

This release was was developed and tested on:
- Ubuntu Linux version 14.04 LTS
- Ubuntu 12.04.5/gcc 4.7
- CentOS 7.0
- Mac OS X (10.10.5)

**********************************************************
INSTALL OPEN SOURCE PACKAGES

Install these packages before building anything.  Ubuntu/Debian
package names:

- libboost-thread-dev (version 48 or higher)
- libboost-system-dev (version should match thread-dev)
- libboost-test-dev   (version should match thread-dev)
- libxml2
- libxml2-dev
- libxml2-utils (for xmllint)
- libreadline6-dev
- libprotobuf-dev
- protobuf-compiler
- devscripts (if you want to build a debian package)
- doxygen
- graphviz

If you are building on a platform with a gcc version earlier than 4.7
(notably, Ubuntu 12.04), you must install gcc-4.7 or higher.  The
implementation uses some C++11 features that are not supported in
earlier versions of gcc.  On Ubuntu 12.04, do:

$ sudo apt-get install gcc-4.7 g++-4.7

If the install fails because the packages are not found, you may
have to register another package repository and retry the install:

$ sudo add-apt-repository ppa:ubuntu-toolchain-r/test
$ sudo apt-get update
$ sudo apt-get install gcc-4.7 g++-4.7


Fedora/RedHat package names:

- boost  
- boost-devel
- xml2  
- libxml2-devel
- readline-devel
- gcc-c++
- clang
- python
- protobuf 
- protobuf-devel
- doxygen
- graphviz 

MacPorts port names:

- protobuf-cpp
- pkgconfig
- boost
- libxml2
- doxygen
- graphviz


**********************************************************
BUILD

We will refer to the directory containing the dlep source as <dlep-top>.
Do the following (shell commands are prefixed with $):

$ cd <dlep-top>

Look through <dlep-top>/defs.mk to see if there is anything you want
to change.  To build on the systems mentioned at the top of this README,
you should not have to change anything.

$ sudo make install

optional (recommended):
$ make doxygen

You can view the doxygen-generated documentation by pointing
your browser at file:///<dlep-top>/doc/doxygen/html/index.html .

**********************************************************
TEST RUN

You will need two different machines on the same network, one to
run the DLEP modem and one for the DLEP router.  Rather than
using physical machines, we suggest initially creating
Linux containers with CORE (Common Open Research Emulator,
http://www.nrl.navy.mil/itd/ncs/products/core).  In CORE,
create a 2-node topology using router nodes, and connect
them to an ethernet switch.  Open a terminal window to each node.
Then:

In window 1 (node with IP 10.0.0.1), run the modem side:
$ cd <dlep-top>
$ ./Dlep config-file config/modem.xml

Dlep will drop into a command line interface (CLI).  You can type help
to get a summary of available commands.  Detailed logging will go to
dlep-modem.log, which comes from the config-file.

In window 2 (node with IP 10.0.0.2), run the router side:
$ cd <dlep-top>
$ ./Dlep config-file config/router.xml

As with the modem, Dlep will drop into a CLI.  Detailed logging 
go to dlep-modem.log.  You should see a "Peer up" message in
both windows indicating that the Dlep session initialization
successfully completed.

To run the tests:
$ cd <dlep-top>/tests
$ ./lib_tests --report_level=detailed --log_level=all

For more information on running Dlep, refer to the User Guide
doc/doxygen/markdown/UserGuide.md, or the doxygen-generated
version at file:///<dlep-top>/doc/doxygen/html/index.html .
