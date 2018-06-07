/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016, 2018 Massachusetts Institute of Technology
 *
 * Contributor: Adjacent Link LLC, Bridgewater, NJ
 */

/// @file
/// Various network utility functions.

#include "NetUtils.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <ifaddrs.h> // for getifaddrs()

namespace LLDLEP
{
namespace internal
{
namespace NetUtils
{

// during testing with localhost modem/router, just use this simple address
void assignLoopbackAddress(LLDLEP::DlepMac & mac)
{
    mac.mac_addr.push_back(0x00);
    mac.mac_addr.push_back(0x00);
    mac.mac_addr.push_back(0x00);
    mac.mac_addr.push_back(0x00);
    mac.mac_addr.push_back(0x00);
    mac.mac_addr.push_back(0x01);
}


boost::asio::ip::address
get_ip_addr_from_iface(const std::string & iface_name,
                       bool want_ipv4_addr,
                       DlepLoggerPtr logger)
{
    std::ostringstream msg;
    struct ifaddrs * ifaddr = nullptr, *ifa = nullptr;
    int addr_family = want_ipv4_addr ? AF_INET : AF_INET6;

    // default value for ipaddr is "unspecified"
    boost::asio::ip::address ipaddr;

    int r = getifaddrs(&ifaddr);
    if (r < 0)
    {
        auto saved_errno = errno;
        msg << "getifaddrs returns " << strerror(saved_errno);
        LOG(DLEP_LOG_ERROR, msg);
        return ipaddr;
    }

    // Loop over the addresses, looking for one we like.

    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == nullptr)
        {
            continue;
        }

        // If the interface name and address family match, this
        // address might work.

        if ((iface_name == ifa->ifa_name) &&
                (ifa->ifa_addr->sa_family == addr_family))
        {
            if (addr_family == AF_INET)
            {
                boost::asio::ip::address_v4::bytes_type ipbytes;

                // Copy the address from the sockaddr to ipbytes.
                // We can't use static_cast<> here because g++
                // complains that it is invalid.  You just can't get
                // away from casting with the sockaddr interface,
                // though, so we resort to old-style C casts, which
                // g++ accepts here.

                struct sockaddr_in * sa_ipv4 =
                    (struct sockaddr_in *)(ifa->ifa_addr);
                std::uint8_t * sa_bytes_ipv4 =
                    (std::uint8_t *)(&sa_ipv4->sin_addr);

                for (std::size_t i = 0; i < ipbytes.size(); i++)
                {
                    ipbytes[i] = sa_bytes_ipv4[i];
                }

                // Make an ipv4 address out of the raw bytes.
                // Wish list: a boost::asio::ip:address constructor
                // that takes a sockaddr.
                ipaddr = boost::asio::ip::address_v4(ipbytes);
                break;
            }
            else // AF_INET6
            {
                assert(addr_family == AF_INET6);

                boost::asio::ip::address_v6::bytes_type ipbytes;

                // Copy the address from the sockaddr to ipbytes.
                // See above comment for why we're using C-style casts.

                struct sockaddr_in6 * sa_ipv6 =
                    (struct sockaddr_in6 *)(ifa->ifa_addr);

                // If this is a link-local address, we'll take it.

                if (IN6_IS_ADDR_LINKLOCAL(&sa_ipv6->sin6_addr))
                {
                    for (std::size_t i = 0; i < ipbytes.size(); i++)
                    {
                        ipbytes[i] = sa_ipv6->sin6_addr.s6_addr[i];
                    }

                    // Make an ipv6 address out of the raw bytes and
                    // the scope id.

                    ipaddr = boost::asio::ip::address_v6(ipbytes,
                                                         sa_ipv6->sin6_scope_id);
                    break;
                }
            } // else AF_INET6
        } // if interface name and address family match what we want
    } // for each interface returned by getifaddrs

    freeifaddrs(ifaddr);

    msg << "for interface=" << iface_name
        << ", address family=" << addr_family
        << ", address found=" << ipaddr;
    LOG(DLEP_LOG_DEBUG, msg);

