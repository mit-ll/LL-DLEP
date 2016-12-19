/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016 Massachusetts Institute of Technology
 */

#ifndef PROTOCOL_CONFIG_IMPL_H
#define PROTOCOL_CONFIG_IMPL_H

#include <cstdint>
#include <string>
#include <boost/bimap.hpp>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include "ProtocolConfig.h"
#include "DlepLogger.h"

namespace LLDLEP
{
namespace internal
{

class ProtocolConfigImpl : public LLDLEP::ProtocolConfig
{
public:
    ProtocolConfigImpl(const std::string & proto_config_schema,
                       const std::string & proto_config_file,
                       DlepLoggerPtr logger);

    std::array<std::uint16_t, 2> get_version() const;

    std::size_t get_data_item_id_size() const;
    std::size_t get_data_item_length_size() const;
    LLDLEP::DataItemIdType get_data_item_id(const std::string & name) const;
    std::string get_data_item_name(LLDLEP::DataItemIdType id) const;
    LLDLEP::DataItemValueType
        get_data_item_value_type(LLDLEP::DataItemIdType id) const;
    LLDLEP::DataItemValueType
        get_data_item_value_type(const std::string & name) const;
    bool is_metric(LLDLEP::DataItemIdType id) const;
    bool is_ipaddr(LLDLEP::DataItemIdType id) const;
    std::vector<ProtocolConfig::DataItemInfo> get_data_item_info() const;
    ProtocolConfig::DataItemInfo
        get_data_item_info(const std::string & di_name) const;
    std::vector<ProtocolConfig::DataItemInfo>
        get_data_item_info(const std::vector<std::string> & di_names) const;

    std::size_t get_signal_id_size() const;
    std::size_t get_signal_length_size() const;
    LLDLEP::SignalIdType get_signal_id(const std::string & name,
                                       bool * is_signal_return = nullptr) const;
    std::string get_signal_name(LLDLEP::SignalIdType id) const;
    std::string get_message_name(LLDLEP::SignalIdType id) const;
    std::string get_message_response_name(const std::string & name) const;
    std::string get_signal_prefix() const;
    std::vector<ProtocolConfig::SignalInfo> get_signal_info() const;
    ProtocolConfig::SignalInfo
        get_signal_info(const std::string & di_name) const;
    std::vector<ProtocolConfig::SignalInfo>
        get_signal_info(const std::vector<std::string> & di_names) const;

    std::size_t get_status_code_size() const;
    LLDLEP::StatusCodeIdType get_status_code_id(const std::string & name) const;
    std::string get_status_code_name(LLDLEP::StatusCodeIdType id) const;
    std::vector<StatusCodeInfo> get_status_code_info() const;
    StatusCodeInfo get_status_code_info(const std::string & sc_name) const;
    std::vector<StatusCodeInfo>
    get_status_code_info(const std::vector<std::string> & sc_names) const ;

    std::vector<ProtocolConfig::ModuleInfo> get_module_info() const;
    ProtocolConfig::ModuleInfo
    get_module_info(const std::string & module_name) const;
    std::vector<ProtocolConfig::ModuleInfo>
    get_module_info(const std::vector<std::string> & module_names) const;
    std::size_t get_extension_id_size() const;
    std::vector<LLDLEP::ExtensionIdType> get_extension_ids() const;
    std::vector<std::string> get_experiment_names() const;

    DlepLoggerPtr get_logger() const;

private:
    std::string load_protocol_config(
        const std::string & proto_config_schema,
        const std::string & proto_config_file);

    void log_xml_tree(xmlNodePtr start_node, int level);
    std::string extract_protocol_config(xmlDocPtr doc);
    void extract_version(xmlNodePtr node);
    void extract_signal_prefix(xmlNodePtr node);
    void extract_field_sizes(xmlNodePtr node);
    void extract_module(xmlNodePtr node);
    void extract_module_signal(xmlNodePtr node, ModuleInfo & modinfo);
    void extract_module_data_item(xmlNodePtr node, ModuleInfo & modinfo);
    void extract_module_status_code(xmlNodePtr node, ModuleInfo & modinfo);
    DataItemForSignal extract_module_signal_data_item(xmlNodePtr node);

    template<typename T>
    void log_node_path_and_value(xmlNodePtr node, T val);
    template<typename T> T extract_node_value(xmlNodePtr node);
    template<typename MapType, typename IdType>
        void insert_id_name_mapping(MapType & id_name_map, IdType id,
                                    const std::string & name,
                                    const std::string & mapname);

    /// map module name --> module info
    std::map<std::string, ProtocolConfig::ModuleInfo> module_info_map;

    /// map signal id <--> name
    boost::bimap<LLDLEP::SignalIdType, std::string> signal_map;

    /// map signal id --> signal info
    std::map<LLDLEP::SignalIdType, ProtocolConfig::SignalInfo>
        signal_info_map;

    /// map message id <--> name
    boost::bimap<LLDLEP::SignalIdType, std::string> message_map;

    /// map message id --> message info
    std::map<LLDLEP::SignalIdType, ProtocolConfig::SignalInfo>
        message_info_map;

    /// map data item id <--> name
    boost::bimap<LLDLEP::DataItemIdType, std::string> data_item_map;

    /// map data item id --> data item info
    std::map<LLDLEP::DataItemIdType, ProtocolConfig::DataItemInfo>
        data_item_info_map;

    /// map status code id <--> name
    boost::bimap<LLDLEP::StatusCodeIdType, std::string> status_code_map;

    /// map status code id --> status code info
    std::map<LLDLEP::StatusCodeIdType, ProtocolConfig::StatusCodeInfo>
        status_code_info_map;

    // other miscellaneous configuration information

    std::array<std::uint16_t, 2> version;
    std::string signal_prefix;
    std::size_t signal_length_size;
    std::size_t signal_id_size;
    std::size_t data_item_length_size;
    std::size_t data_item_id_size;
    std::size_t extension_id_size;
    std::size_t status_code_size;

    DlepLoggerPtr logger;
};

} // namespace internal
} // namespace LLDLEP

#endif // PROTOCOL_CONFIG_IMPL_H
