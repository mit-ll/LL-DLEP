/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016, 2018 Massachusetts Institute of Technology
 */

/// @file
/// class DlepClientImpl is an example implementation of the DlepClient interface.
///
/// The methods in this class are called by the Dlep library to get configuration
/// information and to inform the client (library user) of DLEP-related events.

#include "DlepInit.h"
#include "ExampleDlepClientImpl.h"
#include "Table.h"
#include "build-defs.h" // for INSTALL_CONFIG

#include <boost/lexical_cast.hpp>
#include <algorithm> // for std::max
#include <iostream>
#include <iomanip> // for std::setw
#include <libxml/parser.h> // for xmlDocPtr, xmlNodePtr


DlepClientImpl::DlepClientImpl() :
    info_color("\x1b[32m"),  // green
    error_color("\x1b[31m"), // red
    lib_color("\x1b[36m"),  // cyan
    reset_color("\x1b[0m"), // default
    dlep_service(nullptr)

{
    // If we want to have an option to disable colorized output, it
    // could just set the *_color strings to "".
}

/// Configuration parameter definitions and default values.
/// If any of these change, be sure to update doc/doxygen/markdown/UserGuide.md.
DlepClientImpl::ConfigParameterMapType DlepClientImpl::param_info =
{
    {
        "ack-timeout",              {
            ConfigParameterInfo::ParameterType::UnsignedInteger,
            "3",
            "Seconds to wait for ACK signals"
        }
    },
    {
        "ack-probability",          {
            ConfigParameterInfo::ParameterType::UnsignedInteger,
            "100",
            "Probability (%) of sending required ACK signals (for testing)"
        }
    },
    {
        "config-file",              {
            ConfigParameterInfo::ParameterType::ConfigFile,
            "",
            "XML config file containing parameter settings"
        }
    },
    {
        "destination-advert-enable", {
            ConfigParameterInfo::ParameterType::Boolean,
            "0",
            "Should the modem run the Destination Advertisement protocol?"
        }
    },
    {
        "destination-advert-send-interval", {
            ConfigParameterInfo::ParameterType::UnsignedInteger,
            "5",
            "Time between sending Destination Advertisements"
        }
    },
    {
        "destination-advert-mcast-address",  {
            ConfigParameterInfo::ParameterType::IPAddress,
            "225.6.7.8",
            "address to send Destination Advertisements to"
        }
    },
    {
        "destination-advert-port",   {
            ConfigParameterInfo::ParameterType::UnsignedInteger,
            "33445",
            "UDP Port to send Destination Advertisements to"
        }
    },
    {
        "destination-advert-hold-interval", {
            ConfigParameterInfo::ParameterType::UnsignedInteger,
            "0",
            "Time to wait for Destination Advertisement after destination up"
        }
    },
    {
        "destination-advert-expire-count", {
            ConfigParameterInfo::ParameterType::UnsignedInteger,
            "0",
            "Time to keep Destination Advertisements"
        }
    },
    {
        "destination-advert-rf-id", {
            ConfigParameterInfo::ParameterType::ListOfUnsignedInteger,
            "",
            "RF ID of the local modem"
        }
    },
    {
        "destination-advert-iface", {
            ConfigParameterInfo::ParameterType::String,
            "emane0",
            "Interface that the destination discovery protocol uses, rf interface"
        }
    },
    {
        "discovery-iface",          {
            ConfigParameterInfo::ParameterType::String,
            "eth0",
            "Interface that the router uses for the PeerDiscovery protocol"
        }
    },
    {
        "discovery-interval",       {
            ConfigParameterInfo::ParameterType::UnsignedInteger,
            "60",
            "Time between sending PeerDiscovery signals"
        }
    },
    {
        "discovery-mcast-address",  {
            ConfigParameterInfo::ParameterType::IPAddress,
            "224.0.0.117",
            "address to send PeerDiscovery signals to"
        }
    },
    {
        "discovery-port",           {
            ConfigParameterInfo::ParameterType::UnsignedInteger,
            "854",
            "UDP Port to send PeerDiscovery signals to"
        }
    },
    {
        "discovery-ttl",           {
            ConfigParameterInfo::ParameterType::UnsignedInteger,
            "255",
            "IP TTL to use on PeerDiscovery signals"
        }
    },
    {
        "discovery-enable",         {
            ConfigParameterInfo::ParameterType::Boolean,
            "1",
            "Should the router run the PeerDiscovery protocol?"
        }
    },
    {
        "heartbeat-interval",       {
            ConfigParameterInfo::ParameterType::UnsignedInteger,
            "60",
            "Time between sending Heartbeat signals"
        }
    },
    {
        "heartbeat-threshold",      {
            ConfigParameterInfo::ParameterType::UnsignedInteger,
            "4",
            "Number of missed Heartbeats to tolerate"
        }
    },
    {
        "linkchar-autoreply",               {
            ConfigParameterInfo::ParameterType::Boolean,
            "1",
            "Automatically send reply to linkchar requests?",
        }
    },
    {
        "local-type",               {
            ConfigParameterInfo::ParameterType::String,
            "modem",
            "Which DLEP role to play, modem or router?"
        }
    },
    {
        "log-level",                {
            ConfigParameterInfo::ParameterType::UnsignedInteger,
            "3",
            "1=most logging, 5=least"
        }
    },
    {
        "log-file",                 {
            ConfigParameterInfo::ParameterType::String,
            "dlep.log",
            "File to write log messages to"
        }
    },
    {
        "peer-type",                {
            ConfigParameterInfo::ParameterType::String,
            "",
            "Peer Type data item value"
        }
    },
    {
        "peer-flags",                {
            ConfigParameterInfo::ParameterType::UnsignedInteger,
            "0",
            "Flags field value of Peer Type data item"
        }
    },
    {
        "protocol-config-file",              {
            ConfigParameterInfo::ParameterType::String,
            INSTALL_CONFIG "/dlep-rfc-8175.xml",
            "XML file containing DLEP protocol configuration"
        }
    },
    {
        "protocol-config-schema",              {
            ConfigParameterInfo::ParameterType::String,
            INSTALL_CONFIG "/protocol-config.xsd",
            "XML schema file for protocol-config-file"
        }
    },
    {
        "send-tries",               {
            ConfigParameterInfo::ParameterType::UnsignedInteger,
            "3",
            "Number of times to send a signal before giving up"
        }
    },
    {
        "session-address",             {
            ConfigParameterInfo::ParameterType::IPAddress,
            "",
            "IP address that the modem listens on for session connections"
        }
    },
    {
        "session-iface",             {
            ConfigParameterInfo::ParameterType::String,
            "",
            "Interface that the router uses for session connections"
        }
    },
    {
        "session-port",             {
            ConfigParameterInfo::ParameterType::UnsignedInteger,
            "854",
            "TCP port number that the modem listens on for session connections"
        }
    },
    {
        "session-ttl",           {
            ConfigParameterInfo::ParameterType::UnsignedInteger,
            "255",
            "IP TTL to use on session connections"
        }
    },
};

