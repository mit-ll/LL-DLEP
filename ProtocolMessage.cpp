/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016 Massachusetts Institute of Technology
 */

#include "ProtocolMessage.h"
#include "Dlep.h"
#include <iomanip>
#include <set>

using namespace LLDLEP;
using namespace internal;

ProtocolMessage::ProtocolMessage(ProtocolConfig * protocfg,
                                 DlepLoggerPtr logger) :
    msg_len_index(0),
    header_length(0),
    signal_id(0),
    signal_id_initialized(false),
    is_signal_(false),
    protocfg(protocfg),
    logger(logger)
{
}

void
ProtocolMessage::add_header(const std::string & msg_name)
{
    signal_id = protocfg->get_signal_id(msg_name, &is_signal_);
    signal_id_initialized = true;
    std::size_t id_size = protocfg->get_signal_id_size();

    // Add signal prefix if needed

    if (is_signal_)
    {
        std::string signal_prefix = protocfg->get_signal_prefix();
        if (signal_prefix.length() > 0)
        {
            msgbuf.insert(msgbuf.end(), signal_prefix.begin(), signal_prefix.end());
        }
    }

    // Add the signal ID to the message

    serialize(signal_id, id_size, msgbuf);

    // Remember where the signal length is stored: right after the
    // signal id.  We'll need to know this to update the signal length
    // as data items are added.

    msg_len_index = msgbuf.size();

    // Store the signal length, which does NOT include the header length.
    // Since there are no data items yet, the length is initially 0.

    serialize(0U, protocfg->get_signal_length_size(), msgbuf);

    // Remember the size of the header so that we can account for it
    // when we update the signal length.

    header_length = msgbuf.size();
}

void
ProtocolMessage::add_data_item(const DataItem & di)
{
    std::vector<std::uint8_t> dibuf = di.serialize();

    msgbuf.insert(msgbuf.end(), dibuf.begin(), dibuf.end());

    update_message_length();

    {
        std::ostringstream msg;
        msg << "added data item " << protocfg->get_data_item_name(di.id)
            << " to " << get_signal_name()
            << ", now length=" << get_length();
        LOG(DLEP_LOG_DEBUG, msg);
    }
}

void
ProtocolMessage::add_data_items(const DataItems & data_items)
{
    for (const DataItem & di : data_items)
    {
        add_data_item(di);
    }
}

void
ProtocolMessage::add_allowed_data_items(const DataItems & data_items)
{
    ProtocolConfig::SignalInfo siginfo =
        protocfg->get_signal_info(get_signal_name());

    for (const DataItem & di : data_items)
    {
        for (const auto & difs : siginfo.data_items)
        {
            if (difs.id == di.id)
            {
                add_data_item(di);
                break;
            }
        }
    }
}

void
ProtocolMessage::add_version()
{
    DataItemValue div = protocfg->get_version();
    DataItem di_version {ProtocolStrings::Version, div, protocfg};
    add_data_item(di_version);
}

void
ProtocolMessage::add_heartbeat_interval(DlepClient & dlep_client)
{
    std::ostringstream msg;
    unsigned int heartbeat_interval;
    dlep_client.get_config_parameter("heartbeat-interval",
                                     &heartbeat_interval);

    DataItemInfo di_info =
        protocfg->get_data_item_info(ProtocolStrings::Heartbeat_Interval);

    // convert heartbeat_interval according to its configured units

    if (di_info.units == "milliseconds")
    {
        heartbeat_interval *= 1000;
    }
    else if (di_info.units == "microseconds")
    {
        heartbeat_interval *= 1000000;
    }
    else if ((di_info.units != "") && (di_info.units != "seconds"))
    {
        msg << "invalid units= " << di_info.units << " for "
            << ProtocolStrings::Heartbeat_Interval;
        LOG(DLEP_LOG_ERROR, msg);
    }

    // Use u16 vs. u32 according to the configured data type

    if (di_info.value_type == DataItemValueType::div_u16)
    {
        DataItemValue div = std::uint16_t(heartbeat_interval);
        DataItem di_heartbeat_interval {ProtocolStrings::Heartbeat_Interval,
                                        div, protocfg};
        add_data_item(di_heartbeat_interval);
    }
    else if (di_info.value_type == DataItemValueType::div_u32)
    {
        DataItemValue div = std::uint32_t(heartbeat_interval);
        DataItem di_heartbeat_interval {ProtocolStrings::Heartbeat_Interval,
                                        div, protocfg};
        add_data_item(di_heartbeat_interval);
    }
    else
    {
        msg << "invalid value type= " << to_string(di_info.value_type)
            << " for " << ProtocolStrings::Heartbeat_Interval;
        LOG(DLEP_LOG_ERROR, msg);
    }
}

