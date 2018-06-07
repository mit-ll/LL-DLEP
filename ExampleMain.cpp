/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016, 2018 Massachusetts Institute of Technology
 */

/// @file
/// Example client program that acts as a driver for the Dlep service library.
///
/// This program initializes the library, then drops into a command-line
/// interface that lets the user exercise various parts of the Dlep service
/// API.

#include "DlepInit.h"
#include "ExampleDlepClientImpl.h"
#include "Table.h"

// We use the readline library for interactive input and line editing
#include <readline/readline.h>
#include <readline/history.h>

#include <boost/lexical_cast.hpp>
#include <algorithm> // for std::max
#include <iostream>
#include <iomanip> // for std::setw

class DlepCli
{
public:
    DlepCli(DlepClientImpl * client, LLDLEP::DlepService * service) :
        dlep_client(client),
        dlep_service(service),
        cli_continue(true),
        help_dataitems_printed(false)
    {
        command_info =
        {
            {
                "help",     &DlepCli::help_help,
                &DlepCli::handle_help
            },
            {
                "quit",     &DlepCli::help_quit,
                &DlepCli::handle_quit
            },
            {
                "dest",     &DlepCli::help_destination,
                &DlepCli::handle_destination
            },
            {
                "linkchar",     &DlepCli::help_linkchar,
                &DlepCli::handle_linkchar
            },
            {
                "peer",     &DlepCli::help_peer,
                &DlepCli::handle_peer
            },
            {
                "set",      &DlepCli::help_set,
                &DlepCli::handle_set
            },
            {
                "show",     &DlepCli::help_show,
                &DlepCli::handle_show
            },
        };

        // Get protocol configuration info

        protocfg = dlep_service->get_protocol_config();
        data_item_info = protocfg->get_data_item_info();

        // Tell the client object about the service object so that
        // client methods can call service methods as needed (rare).

        dlep_client->set_dlep_service(dlep_service);
    }

    ~DlepCli() {}

    void run()
    {
        std::cout << "Use 'help' to see a list of possible commands."
                  << std::endl;

        // Set the CLI prompt to the value of the peer-type config parameter,
        // or if that doesn't exist, to the value of the local-type parameter
        std::string prompt;
        try
        {
            dlep_client->get_config_parameter("peer-type", &prompt);
        }
        catch (LLDLEP::DlepClient::BadParameterName)
        {
            dlep_client->get_config_parameter("local-type", &prompt);
        }
        prompt = dlep_client->reset_color + prompt + "> ";

        // read and execute commands one line at a time
        cli_continue = true;
        char * buffer;
        while (cli_continue && (buffer = readline(prompt.c_str())))
        {
            help_dataitems_printed = false;

            if (buffer[0] != '\0')
            {
                add_history(buffer);
            }
            // convert from C-style string to C++ string
            line = std::string(buffer);

            // we have a copy in line now, so we're done with buffer
            free(buffer);

            tokenize();

            if (tokens.empty())
            {
                // line was blank or contained only whitespace
                continue;
            }

            std::string cmd_token = *current_token++;
            bool handler_found = false;

            for (CommandInfo ci : command_info)
            {
                if (0 == cmd_token.compare(ci.cmd_name))
                {
                    handler_found = true;
                    (this->*ci.cmd_handler)(ci.cmd_name);
                    break;
                }
            }
            if (! handler_found)
            {
                std::cerr << dlep_client->error_color
                          << "command " << cmd_token << " not recognized"
                          << std::endl;
            }
        } // end while reading lines
    }

private:

    DlepClientImpl * dlep_client;
    LLDLEP::DlepService * dlep_service;
    LLDLEP::ProtocolConfig * protocfg;
    std::vector<LLDLEP::DataItemInfo> data_item_info;

    /// line of input from the user
    std::string line;

    std::vector<std::string> tokens;
    std::vector<std::string>::iterator current_token;

    bool cli_continue;

    // Has the help blurb for dataitems been printed?  This is only needed
    // when we're printing a help message, to avoid printing the same
    // information more than once.
    bool help_dataitems_printed;

