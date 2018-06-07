/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016 Massachusetts Institute of Technology
 */

/// @file
/// class TestClientImpl is the test implementation of the DlepClient interface.
///
/// The methods in this class are called by the Dlep library to get configuration
/// information and to inform the client (library user) of DLEP-related events.

#include <boost/test/unit_test.hpp>
#include <boost/chrono/chrono.hpp>
#include "DlepInit.h"
#include "TestClientImpl.h"
#include <boost/lexical_cast.hpp>
#include <algorithm> // for std::max
#include <iomanip> // for std::setw
#include <netinet/in.h>
#include <ifaddrs.h> // for getifaddrs()

TestClientImpl::TestClientImpl()
{
}

/// Configuration parameter definitions and default values.
/// If any of these change, be sure to update doc/doxygen/markdown/UserGuide.md.
TestClientImpl::ConfigParameterMapType TestClientImpl::param_info =
{
    { "ack-timeout",              {ConfigParameterInfo::ParameterType::UnsignedInteger} },
    { "ack-probability",          {ConfigParameterInfo::ParameterType::UnsignedInteger} },
    { "config-file",              {ConfigParameterInfo::ParameterType::ConfigFile} },
    { "destination-advert-enable", {ConfigParameterInfo::ParameterType::Boolean} },
    { "destination-advert-send-interval", {ConfigParameterInfo::ParameterType::UnsignedInteger} },
    { "destination-advert-mcast-address",  {ConfigParameterInfo::ParameterType::IPAddress} },
    { "destination-advert-port",   {ConfigParameterInfo::ParameterType::UnsignedInteger} },
    { "destination-advert-hold-interval", {ConfigParameterInfo::ParameterType::UnsignedInteger} },
    { "destination-advert-expire-count", {ConfigParameterInfo::ParameterType::UnsignedInteger} },
    { "destination-advert-rf-id", {ConfigParameterInfo::ParameterType::ListOfUnsignedInteger} },
    { "destination-advert-iface", {ConfigParameterInfo::ParameterType::String} },
    { "discovery-iface",          {ConfigParameterInfo::ParameterType::String } },
    { "discovery-interval",       {ConfigParameterInfo::ParameterType::UnsignedInteger } },
    { "discovery-mcast-address",  {ConfigParameterInfo::ParameterType::IPAddress } },
    { "discovery-port",           {ConfigParameterInfo::ParameterType::UnsignedInteger } },
    { "discovery-enable",         {ConfigParameterInfo::ParameterType::Boolean } },
    { "heartbeat-interval",       {ConfigParameterInfo::ParameterType::UnsignedInteger } },
    { "heartbeat-threshold",      {ConfigParameterInfo::ParameterType::UnsignedInteger } },
    { "local-type",               {ConfigParameterInfo::ParameterType::String } },
    { "log-level",                {ConfigParameterInfo::ParameterType::UnsignedInteger } },
    { "log-file",                 {ConfigParameterInfo::ParameterType::String } },
    { "peer-type",                {ConfigParameterInfo::ParameterType::String } },
    { "protocol-config-file",     {ConfigParameterInfo::ParameterType::String } },
    { "protocol-config-schema",   {ConfigParameterInfo::ParameterType::String } },
    { "send-tries",               {ConfigParameterInfo::ParameterType::UnsignedInteger } },
    { "session-address",          {ConfigParameterInfo::ParameterType::IPAddress} },
    { "session-port",             {ConfigParameterInfo::ParameterType::UnsignedInteger } }
};

void
TestClientImpl::get_config_parameter(const std::string & parameter_name,
                                     ConfigValue * value)
{
    auto findval = config_map.find(parameter_name);
    if (findval != config_map.end())
    {
        *value = findval->second;
    }
    else
    {
        throw BadParameterName(parameter_name);
    }
}

void
TestClientImpl::get_config_parameter(const std::string & parameter_name,
                                     bool * value)
{
    ConfigValue v;
    get_config_parameter(parameter_name, &v);
    *value = boost::get<bool>(v);
}

void
TestClientImpl::get_config_parameter(const std::string & parameter_name,
                                     unsigned int * value)
{
    ConfigValue v;
    get_config_parameter(parameter_name, &v);
    *value = boost::get<unsigned int>(v);
}