void
ProtocolMessage::add_peer_type(DlepClient & dlep_client)
{
    std::string peer_type;

    try
    {
        dlep_client.get_config_parameter("peer-type", &peer_type);
    }
    catch (DlepClient::BadParameterName)
    {
        // There was no peer-type config parameter.  If Peer Type is
        // an optional data item for this signal, don't add it to the
        // message at all.  Otherwise, it isn't optional, so add it
        // with an empty string, which is what peer_type is at this
        // point.

        // XXX29 possibly put this logic in a protocfg method
        ProtocolConfig::SignalInfo siginfo =
            protocfg->get_signal_info(get_signal_name());
        DataItemIdType di_id =
            protocfg->get_data_item_id(ProtocolStrings::Peer_Type);

        for (const auto & difs : siginfo.data_items)
        {
            if (difs.id == di_id)
            {
                if (difs.occurs[0] == '0')
                {
                    // Peer Type is optional for this signal
                    return;
                }
                else
                {
                    // Peer Type is required for this signal
                    break;
                }
            }
        }

        // If we get here, we'll add a Peer Type data item with an
        // empty string.

    } // end catch BadParameterName peer-type

    // Depending on the DLEP draft, the Peer Type data item is
    // either a string, or a uint8 followed by a string.  Check to
    // see which one to use.
    DataItemValueType div_type =
        protocfg->get_data_item_value_type(ProtocolStrings::Peer_Type);
    DataItemValue div;

    if (div_type == DataItemValueType::div_string)
    {
        div = peer_type;
    }
    else
    {
        unsigned int peer_flags = 0;
        try
        {
            dlep_client.get_config_parameter("peer-flags", &peer_flags);
        }
        catch (DlepClient::BadParameterName)
        {
            // If there was no peer-flags config parameter, use default 0
        }
        div = Div_u8_string_t{std::uint8_t(peer_flags), peer_type};
    }

    DataItem di_peer_type {ProtocolStrings::Peer_Type,
                           div, protocfg
                          };
    add_data_item(di_peer_type);
}

void
ProtocolMessage::add_experiment_names()
{
    std::vector<std::string> experiment_names =
        protocfg->get_experiment_names();

    for (const auto & en : experiment_names)
    {
        DataItemValue div = en;
        DataItem di_experiment_name {ProtocolStrings::Experimental_Definition,
                                     div, protocfg
                                    };
        add_data_item(di_experiment_name);
    }
}

void
ProtocolMessage::add_status(std::string status_name,
                            const std::string & reason)
{
    std::ostringstream msg;
    StatusCodeIdType id;
    bool id_found = false;

    while (! id_found)
    {
        try
        {
            id = protocfg->get_status_code_id(status_name);
            id_found = true;
        }
        catch (ProtocolConfig::BadStatusCodeName)
        {
            // Different DLEP drafts define different status codes.
            // We try to hide this from the caller by detecting if the
            // requested status code does not exist in the current
            // protocol configuration, and remapping it to something
            // that does exist instead.  This remapping is imperfect,
            // but it seems like the best we can do.

            msg << "status=" << status_name << " not configured, trying ";

            if (status_name == ProtocolStrings::Invalid_Message)
            {
                status_name = ProtocolStrings::Invalid_Data;
            }
            else if ( (status_name == ProtocolStrings::Invalid_Destination) ||
                      (status_name == ProtocolStrings::Inconsistent_Data) )
            {
                status_name = ProtocolStrings::Invalid_Message;
            }
            else if (status_name == ProtocolStrings::Invalid_Data)
            {
                // If the protocol configuration has neither
                // Invalid_Data nor Invalid_Message, an infinite loop
                // will result.  For the protocol configurations we currently
                // have, this won't happen.
                status_name = ProtocolStrings::Invalid_Message;
            }
            else if (status_name == ProtocolStrings::Not_Interested)
            {
                status_name = ProtocolStrings::Request_Denied;
            }
            else
            {
                status_name = ProtocolStrings::Unknown_Message;
            }

            msg << status_name << " instead";
            LOG(DLEP_LOG_INFO, msg);
        }
    } // while id not found

    // In earlier drafts of DLEP, the status data item is just a
    // uint8.  In later drafts, it's a uint8 followed by an arbitrary
    // string.  We need to handle both.
    DataItemValueType div_type =
        protocfg->get_data_item_value_type(ProtocolStrings::Status);

    DataItemValue div;
    if (div_type == DataItemValueType::div_u8)
    {
        div = std::uint8_t(id);
    }
    else
    {
        Div_u8_string_t status {std::uint8_t(id), reason};
        div = status;
    }

    DataItem di_status {ProtocolStrings::Status, div, protocfg};
    add_data_item(di_status);
}