    return ipaddr;
}

// helper function for get_iface_from_ip_addr()
template<typename T1, typename T2>
static bool
compare_addrs(T1 addr1, T2 addr2, std::size_t len)
{
    for (std::size_t i = 0; i < len; i++)
    {
        if (addr1[i] != addr2[i])
        {
            return false;
        }
    }
    return true;
}

std::string
get_iface_from_ip_addr(const boost::asio::ip::address & ipaddr,
                       DlepLoggerPtr logger)
{
    std::ostringstream msg;
    struct ifaddrs * ifaddr = nullptr, *ifa = nullptr;
    int addr_family = ipaddr.is_v4() ? AF_INET : AF_INET6;
    std::string iface_name;

    int r = getifaddrs(&ifaddr);
    if (r < 0)
    {
        auto saved_errno = errno;
        msg << "getifaddrs returns " << strerror(saved_errno);
        LOG(DLEP_LOG_ERROR, msg);
        return iface_name;
    }

    // Loop over the addresses, looking for one equal to ipaddr.

    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == nullptr)
        {
            continue;
        }

        if (ifa->ifa_addr->sa_family == addr_family)
        {
            if (addr_family == AF_INET)
            {
                // compare IPv4 addresses

                boost::asio::ip::address_v4 ipv4_addr = ipaddr.to_v4();
                boost::asio::ip::address_v4::bytes_type ipbytes =
                    ipv4_addr.to_bytes();

                struct sockaddr_in * sa_ipv4 =
                    (struct sockaddr_in *)(ifa->ifa_addr);

                if (compare_addrs(ipbytes, (std::uint8_t *)(&sa_ipv4->sin_addr),
                                  ipbytes.size()))
                {
                    iface_name = std::string(ifa->ifa_name);
                    break;
                }
            }
            else // AF_INET6
            {
                // compare IPv6 addresses

                assert(addr_family == AF_INET6);

                boost::asio::ip::address_v6 ipv6_addr = ipaddr.to_v6();
                boost::asio::ip::address_v6::bytes_type ipbytes =
                    ipv6_addr.to_bytes();

                struct sockaddr_in6 * sa_ipv6 =
                    (struct sockaddr_in6 *)(ifa->ifa_addr);

                if (compare_addrs(ipbytes, sa_ipv6->sin6_addr.s6_addr,
                                  ipbytes.size()))
                {
                    iface_name = std::string(ifa->ifa_name);
                    break;
                }
            } // else AF_INET6
        } // if the address family matches
    } // for each interface returned by getifaddrs

    freeifaddrs(ifaddr);

    msg << "for address=" << ipaddr
        <<  ", address family=" << addr_family
        << ", found interface=" << iface_name;
    LOG(DLEP_LOG_DEBUG, msg);

    return iface_name;
}

bool
set_ipv6_scope_id(boost::asio::ip::address & ipaddr,
                  unsigned long scope_id)
{
    if (ipaddr.is_v4())
    {
        return false;
    }

    // This seems clumsy, but the only way I can see to do this is to
    // extract the IPv6 address from ipaddr, set its scope id, then
    // copy it back to ipaddr.

    boost::asio::ip::address_v6 ipv6addr = ipaddr.to_v6();
    if ((! ipv6addr.is_link_local()) &&
            (! ipv6addr.is_multicast_link_local()))
    {
        // The scope id can only be set on link-local addresses.
        return false;
    }
    ipv6addr.scope_id(scope_id);
    ipaddr = ipv6addr;
    return true;
}

bool
set_ipv6_scope_id(boost::asio::ip::address & ipaddr,
                  const std::string & iface_name)
{
    unsigned long if_index = if_nametoindex(iface_name.c_str());
    if (if_index == 0)
    {
        return false;
    }

    return set_ipv6_scope_id(ipaddr, if_index);
}