    void tokenize()
    {
        std::string ws(" \t"); // whitespace characters

        // Clear out any previous tokens.
        tokens.clear();

        /// current parsing location in line
        std::string::size_type line_cursor = 0;

        for (;;)
        {
            // skip whitespace in front of next token
            line_cursor = line.find_first_not_of(ws, line_cursor);

            // if end of line, we're done
            if (line_cursor == std::string::npos)
            {
                break;
            }

            // find first whitespace after this token
            std::string::size_type end_of_token =
                line.find_first_of(ws, line_cursor);

            // find length of token
            std::string::size_type token_size;

            if (end_of_token == std::string::npos)
            {
                token_size = line.size() - line_cursor;
            }
            else
            {
                token_size = end_of_token - line_cursor;
            }

            // extract the token and add it to tokens[]
            std::string one_token = line.substr(line_cursor, token_size);
            tokens.push_back(one_token);

            // set up to look for next token
            line_cursor = end_of_token;
        }

        current_token = tokens.begin();
    }

    bool parse_mac_address(LLDLEP::DlepMac * dlep_mac)
    {
        if (current_token == tokens.end())
        {
            std::cerr << dlep_client->error_color
                      << "missing MAC address" << std::endl;
            return false;
        }

        std::string mac_token = *current_token++;

        LLDLEP::DataItem di_mac(LLDLEP::ProtocolStrings::MAC_Address, protocfg);

        bool valid = true;
        try
        {
            di_mac.value_from_string(mac_token);
            try
            {
                LLDLEP::DlepMac mac = boost::get<LLDLEP::DlepMac>(di_mac.value);
                *dlep_mac = mac;
            }
            catch (boost::bad_get)
            {
                valid = false;
                std::cerr << dlep_client->error_color
                          << "internal error converting " << mac_token
                          << " to a MAC address"
                          << std::endl;
            }
        }
        catch (std::invalid_argument)
        {
            valid = false;
            std::cerr << dlep_client->error_color
                      << mac_token << " is not a valid MAC address"
                      << std::endl;
        }

        return valid;
    }

    static void
    skip_whitespace(std::istringstream & ss)
    {
        while (ss && isspace(ss.peek()))
        {
            ss.ignore();
        }
    }

    bool parse_data_items(LLDLEP::DataItems & data_items)
    {
        std::string rest_of_line;

        // Make a string containing all of the remaining tokens,
        // separated by spaces.  This is more convenient for
        // from_istringstream().  The stringstream will keep
        // track of where we are in parsing the line.

        while (current_token != tokens.end())
        {
            rest_of_line.append(*current_token++);
            rest_of_line.append(" ");
        }

        std::istringstream ss(rest_of_line);
        try
        {
            do
            {
                skip_whitespace(ss);
                if (ss.eof())
                {
                    break;
                }
                LLDLEP::DataItem di(protocfg);
                di.from_istringstream(ss);
                data_items.push_back(di);
            }
            while (ss);
        }
        catch (LLDLEP::ProtocolConfig::BadDataItemName e)
        {
            std::cerr << dlep_client->error_color
                      << e.what() << " is not a valid data item name."
                      << " Try 'show dataitems'" << std::endl;
            return false;
        }
        catch (std::invalid_argument e)
        {
            std::cerr << dlep_client->error_color << e.what() << std::endl;
            return false;
        }

        return true;
    }

    /// Pointer to member function for a command handler
    typedef void (DlepCli::*CommandHandler)(const std::string & cmd_name);

    struct CommandInfo
    {
        std::string cmd_name;
        CommandHandler help_handler;
        CommandHandler cmd_handler;
    };

    std::vector<CommandInfo> command_info;

    // command handlers

    void print_dlep_service_return_status(LLDLEP::DlepService::ReturnStatus r)
    {
        std::cout << ((r == LLDLEP::DlepService::ReturnStatus::ok) ?
                      dlep_client->info_color : dlep_client->error_color);
        std::cout << "DlepService returns: ";
        std::string m;
        switch (r)
        {
            case LLDLEP::DlepService::ReturnStatus::ok:
                m = "ok";
                break;
            case LLDLEP::DlepService::ReturnStatus::invalid_data_item:
                m = "invalid data item";
                break;
            case LLDLEP::DlepService::ReturnStatus::invalid_mac_address:
                m = "invalid mac address";
                break;
            case LLDLEP::DlepService::ReturnStatus::destination_exists:
                m = "destination exists";
                break;
            case LLDLEP::DlepService::ReturnStatus::destination_does_not_exist:
                m = "destination does not exist";
                break;
            case LLDLEP::DlepService::ReturnStatus::peer_does_not_exist:
                m = "peer does not exist";
                break;
        }
        std::cout << m << std::endl;
    }

