/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016 Massachusetts Institute of Technology
 *
 * Contributor: Adjacent Link LLC, Bridgewater, NJ
 */

/// @file
/// DestAdvertInfo class declaration.

#ifndef DEST_ADVERT_INFO_H
#define DEST_ADVERT_INFO_H

#include <cstdint>
#include <string>
#include <sstream>
#include "DlepCommon.h"

namespace LLDLEP
{
namespace internal
{

// holds info going in/out of the advertisement msg
struct DestAdvertInfo
{
    std::uint32_t   reportInterval;
    std::int64_t    uptime;
    std::uint32_t   sequenceNumber;
    LLDLEP::DlepMac         rfId;
    LLDLEP::DlepMacAddrs    destinations;
    LLDLEP::DataItems       ipv4DataItems;
    LLDLEP::DataItems       ipv4SnDataItems;
    LLDLEP::DataItems       ipv6DataItems;
    LLDLEP::DataItems       ipv6SnDataItems;

    DestAdvertInfo() :
        reportInterval {},
                   uptime {},
                   sequenceNumber {}
    { }

    DestAdvertInfo(std::uint32_t interval,
                   time_t uptm,
                   std::uint32_t seq,
                   const LLDLEP::DlepMac & id,
                   const LLDLEP::DlepMacAddrs & dests,
                   const LLDLEP::DataItems & ipv4Dest,
                   const LLDLEP::DataItems & ipv4SnDest,
                   const LLDLEP::DataItems & ipv6Dest,
                   const LLDLEP::DataItems & ipv6SnDest) :
        reportInterval {interval},
                   uptime {uptm},
                   sequenceNumber {seq},
                   rfId(id),
                   destinations(dests),
                   ipv4DataItems(ipv4Dest),
                   ipv4SnDataItems(ipv4SnDest),
                   ipv6DataItems(ipv6Dest),
                   ipv6SnDataItems(ipv6SnDest)
    { }

    std::string to_string() const
    {
        std::stringstream ss;

        ss << "report interval=" << reportInterval;
        ss << " uptime="         << uptime;
        ss << " seqnum="         << sequenceNumber;
        ss << " rfid="           << rfId.to_string();
        ss << " dests:";

        for (auto dest : destinations)
        {
            ss << dest.to_string() << ", ";
        }

        for (auto ipdi : ipv4DataItems)
        {
            ss << ipdi.to_string() << ", ";
        }

        for (auto ipdi : ipv4SnDataItems)
        {
            ss << ipdi.to_string() << ", ";
        }

        for (auto ipdi : ipv6DataItems)
        {
            ss << ipdi.to_string() << ", ";
        }

        for (auto ipdi : ipv6SnDataItems)
        {
            ss << ipdi.to_string() << ", ";
        }

        return ss.str();
    }
};

} // namespace internal
} // namespace LLDLEP

#endif

