/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016 Massachusetts Institute of Technology
 *
 * Contributor: Adjacent Link LLC, Bridgewater, NJ
 */

/// @file
/// Various network utility functions.

#ifndef DLEP_NET_UTILS_H
#define DLEP_NET_UTILS_H

#include "DlepCommon.h"
#include "DlepLogger.h"
#include <boost/asio.hpp>
#include <iostream>

namespace LLDLEP
{
namespace internal
{
namespace NetUtils
{
/// Find an IP address on a network interface.
///
/// @param[in] iface_name
///            interface name on which to find an IP address
/// @param[in] want_ipv4_addr
///            If true, find an IPv4 address, else find an IPv6 address.
///            Only link-local IPv6 addresses will be returned.
/// @param[in] logger
///            for logging with the LOG macro
/// @return an IP address that meets the above criteria.  If one
///         couldn't be found, the returned IP address's is_unspecified()
//          method will return true.
boost::asio::ip::address
get_ip_addr_from_iface(const std::string & iface_name,
                       bool want_ipv4_addr,
                       LLDLEP::internal::DlepLoggerPtr logger);

/// Find a network interface given an IP address.
///
/// @param[in] ipaddr
///            IPv4 or IPv6 address to look for
/// @param[in] logger
///            for logging with the LOG macro
/// @return the name of the interface on which \a ipaddr was found,
///         or "" if \a ipaddr was not found on any interface.
std::string get_iface_from_ip_addr(const boost::asio::ip::address & ipaddr,
                                   LLDLEP::internal::DlepLoggerPtr logger);

/// Set a given scope id of a link-local IPv6 address.
///
/// @param[in,out] ipaddr
///                The IPv6 link-local address for which to set the
///                scope ID.
/// @param[in]     scope_id
///                the scope id to set in \a ipaddr.
/// @return true if successful, in which case \a ipaddr is
///         modified, else false and \a ipaddr is not modified.
bool set_ipv6_scope_id(boost::asio::ip::address & ipaddr,
                       unsigned long scope_id);

/// Set the scope id of a link-local IPv6 address from an interface name.
///
/// @param[in,out] ipaddr
///                The IPv6 link-local address for which to set the
///                scope ID.
/// @param[in]     iface_name
///                the network interface name used to set the
///                scope id in \a ipaddr.  The interface's index
///                is determined and used as the scope id.
/// @return true if successful, in which caSE \a ipaddr is modified,
///         else false and \a ipaddr is not modified.
bool set_ipv6_scope_id(boost::asio::ip::address & ipaddr,
                       const std::string & iface_name);

bool ipv4ToEtherMacAddr(const boost::asio::ip::address & addr,
                        LLDLEP::DlepMac & mac,
                        std::string & ifname,
                        std::ostringstream & errMessage);
} // namespace NetUtils
} // namespace internal
} // namespace LLDLEP

#endif
