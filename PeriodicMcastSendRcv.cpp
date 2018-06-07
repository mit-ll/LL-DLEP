/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016, 2018 Massachusetts Institute of Technology
 */

/// @file
/// The PeriodicMcastSendRcv class implements a mechanism to
/// send and/or receive periodic multicast packets on an interface.
/// Classes can inherit from this class and provide their own methods
/// for constructing and interpreting the multicast packets.

#include "PeriodicMcastSendRcv.h"
#include "NetUtils.h"

using namespace LLDLEP;
using namespace LLDLEP::internal;

PeriodicMcastSendRcv::PeriodicMcastSendRcv(
    DlepPtr dlep,
    boost::asio::io_service & io_service,
    std::string interface_name,
    uint16_t udp_port,
    boost::asio::ip::address & multicast_addr,
    unsigned int ttl,
    unsigned int send_interval,
    bool sending,
    bool receiving,
    DlepLoggerPtr logger) :
    udp_port(udp_port),
    dlep(dlep),
    logger(logger),
    interface_name(interface_name),
    multicast_address(multicast_addr),
    receiving(receiving),
    receive_socket(io_service),
    sending(sending),
    send_ttl(ttl),
    send_socket(io_service),
    send_timer(io_service),
    send_interval(send_interval),
    recv_message{0}
{
    // If we have a link-local IPv6 multicast address, we have to set
    // its scope id to the interface index to which it is scoped.
    // Without doing this, bind() will give an EINVAL error.

    if (multicast_address.is_v6())
    {
        std::ostringstream msg;
        if (! NetUtils::set_ipv6_scope_id(multicast_address, interface_name))
        {
            msg << "failed to set scope id for multicast address="
                << multicast_address;
            LOG(DLEP_LOG_ERROR, msg);
        }
        else
        {
            msg << "scoped multicast address=" << multicast_address;
            LOG(DLEP_LOG_DEBUG, msg);
        }
    }
}

PeriodicMcastSendRcv::~PeriodicMcastSendRcv()
{
    stop();
}

bool
PeriodicMcastSendRcv::start()
{
    std::ostringstream msg;

    if (!sending && !receiving)
    {
        // This could happen if peer discovery is disabled.
        msg << "Neither sending nor receiving is enabled";
        LOG(DLEP_LOG_INFO, msg);
        return true;
    }

    // Convert the interface_name parameter to an IP address
    // on that interface.

    interface_address =
        NetUtils::get_ip_addr_from_iface(interface_name,
                                         multicast_address.is_v4(),
                                         logger);
    if (interface_address.is_unspecified())
    {
        msg << "Could not determine IP address from interface " << interface_name;
        LOG(DLEP_LOG_ERROR, msg);
        return false;
    }

    msg << "interface=" << interface_name << " address=" << interface_address;
    LOG(DLEP_LOG_INFO, msg);

    if (receiving)
    {
        if (! setup_receive())
        {
            msg << "Problem setting up reception of multicast packets";
            LOG(DLEP_LOG_ERROR, msg);
            return false;
        }
        start_receive();
    }

    if (sending)
    {
        if (! setup_send())
        {
            msg << "Problem setting up sending of multicast packets";
            LOG(DLEP_LOG_ERROR, msg);
            return false;
        }
        start_send();
    }

    return true;
}

void
PeriodicMcastSendRcv::stop()
{
    std::ostringstream msg("stopping");
    LOG(DLEP_LOG_INFO, msg);

    boost::system::error_code ec;

    std::size_t n = send_timer.cancel(ec);
    if (ec)
    {
        msg << "timer cancel error: " << ec;
        LOG(DLEP_LOG_ERROR, msg);
    }
    else
    {
        msg << n << " timer async operations were cancelled";
        LOG(DLEP_LOG_INFO, msg);
    }

    receive_socket.close(ec);
    if (ec)
    {
        msg << "receive socket close error: " << ec;
        LOG(DLEP_LOG_ERROR, msg);
    }

    send_socket.close(ec);
    if (ec)
    {
        msg << "send socket close error: " << ec;
        LOG(DLEP_LOG_ERROR, msg);
    }
}

//*****************************************************************************

// methods related to sending multicast packets

bool
PeriodicMcastSendRcv::setup_send()
{
    std::ostringstream msg;

    send_endpoint.address(multicast_address);
    send_endpoint.port(udp_port);

    msg << "send endpoint=" << send_endpoint;
    LOG(DLEP_LOG_INFO, msg);

    try
    {
        send_socket.open(send_endpoint.protocol());

        msg << "Done assigning multicast address and port";
        LOG(DLEP_LOG_DEBUG, msg);

        send_socket.set_option(boost::asio::ip::udp::socket::reuse_address(true));
        send_socket.set_option(boost::asio::ip::multicast::enable_loopback(true));
        if (send_ttl != 0)
        {
            send_socket.set_option(boost::asio::ip::multicast::hops(send_ttl));
        }

        // Explicitly bind to (any IP addr, any port) for clarity.
        boost::asio::ip::udp::endpoint bind_endpoint;

        if (multicast_address.is_v6())
        {
            bind_endpoint.address(boost::asio::ip::address_v6::any());
        }
        else
        {
            bind_endpoint.address(boost::asio::ip::address_v4::any());
        }

        // let the kernel pick a port
        bind_endpoint.port(0);

        send_socket.bind(bind_endpoint);

        msg << "Done setting socket enable loopback and socket reuse option, bind to "
            << bind_endpoint << " local endpoint " << send_socket.local_endpoint();
        LOG(DLEP_LOG_DEBUG, msg);

        msg << "Setting outbound interface to: " << interface_address.to_string();
        LOG(DLEP_LOG_DEBUG, msg);
        if (interface_address.is_v6())
        {
            // XXX-BNC: need to figure out how to bind to IPv6 outgoing interface;
            // This gives a "no matching function call" error.
            // send_socket.set_option(
            //    boost::asio::ip::multicast::outbound_interface(interface_address.to_v6()));
        }
        else
        {
            send_socket.set_option(
                boost::asio::ip::multicast::outbound_interface(interface_address.to_v4()));
        }
    }
    catch (std::exception & e)
    {
        msg << "Unable to bind and set options on interface";
        LOG(DLEP_LOG_ERROR, msg);
        return false;
    }

    return true;
}