    void help_help(const std::string & cmd_name)
    {
        std::cout << cmd_name << " : print a help message" << std::endl;
    }

    // cppcheck-suppress unusedFunction
    void handle_help(const std::string &  /*cmd_name*/)
    {
        std::cout << dlep_client->info_color;
        for (CommandInfo ci : command_info)
        {
            (this->*ci.help_handler)(ci.cmd_name);
        }
    }

    void help_quit(const std::string & cmd_name)
    {
        std::cout << cmd_name << " : exit the program cleanly" << std::endl;
    }

    // cppcheck-suppress unusedFunction
    void handle_quit(const std::string &  /*cmd_name*/)
    {
        cli_continue = false;
    }

    void help_destination(const std::string & cmd_name)
    {
        help_destination_up(cmd_name);
        help_destination_update(cmd_name);
        help_destination_down(cmd_name);
        help_destination_response(cmd_name);
    }

    // cppcheck-suppress unusedFunction
    void handle_destination(const std::string & cmd_name)
    {
        if (tokens.size() < 2)
        {
            help_destination(cmd_name);
            return;
        }

        std::string & subcommand = *current_token++;

        if (subcommand == "up")
        {
            handle_destination_up(cmd_name);
        }
        else if (subcommand == "update")
        {
            handle_destination_update(cmd_name);
        }
        else if (subcommand == "down")
        {
            handle_destination_down(cmd_name);
        }
        else if (subcommand == "response")
        {
            handle_destination_response(cmd_name);
        }
        else
        {
            std::cerr << dlep_client->error_color
                      << "invalid subcommand " << subcommand << " for "
                      << cmd_name << std::endl;
            help_destination(cmd_name);
        }
    }

    void help_dataitems()
    {
        if (help_dataitems_printed)
        {
            return;
        }

        std::cout <<
            "    [data-item-name data-item-value] can be repeated to specify multiple data items\n"
            "    data-item-names come from 'show dataitems'\n";
        help_dataitems_printed = true;
    }

    void help_destination_up(const std::string & cmd_name)
    {
        std::cout << cmd_name << " up mac-address [data-item-name data-item-value]...\n"
                  "    declare a destination to be up (modem) or announced (router)\n";
        help_dataitems();
    }

    void handle_destination_up(const std::string & cmd_name, bool update = false)
    {
        LLDLEP::DlepMac mac_address;

        if (tokens.size() < 3)
        {
            help_destination_up(cmd_name);
            return;
        }

        if (! parse_mac_address(&mac_address))
        {
            return;
        }

        LLDLEP::DataItems data_items;
        if (! parse_data_items(data_items))
        {
            return;
        }
        // dlep_client->print_data_items("sending data items:", data_items);

        LLDLEP::DlepService::ReturnStatus r;

        if (update)
        {
            r = dlep_service->destination_update(mac_address, data_items);
        }
        else // this is not an update, so it must be an up
        {
            r = dlep_service->destination_up(mac_address, data_items);
        }

        print_dlep_service_return_status(r);
    }

    void help_destination_update(const std::string & cmd_name)
    {
        std::cout << cmd_name <<
                  " update mac-address [data-item-name data-item-value]...\n"
                  "    update attributes of an existing (up) destination\n";
        help_dataitems();
    }

    void handle_destination_update(const std::string & cmd_name)
    {
        if (tokens.size() < 3)
        {
            help_destination_update(cmd_name);
            return;
        }

        handle_destination_up(cmd_name, true);
    }

    void help_destination_down(const std::string & cmd_name)
    {
        std::cout << cmd_name << " down mac-address" << std::endl;
        std::cout << "    declare a destination to be down" << std::endl;
    }

    void handle_destination_down(const std::string & cmd_name)
    {
        LLDLEP::DlepMac mac_address;

        if (tokens.size() != 3)
        {
            help_destination_down(cmd_name);
            return;
        }

        if (! parse_mac_address(&mac_address))
        {
            return;
        }

        LLDLEP::DlepService::ReturnStatus r =
            dlep_service->destination_down(mac_address);
        print_dlep_service_return_status(r);
    }

    void help_destination_response(const std::string & cmd_name)
    {
        std::cout << cmd_name << " response mac-address status-code-name" << std::endl;
        std::cout << "    define response to a future destination up for mac-address" << std::endl;
        std::cout << "    status-code-name comes from 'show statuscodes'"  << std::endl;
    }

