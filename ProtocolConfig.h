/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016, 2018 Massachusetts Institute of Technology
 */

/// @file
/// This is the abstract interface to protocol configuration information
/// that the DLEP service library presents to the DLEP client (library user).
/// Clients should include this file indirectly by including DlepInit.h.

#ifndef PROTOCOL_CONFIG_H
#define PROTOCOL_CONFIG_H

#include <cstdint>
#include <string>
#include <array>
#include <vector>
#include "DataItem.h"
#include "LatencyRange.h"

namespace LLDLEP
{

/// This is the abstract interface to protocol configuration
/// information that the DLEP service library presents to the DLEP
/// client (library user).  Clients use DlepService::get_protocol_config()
/// to obtain a pointer to a ProtocolConfig object, which can be used
/// until the DlepService terminates.  After the ProtocolConfig object
/// is populated from the protocol configuration file(s) at startup by
/// DlepService, it is effectively a read-only object.  This means
/// we do not need locking for multithreaded access.
class ProtocolConfig
{
public:
    /// @return the configured version number, or (0, 0) if not configured
    virtual std::array<std::uint16_t, 2> get_version() const = 0;

    //--------------------------------------------------------------------------
    //
    // Get information about data items

    /// @return the number of message bytes used for a data item id
    virtual std::size_t get_data_item_id_size() const = 0;

    /// @return the number of message bytes used for a data item length
    virtual std::size_t get_data_item_length_size() const = 0;

    /// Return the data item id for a data item name.
    ///
    /// @param[in] name
    ///            the data item name for which to find the id
    /// @param[in] parent_di_info
    ///            if \p name belongs to a sub data item, this is a pointer
    ///            to the parent data item's DataItemInfo.  Otherwise,
    ///            it must be a nullptr.
    /// @return the data item id for \p name
    /// @throw BadDataItemName if \p name is not a valid data item name
    virtual DataItemIdType
    get_data_item_id(const std::string & name,
                     const LLDLEP::DataItemInfo * parent_di_info = nullptr) const = 0;

    /// Return the data item name for a data item id.
    ///
    /// @param[in] id
    ///            the data item id for which to find the name
    /// @param[in] parent_di_info
    ///            if \p id belongs to a sub data item, this is a pointer
    ///            to the parent data item's DataItemInfo.  Otherwise,
    ///            it must be a nullptr.
    /// @return the data item name for \p id
    /// @throw BadDataItemId if \p id is not a valid data item id
    virtual std::string get_data_item_name(DataItemIdType id,
                   const LLDLEP::DataItemInfo * parent_di_info = nullptr) const = 0;

    /// @return the type for a data item name
    /// @throw BadDataItemName if \p name is not a valid data item name
    virtual DataItemValueType
    get_data_item_value_type(const std::string & name) const = 0;

    /// Determine whether a data item id is a metric.
    ///
    /// @param[in] id
    ///            the data item id to examine
    /// @param[in] parent_di_info
    ///            if \p id belongs to a sub data item, this is a pointer
    ///            to the parent data item's DataItemInfo.  Otherwise,
    ///            it must be a nullptr.
    /// @return true if the data item id is a metric, else false
    /// @throw BadDataItemId if \p id is not a valid data item id
    virtual bool is_metric(DataItemIdType id,
                   const LLDLEP::DataItemInfo * parent_di_info = nullptr) const = 0;

    /// Determine whether a data item id contains an IP address.
    ///
    /// @param[in] id
    ///            the data item id to examine
    /// @param[in] parent_di_info
    ///            if \p id belongs to a sub data item, this is a pointer
    ///            to the parent data item's DataItemInfo.  Otherwise,
    ///            it must be a nullptr.
    /// @return true if the data item id contains an IP address, else false
    /// @throw BadDataItemId if \p id is not a valid data item id
    virtual bool is_ipaddr(DataItemIdType id,
                   const LLDLEP::DataItemInfo * parent_di_info = nullptr) const = 0;

    /// @return information about all configured data items
    virtual std::vector<LLDLEP::DataItemInfo> get_data_item_info() const = 0;

