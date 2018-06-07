/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016, 2018 Massachusetts Institute of Technology
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

    std::array<std::uint16_t, 2> get_version() const override;

    std::size_t get_data_item_id_size() const override;
    std::size_t get_data_item_length_size() const override;
    LLDLEP::DataItemIdType get_data_item_id(const std::string & name,
                 const LLDLEP::DataItemInfo * parent_di_info = nullptr) const override;
    std::string get_data_item_name(LLDLEP::DataItemIdType id,
                 const LLDLEP::DataItemInfo * parent_di_info = nullptr) const override;
    LLDLEP::DataItemValueType
        get_data_item_value_type(const std::string & name) const override;

    bool is_metric(LLDLEP::DataItemIdType id,
                   const LLDLEP::DataItemInfo * parent_di_info = nullptr) const override;
    bool is_ipaddr(LLDLEP::DataItemIdType id,
                   const LLDLEP::DataItemInfo * parent_di_info = nullptr) const override;
    std::vector<LLDLEP::DataItemInfo> get_data_item_info() const override;
    LLDLEP::DataItemInfo get_data_item_info(const std::string & di_name) const override;
    std::vector<LLDLEP::DataItemInfo>
        get_data_item_info(const std::vector<std::string> & di_names) const override;
    LLDLEP::DataItemInfo
        get_data_item_info(DataItemIdType id,
                   const LLDLEP::DataItemInfo * parent_di_info = nullptr) const override;
    std::size_t get_signal_id_size() const override;
    std::size_t get_signal_length_size() const override;
    LLDLEP::SignalIdType get_signal_id(const std::string & name,
                                       bool * is_signal_return = nullptr) const override;
    std::string get_signal_name(LLDLEP::SignalIdType id) const override;
    std::string get_message_name(LLDLEP::SignalIdType id) const override;
    std::string get_message_response_name(const std::string & msg_name) const override;
    std::string get_signal_prefix() const override;
    std::vector<ProtocolConfig::SignalInfo> get_signal_info() const override;
    ProtocolConfig::SignalInfo
        get_signal_info(const std::string & sig_name) const override;
    std::vector<ProtocolConfig::SignalInfo>
        get_signal_info(const std::vector<std::string> & sig_names) const override;

    std::size_t get_status_code_size() const override;
    LLDLEP::StatusCodeIdType get_status_code_id(const std::string & name) const override;
    std::string get_status_code_name(LLDLEP::StatusCodeIdType id) const override;
    std::vector<StatusCodeInfo> get_status_code_info() const override;
    StatusCodeInfo get_status_code_info(const std::string & sc_name) const override;
    std::vector<StatusCodeInfo>
    get_status_code_info(const std::vector<std::string> & sc_names) const override ;

    std::vector<ProtocolConfig::ModuleInfo> get_module_info() const override;
    ProtocolConfig::ModuleInfo
    get_module_info(const std::string & module_name) const override;
    std::vector<ProtocolConfig::ModuleInfo>
    get_module_info(const std::vector<std::string> & module_names) const override;
    std::size_t get_extension_id_size() const override;
    std::vector<LLDLEP::ExtensionIdType> get_extension_ids() const override;
    std::vector<std::string> get_experiment_names() const override;

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
    SubDataItem extract_module_data_item_ref(const std::string & parent_name,
                                             xmlNodePtr node);

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
    /// This only contains mappings for top-level data items, not sub data items
    /// because sub data items can have the same id as a top-level data item.
    boost::bimap<LLDLEP::DataItemIdType, std::string> data_item_map;

    /// map data item name(string) --> data item info
    /// This contains mappings for top-level AND sub data items.
    /// We require the data item name to be unique across all data items.
    std::map<std::string, LLDLEP::DataItemInfo> data_item_info_map;

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