void
PeriodicMcastSendRcv::handle_send(const boost::system::error_code & error)
{
    if (error)
    {
        std::ostringstream msg;
        msg << "error=" << error;
        LOG(DLEP_LOG_ERROR, msg);
    }
    else
    {
        send_timer.expires_from_now(
            boost::posix_time::seconds(send_interval));
        send_timer.async_wait(
            boost::bind(&PeriodicMcastSendRcv::handle_send_timeout,
                        this,
                        boost::asio::placeholders::error));
    }
}

void
PeriodicMcastSendRcv::handle_send_timeout(const boost::system::error_code &
                                          error)
{
    if (error)
    {
        std::ostringstream msg;
        msg << "error=" << error;
        LOG(DLEP_LOG_ERROR, msg);
    }
    else
    {
        start_send();
    }
}

void
PeriodicMcastSendRcv::start_send()
{
    std::ostringstream msg;

    unsigned int msg_len;
    DlepMessageBuffer msg_buffer = get_message_to_send(&msg_len);

    msg << "sending packet with size=" << msg_len << " to " << send_endpoint;
    LOG(DLEP_LOG_DEBUG, msg);

    send_socket.async_send_to(
        boost::asio::buffer(msg_buffer.get(), msg_len), send_endpoint,
        boost::bind(&PeriodicMcastSendRcv::handle_send, this,
                    boost::asio::placeholders::error));
}

//*****************************************************************************

// methods related to receiving multicast packets

bool
PeriodicMcastSendRcv::setup_receive()
{
    std::ostringstream msg;
    unsigned long if_index = if_nametoindex(interface_name.c_str());

    // set up the receive endpoint

    boost::asio::ip::udp::endpoint receive_endpoint;
    receive_endpoint.port(udp_port);
    receive_endpoint.address(multicast_address);

    msg << "receive endpoint=" << receive_endpoint;
    LOG(DLEP_LOG_INFO, msg);

    try
    {
        receive_socket.open(receive_endpoint.protocol());

        // set socket options for multicast:
        receive_socket.set_option(boost::asio::ip::udp::socket::reuse_address(true));
        receive_socket.set_option(boost::asio::ip::multicast::enable_loopback(true));

        receive_socket.bind(receive_endpoint);

        // join multicast group
        msg << "Joining Multicast Group: " << multicast_address
            << ":" << udp_port;
        LOG(DLEP_LOG_INFO, msg);

        if (multicast_address.is_v4())
        {
            receive_socket.set_option(
                boost::asio::ip::multicast::join_group(
                    multicast_address.to_v4(),
                    interface_address.to_v4()));
        }
        else
        {
            receive_socket.set_option(
                boost::asio::ip::multicast::join_group(
                    multicast_address.to_v6(),
                    if_index));
        }
    }
    catch (std::exception & e)
    {
        msg << "Unable to bind and set options on receive socket: " << e.what();
        LOG(DLEP_LOG_ERROR, msg);
        return false;
    }

    msg << "Finished Setting Socket Options - Initiating receive";
    LOG(DLEP_LOG_INFO, msg);
    return true;
}

void
PeriodicMcastSendRcv::start_receive()
{
    receive_socket.async_receive_from(
        boost::asio::buffer(recv_message), remote_endpoint,
        boost::bind(&PeriodicMcastSendRcv::handle_receive, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
}

void
PeriodicMcastSendRcv::handle_receive(const boost::system::error_code & error,
                                     std::size_t bytes_recvd)
{
    std::ostringstream msg;

    if (error)
    {
        msg << "Got a discovery socket receive error of " << error;
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
        // check for self msg since loppback may be enabled
        if (sending and
                (remote_endpoint.address() == interface_address) and
                (remote_endpoint.port()    == send_socket.local_endpoint().port()))
        {
            msg << "Received Message from self " << remote_endpoint
                << " size=" << bytes_recvd << " drop";
            LOG(DLEP_LOG_INFO, msg);
        }
        else
        {
            msg << "Received Message from " << remote_endpoint
                << " size=" << bytes_recvd;
            LOG(DLEP_LOG_INFO, msg);

            // Copy the received message to a DlepMessageBuffer

            DlepMessageBuffer msg_buffer(new uint8_t[bytes_recvd]);
            memcpy(msg_buffer.get(), recv_message, bytes_recvd);

            handle_message(msg_buffer, bytes_recvd, remote_endpoint);
        }
    }

    start_receive();
}

