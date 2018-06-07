MIT Lincoln Laboratory DLEP (LL-DLEP) Implementation

Release 2.1  July 23, 2018
==================================

Unit tests for low-level serialization are added, and defects exposed
by the new tests are fixed.

Many warnings from gcc -Wall, cppcheck, clang-tidy are fixed.

Code for an obsolete credit window extension is removed.

Release 1.10
==================================

Initial public release of the MIT LL implementation of the Dynamic
Link Exchange Protocol (DLEP).  

LL-DLEP features the following:

- A flexible library-based C++11 implementation of the protocol enabling
  clean integration with a wide variety of modem and router devices.

- An example DLEP client (library user) that provides interactive
  access to the protocol via a command-line interface.  The example
  client can be used as a surrogate for either a router or modem
  device.

- Extensive configurability.  The implementation supports multiple
  versions (drafts) of the DLEP protocol by using an XML-based
  protocol configuration file that captures the main protocol
  characteristics that vary across drafts. This simplifies
  interoperation with other DLEP implementations and makes updating to
  new DLEP drafts easier.  A configuration files is provided for draft
  24.  The configuration system also enables convenient integration of
  DLEP extensions.

- Support for running DLEP over either IPv4 or IPv6.

- Partial support for the Credit Windowing extension.

- A supplemental Destination Advertisement protocol that runs
  over-the-air between DLEP modems and enables exchange of additional
  information about destinations that is typically needed.

- Documentation covering the library API, a user guide, and design
  considerations.

- Unit and system test cases.