void
DlepClientImpl::get_config_parameter(const std::string & parameter_name,
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
DlepClientImpl::get_config_parameter(const std::string & parameter_name,
                                     bool * value)
{
    ConfigValue v;
    get_config_parameter(parameter_name, &v);
    *value = boost::get<bool>(v);
}

void
DlepClientImpl::get_config_parameter(const std::string & parameter_name,
                                     unsigned int * value)
{
    ConfigValue v;
    get_config_parameter(parameter_name, &v);
    *value = boost::get<unsigned int>(v);
}

void
DlepClientImpl::get_config_parameter(const std::string & parameter_name,
                                     std::string * value)
{
    ConfigValue v;
    get_config_parameter(parameter_name, &v);
    *value = boost::get<std::string>(v);
}

void
DlepClientImpl::get_config_parameter(const std::string & parameter_name,
                                     boost::asio::ip::address * value)
{
    ConfigValue v;
    get_config_parameter(parameter_name, &v);
    *value = boost::get<boost::asio::ip::address>(v);
}

void
DlepClientImpl::get_config_parameter(const std::string & parameter_name,
                                     std::vector<unsigned int> * value)
{
    ConfigValue v;
    get_config_parameter(parameter_name, &v);
    *value = boost::get<std::vector<unsigned int> >(v);
}

bool
DlepClientImpl::parse_args(int argc, char ** argv)
{
    int argi = 1; // index into argv
    bool ok = true; // set to false if errors found

    while (argi + 1 < argc)
    {
        if (! parse_parameter(argv[argi], argv[argi + 1]))
        {
            ok = false;
            // keep parsing in case there are other errors
        }
        argi += 2;
    }
    if (argi != argc)
    {
        std::cerr << error_color << "extraneous argument " << argv[argi]
                  << std::endl;
        ok = false;
    }

    return ok;
}

bool
DlepClientImpl::load_defaults()
{
    for (auto const & param_item : param_info)
    {
        auto findval = config_map.find(param_item.first);

        // If there was no value for this parameter, set it
        // to the default.
        if (findval == config_map.end())
        {
            const std::string & default_value =
                param_item.second.default_value;

            // There may not even be a default value.
            if (default_value.length() > 0)
            {
                if (! parse_parameter(param_item.first.c_str(),
                                      default_value.c_str()))
                {
                    return false;
                }
            }
        }
    }

    return true;
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
DlepClientImpl::print_config() const
{
    std::cout << info_color << "Configuration parameters:" << std::endl;
    for (auto const & config_item : config_map)
    {
        std::cout << config_item.first << " = "
                  << boost::apply_visitor(print_config_visitor(),
                                          config_item.second)
                  << std::endl;
    }
}

void
DlepClientImpl::usage(const char * progname) const
{
    std::cerr << reset_color << "Usage: " << progname << " [parameters]"
              << std::endl
              << "Any of these parameters can appear either on the command "
              "line or in the config file:" << std::endl;

    Table table(std::vector<std::string>
    {"Parameter name", "Default", "Description"});

    for (auto const & param_item : param_info)
    {
        table.add_field(param_item.first);
        table.add_field(param_item.second.default_value);
        table.add_field(param_item.second.description);
        table.finish_row();
    }
    table.print(std::cerr);
}

bool
DlepClientImpl::parse_config_file(const char * config_filename)
{
    xmlDocPtr doc;
    xmlNodePtr cur;

    // std::cout << "parameter config file is " << config_filename << std::endl;

    // Use the xml2 parser to do the heavy lifting.
    doc = xmlParseFile(config_filename);

    if (doc == nullptr)
    {
        std::cerr << error_color << config_filename
                  << " config file was not parsed successfully" << std::endl;
        return false;
    }

    cur = xmlDocGetRootElement(doc);

    if (cur == nullptr)
    {
        std::cerr << error_color << config_filename
                  << "config file was empty" << std::endl;
        xmlFreeDoc(doc);
        return false;
    }

    if (xmlStrcmp(cur->name, (const xmlChar *) "config"))
    {
        std::cerr << error_color << config_filename
                  << "config file must begin with <config>" << std::endl;
        xmlFreeDoc(doc);
        return false;
    }

    // set to false if any errors occur
    bool ok = true;

    // Loop over the document nodes looking for parameters

    for (cur = cur->xmlChildrenNode;
            cur != nullptr;
            cur = cur->next)
    {
        // std::cout << "node " << cur->name << std::endl;

        // look for a <params> section
        if ((!xmlStrcmp(cur->name, (const xmlChar *)"params")))
        {
            for (xmlNodePtr param_node = cur->xmlChildrenNode;
                    param_node != nullptr;
                    param_node = param_node->next)
            {
                // Ignore "text" nodes.  They don't seem to be needed.
                // Also ignore "comment" nodes.
                if ((!xmlStrcmp(param_node->name, (const xmlChar *)"text")) ||
                    (!xmlStrcmp(param_node->name, (const xmlChar *)"comment")))
                {
                    continue;
                }

                std::string param_value;

                // This is a simple non-list value.
                char * xml_node_value = (char *)
                                        xmlNodeListGetString(doc, param_node->xmlChildrenNode, 1);
                param_value = std::string(xml_node_value);
                xmlFree(xml_node_value);

                if (! parse_parameter((char *)param_node->name,
                                      (char *)param_value.c_str()))
                {
                    ok = false;
                }
            }
        }
    }

    xmlFreeDoc(doc);

    return ok;
}


bool
DlepClientImpl::parse_parameter(const char * param_name,
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
                    // skip over commas between numbers
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
                std::cerr << error_color << "internal error: " << int(cpi.type)
                          << " is not a valid parameter type" << std::endl;
                ok = false;
            }
        }
        else
        {
            std::cerr << error_color << param_name
                      << " is not a valid parameter name" << std::endl;
            ok = false;
        }
    }
    catch (...)
    {
        std::cerr << error_color <<  param_value
                  << " is not a valid parameter value for "
                  << param_name << std::endl;
        ok = false;
    }

    return ok;
}

