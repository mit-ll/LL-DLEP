User Guide  {#userguide}
==========

This document describes the operation and use of the MIT Lincoln
Laboratory DLEP implementation.  DLEP is a radio-to-router interface
realized as an IP network protocol that runs between a radio/modem and
a router over a direct (single hop) connection.  Its primary purpose
is to enable the modem to tell the router about destinations (other
computers/networks) that the modem can reach, and to provide metrics
associated with those destinations.  The router can then make routing
decisions based on that information.

We assume the reader is reasonably familiar with the [DLEP, 
RFC 8175](https://tools.ietf.org/html/rfc8175).  This
implementation supports multiple DLEP drafts by using a protocol
configuration file that captures the main protocol characteristics
that vary across drafts.  Configuration files are provided for
[draft 24](https://tools.ietf.org/html/draft-ietf-manet-dlep-24),
[draft 29](https://tools.ietf.org/html/draft-ietf-manet-dlep-29), and
[RFC 8175](https://tools.ietf.org/html/rfc8175).

When referring to specifics of our %Dlep implementation, this document
uses the word %Dlep.  When referring to the DLEP protocol in general,
it uses DLEP (all caps).

@section impl_limits Implementation Limitations
<!-- leave this comment here, between @section and -----, else section linking won't work -->
-------------

When configured as a modem, Dlep can only have one DLEP peer.

Each instance of LLDLEP::DlepService can only have one discovery interface
configured.  However, a process can create multiple instances of
the LLDLEP::DlepService, each with a different interface configured.

The requirements on received IP TTL values are not enforced.

The @ref dest_advert protocol currently only works on Linux-derived operating
systems, i.e., not Darwin.

@section running_dlep Running Dlep
<!-- leave this comment here, between @section and -----, else section linking won't work -->
-------------
Build the %Dlep program using the instructions in the @ref README.md.
The %Dlep program provides an example of how to use the
%Dlep service library, as well as being a useful test/utility program
for experimenting with DLEP.  It features a command-line interface to
drive the library that is convenient for development and testing, but
it is not meant for production use.  For a real system, you would write
a program that uses the facilities of the %Dlep library.

The %Dlep program can play the role of either peer, modem or router, in
a DLEP session.  The local-type configuration parameter, discussed below,
tells it which role to assume.

You can run %Dlep with no arguments to use all of the built-in default
configuration settings.  Any invalid argument or configuration
parameter will cause %Dlep to exit with a usage message.  For example:

    $ ./Dlep help
    extraneous argument help
    Usage: ./Dlep [parameters]
    Any of these parameters can appear either on the command line or in the config file:
    Parameter name                   Default                                 Description                                                          
    ack-probability                  100                           Probability (%) of sending required ACK signals (for testing)        
    ack-timeout                      3                             Seconds to wait for ACK signals                                      
    config-file                                                    XML config file containing parameter settings                        
    destination-advert-enable        0                             Should the modem run the Destination Advertisement protocol?         
    destination-advert-expire-count  0                             Time to keep Destination Advertisements                              
    destination-advert-hold-interval 0                             Time to wait for Destination Advertisement after destination up      
    destination-advert-iface         emane0                        Interface that the destination discovery protocol uses, rf interface 
    destination-advert-mcast-address 225.6.7.8                     address to send Destination Advertisements to                        
    destination-advert-port          33445                         UDP Port to send Destination Advertisements to                       
    destination-advert-rf-id                                       RF ID of the local modem                                             
    destination-advert-send-interval 5                             Time between sending Destination Advertisements                      
    discovery-enable                 1                             Should the router run the PeerDiscovery protocol?                    
    discovery-iface                  eth0                          Interface that the router uses for the PeerDiscovery protocol        
    discovery-interval               60                            Time between sending PeerDiscovery signals                           
    discovery-mcast-address          224.0.0.117                   address to send PeerDiscovery signals to                             
    discovery-port                   854                           UDP Port to send PeerDiscovery signals to                            
    discovery-ttl                    255                           IP TTL to use on PeerDiscovery signals                               
    heartbeat-interval               60                            Time between sending Heartbeat signals                               
    heartbeat-threshold              4                             Number of missed Heartbeats to tolerate                              
    linkchar-autoreply               1                             Automatically send reply to linkchar requests?                       
    local-type                       modem                         Which DLEP role to play, modem or router?                            
    log-file                         dlep.log                      File to write log messages to                                        
    log-level                        3                             1=most logging, 5=least                                              
    peer-flags                       0                             Flags field value of Peer Type data item                             
    peer-type                                                      Peer Type data item value                                            
    protocol-config-file             /etc/dlep/dlep-rfc-8175.xml   XML file containing DLEP protocol configuration                      
    protocol-config-schema           /etc/dlep/protocol-config.xsd XML schema file for protocol-config-file                             
    send-tries                       3                             Number of times to send a signal before giving up                    
    session-address                                                IP address that the modem listens on for session connections         
    session-iface                                                  Interface that the router uses for session connections               
    session-port                     854                           TCP port number that the modem listens on for session connections    
    session-ttl                      255                           IP TTL to use on session connections                                 

Typically you use the config-file parameter on the command line to
specify a file containing the rest of the parameters, e.g.:

    $ ./Dlep config-file config/modem.xml

However, you can freely mix parameters on the command line with
config-files.  For example, if you want to provide your own value for
peer-type, you can use:

    $ ./Dlep config-file config/modem.xml peer-type my-whizzy-modem

You can use the same idea to override parameters in a config file with
different values.  %Dlep processes its arguments left-to-right, so if
multiple settings for a parameter are encountered, the last one found
takes precedence.  The config-file parameter is somewhat special
because whenever it is encountered, the specified file is loaded, not
just the last occurrence of config-file.  This behavior allows you to
specify multiple config files, or even config files that load other
config files.

@section using_cli Using the Command Line Interface
<!-- leave this comment here, between @section and -----, else section linking won't work -->
--------------------------------

When %Dlep starts, it runs a command line interface (CLI) that you use
to interact with the %Dlep service library.  The output of the help
command looks something like this:

    modem> help
    help : print a help message
    quit : exit the program cleanly
    dest up mac-address [data-item-name data-item-value]...
        declare a destination to be up (modem) or announced (router)
        [data-item-name data-item-value] can be repeated to specify multiple data items
        data-item-names come from 'show dataitems'
    dest update mac-address [data-item-name data-item-value]...
        update attributes of an existing (up) destination
    dest down mac-address
        declare a destination to be down
    dest response mac-address status-code-name
        define response to a future destination up for mac-address
        status-code-name comes from 'show statuscodes'
    linkchar request mac-address [data-item-name data-item-value]...
        request link characteristics for a destination
    linkchar reply peer_id mac-address [data-item-name data-item-value]...
        reply with link characteristics for a destination
    peer update [data-item-name data-item-value]...
        update the local peer with data-items and send peer updates to all existing peers
    set param-name param-value
        set a config parameter value
    show [ dataitems | config | signals | modules | statuscodes | peer | dest ]
        show requested information
    show peer [ peer-id ]
        without peer-id, lists all peers
        with peer-id, prints detailed information about that peer
    show dest [ mac-address ]
        without mac-address, prints info about all destinations
        with mac-address, prints info about that destination

Here are a few notes on some of these commands.

### dest up/down/update, linkchar request/reply

The format of a mac-address for these commands is any number of
colon-separated hex bytes.  The usual case is to give six
bytes, xx:xx:xx:xx:xx:xx, but more or less are allowed.

The destination and linkchar commands also allow you to supply
additional data items that will go in the message.  The "show
dataitems" command will produce a list of the data items defined by
the protocol configuration file:

    modem> show dataitems
    Configured data items:
    ID Name                              Type        Units        Module           Flags  
    1     Status                         u8_string                core                 
    2     IPv4_Connection_Point          u8_ipv4_u16              core                 
    3     IPv6_Connection_Point          u8_ipv6_u16              core                 
    4     Peer_Type                      u8_string                core                 
    5     Heartbeat_Interval             u32         milliseconds core                 
    6     Extensions_Supported           v_extid                  core                 
    7     MAC_Address                    dlepmac                  core                 
    8     IPv4_Address                   u8_ipv4                  core                 
    9     IPv6_Address                   u8_ipv6                  core                 
    10    IPv4_Attached_Subnet           u8_ipv4_u8               core                 
    11    IPv6_Attached_Subnet           u8_ipv6_u8               core                 
    12    Maximum_Data_Rate_Receive      u64         bits/second  core          metric 
    13    Maximum_Data_Rate_Transmit     u64         bits/second  core          metric 
    14    Current_Data_Rate_Receive      u64         bits/second  core          metric 
    15    Current_Data_Rate_Transmit     u64         bits/second  core          metric 
    16    Latency                        u64         microseconds core          metric 
    17    Resources                      u8          percentage   core          metric 
    18    Relative_Link_Quality_Receive  u8          percentage   core          metric 
    19    Relative_Link_Quality_Transmit u8          percentage   core          metric 
    20    Maximum_Transmission_Unit      u16         octets       core          metric 
    65411 Latency_Range                  u64_u64     microseconds Latency Range metric 

To specify one of these data items on a destination command,
give the data item name from the list above (copy/paste recommended),
followed by a space and a value of the specified Type, e.g.:

    modem> dest up 01:02:03:04:05:06 Latency 1000

You can put as many data items as you want:

    modem> dest up 01:02:03:04:05:06 Latency 1000 Relative_Link_Quality_Receive 90

The data item type (Type column in the "show dataitems" output) is a
straight mapping of what DLEP requires for that data item.  In
general, the value in the Type field follows these rules:
- uX is an unsigned integer of X bits
- string is an arbitrary string, no whitespace allowed
- dlepmac is a MAC address as described above
- ipv4 is an IPv4 address A.B.C.D
- ipv6 is an IPv6 address (anything that boost::asio::ip::address_v6 can parse)
- blank means there is no associated value.  A data item name whose type is blank
  must be followed by the next data item name or the end of the line.
- v_X is a variable-length list of X, with list elements separated by commas,
  no embedded whitespace
- aX_Y is a fixed-length list (array) of size X of Y, with list elements
  separated by commas, no embedded whitespace
- An underscore _ marks the start of another field.  Fields must be separated
  by ; or / , with no embedded whitespace.
- sub_data_items means the data item contains a list of other data items
  enclosed in curly braces { }, e.g.,
  DataItemName { sub_data_item_1 val_1 sub_data_item_2 val_2 }

Examples values for specific types:

Type           | Example Value
---------------|-----------------------------
u8             | 128
u8_ipv4_u16    | 1;192.0.2.0;49152
u8_ipv6        | 1;fe80::20c:29ff:fe84:fcba
v_extid        | 3,4,5
dlepmac        | 01:02:03:04:05:06
a2_u64         | 254362436,4567847
sub_data_items | { Resources 50 Latency 100 }

All possible data item types are described in
LLDLEP::DataItemValueType and in the @ref proto_config_schema.

### dest up

If %Dlep is configured as a router, and the protocol configuration
defines the Destination Announce message, then the dest up command
will generate a Destination Announce message instead of a Destination
Up message.  (This is actually handled by the %Dlep service library.)
The modem peer will be expected to send a Destination Announce
Response message.

### set param-name param-value

The possible param-names and param-values are as described in the
Configuration parameters section; i.e., they are the same as the ones
you put in config files or as arguments to %Dlep.  Any of them can be
set (and subsequently queried with "show config"), but whether they
actually have any effect depends on when the implementation decides to
read them.  Many parameters are fetched from the configuration
database every time they are needed, so they will take effect right
away.  Others, like local-type, are only fetched once at startup, so
changing them later will not result in any change in behavior.

### Color-coding of output

Most of these commands produce text which is color-coded as follows:

<span style="color:green">
Green text represents the normal output of the command when nothing
special happened.
</span>

<span style="color:red">
Red text indicates that an error occurred, either in the user's input
or in the execution of the operation.
</span>

<span style="color:cyan">
Cyan text </span> represents information that came from the %Dlep service
library when it called the program's LLDLEP::DlepClient interface.  This
is typically information or an event that originated at the other DLEP
peer and has made its way across the DLEP session, into the local
peer's %Dlep service, and finally out to the client.


Configuration parameters
------------------------
***
These are the parameters that control the behavior of %Dlep.

%Dlep uses two different types of configuration files.  The
configuration parameters file described in this section specifies
items like IP addresses, port numbers, network interface names,
timeout values, and pathnames of other files that are needed.  The
@ref protoconfig controls what the protocol looks like on the wire by
supplying (for example) sizes of certain fields and constant values
that identify specific signals and data items.

The parameter names recognized in the configuration parameters file
are listed below.  On the command line, these parameter names should
appear exactly as given below, without any leading dashes, and should
be followed by whitespace then the parameter value.  In the XML
configuration file, these parameters must be written as:

\<parameter-name\> parameter-value \<\\parameter-name\>

See the example files in the config directory for additional necessary
XML boilerplate.

The parameters are:
<!-- If any of these parameters change, be sure to update ExampleDlepClientImpl.cpp. -->

- ack-probability <integer percent\> default: 100

  The probability that %Dlep will send ACK signals required by DLEP.
  E.g., a value of 100 means all ACK signals are sent, 0 means none
  are sent, and 50 means 50% (randomly chosen) are sent.  This applies
  uniformly to all ACK (aka Response) signals.  **Values other than
  100 should only be used for testing.**

- ack-timeout <integer seconds\> default: 3

  Time to wait for an ACK signal before considering it lost.  This applies
  to all ACK signals.  See the send-tries parameter.

- config-file <filename\> default: N/A

  Name of the XML config file containing these parameter settings.

- destination-advert-enable [ 0 | 1 ] default: 0

  Tells %Dlep if it should use the Destination Advertisement protocol.
  0=no, 1=yes.  See the Destination Advertisement section of the
  @ref designnotes for details.
  If destination advertisements are disabled, none of the other
  destination-advert configuration parameters have any effect.  This
  parameter has no effect on the operation of %Dlep when configured as
  a router.

- destination-advert-iface <interface name\> default: emane0

  Interface used to send/recv  destination advertisement messages.

- destination-advert-send-interval <integer seconds\> default: 5

  Length of time the modem waits between sending successive destination
  advertisements.

- destination-advert-mcast-address <IP address\> default: 225.6.7.8

  Multicast address to which modems send destination advertisements.
  Modems join this group.

- destination-advert-port <port number\> default: 33445

  UDP port to which modems send destination advertisements.

- destination-advert-hold-interval <integer seconds\> default: 0

  Length of time the modem waits between receiving a destination_up for
  a modem neighbor (identified by its rf-id) and receiving a destination
  advertisement from the same modem neighbor.  If this time is exceeded,
  the destination_up is ignored.  A value of 0 means wait forever.

- destination-advert-expire-count <integer count\> default: 0

  The number of missed destination advertisements from a modem
  neighbor that will trigger disposal of the neighbor's advertisement.
  If an advertisement's age is more than the advertisement's interval
  time this expire-count, the advertisement is discarded, and
  destination_down signals are sent to the router peer for all
  destinations in the announcement.  If the expire count is 0,
  advertisements are never discarded.

* destination-advert-rf-id <list of integers\> default: N/A

  The rf-id of the local modem.  This is like the modem's MAC address,
  but it can be any size.  Each integer in the list must fit in one
  byte.  The modem puts this in the rf-id field of destination
  advertisements that it sends, and remote modems will see this rf-id
  as the MAC address in a destination_up when a link is formed with
  this modem.

- discovery-iface <interface name\> default: eth0

  Interface that the router uses for the %PeerDiscovery protocol.

- discovery-interval <integer seconds\> default: 60

  Length of time the router waits between sending successive
  %PeerDiscovery signals (multicast packets).

- discovery-mcast-address <IP address\> default: 224.0.0.117

  Multicast address to which routers send %PeerDiscovery multicast packets.
  The modem joins this group.  If this is an IPv4 multicast address, the
  discovery process and subsequent DLEP TCP session will use IPv4.

  If the multicast address is IPv6, it must be a link-local multicast
  address, and the interface specified by discovery-iface must be
  configured with an IPv6 link-local address.  If those conditions are
  met, the discovery process and subsequent DLEP TCP session will use
  IPv6.

- discovery-port <port number\> default: 854

  UDP port to which routers send %PeerDiscovery multicast packets.
  The modem listens on this port.

- discovery-ttl <hops 0-255\> default: 255

  IP TTL value to put on discovery packets, both multicast and unicast.
  If the value is 0 or not provided, let the system use its default TTL
  value.

- discovery-enable [ 0 | 1 ] default: 1

  Tells the router if it should use the DLEP %PeerDiscovery protocol
  to find its modem peer.  0=no, 1=yes.  If not enabled, the router's
  session-address and session-port configuration parameters must be
  set so that the router can establish the TCP session connection to
  the modem.  If the session address is an IPv6 address, the
  session-iface configuration parameter must also be set.

- heartbeat-interval <integer seconds\> default: 60

  Time to wait between sending successive Heartbeat signals to the peer.
  If 0, do not send heartbeats.

- heartbeat-threshold <integer\> default: 4

  Number of Heartbeat intervals to wait, during which no signals (Heartbeat
  or otherwise) were received from the peer, before declaring the peer to be
  dead and terminating the peer session.

- linkchar-autoreply [ 0 | 1 ] default: 1

  Tells %Dlep whether it should automatically send a reply to linkchar
  requests.  0=no, 1=yes.  If yes, the reply will simply echo the
  same metric data items that were sent in the linkchar request.
  If no, the user must type an appropriate linkchar reply at the CLI
  before the session times out due to the lack of a response.  This
  parameter is only used by the %Dlep example client (CLI), not by the
  %Dlep service library.

- local-type [ modem | router ] default: router

  Establish which DLEP role to play.
  
- log-level <integer\> default: 1

  Log level for log messages.  1=most logging, 5=least logging.

- log-file <filename\> default: dlep.log

  File to write log messages to.

- peer-type <string\> default: N/A

  Value to put in the %Peer Type data item of the signals that allow this
  data item.  If no value is supplied, the Peer Type data item is omitted
  from all signals.

- peer-flags <integer 0-255\> default: 0

  Value to put in flags field of the %Peer Type data item of the
  signals that allow this data item.

- protocol-config-file <filename\> default: /usr/local/etc/dlep/dlep-rfc-8175.xml

  The protocol configuration file that specifies protocol details such as
  message field widths and code points.  See @ref protoconfig for details.

- protocol-config-schema <filename\> default: /usr/local/etc/dlep/protocol-config.xsd

  The XML schema against which the protocol-config-file will be checked.
  If this validation fails, %Dlep will exit with an error message.

- send-tries <integer\> default: 3

  Number of times to send a signal before giving up.  This applies to all signals
  that expect a replying ACK signal.  See the ack-timeout parameter.

- session-address <IP address\> default: none

  The address that the modem listens on for session connections.  If
  no value is supplied, %Dlep uses an address on discovery-iface.
  See discovery-enable for further information.

- session-iface <interface name\> default: N/A

  Interface for the router to use when establishing session
  connections.  This is only needed when when peer discovery is
  disabled and session-address is a link-local IPv6 address so that the
  address's scope id (interface index) can be set.

- session-port <port number\> default: 854

  TCP port number that the modem listens on for session connections.
  The modem requires this.  The router only requires it if peer
  discvoery is disabled.  See discovery-enable for further
  information.

- session-ttl <hops 0-255\> default: 255

  IP TTL value to put on TCP session packets.  If the value is 0 or
  not provided, let the system use its default TTL value.

@section protoconfig Protocol Configuration file
<!-- leave this comment here, between @section and -----, else section linking won't work -->
--------------------------------

The protocol configuration file is an XML file that controls what the
protocol looks like on the wire.  There are many different variations
of the DLEP protocol, partially because of the many different internet
drafts that have been issued over a period of several years.  Many of
these drafts omitted information critical for interoperability, like
the specific numbers that identify each message type and data item.
The purpose of this file is to make it easy to adapt to different
variations of the DLEP protocol so that interoperability is possible
and updating to a new draft is easier.  We have carefully examined
many of the DLEP drafts and identified those parts that change from
draft to draft, and captured many of those changing aspects in this
configuration file.

In addition to adapting to variants of the DLEP core protocol,
this configuration file provides a clean way to incorporate new
signals/messages, data items, and status codes associated with
extensions.

You are encouraged to peruse the @ref rfc8175
before proceeding.  Those familiar with XML may also wish to look at
the @ref proto_config_schema.  If you need to write your own protocol
configuration file, start with a copy of one of the existing ones.  If
you add your file to the definition of DRAFTS_XML in config/Makefile,
it will be XML-validated with every build.  You must fix any errors
reported by XML validation because %Dlep will also validate the file
when it starts, and it will exit if there are any errors.

We now describe the layout of protocol configuration file using the
actual XML element names but without the XML boilerplate which you can
get from the example files.  The file starts with some information that
applies protocol-wide:

- version (optional) : this value will go in the Version data item of
  signals that require it.  Note that later drafts eliminated this
  data item.  You can still specify a version; it just won't be used.

    - major, a non-negative integer
    - minor, a non-negative integer

- signal_prefix (optional), a string.  If present, messages that are
  identified as signals will be prefixed with this string.  This
  should be "DLEP" for drafts that make a distinction between messages
  and signals, and can be omitted for drafts that do not.  Nothing
  stops you from setting this to anything you want, though.

- field_sizes (required).  Several of the fields in DLEP messages have
  changed in size over time.  These elements let you specify the sizes
  to use for these fields, between 1 and 8 bytes inclusive.  All of
  the following field lengths are required, in this order:

    - signal_length
    - signal_id
    - data_item_length
    - extension_id
    - status_code (value is not currently used, status code is always
      1 byte long)

Next come the module definintions.  A module is a cohesive chunk of
protocol that has a name and some optional signals/messages, data
items, and status codes.  The core protocol is represented by one
module, and each extension has its own module.  A module's elements
are:

- name (required), a string that identifies the module.  The core protocol
  module's name is "core".  This gets used in some library APIs and
  appears in the "module" column of some of the CLI commands, like
  "show dataitems".

- draft (optional), a non-negative integer.  The IETF draft number
  on which this module is based.  Informational only.

- experiment_name (optional), a string.  If present, this value will
  go in an Experimental Definition data item of signals that allow it.
  Note that later drafts eliminated this data item.  You can still
  specify an experiment_name; it just won't be used.

- extension_id (optional), a non-negative integer.  The core module
  uses 0 to signify that it is not an extension (0 is not a valid
  extension id).  Actual extensions should specify the extension id
  they want to use.  If present, this value will go in an
  Extensions Supported data item (along with extension ids from
  other modules) in signals that allow it.

- data_item (optional, can be repeated).  This specifies a data item
  provided by this module.  A data item's elements are:

    - name (required), a string.  The name of the data item.  For
      core data items, this must exactly match the name for the
      same data item as given in LLDLEP::ProtocolStrings
      because that is how the code and the configuration are connected.

    - type (required), a string enumeration value specifying the data
      type of this data item.  These are too numerous to repeat here;
      see LLDLEP::DataItemValueType.  You must remove the "div_"
      prefix from these type names when you use them in the protocol
      configuration file.  There is also some discussion of data item
      types in @ref using_cli.

    - id (optional), a non-negative integer.  This will go in the Data
      Item Type field of the data item.  Only one data item can have a
      given id value across all modules; conflicts are detected.  If
      omitted, this data item will only be used as a sub data item
      inside some other data item(s), and its id must be defined in a
      sub_data_item section of that other data item.  See the
      description of sub_data_item below.  A data item that is used
      as a sub data item can still have an id defined here, as long as
      the id doesn't conflict with another data item.  In this case,
      the id can be omitted in the sub_data_item section, and the id
      defined here will be used.

    - metric (required), true/false.  Is this data item considered
     a metric?  Any data item marked as a metric will be announced
     in the Session Initialization Response message.

    - units (optional), a string.  The unit of measure for this
      data item.  If present, it must be one of:
          - seconds
          - milliseconds
          - microseconds
          - bits/second
          - percentage
          - octets

    - sub_data_item (optional, can be repeated).  This specifies a
      data item that can be nested inside the data item being defined.
      The nested data item is called a sub data item.  Its elements
      are:

        - name (required), a string.  This is the name of the sub data
          item, which must appear as a previously defined data item.

        - id (optional), a non-negative integer.  If present, when
          this sub data item appears within the data item being
          defined, its id will have this value.  The same sub data
          item (identified by name) can have different ids depending
          on which data item it is nested within.  If id is not
          present here, the sub data item's id will be the value given
          when the sub data item was defined as a regular data item.
          It is an error for the id to be undefined in both places.

        - occurs (required), describes how many times this sub data item
          can occur in this data item.  See the description of occurs
          for signals for details.

- signal (optional, can be repeated).  This specifies a signal/message
  provided or modified by this module.  A signal's elements are:

    - name (required), a string.  The name of the signal/message.  For
      core signals, this must exactly match the name for the same
      signal as given in LLDLEP::ProtocolStrings because that is how
      the code and the configuration are connected.  The same signal
      name can appear in multiple modules.  The first occurrence of a
      signal name is its definition.  Subsequent appearances of the
      same signal name are references to already-defined signals,
      usually by extension modules wanting to modify the signal in
      some way (see below).

    - id (optional), a non-negative integer.  This will go in the
      Signal/Message Type field of the signal or message.  Only one
      signal can have a given id value across all modules; conflicts
      are detected.  If this is the definition of the signal, the id
      field is required.  Subsequent references to the signal must NOT
      specify the id.

    - message (optional), true/false.  Is this signal considered a
      message, as opposed to a signal?  If it's a signal, the
      signal_prefix will be prepended to the signal on the wire.  If
      this is the definition of the signal, the message field is
      required.  Subsequent references to the signal can modify this
      field.

    - sender (optional), a string.  Specifies which side is allowed to
      send this message.  If this is the definition of the signal, the
      sender field is required.  Subsequent references to the signal
      can modify this field.  The value can be one of:

        - modem , the modem can send this message
        - router, the router can send this message
        - both ,  both sides can send this message

    - response (optional), a string.  Specifies the message name that
      is the response to the message being defined.  The response
      message must be defined prior to referring to it in this field.

    - data_item (optional, can be repeated) A data item that can
      appear in this signal.  Data items that cannot appear in this
      signal are not mentioned.  References to a predefined signal can
      add new data items to the signal.

        - name (required), a string.  This must refer to a data item
          name that has already been defined by some (any) module.

        - occurs (required), describes how many times this data item
          can occur in this signal.  This is mainly used when validating
          received messages, but is also used when constructing some
          messages.  The value can be one of:

            - 1 ,  exactly once (i.e., required)
            - 0-1 , at most once (i.e., optional)
            - 1+ , one or more times (i.e., required, repeatable)
            - 0+ , zero or more times (i.e., optional, repeatable)

- status_code (optional, can be repeated).  This specifies a status
  code provided by this module.  A status code's elements are:

    - name (required), a string.  The name of the status code.  For
      core status codes, this must exactly match the name for the same
      status code as given in LLDLEP::ProtocolStrings because that is
      how the code and the configuration are connected.

    - id (required), a non-negative integer.  This will go in the
      Status Code field of a Status data item.  Only one status code
      have a given id value across all modules; conflicts are
      detected.

    - failure_mode (required), a string.  This specifies what a DLEP
      participant must do when it receives a Status data item with this
      status code.  The value can be one of:
        - continue , the peer session must remain active
        - terminate , the peer session must be terminated

Extension modules should each have their own file so that they can be
included (with XInclude) in the core module file to compose a complete
configuration.  @ref rfc8175 (near the end) and the @ref
latency_range_config provide an example of this technique.

The extensive protocol configurability presents many opportunities for
testing other DLEP implementations.  The protocol can be tweaked to
look almost, but not quite, like what the other DLEP implementation
expects, for example by changing a data item's type or ID.
Introducing small, specific errors allows testing to target error
handling logic that might otherwise be hard to exercise.

Configuration can only go so far in accommodating different drafts.
Protocol behavior changes will normally require supporting code
changes.  The protocol configuration system handles many of the easy
changes, freeing up developer time to deal with the more difficult
ones.