    void handle_destination_response(const std::string & cmd_name)
    {
        LLDLEP::DlepMac mac_address;

        if (tokens.size() != 4)
        {
            help_destination_response(cmd_name);
            return;
        }

        if (! parse_mac_address(&mac_address))
        {
            return;
        }

        // See if we got a valid status code

        std::string & statusname = *current_token++;
        try
        {
            (void)protocfg->get_status_code_id(statusname);
        }
        catch (LLDLEP::ProtocolConfig::BadStatusCodeName)
        {
            std::cout << dlep_client->error_color
                      << statusname << " is not a valid status code name."
                      << " Try 'show statuscodes'" << std::endl;
            return;
        }

        dlep_client->set_destination_response(mac_address, statusname);
    }

    void help_linkchar_request(const std::string & cmd_name)
    {
        std::cout << cmd_name
                  << " request mac-address [data-item-name data-item-value]...\n"
                  "    request link characteristics for a destination\n";
        help_dataitems();
    }

    void help_linkchar_reply(const std::string & cmd_name)
    {
        std::cout << cmd_name
                  << " reply peer_id mac-address [data-item-name data-item-value]...\n"
                  "    reply with link characteristics for a destination\n";
        help_dataitems();
    }

    void help_linkchar(const std::string & cmd_name)
    {
        help_linkchar_request(cmd_name);
        help_linkchar_reply(cmd_name);
    }

    void handle_linkchar_request(const std::string &  /*cmd_name*/)
    {
        LLDLEP::DlepMac mac_address;
        if (! parse_mac_address(&mac_address))
        {
            return;
        }

        LLDLEP::DataItems data_items;
        if (! parse_data_items(data_items))
        {
            return;
        }

        LLDLEP::DlepService::ReturnStatus r =
            dlep_service->linkchar_request(mac_address, data_items);
        print_dlep_service_return_status(r);
    }

    void handle_linkchar_reply(const std::string & cmd_name)
    {
        if (tokens.size() < 3)
        {
            help_linkchar_reply(cmd_name);
            return;
        }

        std::string peer_id = *current_token++;

        LLDLEP::DlepMac mac_address;
        if (! parse_mac_address(&mac_address))
        {
            return;
        }

        LLDLEP::DataItems data_items;
        if (! parse_data_items(data_items))
        {
            return;
        }

        LLDLEP::DlepService::ReturnStatus r =
            dlep_service->linkchar_reply(peer_id, mac_address, data_items);
        print_dlep_service_return_status(r);
    }

    void handle_linkchar(const std::string & cmd_name)
    {
        if (tokens.size() < 2)
        {
            help_linkchar(cmd_name);
            return;
        }

        std::string subcommand = *current_token++;

        if (subcommand == "request")
        {
            handle_linkchar_request(cmd_name);
        }
        else if (subcommand == "reply")
        {
            handle_linkchar_reply(cmd_name);
        }
        else
        {
            std::cerr << dlep_client->error_color
                      << "invalid subcommand " << subcommand << " for "
                      << cmd_name << std::endl;

            help_linkchar(cmd_name);
        }
    }

    void help_peer(const std::string & cmd_name)
    {
        help_peer_update(cmd_name);
    }

    void handle_peer(const std::string & cmd_name)
    {
        if (tokens.size() < 2)
        {
            help_peer(cmd_name);
            return;
        }

        std::string & subcommand = *current_token++;

        if (subcommand == "update")
        {
            handle_peer_update(cmd_name);
        }
        else
        {
            std::cerr << dlep_client->error_color
                      << "invalid subcommand " << subcommand << " for "
                      << cmd_name << std::endl;
            help_peer(cmd_name);
        }
    }

    void help_peer_update(const std::string & cmd_name)
    {
        std::cout << cmd_name << " update [data-item-name data-item-value]...\n"
                  "    update the local peer with data-items and send peer updates to all existing peers\n";
        help_dataitems();
    }

    void handle_peer_update(const std::string &  /*cmd_name*/)
    {
        LLDLEP::DataItems data_items;

        if (! parse_data_items(data_items))
        {
            return;
        }

        LLDLEP::DlepService::ReturnStatus r =
            dlep_service->peer_update(data_items);
        print_dlep_service_return_status(r);
    }