void
DlepClientImpl::set_destination_response(const LLDLEP::DlepMac & mac_address,
                                         const std::string & status_code)
{
    destination_responses[mac_address] = status_code;
}

void
DlepClientImpl::set_dlep_service(LLDLEP::DlepService *d)
{
    dlep_service = d;
}

void
DlepClientImpl::print_data_items(const std::string & msg,
                                 const LLDLEP::DataItems & data_items)
{
    if ((!data_items.empty()) && (msg.length() > 0))
    {
        std::cout << msg << std::endl;
    }

    for (const auto & di : data_items)
    {
        std::cout << di.to_string() << std::endl;
    }
}

void
DlepClientImpl::print_peer_info(const LLDLEP::PeerInfo & peer_info)
{
    std::cout << lib_color << "peer id = " << peer_info.peer_id  << std::endl
              << "type = " << peer_info.peer_type << std::endl
              << "heartbeat interval = " << peer_info.heartbeat_interval
              << std::endl;

    if (!peer_info.extensions.empty())
    {
        std::cout << "extensions = ";
        std::string comma = "";
        for (auto extid : peer_info.extensions)
        {
            // We cast extid to int because when ExtensionId is a uint8,
            // that's the same as an unsigned char and will be printed
            // as such without the cast.
            std::cout << comma << int(extid);
            comma = ",";
        }
        std::cout << std::endl;
    }

    for (std::string ename : peer_info.experiment_names)
    {
        std::cout << "experiment name = " << ename << std::endl;
    }

    print_data_items("default metrics:", peer_info.data_items);

    if (!peer_info.destinations.empty())
    {
        std::cout << "destinations:" << std::endl;
        for (auto mac : peer_info.destinations)
        {
            std::cout << mac << std::endl;
        }
    }
}

