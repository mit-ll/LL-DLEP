The MIT Lincoln Laboratory DLEP (LL-DLEP) is an implementation of the
Dynamic Link Exchange Protocol, an IETF standards-track protocol
(https://datatracker.ietf.org/doc/rfc8175/) that specifies a
radio-to-router interface.  It is an IP network protocol that runs
between a radio/modem and a router over a direct (single hop)
connection. Its primary purpose is to enable the modem to tell the
router about destinations (other computers/networks) that the modem
can reach, and to provide metrics associated with those
destinations. The router can then make routing decisions based on that
information.

This README describes how to build and run the Dlep implementation
developed at MIT Lincoln Laboratory.  See RELEASENOTES.md for what has
changed in this release.

This release was was developed and tested on:
- Ubuntu Linux version 14.04 LTS
- Ubuntu Linux version 16.04 LTS
- CentOS 7
- CentOS 6
- Mac OS X (10.12.6)

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
- doxygen
- graphviz
- cmake

Fedora/Red Hat (EL7) package names (for EL6, see below):

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
- cmake
- rpm-build


MacPorts port names:

- protobuf3-cpp
- pkgconfig
- boost
- libxml2
- doxygen
- graphviz
- cmake

**********************************************************

Red Hat/CentOS 6 specific notes:

The default GCC and Boost on EL6 (GCC 4.4.7 and Boost 1.41) are not new enough to build DLEP. However, EL6 includes a parallel install updated version of Boost (v1.48). The Software Collections project contains a group of packages under the "DevToolSet" umbrella, including newer gcc toolchains. To build DLEP on EL6, use the following steps (tested on CentOS 6.10):

```
# Add the Software Collections repository
sudo yum install centos-release-scl
# Install the standard DLEP dependencies
sudo yum install cmake libxml2-devel readline-devel protobuf-devel doxygen graphviz rpm-build
# Install EL6 Boost 1.48 and Software Collections GCC8
sudo yum install boost148-devel devtoolset-8-toolchain
# From the build directory in dlep source code directory, run the following cmake command
CC=/opt/rh/devtoolset-8/root/usr/bin/gcc CXX=/opt/rh/devtoolset-8/root/usr/bin/g++ cmake -DBOOST_INCLUDEDIR=/usr/include/boost148/ -DBOOST_LIBRARYDIR=/usr/lib64/boost148/ -DWARNINGS_AS_ERRORS=OFF ..
# Build DLEP
make
```

**********************************************************
BUILD

We will refer to the directory containing the dlep source as dlep-top.
Do the following (shell commands are prefixed with $):

    $ cd dlep-top/build
    $ cmake ..
    $ make
    $ sudo make install

Some useful build variations are described below.

By default, the DLEP build treats all compiler warnings as errors
(-Werror compiler flag). If you are experiencing problems compiling on
your system due to this setting, you can disable it with:

    $ cmake -DWARNINGS_AS_ERRORS=OFF ..

To build with debugging symbols, replace the cmake line above with:

    $ cmake -DCMAKE_BUILD_TYPE=Debug ..

To build with the GNU C++ Lbrary debugging (error checking) enabled
(see https://gcc.gnu.org/onlinedocs/libstdc++/manual/debug_mode.html),
replace the cmake line above with:

    $ cmake -DCMAKE_CXX_FLAGS="-D_GLIBCXX_DEBUG" ..

To build with the clang compiler, replace the cmake line above with:

    $ CC=clang CXX=clang++ cmake  ..

To build a package for the type of system you're building on:

    $ cmake -DPACKAGE=on ..
    $ make package

To see the build output on the terminal as it is produced:

    $ make VERBOSE=1

**********************************************************
TEST

To run the unit tests after building:

    $ cd dlep-top/build
    $ make test

If any tests fail, you can get more information with:

    $ CTEST_OUTPUT_ON_FAILURE=1 make test

To exercise the example DLEP client, you will need two different
machines on the same network, one to run the DLEP modem and one for
the DLEP router.  Rather than using physical machines, we suggest
initially creating Linux containers with CORE (Common Open Research
Emulator, http://www.nrl.navy.mil/itd/ncs/products/core).  In CORE,
create a 2-node topology using router nodes, and connect them to an
ethernet switch.  Open a terminal window to each node.  Then:

In window 1 (node with IP 10.0.0.1), run the modem side:

    $ cd dlep-top
    $ ./Dlep config-file config/modem.xml

Dlep will drop into a command line interface (CLI).  You can type help
to get a summary of available commands.  Detailed logging will go to
dlep-modem.log, which comes from the config-file.

In window 2 (node with IP 10.0.0.2), run the router side:

    $ cd dlep-top
    $ ./Dlep config-file config/router.xml

As with the modem, Dlep will drop into a CLI.  Detailed logging 
will go to dlep-modem.log.  You should see a "Peer up" message in
both windows indicating that the Dlep session initialization
successfully completed.

**********************************************************
MORE INFORMATION

Refer to the User Guide doc/doxygen/markdown/UserGuide.md, or the
doxygen-generated version at
file:///dlep-top/build/doc/doxygen/html/index.html .
The doxygen files are also installed in an OS-dependent location,
e.g., /usr/local/share/dlep/html/index.html.
