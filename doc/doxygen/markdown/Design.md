Design Notes {#designnotes}
============

The core of our implementation is the %Dlep service library.  It is a
shared library that speaks DLEP on behalf of its user (the program
that calls it), sometimes referred to as the client.  When initialized
(LLDLEP::DlepInit), the %Dlep service library spawns its own threads
to manage the protocol interactions, and returns to the caller.
Subsequently, the client and the %Dlep service library communicate
with each other via interface objects that they traded at
initialization time.  The service library calls methods on the
client-supplied LLDLEP::DlepClient object to ask/tell it information.
The client calls methods on LLDLEP::DlepService to steer the actions
of the %Dlep service library, usually by causing it to send DLEP
messages to its peers.

All symbols in the client/service interface are in namespace LLDLEP.
DlepInit.h is the main header file that clients should include.

The design provides extensive configurability of the protocol,
enabling interoperation with a wide range of DLEP implemenations.  See
@ref protoconfig for details.  Adding a new metric can usually be done
by simply editing the protocol configuration XML file, though if the
metric uses a new value type, it will have to be added to class
LLDLEP::DataItem.

The implementation uses the [Boost](http://www.boost.org) libraries,
especially:
- [boost/thread](http://www.boost.org/doc/libs/1_48_0/doc/html/thread.html)
for multithreading (Ubuntu 12.04 only; other platforms use std::thread)
- [boost/asio](http://www.boost.org/doc/libs/1_48_0/doc/html/boost_asio.html)
for network I/O
- [boost/shared_ptr](http://www.boost.org/doc/libs/1_48_0/libs/smart_ptr/shared_ptr.htm)
for memory management (refcounted pointers)

See also the section on @ref impl_limits.

Destination Advertisement {#dest_advert}
=========================

The Destination Advertisement protocol causes %Dlep to send periodic
destination advertisements to its modem neighbors, and to use received
advertisements to perform destination translation when sending
destination up/down/update signals to its router peer.  Destination
Advertisement only affects the behavior of %Dlep when configured as a
modem.  If Destination Advertisement is disabled, none of the above
happens, and destinations are passed through unchanged to routers.

### Motivation

The DLEP Destination Up, Destination Down, and Destination Update
signals sent from the DLEP modem to the DLEP router require a MAC
address to identify the destination.  An important question is this:
do destination MAC addresses belong to modems or routers?

\image html modem-router-mac.png "Choice of MAC address between two modem-router pairs."

When a link comes up between Modem 1 and Modem 2, does the Destination
Up signal sent to Router 2 contain MAC-R1 or MAC-M1?

The modem's native over-the-air (OTA) protocol is outside the scope of
DLEP.  It may or may not provide any information about the router MAC
addresses.  In many cases, modems will only learn about MAC addresses
(or their equivalent) of other modems, so the natural answer to the
above question in this case is MAC-M1.  However, the routing protocols
on the routers often need to know the identity (MAC address) of the
other routers.  I.e., their preferred answer to the above question is
MAC-R1.  The presence of two intervening modems with their own MAC
addresses is of no interest to the routers.  Thus there is often a
disconnect between the information that is available from the modem
and the information needed by the router that DLEP does not address.

To fill this gap, our DLEP implementation can use another protocol
called Destination Advertisement, running in parallel with DLEP,
to convey the router MAC addresses to other routers.

### Operation

Destination Advertisement is not a DLEP extension.  It sends IP (UDP)
multicast packets OTA between modems to allow each modem to advertise
the destination(s) (routers) to which it provides connectivity.

\image html modem-using-dest-advert.png "Destination Up when Destination Advertisement is used."

Now when a link comes up between Modem 1 and Modem 2, Modem 2 looks up
the Destination Advertisement that it received from Modem 1 and sends
a Destination Up signal to Router 2 specifying the destination as
MAC-R1, the remote router.  In other words, the modem's MAC address is
translated to its router's MAC address using the information in the
Destination Advertisement.  Modem MAC addresses never appear in
Destination Up/Down signals to routers when Destination Advertisement
is enabled.

The following diagram shows the sequence of events leading to a
DLEP modem sending a Destination Up signal to a router.

\image html dest-advert-sequence.png "Message Flow for Destination Advertisement."

1.  The DLEP modems and their respective routers declare DLEP %Peer
Up.  The modems determine the MAC address of their peer router by
inspecting their local ARP cache.  Since the DLEP session uses TCP/IP,
the MAC address of the remote peer should be available by the time the
session is up.  The router's MAC address will go in the modem's
Destination Advertisement.

2.  The modems establish an OTA link.  Some radio protocols do not have
an 802.3-style 6-byte MAC address, so we use the term RF ID to refer to
the radio's unique identifier.

3.  The modems report the link up to the DLEP modem by calling
LLDLEP::DlepService::destination_up, giving the RF ID of the remote modem.

4.  DLEP Modem 1 prepares to send a Destination Up signal to its peer
router.  (For simplicity we only describe the events for Router/Modem
1, but parallel events take place for Router/Modem 2.)  DLEP Modem 1
consults a table of received Destination Advertisements, but does not
find an advertisement from RF_ID 0x0002, so it adds a pending entry to
the table to note that it is waiting for one.

5.  Modem 1's periodic timer for sending Destination Advertisements
fires.  It sends a Destination Advertisement containing its RF ID and
the MAC address of its peer router.  The RF ID is determined via
configuration, and the router's MAC address was determined in step 1.
This UDP multicast message goes OTA and is received by DLEP Modem 2.

6.  Modem 2 replaces the pending entry in its Destination
Advertisement table from RF ID 0x0001 (see step 4) with the real
entry it just received.

7.  Now that Modem 2 knows how to map RF ID 0x0001 to a router
destination, it sends the Destination Up signal to Router 2, giving
Router 1's MAC address.

If the Destination Advertisement protocol was not enabled in this
example, when the modems called LLDLEP::DlepService::destination_up
with the remote modem's RF ID (step 3), that ID would have been
immediately relayed as a destination MAC address in a Destination Up
signal to their respective routers.

### Destination Advertisement Message

The entire Destination Advertisement protocol consists of one message
that is sent and received via UDP multicast by the %Dlep library when
the protocol is enabled.  The message is transmitted periodically
according to a configured interval.  The
[message](../../../destadvert.proto), specified using Google
Protocol Buffers, has the following fields:

- the interval (seconds) between sending messages
- a sequence number that increments for each message sent
- the number of seconds that the sending %Dlep has been up
- the RF_ID of the local modem
- a list of destinations, containing at least the router peer's MAC address

#### Transmit Logic for the Destination Advertisement Message

A periodic timer triggers %Dlep to send a Destination Advertisement.
The only field that requires more than trivial effort to fill in is
the list of destinations.  If %Dlep has a router peer, then the MAC
address of that router is added as a destination.  If there are other
known destinations, i.e., additional nodes that are behind the router,
they could be added here, though this is not currently implemented.
%Dlep then multicasts the message on the modem's OTA interface,
even if it contains no destinations.

#### Receive Logic for the Destination Advertisement Message

When %Dlep receives a Destination Advertisement, it updates the
Destination Advertisement table.  This table maps RF_IDs to
entries.  An entry contains:
- state, Up or Down, reflecting what the DlepClient has declared
  for this RF_ID
- timestamp, the time that the entry was created or last updated
- the contents of the Destination Advertisement, which can be a
  placeholder
- metrics associated with this RF_ID, which can be null

When the state is Up and the Destination Advertisement is a
placeholder, this indicates that the DlepClient has declared the RF_ID
to be up, but we do not (yet) have a Destination Advertisement from
the RF_ID.  This is referred to in the diagram above as the "pending"
state.

##### Nonexistent entry

If there is not already an entry in the table for the RF_ID in the
received Destination Advertisement, then one is added, setting
timestamp = current time, state = Down, and metrics = null.

##### Existing entry

If there is already an entry in the table for the RF_ID in the
received Destination Advertisement, and its state is Up, then a diff
is performed between the list of destinations in the existing entry
and the newly received entry.  The diff results in two sets:

- the set of added destinations, i.e., destinations that were in the
new advertisement but not in the existing one
- the set of deleted destinations, i.e., destinations that were in the
existing advertisement but not in the new one

For each destination in the set of added destinations, %Dlep is
informed that the destination is up, supplying the metrics that are
associated with the entry.  If the existing entry is a placeholder
(discussed further below), its list of destinations will be empty, so
the above logic will naturally lead to all of the destinations in the
new advertisement being declared up.

Likewise, for each destination in the set of deleted destinations,
%Dlep is informed that the destination is down.

Regardless of the state (Up or Down) of the existing entry, the new
Destination Advertisement replaces the existing one in the table, the
entry's timestamp is set to the current time, and the state and
metrics are preserved from the previous entry.

#### Destination Up

When a destination is declared up via the LLDLEP::DlepService
interface, the MAC address is used as an RF_ID to look up the entry in
the Destination Advertisement %Table.  If the entry does not exist, one
is created with state = Up, timestamp = current time, a placehholder
Destination Advertisement, and metrics = metrics supplied in the call
to LLDLEP::DlepService::destination_up.

If the entry exists:
- The metrics supplied in the call to LLDLEP::DlepService::destination_up
  are stored in the entry.
- If the entry's state is already Up, then no further action is taken.
- Otherwise, its state is Down.  For each destination
  in the entry's Destination Advertisement, %Dlep is informed that the
  destination is up, supplying the metrics that are associated with the
  entry.  The entry's state is then set to Up.

#### Destination Update

When a destination is updated via the LLDLEP::DlepService interface,
the MAC address is used as an RF_ID to look up the entry in the
Destination Advertisement %Table.  If the entry does not exist, no
further action is taken.

If the entry exists but is in state Down, the updated metrics are
stored in the entry, and no further action is taken.

Otherwise, the entry exists and is in state Up.  The updated metrics
are stored in the entry.  For each destination in the entry's
Destination Advertisement, %Dlep is informed that the destination's
metrics are updated.

#### Destination Down

When a destination is declared down via the LLDLEP::DlepService
interface, the MAC address is used as an RF_ID to look up the entry in
the Destination Advertisement %Table.  If the entry does not exist, or
it exists but its state is already Down, then no action is taken.

Otherwise, the entry exists and is in state Up.  For each destination
in the entry's Destination Advertisement, %Dlep is informed that the
destination is down.  The entry's state is then set to Down.

#### Purge Timer

Periodically, approximately once a second, a timer fires that triggers
examination of all of the destination advertisements for possible removal.

For each entry in the  Destination Advertisement %Table:

- If the entry is Up and the Destination Advertisement is a
  placeholder (i.e., "pending"), then if the entry is older than a
  configured time interval, the entry is removed.  This happens when
  the client declares a destination to be up
  (LLDLEP::DlepService::destination_up) but the corresponding
  Destination Advertisement does not arrive in time.

- Otherwise, if the entry's Destination Advertisement is older than a
  configured multiple of the advertisement's refresh time, then for
  each destination in the Destination Advertisement, a Destination Down
  signal is sent to the router peer.  Then the entry's Destination
  Advertisement is removed.

The "older than" comparison uses (current time - entry's timestamp)
to determine the entry's age.

### Configuration

All configuration parameters start with the string "destination-advert".
See the Configuration section in the [User Guide](@ref userguide) for details.
