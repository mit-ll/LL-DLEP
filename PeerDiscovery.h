/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016 Massachusetts Institute of Technology
 */

/// @file
/// PeerDiscovery class declaration.

#ifndef PEER_DISCOVERY_H
#define PEER_DISCOVERY_H

#include "PeriodicMcastSendRcv.h"

namespace LLDLEP
{
namespace internal
{

/// This class handles most of the PeerDiscovery part of DLEP.
/// It combines both the router and the modem parts.
/// Documentation for override methods is in the base class.
class PeerDiscovery : public PeriodicMcastSendRcv
{
public:

    /// Constructor.
    /// @param[in] dlep           back-pointer to dlep instance
    /// @param[in] io_service     for handling network I/O and timers
    /// @param[in] interface_name name of the network interface to use
    /// @param[in] udp_port       UDP port number to put on multicast packets
    /// @param[in] multicast_address  multicast address to put on packets
    /// @param[in] discovery_ttl  IP TTL to put on discovery packets
    /// @param[in] send_interval  number of seconds between sending packets
    /// @param[in] sending        true iff we are sending packets
    /// @param[in] receiving      true iff we are receiving packets
    PeerDiscovery(
        DlepPtr dlep,
        boost::asio::io_service & io_service,
        std::string interface_name,
        uint16_t udp_port,
        boost::asio::ip::address & multicast_address,
        unsigned int discovery_ttl,
        unsigned int send_interval,
        bool sending,
        bool receiving);

    virtual ~PeerDiscovery();

    bool start() override;

private:

    void stop() override;

    /// Initiate boost::asio async receive to get Peer Offer (unicast) packets.
    /// Router only.
    void start_receive();


    /// boost::asio calls this to deliver received unicast packets to us
    /// Router only.
    /// param[in] bytes_recvd number of bytes in received message.
    ///           The buffer containing the actual message is specified
    ///           beforehand in the boost asio call async_receive_from().

    void handle_receive(const boost::system::error_code & error,
                        std::size_t bytes_recvd);

    /// Handle a received peer discovery or offer message.
    /// Router and modem.
    /// @param[in] msg_buffer      payload bytes of the packet
    /// @param[in] msg_buffer_len  number of bytes in msg_buffer
    /// @param[in] from_endpoint   source IP addr/port of this packet
    void handle_message(DlepMessageBuffer msg_buffer,
                        unsigned int msg_buffer_len,
                        boost::asio::ip::udp::endpoint from_endpoint) override;

    /// Handle a received peer discovery message.
    /// Modem only.
    /// @param[in] pm    the peer discovery message
    /// @param[in] from_endpoint  source IP addr/port of this packet
    void handle_discovery(ProtocolMessage & pm,
                          boost::asio::ip::udp::endpoint from_endpoint);

    /// Handle a received peer offer message.
    /// Router only.
    /// @param[in] pm    the peer offer message
    /// @param[in] from_endpoint  source IP addr/port of this packet
    void handle_peer_offer(ProtocolMessage & pm,
                           boost::asio::ip::udp::endpoint from_endpoint);

    /// Router only.
    DlepMessageBuffer get_message_to_send(unsigned int * msg_len) override;

    /// Build and send a peer offer message.
    /// Modem only.
    /// @param[in] to_endpoint contains the IP address to send the message to
    void send_peer_offer(boost::asio::ip::udp::endpoint to_endpoint);

    /// boost::asio calls this when the send is complete.
    /// Modem only.
    void handle_send_peer_offer(const boost::system::error_code & error);

    /// Socket used to send/receive the unicast Peer Offer signal
    /// Router and modem.
    boost::asio::ip::udp::socket peer_offer_socket;

    /// Source of a received Peer Offer message
    /// Router only.
    boost::asio::ip::udp::endpoint modem_endpoint;

    /// IP TTL to put on discovery packets
    unsigned int send_ttl;

    /// Buffer to hold received Peer Offer messages
    /// Router only.
    uint8_t recv_peer_offer_message[ProtocolMessage::MAX_SIGNAL_SIZE];
};

} // namespace internal
} // namespace LLDLEP

#endif // PEER_DISCOVERY_H
