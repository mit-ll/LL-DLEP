/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016, 2018 Massachusetts Institute of Technology
 */

/// @file
/// Example DLEP client implementation class declaration.

#ifndef DLEP_CLIENT_IMPL_H
#define DLEP_CLIENT_IMPL_H

#include "DlepClient.h" // base class
#include "DlepService.h"

/// Example instantiation of the DlepClient interface.
class DlepClientImpl : public LLDLEP::DlepClient
{
public:
    DlepClientImpl();

    /// @see DlepClient for documentation of inherited methods

    void get_config_parameter(const std::string & parameter_name,
                              ConfigValue * value) override;
    void get_config_parameter(const std::string & parameter_name,
                              bool * value) override;
    void get_config_parameter(const std::string & parameter_name,
                              unsigned int * value) override;
    void get_config_parameter(const std::string & parameter_name,
                              std::string * value) override;
    void get_config_parameter(const std::string & parameter_name,
                              boost::asio::ip::address * value) override;
    void get_config_parameter(const std::string & parameter_name,
                              std::vector<unsigned int> * value) override;

    void print_data_items(const std::string & msg,
                          const LLDLEP::DataItems & data_items);

    void print_peer_info(const LLDLEP::PeerInfo & peer_info);

    void peer_up(const LLDLEP::PeerInfo & peer_info) override;

    void peer_update(const std::string & peer_id,
                     const LLDLEP::DataItems & data_items) override;

    void peer_down(const std::string & peer_id) override;

    std::string destination_up(const std::string & peer_id,
                               const LLDLEP::DlepMac & mac_address,
                               const LLDLEP::DataItems & data_items) override;

    void destination_update(const std::string & peer_id,
                            const LLDLEP::DlepMac & mac_address,
                            const LLDLEP::DataItems & data_items) override;

    void destination_down(const std::string & peer_id,
                          const LLDLEP::DlepMac & mac_address) override;

    void linkchar_request(const std::string & peer_id,
                          const LLDLEP::DlepMac & mac_address,
                          const LLDLEP::DataItems & data_items) override;

    void linkchar_reply(const std::string & peer_id,
                        const LLDLEP::DlepMac & mac_address,
                        const LLDLEP::DataItems & data_items) override;

    /// Parse command line arguments.
    /// Loads the configuration database with values from the command line
    /// and any config files that were specified.
    /// @param[in] argc argument count
    /// @param[in] argv array of argument strings
    /// @return false if any parameter names or values were invalid,
    /// else true indicating success.
    bool parse_args(int argc, char ** argv);

    /// Fill in default configuration parameter values.
    bool load_defaults();

    /// Parse one configuration parameter and put it into the configuration
    /// database.
    /// @param[in] param_name  string name of the parameter
    /// @param[in] param_value string value of the parameter
    /// @return true if success, else false.
    bool parse_parameter(const char * param_name, const char * param_value);

    /// Print the contents of the configuration database.
    void print_config() const;

    /// Print a usage message.
    void usage(const char * progname) const;

    void set_dlep_service(LLDLEP::DlepService *);

    /// Define the status code to use in response to a future destination up.
    /// @param[in] mac_address destination in the destination up message
    /// @param[in] status_code status to send for \p destination
    void set_destination_response(const LLDLEP::DlepMac & mac_address,
                                  const std::string & status_code);

    // support for colorized output

    /// text color to use for normal output
    std::string info_color;

    /// text color to use for error messages
    std::string error_color;

    /// text color to use for information that comes from the service library
    /// (when it calls a DlepClient method)
    std::string lib_color;

    /// reset to the default color
    std::string reset_color;

private:
    /// Load configuration parameters from a file.
    /// @param[in] config_filename name of the configuration file to read
    /// @return true if success, else false.
    bool parse_config_file(const char * config_filename);

    /// Configuration database.
    /// Maps parameter names to their values.
    std::map<std::string, DlepClient::ConfigValue> config_map;

    /// Metadata information about one configuration parameter.
    struct ConfigParameterInfo
    {
        /// Types that config parameters can have
        enum class ParameterType
        {
            UnsignedInteger,
            String,
            IPAddress,
            Boolean,
            ConfigFile, // special case of string
            ListOfUnsignedInteger
        };

        ParameterType type;

        /// Value to use for the parameter if none is supplied.
        /// If "", there is no default value.
        std::string default_value;

        /// Description of the parameter, printed in the usage message.
        std::string description;
    };

    /// Map of parameter names to their metadata.
    typedef std::map<std::string, ConfigParameterInfo> ConfigParameterMapType;

    /// Actual config parameter definitions.
    static ConfigParameterMapType param_info;

    /// Mapping from destination to status string.  This gives the
    /// status to use in response to a Destination Up.  The "dest response
    /// mac-address status-code-name" CLI command populates this.
    std::map<LLDLEP::DlepMac, std::string> destination_responses;

    LLDLEP::DlepService * dlep_service;
};

#endif // DLEP_CLIENT_IMPL_H
