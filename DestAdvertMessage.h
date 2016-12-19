/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016 Massachusetts Institute of Technology
 *
 * Contributor: Adjacent Link LLC, Bridgewater, NJ
 */


#ifndef DEST_ADVERT_MESSAGE_H
#define DEST_ADVERT_MESSAGE_H

#include <cstdint>
#include <string>
#include "DlepCommon.h"
#include "destadvert.pb.h"
#include "DestAdvertInfo.h"

namespace
{

void doCopyStrToMac(const std::string & str, LLDLEP::DlepMac & mac)
{
    mac.mac_addr.resize(str.size());

    for (size_t i = 0; i < str.size(); ++i)
    {
        mac.mac_addr[i] = static_cast<std::uint8_t>(str[i]);
    }
}

void doCopyMacToSTR(const LLDLEP::DlepMac & mac, std::string & str)
{
    str.resize(mac.mac_addr.size());

    for (size_t i = 0; i < mac.mac_addr.size(); ++i)
    {
        str[i] = static_cast<char>(mac.mac_addr[i]);
    }
}
}


namespace LLDLEP
{
namespace internal
{

// build/serialize for sending
inline  std::pair<bool, std::string> build_destination_advert(
    const DestAdvertInfo & info)
{
    std::string str;

    DestinationAdvertisement da;

    da.set_reportinterval(info.reportInterval);

    da.set_uptimeinseconds(info.uptime);

    da.set_sequencenumber(info.sequenceNumber);

    doCopyMacToSTR(info.rfId, *da.mutable_localid());

    for (auto dest : info.destinations)
    {
        std::string s;

        doCopyMacToSTR(dest, s);

        da.add_destinations(s);
    }

    const bool rc = da.SerializeToString(&str);

    return std::pair<bool, std::string>(rc, str);
}


// un-build/de-serialize for reading
inline  std::pair<bool, DestAdvertInfo> unbuild_destination_advert(
    const uint8_t * buff, size_t len)
{
    DestAdvertInfo info;

    DestinationAdvertisement da;

    const bool rc = da.ParseFromArray(buff, len);

    if (rc)
    {
        info.reportInterval = da.reportinterval();

        info.uptime = da.uptimeinseconds();

        info.sequenceNumber = da.sequencenumber();

        doCopyStrToMac(da.localid(), info.rfId);

        for (int i = 0; i < da.destinations_size(); ++i)
        {
            DlepMac mac;

            doCopyStrToMac(da.destinations(i), mac);

            info.destinations.insert(mac);
        }
    }

    return std::pair<bool, DestAdvertInfo>(rc, info);
}

} // namespace internal
} // namespace LLDLEP

#endif