    /// @return information about the data item \p di_name
    /// @throw BadDataItemName if \p di_name is not a valid data item name
    virtual LLDLEP::DataItemInfo
    get_data_item_info(const std::string & di_name) const = 0;

    virtual LLDLEP::DataItemInfo
    get_data_item_info(DataItemIdType id,
                   const LLDLEP::DataItemInfo * parent_di_info = nullptr) const = 0;

    /// @return information about all of the data items in \p di_names
    /// @throw BadDataItemName if any of \p di_names is not a valid data item
    /// name
    virtual std::vector<LLDLEP::DataItemInfo>
    get_data_item_info(const std::vector<std::string> & di_names) const = 0;

    //--------------------------------------------------------------------------
    //
    // Get information about signals

    /// @return the number of message bytes used for a signal/message id
    virtual std::size_t get_signal_id_size() const = 0;

    /// @return the number of message bytes used for a signal/message length
    virtual std::size_t get_signal_length_size() const = 0;

    /// We assume there will never be a signal and a message with exactly the
    /// same name.
    /// @param[in] name               the signal or message name
    /// @param[out] is_signal_return  if non-null, then upon return the value
    ///                               pointed to is set to true if \p name was
    ///                               a signal and false if it was a message
    /// @return the signal/message id for a signal/message name
    /// @throw BadSignalName if \p name is not a valid signal/message name
    virtual SignalIdType get_signal_id(const std::string & name,
                                       bool * is_signal_return = nullptr) const = 0;

    /// @return the signal name for a signal id
    /// @throw BadSignalId if \p id is not a valid signal id
    virtual std::string get_signal_name(SignalIdType id) const = 0;

    /// @return the message name for a message id
    /// @throw BadSignalId if \p id is not a valid message id
    virtual std::string get_message_name(SignalIdType id) const = 0;

    /// @return the message response name for a message name, or
    ///         "" if there is no response
    /// @throw BadSignalId if \p id is not a valid message id
    virtual std::string get_message_response_name(const std::string & name) const = 0;

    /// @return the configured signal prefix, or "" if not configured
    virtual std::string get_signal_prefix() const = 0;

    /// Information about one data item allowed on a signal
    typedef struct SubDataItem DataItemForSignal;

    /// Information about one signal/message
    struct SignalInfo
    {
        std::string name; ///< name of this signal
        SignalIdType id;  ///< id of this signal

        /// boolean flag definitions
        enum Flags : std::uint32_t
        {
            message      = (1 << 0), ///< this is a message, not a signal
            modem_sends  = (1 << 1), ///< modem can send this signal/message
            router_sends = (1 << 2)  ///< router can send this signal/message
            // next flag = (1 << 3)
        };

        std::uint32_t flags; ///< OR-combination of enum Flags

        /// all data items allowed for this signal
        std::vector<DataItemForSignal> data_items;

        /// If this signal/message has a matching response, this is its id.
        /// This field is 0 if the signal has no response.
        SignalIdType response_id;

        std::string module; ///< module that provides this signal
        SignalInfo() :
        name(""), id(0), flags(0), response_id(0), module("") {}
    };

    /// @return information about all configured signals/messages
    virtual std::vector<SignalInfo> get_signal_info() const = 0;

    /// @return information about the signal/message \p sig_name
    /// @throw BadSignalName if \p sig_name is not a valid signal/message name
    virtual SignalInfo get_signal_info(const std::string & sig_name) const = 0;

    /// @return information about all of the signals/messages in \p sig_names
    /// @throw BadSignalName if any of \p sig_names is not a valid
    /// signal/message name
    virtual std::vector<SignalInfo>
    get_signal_info(const std::vector<std::string> & sig_names) const = 0;

    //--------------------------------------------------------------------------
    //
    // Get information about status codes

    /// @return the number of message bytes used for a status id
    /// XXX not currently used
    virtual std::size_t get_status_code_size() const = 0;