void
DlepClientImpl::peer_up(const LLDLEP::PeerInfo & peer_info)
{
    std::cout << lib_color << "\nPeer up, ";

    print_peer_info(peer_info);
}

void
DlepClientImpl::peer_update(const std::string & peer_id,
                            const LLDLEP::DataItems & data_items)
{
    std::cout << lib_color << "\nPeer update, peer = " << peer_id << std::endl;
    print_data_items("", data_items);
}

void DlepClientImpl::peer_down(const std::string & peer_id)
{
    std::cout << lib_color << "\nPeer down, peer id = " << peer_id
              << reset_color << std::endl;
}

std::string
DlepClientImpl::destination_up(const std::string & peer_id,
                               const LLDLEP::DlepMac & mac_address,
                               const LLDLEP::DataItems & data_items)
{
    std::cout << lib_color << "\nDestination up, peer = " << peer_id
              << " mac = " << mac_address << std::endl;
    print_data_items("", data_items);

    std::string statusname = LLDLEP::ProtocolStrings::Success;
    const auto & it = destination_responses.find(mac_address);
    if (it != destination_responses.end())
    {
        statusname = it->second;
    }
    std::cout << "Responding with status = " << statusname << "\n";
    return statusname;
}

void
DlepClientImpl::destination_update(const std::string & peer_id,
                                   const LLDLEP::DlepMac & mac_address,
                                   const LLDLEP::DataItems & data_items)
{
    std::cout << lib_color << "\nDestination update, peer = " << peer_id
              << " mac = " << mac_address << std::endl;
    print_data_items("", data_items);
}

void
DlepClientImpl::destination_down(const std::string & peer_id,
                                 const LLDLEP::DlepMac & mac_address)
{
    std::cout << lib_color << "\nDestination down, peer = " << peer_id
              << " mac = " << mac_address << std::endl;
}

void
DlepClientImpl::linkchar_request(const std::string & peer_id,
                                 const LLDLEP::DlepMac & mac_address,
                                 const LLDLEP::DataItems & data_items)
{
    std::cout << lib_color << "\nLinkchar request, peer = " << peer_id
              << ", mac = " << mac_address << std::endl;
    print_data_items("", data_items);

    bool autoreply = false;
    get_config_parameter("linkchar-autoreply", &autoreply);
    if (autoreply)
    {
        dlep_service->linkchar_reply(peer_id, mac_address, data_items);
    }
}

void
DlepClientImpl::linkchar_reply(const std::string & peer_id,
                                 const LLDLEP::DlepMac & mac_address,
                                 const LLDLEP::DataItems & data_items)
{
    std::cout << lib_color << "\nLinkchar reply, peer = " << peer_id
              << ", mac = " << mac_address << std::endl;
    print_data_items("", data_items);
}