void
ProtocolMessage::add_extensions(const std::vector<ExtensionIdType> & extensions)
{
    Div_v_extid_t vextid {extensions};
    DataItemValue div {vextid};
    DataItem di_extensions {ProtocolStrings::Extensions_Supported,
                            div, protocfg
                           };
    add_data_item(di_extensions);
}

void
ProtocolMessage::add_mac(const DlepMac & mac)
{
    DataItemValue div {mac};
    DataItem di_mac {ProtocolStrings::MAC_Address,
                     div, protocfg
                    };
    add_data_item(di_mac);
}

void
ProtocolMessage::add_common_data_items(DlepClient & dlep_client)
{
    std::string signal_name = get_signal_name();
    ProtocolConfig::SignalInfo siginfo =
        protocfg->get_signal_info(signal_name);

    // Look at all of the data items that this signal can have.
    // Add any that we recognize and know how to fill in.

    for (const auto & difs : siginfo.data_items)
    {
        std::string di_name =
            protocfg->get_data_item_name(difs.id);

        if (di_name == ProtocolStrings::Version)
        {
            add_version();
        }
        else if (di_name == ProtocolStrings::Heartbeat_Interval)
        {
            add_heartbeat_interval(dlep_client);
        }
        else if (di_name == ProtocolStrings::Peer_Type)
        {
            add_peer_type(dlep_client);
        }
        else if (di_name == ProtocolStrings::Experimental_Definition)
        {
            add_experiment_names();
        }

        else if (di_name == ProtocolStrings::Status)
        {
            add_status(ProtocolStrings::Success, "");
        }
    } // for each data item supported by this signal
}

const std::uint8_t *
ProtocolMessage::get_buffer() const
{
    return msgbuf.data();
}

std::size_t
ProtocolMessage::get_length() const
{
    return msgbuf.size();
}

void
ProtocolMessage::update_message_length()
{
    assert(msgbuf.size() >= header_length);
    assert((msgbuf.begin() + msg_len_index) < msgbuf.end());
    serialize(
        msgbuf.size() - header_length,        // new length to store
        protocfg->get_signal_length_size(), // size of the length field
        msgbuf,                                   // message being built
        msgbuf.begin() + msg_len_index);          // where to store the new len
}

bool
ProtocolMessage::is_complete_message(const ProtocolConfig * protocfg,
                                     std::uint8_t * buf, std::size_t buflen,
                                     std::size_t & msg_size)
{
    // How big is the message header?
    // We do not consider the signal_prefix here because this method
    // should only be called for messages from the TCP session, which
    // are never signals and thus never start with the signal prefix.
    // UDP messages are signals, but we always receive them all at
    // once, so we never need to check if we have a complete signal.

    const size_t header_size = protocfg->get_signal_id_size() +
                               protocfg->get_signal_length_size();

    if (buflen < header_size)
    {
        return false;
    }

    SignalIdType sid;
    std::size_t signal_len;

    std::uint8_t * bufend = buf + buflen;

    deserialize(sid, protocfg->get_signal_id_size(),
                buf, bufend);
    deserialize(signal_len, protocfg->get_signal_length_size(),
                buf, bufend);

    msg_size = header_size + signal_len;

    return (buflen >= msg_size);
}