    /// @return the status code id for a status code name
    /// @throw BadStatusCodeName if \p name is not a valid status code name
    virtual StatusCodeIdType
    get_status_code_id(const std::string & name) const = 0;

    /// @return the status code name for a status code id
    /// @throw BadStatusCodeId if \p id is not a valid status code id
    virtual std::string get_status_code_name(StatusCodeIdType id) const = 0;

    /// Information about one status code
    struct StatusCodeInfo
    {
        std::string name;    ///< name of this status code
        StatusCodeIdType id; ///< id of this status code
        // There are no flags defined for status codes, but we include the
        // field here for future expandability.
        std::uint32_t flags; ///< OR-combination of enum Flags
        std::string module;  ///< module that provides this status code
        std::string failure_mode; ///< module that provides this status code
        StatusCodeInfo() :
        name(""), id(0), flags(0), module(""), failure_mode("") {} ;
    };

    /// @return information about all configured status codes
    virtual std::vector<StatusCodeInfo> get_status_code_info() const = 0;

    /// @return information about the status code \p sc_name
    /// @throw BadStatusCodeName if \p sc_name is not a valid status code name
    virtual StatusCodeInfo
    get_status_code_info(const std::string & sc_name) const = 0;

    /// @return information about all of the status codes in \p sc_names
    /// @throw BadStatusCodeName if any of \p sc_names is not a valid status code
    /// name
    virtual std::vector<StatusCodeInfo>
    get_status_code_info(const std::vector<std::string> & sc_names) const = 0;

    //--------------------------------------------------------------------------
    //
    // Get information about modules

    /// Information about one module
    struct ModuleInfo
    {
        std::string name;  ///< name of this module
        std::string draft; ///< draft of this module, or "" if not configured

        /// experiment_name of this module, or "" if not configured
        std::string experiment_name;

        /// extension id of this module, or 0 if not configured
        ExtensionIdType extension_id;

        /// data items provided by this module.  string is the data item name.
        std::vector<std::string> data_items;

        /// signals provided by this module
        std::vector<SignalIdType> signals;

        /// messages provided by this module
        std::vector<SignalIdType> messages;

        /// status codes provided by this module
        std::vector<StatusCodeIdType> status_codes;

        ModuleInfo() :
            name(""), draft(""), experiment_name(""), extension_id(0) {}
    };

    /// @return information about all configured modules
    virtual std::vector<ModuleInfo> get_module_info() const = 0;

    /// @return information about the module \p module_name
    /// @throw BadDataItemName if \p module_name is not a valid module name
    virtual ModuleInfo
        get_module_info(const std::string & module_name) const = 0;

    /// @return information about all of the modules in \p module_names
    /// @throw BadModuleName if any of \p module_names is not a valid module
    /// name
    virtual std::vector<ModuleInfo>
        get_module_info(const std::vector<std::string> & module_names) const = 0;

    /// @return the number of message bytes used for an extension id
    virtual std::size_t get_extension_id_size() const = 0;

    /// @return all extension ids defined across all modules
    virtual std::vector<ExtensionIdType> get_extension_ids() const = 0;

    /// @return all experiment names defined across all modules
    virtual std::vector<std::string> get_experiment_names() const = 0;

    virtual ~ProtocolConfig() {};

    //--------------------------------------------------------------------------
    //
    // exception classes used by the above methods

    /// exception class used when an invalid module name is encountered
    struct BadModuleName : public std::invalid_argument
    {
        explicit BadModuleName(const std::string & m) : std::invalid_argument(m) { }
    };

    /// exception class used when an invalid signal/message id is encountered
    struct BadSignalId : public std::invalid_argument
    {
        explicit BadSignalId(const std::string & m) : std::invalid_argument(m) { }
    };

    /// exception class used when an invalid signal/message name is encountered
    struct BadSignalName : public std::invalid_argument
    {
        explicit BadSignalName(const std::string & m) : std::invalid_argument(m) { }
    };

    /// exception class used when an invalid data item id is encountered
    struct BadDataItemId : public std::invalid_argument
    {
        explicit BadDataItemId(const std::string & m) : std::invalid_argument(m) { }
    };

