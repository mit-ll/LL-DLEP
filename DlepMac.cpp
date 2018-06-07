/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016, 2018 Massachusetts Institute of Technology
 */

#include <sstream>
#include <iomanip>
#include "DlepCommon.h"

namespace LLDLEP
{
/// Return true iff mac1 < mac2
bool operator<(const DlepMac & mac1, const DlepMac & mac2)
{
    return (mac1.mac_addr < mac2.mac_addr);
}

/// Return true iff mac1 == mac2
bool operator==(const DlepMac & mac1, const DlepMac & mac2)
{
    return (mac1.mac_addr == mac2.mac_addr);
}

std::ostream & operator<<(std::ostream & os, const DlepMac & mac)
{
    os << mac.to_string();
    return os;
}

std::string DlepMac::to_string() const
{
    std::ostringstream s;
    s << std::hex << std::setfill('0');
    std::string colon = "";
    for (auto mac_byte : mac_addr)
    {
        s << std::setw(0) << colon
          << std::setw(2) << (unsigned int)mac_byte;
        colon = ":";
    }

    return s.str();
}

} // namespace LLDLEP