void
TestClientImpl::get_config_parameter(const std::string & parameter_name,
                                     std::string * value)
{
    ConfigValue v;
    get_config_parameter(parameter_name, &v);
    *value = boost::get<std::string>(v);
}

void
TestClientImpl::get_config_parameter(const std::string & parameter_name,
                                     boost::asio::ip::address * value)
{
    ConfigValue v;
    get_config_parameter(parameter_name, &v);
    *value = boost::get<boost::asio::ip::address>(v);
}

void
TestClientImpl::get_config_parameter(const std::string & parameter_name,
                                     std::vector<unsigned int> * value)
{
    ConfigValue v;
    get_config_parameter(parameter_name, &v);
    *value = boost::get<std::vector<unsigned int> >(v);
}

// helper class for printing boost variant ConfigValue
class print_config_visitor : public boost::static_visitor<std::string>
{
public:
    // format vectors of unsigned int
    std::string operator()(const std::vector<unsigned int> & operand) const
    {
        std::ostringstream s;
        std::string comma = "";
        for (unsigned int ui : operand)
        {
            s << comma << ui;
            comma = ",";
        }
        return s.str();
    }

    // format everything else
    template <typename T>
    std::string operator()(const T & operand) const
    {
        std::ostringstream s;
        s << operand;
        return s.str();
    }
};

void
TestClientImpl::print_config() const
{
    BOOST_TEST_MESSAGE(__FILE__ << ":" << __LINE__
                       << ": Configuration parameters:");
    for (auto const & config_item : config_map)
    {
        BOOST_TEST_MESSAGE(config_item.first << " = "
                           << boost::apply_visitor(print_config_visitor(),
                                                   config_item.second)
                          );
    }
}

bool
TestClientImpl::parse_config_file(const char * config_filename)
{
    xmlDocPtr doc;
    xmlNodePtr cur;

    // std::cout << "parameter config file is " << config_filename << std::endl;

    // Use the xml2 parser to do the heavy lifting.
    doc = xmlParseFile(config_filename);

    if (doc == NULL)
    {
        BOOST_TEST_MESSAGE(__FILE__ << ":" << __LINE__ << ": "
                           << config_filename <<
                           " config file was not parsed successfully");
        return false;
    }

    cur = xmlDocGetRootElement(doc);

    if (cur == NULL)
    {
        BOOST_TEST_MESSAGE(__FILE__ << ":" << __LINE__ << ": "
                           << config_filename << " config file was empty");
        xmlFreeDoc(doc);
        return false;
    }

    if (xmlStrcmp(cur->name, (const xmlChar *) "config"))
    {
        BOOST_TEST_MESSAGE(__FILE__ << ":" << __LINE__<< ": "
                           << config_filename
                           << " config file must begin with <config>");
        xmlFreeDoc(doc);
        return false;
    }

    // set to false if any errors occur
    bool ok = true;

    // Loop over the document nodes looking for parameters

    for (cur = cur->xmlChildrenNode;
            cur != NULL;
            cur = cur->next)
    {
        // std::cout << "node " << cur->name << std::endl;

        // look for a <params> section
        if ((!xmlStrcmp(cur->name, (const xmlChar *)"params")))
        {
            for (xmlNodePtr param_node = cur->xmlChildrenNode;
                    param_node != NULL;
                    param_node = param_node->next)
            {
                // Ignore "text" nodes.  They don't seem to be needed.
                if ((!xmlStrcmp(param_node->name, (const xmlChar *)"text")))
                {
                    continue;
                }

                std::string param_value;

                // generalize for other list types

                if (!xmlStrcmp(param_node->name, (const xmlChar *)"extensions"))
                {
                    // Parse list of extension IDs, building up a
                    // comma-separated string of them in param_value.

                    bool first_id = true;

                    for (xmlNodePtr list_node = param_node->xmlChildrenNode;
                            list_node != NULL;
                            list_node = list_node->next)
                    {
                        if (!xmlStrcmp(list_node->name, (const xmlChar *)"text"))
                        {
                            // Ignore "text" nodes.  They don't seem to be needed.
                            continue;
                        }
                        else if (!xmlStrcmp(list_node->name, (const xmlChar *)"id"))
                        {
                            // Found an extension ID.  Add it to param_value.
                            if (! first_id)
                            {
                                param_value += ",";
                            }
                            first_id = false;

                            char * xml_node_value = (char *)
                                                    xmlNodeListGetString(doc,
                                                                         list_node->xmlChildrenNode,
                                                                         1);
                            param_value += std::string(xml_node_value);
                            xmlFree(xml_node_value);
                        }
                        else
                        {
                            BOOST_TEST_MESSAGE(__FILE__ << ":" << __LINE__ << ": "
                                               << list_node->name
                                               << " is not a valid extensions list member");
                            ok = false;
                        }
                    } // for each extension id in the list
                } // if "extensions"
                else
                {
                    // This is a simple non-list value.
                    char * xml_node_value = (char *)
                                            xmlNodeListGetString(doc,
                                                                 param_node->xmlChildrenNode,
                                                                 1);
                    param_value = std::string(xml_node_value);
                    xmlFree(xml_node_value);
                }

                if (! parse_parameter((char *)param_node->name,
                                      (char *)param_value.c_str()))
                {
                    ok = false;
                }
            }
        }
    }

    xmlFreeDoc(doc);

    if (ok)
    {
        // Force the discovery-iface parameter to be something reasonable.
        // This will override any discovery-iface setting in the file,
        // but that's always what we want.  We choose between IPv4 and IPv6
        // depending on which one is used by the discovery-mcast-address
        // configuration parameter.

        try
        {
            boost::asio::ip::address multicast_addr;
            get_config_parameter("discovery-mcast-address", &multicast_addr);
            set_discovery_iface(multicast_addr.is_v4());
        }
        catch (BadParameterName)
        {
            ok = false;
        }
    }

    return ok;
}


