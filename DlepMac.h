/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016 Massachusetts Institute of Technology
 */

/// @file
/// Declaration of MAC address type.

#ifndef DLEP_MAC_H
#define DLEP_MAC_H

#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace LLDLEP
{

/// MAC address.  It is variable size so that we can support 6 or 8
/// byte addresses as required by the draft, or even 2 byte addresses
/// to hold EMANE NEM IDs.
typedef struct d_mac
{
    std::vector<std::uint8_t> mac_addr;

    /// Convert to string representation
    std::string to_string() const;

} DlepMac;


/// Compare (<) two DlepMacs.  With this, we can use a DlepMac as the
/// key in a std::map.
bool operator<(const DlepMac & mac1, const DlepMac & mac2);

/// Compare two DlepMacs for equality.
bool operator==(const DlepMac & mac1, const DlepMac & mac2);

/// Output operator for convenient output to an ostream
std::ostream & operator<<(std::ostream & os, const DlepMac & mac);


/// set of DlepMac addrs
using DlepMacAddrs = std::set<LLDLEP::DlepMac>;

/// Find the difference between to sets of DlepMac addrs
/// possibly use std::set_difference from \<algorithm> instead
inline void getDifference(const DlepMacAddrs & A, const DlepMacAddrs & B,
                          DlepMacAddrs & C)
{
    // whats in here
    for (const auto a : A)
    {
        bool found = false;

        // and not in here
        for (const auto b : B)
        {
            if (a == b)
            {
                found = true;

                break;
            }
        }

        if (! found)
        {
            C.insert(a);
        }
    }
}


}  // namespace LLDLEP

#endif // DLEP_MAC_H