SignalIdType
ProtocolMessage::get_signal_id() const
{
    if (! signal_id_initialized)
    {
        throw SignalIdNotInitialized(std::to_string(signal_id));
    }
    return signal_id;
}

std::string
ProtocolMessage::get_signal_name() const
{
    if (! signal_id_initialized)
    {
        throw SignalIdNotInitialized(std::to_string(signal_id));
    }

    if (is_signal_)
    {
        return protocfg->get_signal_name(signal_id);
    }

    return protocfg->get_message_name(signal_id);
}

std::string
ProtocolMessage::parse(const uint8_t * buf, size_t buflen, bool is_signal,
                       const std::string & log_prefix)
{
    // Copy the input data to the internal message buffer
    msgbuf.resize(buflen);
    memcpy(msgbuf.data(), buf, buflen);
    is_signal_ = is_signal;

    return parse(log_prefix);
}

std::string
ProtocolMessage::parse(const std::string & log_prefix)
{
    std::ostringstream msg;
    std::string err;

    // First log the raw bytes of the message.

    msg << log_prefix << " message length=" << msgbuf.size() << " bytes="
        << std::hex << std::setfill('0');
    for (std::uint8_t byte : msgbuf)
    {
        msg << std::setw(2) << (unsigned int)byte << " ";
    }
    LOG(DLEP_LOG_DEBUG, msg);

    // Wipe out data items from any previous call to parse() on this
    // object.  This method parses the entire message from scratch
    // every time.

    data_items.clear();

    // msgbuf_it will step through msgbuf as we parse it
    auto msgbuf_it = msgbuf.cbegin();

    // Handle signal prefix if it's there

    if (is_signal_)
    {
        std::string signal_prefix =  protocfg->get_signal_prefix();
        if (msgbuf.size() <= signal_prefix.length())
        {
            err = "signal is too short to have expected prefix " + signal_prefix;
            goto bailout;
        }

        for (std::uint8_t ch : signal_prefix)
        {
            if (*msgbuf_it != ch)
            {
                std::string ch_str(1, ch);
                std::string msgch_str(1, *msgbuf_it);
                err = "signal prefix " + signal_prefix + " mismatch: "
                      + msgch_str + " != " + ch_str;
                goto bailout;
            }
            ++msgbuf_it;
        }
    }

    try
    {
        std::size_t signal_len;

        // get the signal id and length from the header

        deserialize(signal_id, protocfg->get_signal_id_size(),
                    msgbuf_it, msgbuf.cend());
        signal_id_initialized = true;
        deserialize(signal_len, protocfg->get_signal_length_size(),
                    msgbuf_it, msgbuf.cend());

        msg << std::dec << log_prefix << " signal id=" << signal_id
            << "(" << get_signal_name() << ")"
            << " length=" << signal_len;
        LOG(DLEP_LOG_INFO, msg);

        // Now parse all of the data items.

        while (msgbuf_it < msgbuf.cend())
        {
            auto di_start = msgbuf_it; // remember start of this data item
            DataItemIdType di_id;
            std::size_t di_len;
            int di_index = msgbuf_it - msgbuf.cbegin();

            deserialize(di_id, protocfg->get_data_item_id_size(),
                        msgbuf_it, msgbuf.cend());
            deserialize(di_len, protocfg->get_data_item_length_size(),
                        msgbuf_it, msgbuf.cend());
            msg << "  at index=" << di_index << " data item id=" << di_id
                << " length=" << di_len << " ";

            // reset msgbuf_it to the start of this data item
            msgbuf_it = di_start;

            // parse, log, and record this data item

            DataItem di {protocfg};
            di.deserialize(msgbuf_it, msgbuf.cend());
            msg << di.to_string();
            LOG(DLEP_LOG_INFO, msg);

            data_items.push_back(di);

            // If all went well, the deserialization should have
            // advanced it to the next data item, or to the end of the
            // buffer.

        } // while more data items to parse
    }
    catch (std::exception & e)
    {
        err = e.what();
    }

bailout:
    if (err != "")
    {
        err += " " + log_prefix;
        LOG(DLEP_LOG_ERROR, std::ostringstream(err));
    }
    return err;
}