bool
TestClientImpl::parse_parameter(const char * param_name,
                                const char * param_value)
{
    bool ok = true;

    // std::cout << param_name << " unparsed value = " << param_value << std::endl;

    try
    {
        // see if we understand this parameter name
        auto it = param_info.find(param_name);
        if (it != param_info.end())
        {
            ConfigParameterInfo & cpi = it->second;

            // Convert param_value to the expected type.

            if (cpi.type == ConfigParameterInfo::ParameterType::UnsignedInteger)
            {
                config_map[param_name] =
                    boost::lexical_cast<unsigned int>(param_value);
            }
            else if (cpi.type == ConfigParameterInfo::ParameterType::Boolean)
            {
                config_map[param_name] =
                    boost::lexical_cast<bool>(param_value);
            }
            else if (cpi.type == ConfigParameterInfo::ParameterType::String)
            {
                config_map[param_name] = std::string(param_value);
            }
            else if (cpi.type == ConfigParameterInfo::ParameterType::IPAddress)
            {
                config_map[param_name] =
                    boost::asio::ip::address::from_string(param_value);
            }
            else if (cpi.type == ConfigParameterInfo::ParameterType::ListOfUnsignedInteger)
            {
                std::vector<unsigned int> vui;
                unsigned int ui;
                std::stringstream ss(param_value);
                while (ss >> ui)
                {
                    vui.push_back(ui);
                    // skip over commands between numbers
                    if (ss.peek() == ',')
                    {
                        ss.ignore();
                    }
                }

                // If we didn't parse all the way to the end of the string,
                // there must have been non-numeric characters in the string,
                // and that's an error.
                if (! ss.eof())
                {
                    throw std::invalid_argument(param_value);
                }

                // If everything was successful, store the new parameter value
                config_map[param_name] = vui;
            }
            else if (cpi.type == ConfigParameterInfo::ParameterType::ConfigFile)
            {
                // This special-case type causes us to use param_value
                // as a config file.  parse_parameter() and
                // parse_config_file() are mutually recursive.
                ok = parse_config_file(param_value);
            }
            else
            {
                BOOST_TEST_MESSAGE(__FILE__ << ":" << __LINE__
                                   << ": internal error: " << int(cpi.type)
                                   << " is not a valid parameter type");
                ok = false;
            }
        }
        else
        {
            BOOST_TEST_MESSAGE(__FILE__ << ":" << __LINE__ << ": "
                               << param_name
                               << " is not a valid parameter name");
            ok = false;
        }
    }
    catch (...)
    {
        BOOST_TEST_MESSAGE(__FILE__ << ":" << __LINE__ << ": "
                           << param_value
                           << " is not a valid parameter value for "
                           << param_name);
        ok = false;
    }

    return ok;
}

