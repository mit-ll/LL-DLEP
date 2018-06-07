/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016 Massachusetts Institute of Technology
 */

#ifndef PROTOCOL_MESSAGE_H
#define PROTOCOL_MESSAGE_H

#include "DlepClient.h"
#include "ProtocolConfig.h"
#include "DlepLogger.h"

namespace LLDLEP
{
namespace internal
{

class ProtocolMessage
{
public:

    ProtocolMessage(LLDLEP::ProtocolConfig * protocfg,
                    DlepLoggerPtr logger);

    // methods for building the protocol message

    void add_header(const std::string & msg_name);
    void add_data_item(const LLDLEP::DataItem & di);
    void add_data_items(const LLDLEP::DataItems & data_items);
    void add_allowed_data_items(const LLDLEP::DataItems & data_items);

    void add_version();
    void add_heartbeat_interval(LLDLEP::DlepClient & dlep_client);
    void add_peer_type(LLDLEP::DlepClient & dlep_client);
    void add_experiment_names();
    void add_status(std::string status_name,
                    const std::string & reason);
    void add_extensions(const std::vector<LLDLEP::ExtensionIdType> & extensions);
    void add_mac(const LLDLEP::DlepMac & mac);
    void add_common_data_items(LLDLEP::DlepClient & dlep_client);

    // low-level buffer manipulation

    static const unsigned int MAX_SIGNAL_SIZE = (1024 * 64);

    /// return the serialized message bytes
    const std::uint8_t * get_buffer() const;

    /// return the number of bytes in the serialized message
    /// (the size of the buffer returned by get_buffer() )
    std::size_t get_length() const;

    static bool is_complete_message(const LLDLEP::ProtocolConfig * protocfg,
                                    std::uint8_t * buf,
                                    std::size_t buflen,
                                    std::size_t & msg_size);
    // retrieval methods

    std::string parse(const uint8_t * buf, size_t buflen, bool is_signal,
                      const std::string & log_prefix);

    /// Parse the raw message bytes into a message/signal with data items.
    ///
    /// @param log_prefix   string to put at the beginning of the log message
    std::string parse(const std::string & log_prefix);
    std::string validate(bool modem_sender) const;

    std::string parse_and_validate(const uint8_t * msgbuf, size_t length,
                                   bool is_signal, bool modem_sender,
                                   const std::string & log_prefix);
    std::string parse_and_validate(bool modem_sender,
                                   const std::string & log_prefix);

    LLDLEP::SignalIdType get_signal_id() const;
    std::string get_signal_name() const;
    bool is_signal() const;

    LLDLEP::DlepMac get_mac() const;
    std::string get_peer_type() const;
    std::string get_status() const;
    std::vector<std::string> get_experiment_names() const;
    unsigned int get_heartbeat_interval() const;
    std::vector<LLDLEP::ExtensionIdType> get_extensions() const;
    std::uint16_t get_port() const;
    LLDLEP::Div_u8_ipv4_t get_ipv4_address() const;
    LLDLEP::Div_u8_ipv6_t get_ipv6_address() const;
    LLDLEP::Div_u8_ipv4_u16_t get_ipv4_conn_point() const;
    LLDLEP::Div_u8_ipv6_u16_t get_ipv6_conn_point() const;

    LLDLEP::DataItems get_metrics_and_ipaddrs() const;
    LLDLEP::DataItems get_data_items_no_mac() const;
    LLDLEP::DataItems get_data_items() const;
    bool get_data_item_exists(const std::string & data_item_name) const;

    // exception classes

    struct DataItemWrongType : public std::runtime_error
    {
        explicit DataItemWrongType(const std::string & s) : std::runtime_error(s) {}
    };

    struct DataItemNotPresent : public std::runtime_error
    {
        explicit DataItemNotPresent(const std::string & s) : std::runtime_error(s) {}
    };

    struct SignalIdNotInitialized : public std::runtime_error
    {
        explicit SignalIdNotInitialized(const std::string & s) : std::runtime_error(s) {}
    };

private:

    void update_message_length();

    template <typename T>
    T get_data_item_value(const std::string & data_item_name) const;

    template <typename T>
    std::vector<T> get_data_item_values(const std::string & data_item_name) const;

    /// Index in msgbuf where the message length is stored.
    std::size_t msg_len_index;

    /// length in bytes of the entire message header.  Data items start
    /// right after this.
    std::size_t header_length;

    /// raw message bytes (serialized)
    std::vector<std::uint8_t> msgbuf;

    /// message ID for convenient access
    LLDLEP::SignalIdType signal_id;

    /// has signal_id been initialized yet?
    bool signal_id_initialized;

    /// Is signal_id a "signal" vs. a "message" ?
    /// Signals have the signal prefix at the beginning of the packet.
    bool is_signal_;

    /// data items in this signal, populated by parse()
    LLDLEP::DataItems data_items;

    /// DLEP protocol configuration interface
    LLDLEP::ProtocolConfig * protocfg;

    /// logger, for LOG macro
    DlepLoggerPtr logger;
};

} // namespace internal
} // namespace LLDLEP

#endif // PROTOCOL_MESSAGE_H