std::string
ProtocolMessage::validate(bool modem_sender) const
{
    std::string signal_name = get_signal_name();
    // description of any error we find
    std::string err;

    ProtocolConfig::SignalInfo siginfo =
        protocfg->get_signal_info(signal_name);

    // Check that the sender is allowed to send this signal

    std::uint32_t sender_flag =
        modem_sender ? ProtocolConfig::SignalInfo::Flags::modem_sends
        : ProtocolConfig::SignalInfo::Flags::router_sends;

    if (!(siginfo.flags & sender_flag))
    {
        err = "cannot be sent by ";

        // clang did not like (modem_sender ? "modem" : "router"), why?

        if (modem_sender)
        {
            err += "modem";
        }
        else
        {
            err += "router";
        }
    }

    // If we haven't found an error, check the number and kind of data
    // items in the signal against what is allowed for this signal.

    if (err == "")
    {
        err = DataItem::validate_occurrences(data_items, siginfo.data_items,
                                             protocfg);
    } // if no error yet

    // If we still haven't found an error, validate the innards of
    // each data item in the message.

    if (err == "")
    {
        for (const auto & di : data_items)
        {
            err = di.validate();
            if (err != "")
            {
                break;
            }
        }
    }

    if (err != "")
    {
        err = signal_name + " " + err;
        LOG(DLEP_LOG_ERROR, std::ostringstream(err));
    }
    return err;
}

std::string
ProtocolMessage::parse_and_validate(const uint8_t * msgbuf, size_t length,
                                    bool is_signal,
                                    bool modem_sender,
                                    const std::string & log_prefix)
{
    std::string err = parse(msgbuf, length, is_signal, log_prefix);
    if (err != "")
    {
        return err;
    }
    return validate(modem_sender);
}

std::string
ProtocolMessage::parse_and_validate(bool modem_sender,
                                    const std::string & log_prefix)
{
    std::string err = parse(log_prefix);
    if (err != "")
    {
        return err;
    }
    return validate(modem_sender);
}

bool
ProtocolMessage::is_signal() const
{
    return is_signal_;
}

template <typename T>
T
ProtocolMessage::get_data_item_value(const std::string & data_item_name) const
{
    DataItemIdType id = protocfg->get_data_item_id(data_item_name);
    for (const auto & di : data_items)
    {
        if (di.id == id)
        {
            try
            {
                T val = boost::get<T>(di.value);
                return val;
            }
            catch (boost::bad_get)
            {
                throw DataItemWrongType(data_item_name);
            }
        }
    }

    // We didn't find the desired data item, so throw an exception.

    throw DataItemNotPresent(data_item_name);

    // This return statement never gets executed, but it quiets the compiler
    // warning "control reaches end of non-void function"
    return T();
}

template <typename T>
std::vector<T>
ProtocolMessage::get_data_item_values(const std::string & data_item_name) const
{
    std::vector<T> vecT;
    DataItemIdType id = protocfg->get_data_item_id(data_item_name);

    for (const auto & di : data_items)
    {
        if (di.id == id)
        {
            try
            {
                T val = boost::get<T>(di.value);
                vecT.push_back(val);
            }
            catch (boost::bad_get)
            {
                throw DataItemWrongType(data_item_name);
            }
        }
    }

    // caller must deal with possibly empty vector
    return vecT;
}

bool
ProtocolMessage::get_data_item_exists(const std::string & data_item_name) const
{
    DataItemIdType id = protocfg->get_data_item_id(data_item_name);

    for (const auto & di : data_items)
    {
        if (di.id == id)
        {
            return true;
        }
    }

    return false;
}

DlepMac
ProtocolMessage::get_mac() const
{
    return get_data_item_value<DlepMac>(
               ProtocolStrings::MAC_Address);
}

std::string
ProtocolMessage::get_peer_type() const
{
    // In earlier drafts of DLEP, the peer type data item is just a
    // string.  In later drafts, it's a uint8 followed by a string.
    // We need to handle both.

    std::string peer_type;
    DataItemValueType div_type =
        protocfg->get_data_item_value_type(ProtocolStrings::Peer_Type);

    if (div_type == DataItemValueType::div_string)
    {
        peer_type =
            get_data_item_value<std::string>(ProtocolStrings::Peer_Type);
    }
    else
    {
        Div_u8_string_t peer_val =
            get_data_item_value<Div_u8_string_t>(ProtocolStrings::Peer_Type);
        peer_type = peer_val.field2;
    }

    return peer_type;
}