    void help_set(const std::string & cmd_name)
    {
        std::cout << cmd_name << " param-name param-value" << std::endl;
        std::cout << "    set a config parameter value" << std::endl;
    }

    // cppcheck-suppress unusedFunction
    void handle_set(const std::string & cmd_name)
    {
        if (tokens.size() != 3)
        {
            help_set(cmd_name);
            return;
        }

        std::string param_name = *current_token++;
        std::string param_value = *current_token++;

        dlep_client->parse_parameter(param_name.c_str(), param_value.c_str());
    }

    void handle_show_data_item_info()
    {
        std::cout << dlep_client->info_color
                  << "Configured data items:" << std::endl;
        Table table(std::vector<std::string>
                    {"ID", "Name", "Type", "Units", "Module", "Flags",
                     "SubDataItem ID", "SubDataItem Name", "Occurs"});

        for (const auto & di_info : data_item_info)
        {
            if (di_info.id != LLDLEP::IdUndefined)
            {
                table.add_field(std::to_string(di_info.id));
            }
            table.add_field("Name", di_info.name);
            table.add_field(to_string(di_info.value_type));
            table.add_field(di_info.units);
            table.add_field(di_info.module);
            if (di_info.flags &
                    LLDLEP::DataItemInfo::Flags::metric)
            {
                table.add_field("metric");
            }

            // add the sub data items for this data item, if any
            for (const auto & sub_di_info : di_info.sub_data_items)
            {
                table.add_field("SubDataItem ID",
                                std::to_string(sub_di_info.id));
                table.add_field(protocfg->get_data_item_name(sub_di_info.id,
                                                             &di_info));
                table.add_field(sub_di_info.occurs);
                table.finish_row();
            }

            table.finish_row();
        } // end for each data item

        table.print(std::cout);
    }

    void handle_show_signals()
    {
        std::cout << dlep_client->info_color
                  << "Configured signals/messages:" << std::endl;
        Table table(std::vector<std::string>
        {"ID", "Name", "Module", "Response", "Flags", "Data Item", "Occurs"});

        std::vector<LLDLEP::ProtocolConfig::SignalInfo> signal_info =
            protocfg->get_signal_info();

        for (const auto & sig_info : signal_info)
        {
            unsigned int sig_starting_row = table.get_row_index();
            table.add_field(std::to_string(sig_info.id));
            table.add_field(sig_info.name);
            table.add_field(sig_info.module);
            if (sig_info.response_id)
            {
                std::string response_name;

                // We assume the response is the same type as the
                // thing being responded to (message vs. signal).

                if (sig_info.flags & LLDLEP::ProtocolConfig::SignalInfo::Flags::message)
                {
                    response_name = protocfg->get_message_name(sig_info.response_id);
                }
                else
                {
                    response_name = protocfg->get_signal_name(sig_info.response_id);
                }

                table.add_field("Response", response_name);
            }

            // convert flags to a string

            if (sig_info.flags & LLDLEP::ProtocolConfig::SignalInfo::Flags::message)
            {
                table.add_field("Flags", "message");
            }
            else
            {
                table.add_field("Flags", "signal");
            }
            table.finish_row();

            if (sig_info.flags & LLDLEP::ProtocolConfig::SignalInfo::Flags::modem_sends)
            {
                table.add_field("Flags", "modem sends");
                table.finish_row();
            }

            if (sig_info.flags & LLDLEP::ProtocolConfig::SignalInfo::Flags::router_sends)
            {
                table.add_field("Flags", "router sends");
                table.finish_row();
            }

            // add a row for each data item allowed on this signal
            table.set_row_index(sig_starting_row);
            for (const auto & difs : sig_info.data_items)
            {
                table.add_field("Data Item",
                                protocfg->get_data_item_name(difs.id));
                table.add_field(difs.occurs);
                table.finish_row();
            }

            // next signal should appear after everything added so far
            table.set_row_index_end();
        } // end for each signal

        table.print(std::cout);
    }

