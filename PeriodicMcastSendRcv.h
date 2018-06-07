/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016, 2018 Massachusetts Institute of Technology
 */

/// @file
/// PeriodicMcastSendRcv class declaration.

#ifndef PERIODIC_MCAST_SEND_RCV_H
#define PERIODIC_MCAST_SEND_RCV_H

#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <vector>

#include "Dlep.h"
#include "DlepLogger.h"
#include "ProtocolMessage.h"

namespace LLDLEP
{
namespace internal
{

/// This class provides a way to send and/or receive periodic multicast
/// packets.
/// To use this class:
/// 1.  Create a class that inherits from PeriodicMcastSendRcv.
/// 2.  In the constructor of your subclass, call the PeriodicMcastSendRcv
///     constructor with the appropriate configuration.  You can choose
///     to send, receive, both, or neither.
/// 3.  If you have specified sending, provide an implementation of
///     get_message_to_send().
/// 4.  If you have specified receiving, provide an implementation of
///     handle_message().
class PeriodicMcastSendRcv
{
public:

    /// Constructor.
    /// @param[in] dlep           back-pointer to dlep instance
    /// @param[in] io_service     for handling network I/O and timers
    /// @param[in] interface_name name of the network interface to use
    /// @param[in] udp_port       UDP port number to put on multicast packets
    /// @param[in] multicast_addr multicast address to put on packets
    /// @param[in] ttl            IP TTL to put on multicast packets
    /// @param[in] send_interval  number of seconds between sending packets
    /// @param[in] sending        true iff we are sending packets
    /// @param[in] receiving      true iff we are receiving packets
    /// @param[in] logger         logger, for LOG macro
    PeriodicMcastSendRcv(DlepPtr dlep,
                         boost::asio::io_service & io_service,
                         std::string interface_name,
                         uint16_t udp_port,
                         boost::asio::ip::address & multicast_addr,
                         unsigned int ttl,
                         unsigned int send_interval,
                         bool sending,
                         bool receiving,
                         DlepLoggerPtr logger);
    virtual ~PeriodicMcastSendRcv();

    /// Start send/receive operations.
    /// @return true if successful, else false.
    virtual bool start();

    /// Stop all send/receive operations.
    /// After this is called, start() cannot be called again.
    virtual void stop();

protected:

    /// Handle the payload of a received multicast packet.
    ///
    /// @param[in] msg_buffer      payload bytes of the multicast packet
    /// @param[in] msg_buffer_len  number of bytes in msg_buffer
    /// @param[in] from_endpoint   source IP addr/port of this packet
    virtual void handle_message(DlepMessageBuffer msg_buffer,
                                unsigned int msg_buffer_len,
                                boost::asio::ip::udp::endpoint from_endpoint) = 0;

    /// Called when it is time to send a multicast packet.
    ///
    /// @param[out] msg_len upon return, this contains the number of bytes
    ///                     in the returned message buffer
    ///
    /// @return the payload bytes of the multicast packet to be sent
    virtual DlepMessageBuffer get_message_to_send(unsigned int * msg_len) = 0;

    /// UDP port number for multicast packets
    uint16_t udp_port;

    /// IP address of the local interface being used for sending/receiving
    /// multicast packets
    boost::asio::ip::address interface_address;

    /// back-pointer to dlep instance
    DlepPtr dlep;

    /// logger that gets shared throughout the library
    DlepLoggerPtr logger;

private:

    /// Initialize reception of multicast packets.
    bool setup_receive();

    /// Initiate boost::asio async receive to get multicast packets.
    void start_receive();

    /// boost::asio calls this to deliver received multicast packets to us
    /// Calls the subclass's handle_message().
    /// param[in] bytes_recvd number of bytes in received message.
    ///           The buffer containing the actual message is specified
    ///           beforehand in the boost asio call async_receive_from().
    void handle_receive(const boost::system::error_code & error,
                        std::size_t bytes_recvd);

    /// Initialize periodic sending of multicast packets.
    /// @return true if successful, else false
    bool setup_send();

    /// Build and send a multicast packet.
    /// Calls the subclass's get_message_to_send().
    void start_send();

    /// boost::asio calls this when the send is complete
    /// param[in] error boost error code, 0 if no error
    void handle_send(const boost::system::error_code & error);

    /// boost::asio calls this when the send_timer expires
    /// param[in] error boost error code, 0 if no error
    void handle_send_timeout(const boost::system::error_code & error);

    /// Name of network interface to send/receive packets on
    std::string interface_name;

    /// multicast address to send to/receive from
    boost::asio::ip::address multicast_address;

    /// Are we supposed to receive multicast packets?
    bool receiving;

    /// Socket used to receive multicast packets
    boost::asio::ip::udp::socket receive_socket;

    /// Remote info of received multicast packets
    boost::asio::ip::udp::endpoint remote_endpoint;

    /// Multicast address/port to which packets are sent
    boost::asio::ip::udp::endpoint send_endpoint;

    /// Are we supposed to send multicast packets?
    bool sending;

    // IP TTL to put on multicast packets
    unsigned int send_ttl;

    /// Socket used to send multicast packets
    boost::asio::ip::udp::socket send_socket;

    /// Timer for sending periodic multicast packets
    boost::asio::deadline_timer send_timer;

    /// Interval (seconds) between sending discovery messages
    unsigned int send_interval;

    /// Buffer to hold received messages
    uint8_t recv_message[ProtocolMessage::MAX_SIGNAL_SIZE];
};

} // namespace internal
} // namespace LLDLEP

#endif // PERIODIC_MCAST_SEND_RCV_H
