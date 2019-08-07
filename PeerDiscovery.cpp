/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016 Massachusetts Institute of Technology
 */

/// @file
/// The PeerDiscovery class implements the DLEP protocol discovery phase,
/// before peers have a TCP connection established.  Much of the work is
/// done by class PeriodicMcastSendRcv, but the unicast Peer Offer signal
/// is handled in this file.

#include "PeerDiscovery.h"
#include "ProtocolConfig.h"
#include "ProtocolMessage.h"

using namespace LLDLEP;
using namespace LLDLEP::internal;

PeerDiscovery::PeerDiscovery(
    DlepPtr dlep,
    boost::asio::io_service & io_service,
    std::string interface_name,
    uint16_t udp_port,
    boost::asio::ip::address & multicast_address,
    unsigned int discovery_ttl,
    unsigned int send_interval,
    bool sending,
    bool receiving) :
    PeriodicMcastSendRcv(dlep, io_service, interface_name, udp_port,
                         multicast_address, discovery_ttl, send_interval,
                         sending, receiving, dlep->logger),
    peer_offer_socket(io_service),
    send_ttl(discovery_ttl),
    recv_peer_offer_message{0}
{
}

PeerDiscovery::~PeerDiscovery()
{
}

bool
PeerDiscovery::start()
{
    std::ostringstream msg;
    boost::recursive_mutex::scoped_lock lock(dlep->mutex);

    if (! PeriodicMcastSendRcv::start())
    {
        return false;
    }

    // Initialize peer_offer_socket.  The modem sends the Peer Offer
    // on this socket, and the router receives it.  This socket has
    // nothing to do with the TCP session.  Dlep.cpp handles the
    // connect/accept for the TCP session.

    try
    {
        boost::asio::ip::udp::endpoint endpoint(interface_address,
                                                udp_port);
        peer_offer_socket.open(endpoint.protocol());
        if (send_ttl != 0)
        {
            peer_offer_socket.set_option(
                boost::asio::ip::unicast::hops(send_ttl));
        }

        // If we are the router, bind the socket to the expected
        // udp port and local interface so that we get the only the
        // packets that were unicast to us.

        if (! dlep->is_modem())
        {
            msg << "binding peer_offer socket to " << endpoint;
            LOG(DLEP_LOG_INFO, msg);
            peer_offer_socket.bind(endpoint);
            // start receiving packets from the peer_offer socket
            start_receive();
        }
    }
    catch (const std::exception & e)
    {
        msg << "error initializing peer_offer socket: " << e.what();
        LOG(DLEP_LOG_ERROR, msg);
        return false;
    }

    return true;
}

void
PeerDiscovery::stop()
{
    std::ostringstream msg;

    PeriodicMcastSendRcv::stop();

    boost::system::error_code ec;
    peer_offer_socket.close(ec);
    if (ec)
    {
        msg << "socket close error: " << ec;
        LOG(DLEP_LOG_ERROR, msg);
    }
}

DlepMessageBuffer
PeerDiscovery::get_message_to_send()
{
//    std::ostringstream msg;
    boost::recursive_mutex::scoped_lock lock(dlep->mutex);

    ProtocolMessage pm {dlep->protocfg, dlep->logger};

    pm.add_header(LLDLEP::ProtocolStrings::Peer_Discovery);
    pm.add_common_data_items(dlep->dlep_client);

    // A freshly built message should be parseable.

    std::string err = pm.parse_and_validate(dlep->is_modem(), __func__);
    assert(err == "");

    // Copy the protocol message into a DlepMessageBuffer

    std::size_t pm_length = pm.get_length();
    DlepMessageBuffer pm_buffer {new std::vector<std::uint8_t>(pm_length)};
    memcpy(pm_buffer->data(), pm.get_buffer(), pm_length);

    return pm_buffer;
}