    /// exception class used when an invalid data item name is encountered
    struct BadDataItemName : public std::invalid_argument
    {
        explicit BadDataItemName(const std::string & m) : std::invalid_argument(m) { }
    };

    /// exception class used when an invalid status code id is encountered
    struct BadStatusCodeId : public std::invalid_argument
    {
        explicit BadStatusCodeId(const std::string & m) : std::invalid_argument(m) { }
    };

    /// exception class used when an invalid status code name is encountered
    struct BadStatusCodeName : public std::invalid_argument
    {
        explicit BadStatusCodeName(const std::string & m) : std::invalid_argument(m) { }
    };

    /// exception class used when a problem with the protocol
    /// configuration is encountered
    struct BadProtocolConfig : public std::invalid_argument
    {
        explicit BadProtocolConfig(const std::string & m) : std::invalid_argument(m) { }
    };
};

/// Signals/messages, data items, and status codes ("protocol
/// elements" below) are assigned string names.  These names are the
/// mechanism by which the code and the configuration are tied
/// together.  The protocol configuration file specifies string names
/// of protocol elements.  When the code needs information about a
/// protocol element, it usually supplies the string name of the
/// protocol element to a ProtocolConfig method.  It is critical that
/// the protocol element names used by the code match those in the
/// protocol configuration file.  Rather than have many hardcoded
/// strings scattered throughout the code, we define symbolic
/// constants below for the strings and use those everywhere instead.
/// This way, accidental misspellings are caught at compile time
/// rather than run time.
namespace ProtocolStrings
{
// signal/message strings

extern const std::string Peer_Discovery;
extern const std::string Peer_Offer;
extern const std::string Session_Initialization;
extern const std::string Session_Initialization_Response;
extern const std::string Session_Termination;
extern const std::string Session_Termination_Response;
extern const std::string Session_Update;
extern const std::string Session_Update_Response;
extern const std::string Destination_Up;
extern const std::string Destination_Up_Response;
extern const std::string Destination_Down;
extern const std::string Destination_Down_Response;
extern const std::string Destination_Update;
extern const std::string Link_Characteristics_Request;
extern const std::string Link_Characteristics_Response;
extern const std::string Heartbeat;
extern const std::string Destination_Announce;
extern const std::string Destination_Announce_Response;

// data item strings

extern const std::string Version;
extern const std::string Port;
extern const std::string Peer_Type;
extern const std::string MAC_Address;
extern const std::string IPv4_Address;
extern const std::string IPv6_Address;
extern const std::string Status;
extern const std::string Heartbeat_Interval;
extern const std::string Link_Characteristics_Response_Timer;
extern const std::string IPv4_Attached_Subnet;
extern const std::string IPv6_Attached_Subnet;
extern const std::string Extensions_Supported;
extern const std::string Experimental_Definition;
extern const std::string IPv4_Connection_Point;
extern const std::string IPv6_Connection_Point;

// required metrics strings

extern const std::string Maximum_Data_Rate_Receive;
extern const std::string Maximum_Data_Rate_Transmit;
extern const std::string Current_Data_Rate_Receive;
extern const std::string Current_Data_Rate_Transmit;
extern const std::string Latency;
extern const std::string Resources;
extern const std::string Resources_Receive;
extern const std::string Resources_Transmit;
extern const std::string Relative_Link_Quality_Receive;
extern const std::string Relative_Link_Quality_Transmit;
extern const std::string Maximum_Transmission_Unit;

// status code strings

extern const std::string Success;
extern const std::string Unknown_Message;
extern const std::string Invalid_Message;
extern const std::string Unexpected_Message;
extern const std::string Request_Denied;
extern const std::string Timed_Out;
extern const std::string Invalid_Data;
extern const std::string Invalid_Destination;
extern const std::string Not_Interested;
extern const std::string Inconsistent_Data;

} // namespace ProtocolStrings

} // namespace LLDLEP

#endif // PROTOCOL_CONFIG_H