bool
ipv4ToEtherMacAddr(const boost::asio::ip::address & addr,
                   LLDLEP::DlepMac & mac, std::string & ifname,
                   std::ostringstream & errMessage)
{
#if __linux__
    if (addr.is_v4())
    {
        const in_addr inaddr
        {
            htonl(addr.to_v4().to_ulong())
        };

        // handle loopback
        if (inaddr.s_addr == htonl(INADDR_LOOPBACK))
        {
            assignLoopbackAddress(mac);

            return true;
        }
        else
        {
            char buff[256 * sizeof(struct ifreq)] = {0};

            struct ifconf ifc;
            bzero(&ifc, sizeof(ifc));

            ifc.ifc_buf = buff;
            ifc.ifc_len = sizeof(buff);

            const int sock = socket(AF_INET, SOCK_DGRAM, 0);

            if (sock < 0)
            {
                errMessage << __func__ << "socket:" << strerror(errno);

                return false;
            }

            if (ioctl(sock, SIOCGIFCONF, &ifc) < 0)
            {
                errMessage << __func__ << "ioctl:getifconfig" << strerror(errno);

                close(sock);

                return false;
            }

            for (struct ifreq * ifrp = (struct ifreq *) ifc.ifc_buf;
                    ifrp < (struct ifreq *)(ifc.ifc_buf + ifc.ifc_len); ++ifrp)
            {
                // match discovery interface
                if (ifname == ifrp->ifr_name)
                {
                    // IPv4
                    if (ifrp->ifr_addr.sa_family == AF_INET)
                    {
                        struct ifreq ifr;
                        bzero(&ifr, sizeof(ifr));

                        strncpy(ifr.ifr_name, ifrp->ifr_name, sizeof(ifr.ifr_name));

                        // src matches ifaddr, must be local
                        if (((struct sockaddr_in *) & (ifrp->ifr_addr))->sin_addr.s_addr ==
                                inaddr.s_addr)
                        {
                            if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0)
                            {
                                errMessage << __func__ << "ioctl:getifhwaddr:" << ifrp->ifr_name << ":" <<
                                           strerror(errno);

                                close(sock);

                                return false;
                            }

                            mac.mac_addr.resize(ETH_ALEN);

                            for (int i = 0; i < ETH_ALEN; ++i)
                            {
                                mac.mac_addr[i] = ifr.ifr_hwaddr.sa_data[i];
                            }

                            close(sock);

                            return true;
                        }

                        // get ifflags
                        if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0)
                        {
                            errMessage << __func__ << "ioctl:getifflags:" << ifrp->ifr_name << ":" <<
                                       strerror(errno);

                            close(sock);

                            return false;
                        }

                        // skip loopback interface
                        if (ifr.ifr_flags & IFF_LOOPBACK)
                        {
                            continue;
                        }

                        // check ifa is up
                        if (ifr.ifr_flags & IFF_UP)
                        {
                            struct arpreq req;
                            bzero(&req, sizeof(req));

                            ((struct sockaddr_in *) & (req.arp_pa))->sin_addr.s_addr = inaddr.s_addr;
                            ((struct sockaddr_in *) & (req.arp_pa))->sin_family      = AF_INET;

                            strncpy(req.arp_dev, ifrp->ifr_name, IFNAMSIZ);

                            if (ioctl(sock, SIOCGARP, &req) < 0)
                            {
                                errMessage << __func__ << "ioctl:getarp:" << req.arp_dev << ":" << inet_ntoa(
                                               inaddr) << ":" << strerror(errno);

                                close(sock);

                                return false;
                            }

                            // entry found
                            if (req.arp_flags & ATF_COM)
                            {
                                // expect ether
                                if (req.arp_ha.sa_family == ARPHRD_ETHER)
                                {
                                    mac.mac_addr.insert(mac.mac_addr.begin(), req.arp_ha.sa_data,
                                                        req.arp_ha.sa_data + 6);

                                    close(sock);

                                    // found
                                    return true;
                                }
                            }
                        }
                    }
                }
            }

            close(sock);

            errMessage << __func__ << ":" << inet_ntoa(inaddr) << " not found";

            return false;
        }
    }
    else
    {
        errMessage << __func__ << ":" << addr.to_string() << " not ipv4 address";

        return false;
    }
#else
    return false;
#endif // linux
}

} // namespace NetUtils
} // namespace internal
} // namespace LLDLEP