void
TestClientImpl::set_session_port(uint16_t session_port)
{
    parse_parameter("session-port", std::to_string(session_port).c_str());
}

// See similar function NetUtils::get_ip_addr_from_iface()
void
TestClientImpl::set_discovery_iface(bool want_ipv4_addr)
{
    struct ifaddrs * ifaddr = NULL, *ifa = NULL;
    char * ifname = NULL;
    unsigned int search_flags = IFF_UP;
    int addr_family = want_ipv4_addr ? AF_INET : AF_INET6;

    int r = getifaddrs(&ifaddr);
    BOOST_REQUIRE(r == 0);

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL)
        {
            continue;
        }

        if (((ifa->ifa_flags & search_flags) == search_flags) &&
                (ifa->ifa_addr->sa_family == addr_family))
        {
            if (addr_family == AF_INET)
            {
                ifname = ifa->ifa_name;
                break;
            }
            else // AF_INET6
            {
                struct sockaddr_in6 * sa_ipv6 =
                    (struct sockaddr_in6 *)(ifa->ifa_addr);

                // If this is a link-local address, we'll take it.

                if (IN6_IS_ADDR_LINKLOCAL(&sa_ipv6->sin6_addr))
                {
                    ifname = ifa->ifa_name;
                    break;
                }
            } // else AF_INET6
        } // if interface name and address family match what we want
    } // for each interface returned by getifaddrs

    BOOST_REQUIRE(ifname != NULL);

    parse_parameter("discovery-iface", ifname);
    freeifaddrs(ifaddr);
}

// See similar function NetUtils::get_ip_addr_from_iface()
void
TestClientImpl::set_loopback_iface(std::string & param_name)
{
    struct ifaddrs * ifaddr = NULL, *ifa = NULL;
    char * ifname = NULL;
    unsigned int search_flags = IFF_LOOPBACK;

    int r = getifaddrs(&ifaddr);
    BOOST_REQUIRE(r == 0);

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL)
        {
            continue;
        }

        if (((ifa->ifa_flags & search_flags) == search_flags))
        {
            ifname = ifa->ifa_name;
            break;
        }
    } // for each interface returned by getifaddrs

    BOOST_REQUIRE(ifname != NULL);

    BOOST_TEST_MESSAGE(__FILE__ << ":" << __LINE__
                       << ": loopback interface name=" << ifname);
    parse_parameter(param_name.c_str(), ifname);
    freeifaddrs(ifaddr);
}

void
TestClientImpl::peer_up(const LLDLEP::PeerInfo & peer_info)
{
    peer_up_waiter.notify(peer_info);
}

void
TestClientImpl::peer_update(const std::string & peer_id,
                            const LLDLEP::DataItems &  /*data_items*/)
{
    peer_update_waiter.notify(peer_id);
}

void TestClientImpl::peer_down(const std::string & peer_id)
{
    peer_down_waiter.notify(peer_id);
}

std::string
TestClientImpl::destination_up(const std::string &  /*peer_id*/,
                               const LLDLEP::DlepMac & mac_address,
                               const LLDLEP::DataItems &  /*data_items*/)
{
    destination_up_waiter.notify(mac_address);
    return "";
}

void
TestClientImpl::destination_update(const std::string &  /*peer_id*/,
                                   const LLDLEP::DlepMac & mac_address,
                                   const LLDLEP::DataItems &  /*data_items*/)
{
    destination_update_waiter.notify(mac_address);
}

void
TestClientImpl::destination_down(const std::string &  /*peer_id*/,
                                 const LLDLEP::DlepMac & mac_address)
{
    destination_down_waiter.notify(mac_address);
}

void
TestClientImpl::linkchar_request(const std::string &  /*peer_id*/,
                                 const LLDLEP::DlepMac &  /*mac_address*/,
                                 const LLDLEP::DataItems &  /*data_items*/)
{
}

void
TestClientImpl::linkchar_reply(const std::string &  /*peer_id*/,
                               const LLDLEP::DlepMac &  /*mac_address*/,
                               const LLDLEP::DataItems &  /*data_items*/)
{
}
