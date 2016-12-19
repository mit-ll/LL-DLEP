/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016 Massachusetts Institute of Technology
 */

/// @file
/// Declarations that are common to (i.e., needed by) both the client and
/// the library.

#ifndef DLEP_COMMON_H
#define DLEP_COMMON_H

#include "DataItem.h"
#include "IdTypes.h"

namespace LLDLEP
{

/// Information about a peer
struct PeerInfo
{
    /// Uniquely identifies the peer
    std::string peer_id;

    /// value of the %Peer Type data item sent by the peer during DLEP
    /// session negotiation
    std::string peer_type;

    /// the set of extensions to use with this peer (mutually agreed
    /// upon during DLEP session negotiation)
    std::vector<ExtensionIdType> extensions;

    /// values of the Experiment Name data items sent by the peer during
    /// DLEP session negotiation
    std::vector<std::string> experiment_names;

    /// metrics and IP addresses associated with the peer
    DataItems data_items;

    /// destinations declared up by the peer
    std::vector<DlepMac> destinations;

    /// heartbeat interval declared by the peer during DLEP session
    /// negotiation.  This is the exact value sent by the peer in the
    /// Heartbeat Interval data item; no unit conversion has been
    /// performed.
    uint32_t heartbeat_interval;
};

/// Information about a destination
struct DestinationInfo
{
    /// MAC address of the destination
    DlepMac mac_address;

    /// peer to which this destination belongs
    std::string peer_id;

    /// boolean flags for this destination (none defined yet)
    std::uint32_t flags;

    /// metrics and IP addresses for this destination
    DataItems data_items;
};

}  // namespace LLDLEP

#endif // DLEP_COMMON_H