void
PeerDiscovery::start_receive()
{
    peer_offer_socket.async_receive_from(
        boost::asio::buffer(recv_peer_offer_message), modem_endpoint,
        boost::bind(&PeerDiscovery::handle_receive, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
}

void
PeerDiscovery::handle_receive(const boost::system::error_code & error,
                              std::size_t bytes_recvd)
{
    std::ostringstream msg;

    if (error)
    {
        msg << "Got a peer offer receive error of " << error;
        LOG(DLEP_LOG_ERROR, msg);
        if (error == boost::asio::error::operation_aborted)
        {
            // The operation was cancelled, which means we're
            // shutting down.  We don't want to restart the
            // read at the end of this function, so return here.
            return;
        }
    }
    else
    {
        msg << "Received Message from " << modem_endpoint
            << " size=" << bytes_recvd;
        LOG(DLEP_LOG_INFO, msg);

        DlepMessageBuffer msg_buffer {new std::vector<std::uint8_t>(bytes_recvd)};
        memcpy(msg_buffer->data(), recv_peer_offer_message, bytes_recvd);

        handle_message(msg_buffer, modem_endpoint);
    }

    start_receive();
}

void
PeerDiscovery::handle_message(DlepMessageBuffer msg_buffer,
                              boost::asio::ip::udp::endpoint from_endpoint)
{
    std::ostringstream msg;
    boost::recursive_mutex::scoped_lock lock(dlep->mutex);

    if (msg_buffer->size() > 0)
    {
        msg << "from=" << from_endpoint << " size=" << msg_buffer->size();
        LOG(DLEP_LOG_INFO, msg);

        ProtocolMessage pm {dlep->protocfg, dlep->logger};

        std::string err =
            pm.parse_and_validate(msg_buffer->data(), msg_buffer->size(), true,
                                  ! dlep->is_modem(), __func__);

        if (err != "")
        {
            msg << "invalid message: " << err << " from " << from_endpoint
                << ", ignoring";
            LOG(DLEP_LOG_ERROR, msg);
            return;
        }

        // Discovery and Peer Offer don't have peers setup yet

        std::string msgname = pm.get_signal_name();

        if (msgname == ProtocolStrings::Peer_Discovery)
        {
            handle_discovery(pm, from_endpoint);
        }
        else if (msgname == ProtocolStrings::Peer_Offer)
        {
            handle_peer_offer(pm, from_endpoint);
        }
        else
        {
            msg << "unhandled message " << msgname;
            LOG(DLEP_LOG_ERROR, msg);
        }
    }
    else
    {
        msg << "empty packet received";
        LOG(DLEP_LOG_ERROR, msg);
    }
}

void
PeerDiscovery::handle_discovery(ProtocolMessage & pm,
                                boost::asio::ip::udp::endpoint from_endpoint)
{
    std::ostringstream msg;

    std::string peer_id = from_endpoint.address().to_string() + ":" +
                          std::to_string(from_endpoint.port());

    msg << "Received " << pm.get_signal_name() << " from " << peer_id;
    LOG(DLEP_LOG_INFO, msg);

    if (! dlep->is_modem())
    {
        msg << "Ignoring  " << pm.get_signal_name()
            << " because we're the router";
        LOG(DLEP_LOG_INFO, msg);
        return;
    }

    // Check to see if we don't have an existing session before proceeding:
    if (dlep->peer_state(peer_id) != PeerState::nonexistent)
    {
        msg << "peer=" << peer_id << " already in session";
        LOG(DLEP_LOG_INFO, msg);
        return;
    }
    else
    {
        msg << "peer=" << peer_id << " is in PeerState::nonexistent state";
        LOG(DLEP_LOG_INFO, msg);
    }

    // XXX_ser check version?

    send_peer_offer(from_endpoint);
}

void
PeerDiscovery::handle_peer_offer(ProtocolMessage & pm,
                                 boost::asio::ip::udp::endpoint from_endpoint)
{
    std::ostringstream msg;

    msg << "Received " << pm.get_signal_name() << " from "
        << from_endpoint.address().to_string();
    LOG(DLEP_LOG_INFO, msg);

    if (dlep->is_modem())
    {
        msg << "Ignoring " << pm.get_signal_name()
            << " because we're the modem";
        LOG(DLEP_LOG_INFO, msg);
        return;
    }

    // By default, connect to the configured session port and the
    // address from whence the Peer Offer came.

    unsigned int session_port;
    dlep->dlep_client.get_config_parameter("session-port", &session_port);
    boost::asio::ip::address dest_ip = from_endpoint.address();

    // Get the optional session port from the message.  If there isn't
    // one, use the configured session-port from above.

    try
    {
        session_port = pm.get_port();
    }
    catch (const ProtocolMessage::DataItemNotPresent &)
    {
        /* no-op */
    }
    catch (const ProtocolConfig::BadDataItemName &)
    {
        /* no-op */
    }

    // Try to get the session connection address (dest_ip) from data
    // items in the Peer Offer.  Look for the following data items in
    // this order: IPv4_Address, IPv4_Connection_Point, IPv6_Address,
    // IPv6_Connection_Point.  The last one that is found wins.
    // Fetching any of these data items may fail either because the
    // data item doesn't exist in the Peer Offer (DataItemNotPresent),
    // or the data item doesn't exist in the current protocol
    // configuration (BadDataItemName).  These are recoverable errors;
    // we just move on and try the next one.  If they all fail, we
    // have address/port defaults established above.
    //
    // IPv4/6_Connection_Points also specify the port number to
    // connect to (session_port).

    // If a data item specifies an IPv6 address, it may need to be
    // locally augmented with a scope id (interface index) if it is a
    // link-local address.  may_need_scope_id will be set to true in
    // this case.

    bool may_need_scope_id = false;

    try // IPv4_Address data item
    {
        Div_u8_ipv4_t u8ipv4 = pm.get_ipv4_address();
        if (u8ipv4.field1)
        {
            dest_ip = u8ipv4.field2;
        }
        else
        {
            msg << ProtocolStrings::IPv4_Address << " " << u8ipv4.field2
                <<  " has add/drop=" << u8ipv4.field1 << ", ignoring";
            LOG(DLEP_LOG_ERROR, msg);
        }
    }
    catch (const ProtocolMessage::DataItemNotPresent &)
    {
        /* no-op */
    }
    catch (const ProtocolConfig::BadDataItemName &)
    {
        /* no-op */
    }

    try // IPv4_Connection_Point data item
    {
        Div_u8_ipv4_u16_t ipv4_connpt = pm.get_ipv4_conn_point();
        session_port = ipv4_connpt.field3;
        dest_ip = boost::asio::ip::address(ipv4_connpt.field2);
    }
    catch (const ProtocolMessage::DataItemNotPresent &)
    {
        /* no-op */
    }
    catch (const ProtocolConfig::BadDataItemName &)
    {
        /* no-op */
    }

    try // IPv6_Address data item
    {
        Div_u8_ipv6_t u8ipv6 = pm.get_ipv6_address();
        if (u8ipv6.field1)
        {
            dest_ip = u8ipv6.field2;
            may_need_scope_id = true;
        }
        else
        {
            msg << ProtocolStrings::IPv6_Address << " " << u8ipv6.field2
                <<  " has add/drop=" << u8ipv6.field1 << ", ignoring";
            LOG(DLEP_LOG_ERROR, msg);
        }
    }
    catch (const ProtocolMessage::DataItemNotPresent &)
    {
        /* no-op */
    }
    catch (const ProtocolConfig::BadDataItemName &)
    {
        /* no-op */
    }

    try // IPv6_Connection_Point data item
    {
        // XXX check flags field?
        Div_u8_ipv6_u16_t ipv6_connpt = pm.get_ipv6_conn_point();
        session_port = ipv6_connpt.field3;
        dest_ip = boost::asio::ip::address(ipv6_connpt.field2);
        may_need_scope_id = true;
    }
    catch (const ProtocolMessage::DataItemNotPresent &)
    {
        /* no-op */
    }
    catch (const ProtocolConfig::BadDataItemName &)
    {
        /* no-op */
    }

    // If we may need a scope id, see if dest_ip is an IPv6 link-local
    // address.  If it is, copy the scope id to dest_ip from the
    // address in from_endpoint, which is the source of the Peer
    // Offer.  It's not clear that this is always the right answer;
    // dest_ip might actually only be reachable via some interface
    // other than the one we received the Peer Offer on.  I don't know
    // how to find the right interface in that case, though.

    if (may_need_scope_id)
    {
        boost::asio::ip::address_v6 dest_ip_v6 = dest_ip.to_v6();
        if (dest_ip_v6.is_link_local())
        {
            if (from_endpoint.address().is_v6())
            {
                boost::asio::ip::address_v6 from_ip_v6 =
                    from_endpoint.address().to_v6();
                unsigned long scope_id = from_ip_v6.scope_id();
                dest_ip_v6.scope_id(scope_id);
                dest_ip = dest_ip_v6;
                msg << "scope id " << scope_id
                    << " copied from Peer Offer origin " << from_endpoint
                    << " to session connect address= " << dest_ip;
                LOG(DLEP_LOG_INFO, msg);
            }
            else // from_endpoint is an IPv4 address
            {
                msg << "cannot determine scope id for session connect address= "
                    << dest_ip;
                LOG(DLEP_LOG_ERROR, msg);
                return;
            }
        }
    } // may_need_scope_id

    // Now that we have dest_ip in its final form, check to see if we
    // don't have an existing session before proceeding.

    std::string peer_id =
        dest_ip.to_string() + ":" + std::to_string(session_port);
    if (dlep->peer_state(peer_id) != PeerState::nonexistent)
    {
        msg << "peer=" << peer_id << " already in session";
        LOG(DLEP_LOG_INFO, msg);
        return;
    }
    else
    {
        msg << "peer=" << peer_id << " is in PeerState::nonexistent state";
        LOG(DLEP_LOG_INFO, msg);
    }

    try
    {
        msg << "Creating and connecting to " << peer_id;
        LOG(DLEP_LOG_INFO, msg);

        // Initiate the TCP connection
        dlep->start_async_connect(dest_ip, session_port);

    }
    catch (std::exception & e)
    {
        msg << "exception: " << e.what();
        LOG(DLEP_LOG_ERROR, msg);
    }
}

void
PeerDiscovery::handle_send_peer_offer(const boost::system::error_code & error)
{
    if (error)
    {
        std::ostringstream msg;
        msg << "error=" << error;
        LOG(DLEP_LOG_ERROR, msg);
    }
}

void
PeerDiscovery::send_peer_offer(boost::asio::ip::udp::endpoint to_endpoint)
{
    std::ostringstream msg;

    ProtocolMessage pm {dlep->protocfg, dlep->logger};

    pm.add_header(LLDLEP::ProtocolStrings::Peer_Offer);
    pm.add_common_data_items(dlep->dlep_client);

    // Get the configured session port (required)

    unsigned int session_port;
    dlep->dlep_client.get_config_parameter("session-port", &session_port);

    // Get the configured session address (optional)

    boost::asio::ip::address session_address;
    bool have_session_address = false;
    try
    {
        dlep->dlep_client.get_config_parameter("session-address",
                                               &session_address);
        have_session_address = true;
    }
    catch (const DlepClient::BadParameterName &)
    {
        // The session_address config paramater is optional, so we
        // don't need to do anything if it wasn't found.

        // XXX_ser happens if it's configured, but not a valid address?
    }

    // Try to set the Port and IPvX_Address data items.  This may fail
    // if the protocol configuration does not allow the Port data item.

    try
    {
        DataItemValue div_port = std::uint16_t(session_port);
        DataItem di_session_port {LLDLEP::ProtocolStrings::Port, div_port,
                                  dlep->protocfg
                                 };
        pm.add_data_item(di_session_port);

        if (have_session_address)
        {
            DataItemValue div;
            std::string di_name;
            if (session_address.is_v4())
            {
                Div_u8_ipv4_t ipv4_val {1, session_address.to_v4()};
                div = ipv4_val;
                di_name = LLDLEP::ProtocolStrings::IPv4_Address;
            }
            else
            {
                assert(session_address.is_v6());
                Div_u8_ipv6_t ipv6_val {1, session_address.to_v6()};
                div = ipv6_val;
                di_name = LLDLEP::ProtocolStrings::IPv6_Address;
            }

            DataItem di_session_address {di_name, div, dlep->protocfg};
            pm.add_data_item(di_session_address);
        }
    }
    catch (const LLDLEP::ProtocolConfig::BadDataItemName &)
    {
        // We are using a protocol configuration for a more recent
        // draft that doesn't have the Port data item.  Try again with
        // the IPvX_Connection_Point data item.

        if (! have_session_address)
        {
            // The IPvX_Connection_Point data item requires that we
            // fill in a connection address.  We'll just reuse the
            // the discovery address from our PeriodicMcastSendRcv
            // parent class.  Perhaps there should be a separate
            // session-iface config parameter.

            session_address = interface_address;
        }

        DataItemValue div;
        std::string di_name;
        if (session_address.is_v4())
        {
            Div_u8_ipv4_u16_t ipv4_connpt {0, session_address.to_v4(),
                                           std::uint16_t(session_port)
                                          };
            div = ipv4_connpt;
            di_name = LLDLEP::ProtocolStrings::IPv4_Connection_Point;
        }
        else
        {
            assert(session_address.is_v6());
            Div_u8_ipv6_u16_t ipv6_connpt {0, session_address.to_v6(),
                                           std::uint16_t(session_port)
                                          };
            div = ipv6_connpt;
            di_name = LLDLEP::ProtocolStrings::IPv6_Connection_Point;
        }

        DataItem di_conn_pt {di_name, div, dlep->protocfg};
        pm.add_data_item(di_conn_pt);
    }

    boost::asio::ip::udp::endpoint send_endpoint(to_endpoint.address(),
                                                 udp_port);
    msg << "Sending signal to " << send_endpoint;
    LOG(DLEP_LOG_INFO, msg);

    // A freshly built message should be parseable.

    std::string err = pm.parse_and_validate(dlep->is_modem(), __func__);
    assert(err == "");

    peer_offer_socket.async_send_to(
        boost::asio::buffer(pm.get_buffer(), pm.get_length()), send_endpoint,
        boost::bind(&PeerDiscovery::handle_send_peer_offer, this,
                    boost::asio::placeholders::error));
}