    void handle_show_modules()
    {
        std::cout << dlep_client->info_color
                  << "Configured modules:" << std::endl;

        Table table(std::vector<std::string>
        {"Name", "Draft", "ExpName", "ExtId", "Provides", "Provided Name"});

        std::vector<LLDLEP::ProtocolConfig::ModuleInfo> module_info =
            protocfg->get_module_info();

        for (const auto & modinfo : module_info)
        {
            table.add_field(modinfo.name);
            table.add_field(modinfo.draft);
            table.add_field(modinfo.experiment_name);
            if (modinfo.extension_id)
            {
                table.add_field(std::to_string(modinfo.extension_id));
            }
            table.finish_row();

            for (const auto & di_name : modinfo.data_items)
            {
                table.add_field("Provides", "data item");
                table.add_field(di_name);
                table.finish_row();
            }

            for (const auto & id : modinfo.signals)
            {
                table.add_field("Provides", "signal");
                table.add_field(protocfg->get_signal_name(id));
                table.finish_row();
            }

            for (const auto & id : modinfo.messages)
            {
                table.add_field("Provides", "message");
                table.add_field(protocfg->get_message_name(id));
                table.finish_row();
            }

            for (const auto & id : modinfo.status_codes)
            {
                table.add_field("Provides", "status code");
                table.add_field(protocfg->get_status_code_name(id));
                table.finish_row();
            }
        }
        table.print(std::cout);
    }

    void handle_show_statuscodes()
    {
        std::cout << dlep_client->info_color
                  << "Configured status codes:" << std::endl;

        Table table(std::vector<std::string> {"ID", "Name", "FailureMode", "Module"});

        std::vector<LLDLEP::ProtocolConfig::StatusCodeInfo> status_code_info =
            protocfg->get_status_code_info();

        for (const auto & sc_info : status_code_info)
        {
            table.add_field(std::to_string(sc_info.id));
            table.add_field(sc_info.name);
            table.add_field(sc_info.failure_mode);
            table.add_field(sc_info.module);
            table.finish_row();
        }
        table.print(std::cout);
    }

    void help_show_peer(const std::string & cmd_name)
    {
        std::cout << cmd_name << " peer [ peer-id ]" << std::endl;
        std::cout << "    without peer-id, lists all peers" << std::endl;
        std::cout << "    with peer-id, prints detailed information about that peer"
                  << std::endl;
    }

    void handle_show_peer(const std::string & cmd_name)
    {
        if (tokens.size() == 2)
        {
            // get a list of all of the peers

            std::vector<std::string> peers;

            LLDLEP::DlepService::ReturnStatus r =
                dlep_service->get_peers(peers);

            std::cout << dlep_client->info_color << "peer ids:" << std::endl;
            for (std::string & peer_id : peers)
            {
                std::cout << peer_id << std::endl;
            }

            print_dlep_service_return_status(r);
        }
        else if (tokens.size() == 3)
        {
            // get info about a specfic peer

            std::string peer_id = *current_token++;
            LLDLEP::PeerInfo peer_info;
            LLDLEP::DlepService::ReturnStatus r =
                dlep_service->get_peer_info(peer_id, peer_info);

            if (r == LLDLEP::DlepService::ReturnStatus::ok)
            {
                dlep_client->print_peer_info(peer_info);
            }

            print_dlep_service_return_status(r);
        }
        else
        {
            help_show_peer(cmd_name);
        }
    }

    void help_show_destination(const std::string & cmd_name)
    {
        std::cout << cmd_name << " dest [ mac-address ]" << std::endl;
        std::cout << "    without mac-address, prints info about all destinations"
                  << std::endl;
        std::cout << "    with mac-address, prints info about that destination"
                  << std::endl;
    }

