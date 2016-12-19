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

    DestAdvertInfo() :
        reportInterval {},
                   uptime {},
                   sequenceNumber {}
    { }

    DestAdvertInfo(std::uint32_t interval,
                   time_t uptm,
                   std::uint32_t seq,
                   const LLDLEP::DlepMac & id,
                   const LLDLEP::DlepMacAddrs & dests) :
        reportInterval {interval},
                   uptime {uptm},
                   sequenceNumber {seq},
                   rfId(id),
                   destinations(dests)
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

        return ss.str();
    }
};

} // namespace internal
} // namespace LLDLEP

#endif