std::string
ProtocolMessage::get_status() const
{
    // In earlier drafts of DLEP, the status data item is just a
    // uint8.  In later drafts, it's a uint8 followed by an arbitrary
    // string.  We need to handle both.

    StatusCodeIdType status_code_id = 0;
    DataItemValueType div_type =
        protocfg->get_data_item_value_type(ProtocolStrings::Status);

    if (div_type == DataItemValueType::div_u8)
    {
        status_code_id =
            get_data_item_value<std::uint8_t>(ProtocolStrings::Status);
    }
    else
    {
        Div_u8_string_t status_val =
            get_data_item_value<Div_u8_string_t>(ProtocolStrings::Status);
        status_code_id = status_val.field1;
    }

    return protocfg->get_status_code_name(status_code_id);
}

std::vector<std::string>
ProtocolMessage::get_experiment_names() const
{
    return get_data_item_values<std::string>(
               ProtocolStrings::Experimental_Definition);
}

unsigned int
ProtocolMessage::get_heartbeat_interval() const
{
    DataItemValueType div_type =
        protocfg->get_data_item_value_type(ProtocolStrings::Heartbeat_Interval);
    unsigned int v = 0;

    // handle 16 vs. 32 bit heartbeat interval

    if (div_type == DataItemValueType::div_u16)
    {
        v = get_data_item_value<std::uint16_t>(
            ProtocolStrings::Heartbeat_Interval);
    }
    else if (div_type == DataItemValueType::div_u32)
    {
        v = get_data_item_value<std::uint32_t>(
            ProtocolStrings::Heartbeat_Interval);
    }
    else
    {
        throw DataItemWrongType(ProtocolStrings::Heartbeat_Interval);
    }

    return v;
}

std::vector<ExtensionIdType>
ProtocolMessage::get_extensions() const
{
    Div_v_extid_t extids = get_data_item_value<Div_v_extid_t>(
                               ProtocolStrings::Extensions_Supported);
    return extids.field1;
}

std::uint16_t
ProtocolMessage::get_port() const
{
    return get_data_item_value<std::uint16_t>(
               ProtocolStrings::Port);
}

Div_u8_ipv4_t
ProtocolMessage::get_ipv4_address() const
{
    return get_data_item_value<Div_u8_ipv4_t>(
               ProtocolStrings::IPv4_Address);
}

Div_u8_ipv6_t
ProtocolMessage::get_ipv6_address() const
{
    return get_data_item_value<Div_u8_ipv6_t>(
               ProtocolStrings::IPv6_Address);
}

Div_u8_ipv4_u16_t
ProtocolMessage::get_ipv4_conn_point() const
{
    return get_data_item_value<Div_u8_ipv4_u16_t>(
               ProtocolStrings::IPv4_Connection_Point);
}

Div_u8_ipv6_u16_t
ProtocolMessage::get_ipv6_conn_point() const
{
    return get_data_item_value<Div_u8_ipv6_u16_t>(
               ProtocolStrings::IPv6_Connection_Point);
}

// XXX_ser pass a predicate function?
DataItems
ProtocolMessage::get_metrics_and_ipaddrs() const
{
    DataItems metrics_and_ip_items;

    for (const DataItem & di : data_items)
    {
        if (protocfg->is_metric(di.id) ||
                protocfg->is_ipaddr(di.id))
        {
            metrics_and_ip_items.push_back(di);
        }
    }

    return metrics_and_ip_items;
}

DataItems
ProtocolMessage::get_data_items_no_mac() const
{
    DataItems di_no_mac;
    DataItemIdType mac_id =
        protocfg->get_data_item_id(ProtocolStrings::MAC_Address);

    for (const DataItem & di : data_items)
    {
        if (di.id != mac_id)
        {
            di_no_mac.push_back(di);
        }
    }

    return di_no_mac;
}

DataItems
ProtocolMessage::get_data_items() const
{
    return data_items;
}

