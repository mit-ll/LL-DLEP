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

void doCopyStrToIpdi(const std::string & str, LLDLEP::DataItem & ipdi)
{
    std::vector<unsigned char> ipdi_ser;
    ipdi_ser.resize(str.size());
    for (size_t i = 0; i < str.size(); i++)
    {
        ipdi_ser[i] = str[i];
    }
    std::vector<unsigned char>::const_iterator it = ipdi_ser.begin();
    std::vector<unsigned char>::const_iterator it_end = ipdi_ser.end();

    ipdi.deserialize(it, it_end);
}

void doCopyIpdiToSTR(const LLDLEP::DataItem & ipdi, std::string & str)
{
    std::vector<std::uint8_t> ipdi_ser = ipdi.serialize();
    str.resize(ipdi_ser.size());
    for (size_t i = 0; i < ipdi_ser.size(); ++i)
    {
        str[i] = static_cast<char>(ipdi_ser[i]);
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

    for (auto ipdi : info.ipv4DataItems)
    {
        std::string s;
        doCopyIpdiToSTR(ipdi, s);
        da.add_ipv4dataitems(s);
    }

    for (auto ipdi : info.ipv4SnDataItems)
    {
        std::string s;
        doCopyIpdiToSTR(ipdi, s);
        da.add_ipv4sndataitems(s);
    }

    for (auto ipdi : info.ipv6DataItems)
    {
        std::string s;
        doCopyIpdiToSTR(ipdi, s);
        da.add_ipv6dataitems(s);
    }

    for (auto ipdi : info.ipv6SnDataItems)
    {
        std::string s;
        doCopyIpdiToSTR(ipdi, s);
        da.add_ipv6sndataitems(s);
    }

    const bool rc = da.SerializeToString(&str);

    return std::pair<bool, std::string>(rc, str);
}


// un-build/de-serialize for reading
// need to transfer DlepPtr parameter for getting the protocfg,
// for construct the dataitem. 
inline  std::pair<bool, DestAdvertInfo> unbuild_destination_advert(
    const uint8_t * buff, size_t len, DlepPtr dlep)
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

        for (int i = 0; i < da.ipv4dataitems_size(); ++i)
        {
            DataItem ipdi(ProtocolStrings::IPv4_Address, dlep->protocfg);
            doCopyStrToIpdi(da.ipv4dataitems(i), ipdi);
            info.ipv4DataItems.push_back(ipdi);
        }

        for (int i = 0; i < da.ipv4sndataitems_size(); ++i)
        {
            DataItem ipdi(ProtocolStrings::IPv4_Attached_Subnet, dlep->protocfg);
            doCopyStrToIpdi(da.ipv4sndataitems(i), ipdi);
            info.ipv4SnDataItems.push_back(ipdi);
        }

        for (int i = 0; i < da.ipv6dataitems_size(); ++i)
        {
            DataItem ipdi(ProtocolStrings::IPv6_Address, dlep->protocfg);
            doCopyStrToIpdi(da.ipv6dataitems(i), ipdi);
            info.ipv6DataItems.push_back(ipdi);
        }

        for (int i = 0; i < da.ipv6sndataitems_size(); ++i)
        {
            DataItem ipdi(ProtocolStrings::IPv6_Attached_Subnet, dlep->protocfg);
            doCopyStrToIpdi(da.ipv6sndataitems(i), ipdi);
            info.ipv6SnDataItems.push_back(ipdi);
        }
    }

    return std::pair<bool, DestAdvertInfo>(rc, info);
}

} // namespace internal
} // namespace LLDLEP

#endif