    void handle_show_destination(const std::string & cmd_name)
    {
        if (tokens.size() > 3)
        {
            help_show_destination(cmd_name);
            return;
        }

        // If the user specified a MAC address on the command line,
        // store it in specific_mac and remember that we have it
        // with have_specific_mac.

        LLDLEP::DlepMac specific_mac;
        bool have_specific_mac = false;

        if (tokens.size() == 3)
        {
            if (! parse_mac_address(&specific_mac))
            {
                return;
            }
            have_specific_mac = true;
        }

        Table table(std::vector<std::string>
        {
            "Destination MAC", "Peer", "Flags", "Data Item Name",
            "Data Item Value"
        });

        // get a list of all of the peers

        std::vector<std::string> peers;
        LLDLEP::DlepService::ReturnStatus r =
            dlep_service->get_peers(peers);
        if (r != LLDLEP::DlepService::ReturnStatus::ok)
        {
            print_dlep_service_return_status(r);
            return;
        }

        // Loop over all the peers

        for (const std::string & peer_id : peers)
        {
            LLDLEP::PeerInfo peer_info;
            r = dlep_service->get_peer_info(peer_id, peer_info);

            if (r != LLDLEP::DlepService::ReturnStatus::ok)
            {
                continue;
            }

            // Loop over all of this peer's destinations

            for (const LLDLEP::DlepMac & mac_address : peer_info.destinations)
            {
                // If we're looking for a specific mac address, and
                // this isn't it, skip to the next destination.

                if (have_specific_mac && !(specific_mac == mac_address))
                {
                    continue;
                }

                // Get information about this destination and add it
                // to the table.

                LLDLEP::DestinationInfo  dest_info;
                r = dlep_service->get_destination_info(peer_id, mac_address,
                                                       dest_info);
                if (r != LLDLEP::DlepService::ReturnStatus::ok)
                {
                    continue;
                }

                table.add_field(dest_info.mac_address.to_string());
                table.add_field(dest_info.peer_id);
                table.add_field(std::to_string(dest_info.flags));

                // Get information about each data item for this
                // destination and add it to the table.

                for (const auto & di : dest_info.data_items)
                {
                    std::string di_name = di.name();

                    // We've already filled in a table column for the
                    // MAC address, so skip that data item (and only
                    // that one) if encountered.

                    if (di_name != LLDLEP::ProtocolStrings::MAC_Address)
                    {
                        table.add_field("Data Item Name", di.name());
                        table.add_field("Data Item Value", di.value_to_string());
                        table.finish_row();
                    }
                } // end for each data item of this destination

                table.finish_row(); // for this destination

            } // end for each destination of this peer
        }  // end for each peer

        std::cout << dlep_client->info_color;
        table.print(std::cout);
        print_dlep_service_return_status(r);
    }

    void help_show(const std::string & cmd_name)
    {
        std::cout << cmd_name
                  << " [ dataitems | config | signals | modules | statuscodes | peer | dest ]"
                  << std::endl;
        std::cout << "    show requested information" << std::endl;
        help_show_peer(cmd_name);
        help_show_destination(cmd_name);
    }

    // cppcheck-suppress unusedFunction
    void handle_show(const std::string & cmd_name)
    {
        if (tokens.size() < 2)
        {
            help_show(cmd_name);
            return;
        }

        std::string subcommand = *current_token++;

        if (subcommand == "dataitems")
        {
            handle_show_data_item_info();
        }
        else if (subcommand == "config")
        {
            dlep_client->print_config();
        }
        else if (subcommand == "signals")
        {
            handle_show_signals();
        }
        else if (subcommand == "modules")
        {
            handle_show_modules();
        }
        else if (subcommand == "statuscodes")
        {
            handle_show_statuscodes();
        }
        else if (subcommand == "peer")
        {
            handle_show_peer(cmd_name);
        }
        else if (subcommand == "dest")
        {
            handle_show_destination(cmd_name);
        }
        else
        {
            std::cerr << dlep_client->error_color
                      << "invalid subcommand " << subcommand << " for "
                      << cmd_name << std::endl;

            help_show(cmd_name);
        }
    }
}; // class DlepCli


// Main program so that we can link an executable program.
int main(int argc, char ** argv)
{
    DlepClientImpl client;

    if (! client.parse_args(argc, argv))
    {
        client.usage(argv[0]);
        exit(1);
    }

    if (! client.load_defaults())
    {
        std::cerr << client.error_color
                  << "Internal error: failed to load default configuration information"
                  << client.reset_color << std::endl;
        exit(2);
    }

    client.print_config();

    LLDLEP::DlepService * dlep_service = LLDLEP::DlepInit(client);

    if (dlep_service)
    {
        std::cout << client.info_color << "DlepInit succeeded" << std::endl;

        DlepCli cli(&client, dlep_service);
        cli.run();
        dlep_service->terminate();
        delete dlep_service;
    }
    else
    {
        // try to get the log file name so we can give a more
        // informative error message

        std::string log_file;
        try
        {
            client.get_config_parameter("log-file", &log_file);
        }
        catch (LLDLEP::DlepClient::BadParameterName) { }

        std::cerr << client.error_color << "DlepInit failed";
        if (log_file.length() > 0)
        {
            std::cerr << ", check log file " << log_file << " for details"
                      << std::endl;
            std::string grepcommand = "grep FATAL: " + log_file;
            system(grepcommand.c_str());
        }
        std::cerr << client.reset_color << std::endl;
        exit(4);
    }
}
