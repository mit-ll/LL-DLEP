/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2013, 2014, 2015, 2016 Massachusetts Institute of Technology
 */

/// @file
/// Peer class declaration

#ifndef PEER_H
#define PEER_H

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <strings.h>
#include <time.h>
#include "Thread.h"
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <map>
#include <queue>

#include "DlepLogger.h"
#include "DlepCommon.h"
#include "Dlep.h"
#include "InfoBaseMgr.h"
#include "IdTypes.h"
#include "ProtocolMessage.h"

namespace LLDLEP
{
namespace internal
{

class Dlep;
class DestinationData;
class PeerData;
class ProtocolMessage;

/// State of the connection to this peer.
enum class PeerState
{
    /// This peer does not exist.  An actual Peer object will never be
    /// in this state, but it is convenient to have it as a return value
    /// for some peer lookup functions.
    nonexistent,

    /// TCP connection to Peer is established.  However, the DLEP
    /// init/init_response handshake is not complete.
    connected,

    /// TCP connection established and DLEP init/init_response handshake IS
    /// complete.  This corresponds to the in-session state described
    /// in the DLEP draft.
    in_session,

    /// Peer is in the process of being terminated.  One of the following
    /// has occurred:
    /// - The Peer has triggered an error condition, e.g. sending us
    ///   an invalid signal.  We have sent it a PEER_TERMINATION
    ///   signal and are waiting for a PEER_TERMINATION_Response from the
    ///   Peer.
    /// - We have received a PEER_TERMINATION signal from the Peer and
    ///   have sent it a PEER_TERMINATION_Response.
    terminating
};

/// Stream output operator for PeerState.
std::ostream & operator<<(std::ostream &, PeerState);

/// Information about a response signal from a peer that we
/// are expecting to receive.  This is used as the value in a
/// map<DlepMac, ResponsePending>.
struct ResponsePending
{
    // Constructor

    /// @param[in] protocfg  protocol configuration object
    /// @param[in] pm        the protocol message for which the response is
    ///                      expected
    ResponsePending(const ProtocolConfig * protocfg,
                    const ProtocolMessage & pm);

    /// Name of the queue that this ResponsePending belongs on, for logging.
    std::string queue_name() const;

    /// id of response expected
    LLDLEP::SignalIdType response_id;

    /// Name of response expected
    std::string response_name;

    /// Destination expected in the response.  If it is a session
    /// message/response that doesn't involve a destination, leave it
    /// as the default value.
    LLDLEP::DlepMac destination;

    /// Has this been added to a ResponsePending queue?
    bool queued;

    /// Buffer containing the complete, serialized signal to which a
    /// response is expected.  Used to retransmit the signal in case the
    /// response is lost.
    DlepMessageBuffer msg_buffer;

    /// Number of bytes in msg_buffer
    unsigned int msg_buffer_len;

    /// Last time at which the message in msg_buffer was sent.
    /// This gets reset to the current time when it is retransmitted.
    std::time_t send_time;

    /// Total number of times the message in msg_buffer was sent.
    /// This counts the original transmission.
    unsigned int send_tries;
};

typedef boost::shared_ptr<ResponsePending> ResponsePendingPtr;

//
// A Peer object manages the connection to a peer.  In general, a Modem
// will have one peer (the router) while a router could have several peers
// (modems).
//
// The data for the peer, including Peer information and Destination information
// is contained in the Information Base.
//
class Peer : public boost::enable_shared_from_this<Peer>
{
public:

    /// Peer constructor.  A peer does not exist until we have an
    /// open socket to it.  A peer starts out in state ps_connected.
    /// @param peer_socket established socket connection to the peer
    /// @param dlep        main Dlep object
    Peer(boost::asio::ip::tcp::socket * peer_socket, DlepPtr dlep);
    ~Peer();

    // Start and stop the Peer thread
    void start_peer();
    void stop_peer();

    /// Terminate a peer.  Send a Session_Termination signal to the peer
    /// and move the peer to the PeerState::terminating state.
    ///
    /// @param[in] status_name  ProtocolStrings name of the status code to put
    ///                         in the Peer Termination's Status data item
    ///                         that is sent to the peer
    /// @param[in] reason       explanatory string to include in the Status
    ///                         data item
    void terminate(const std::string & status_name,
                   const std::string & reason = "");
    void cancel_session();

    /// Get current peer state.
    PeerState get_state();
    void get_info(LLDLEP::PeerInfo & peer_info);

    // Generally, modems call these to bring up or down a Destination, and
    // update its metrics.  These cause the appropriate Dlep messages
    // to be sent to peers
    void destination_up(const LLDLEP::DlepMac & destination_mac,
                        const LLDLEP::DataItems & initial_data_items);
    void destination_down(const LLDLEP::DlepMac & destination_mac);
    void destination_update(const LLDLEP::DlepMac & mac,
                            const LLDLEP::DataItems & updates);

    // Peer metric updates.  Causes Peer_update message to peer, waits
    // for ack with a timeout and number of retries specified in config file
    bool peer_update(const LLDLEP::DataItems & updates);

    void link_characteristics_request(const LLDLEP::DlepMac & mac,
                                      const LLDLEP::DataItems & requests);

    void link_characteristics_response(const LLDLEP::DlepMac & mac,
                                  const LLDLEP::DataItems & updates);

    // Return info about a destination with the given MAC
    // address. NULL if no such destination exists.
    bool get_destination(const LLDLEP::DlepMac & mac,
                         boost::shared_ptr<DestinationData> & destination);

    bool remove_destination(const LLDLEP::DlepMac & mac);

    /// Search for an IP address on this peer.
    /// @param[in] ip_data_item
    ///            data item containing an IP address to search for in this
    ///            peer's IP addresses
    /// @return "" if found, else a non-empty string identifying where the IP
    ///         address was found: this peer, or one of its destinations.
    std::string find_ip_data_item(const DataItem & ip_data_item) const;

    /// Validate a set of new data items against the existing data
    /// items belonging to a peer or destination.  Look for inconsistencies
    /// in IP data items such as:
    /// - adding an IP data item that already exists on this or any other
    ///   peer, or any destination of any peer
    /// - removing an IP data item that does not exist on this peer/destination
    ///
    /// @param[in] new_data_items
    ///            the set of new data items given for this peer.
    ///            Each IP data item has its own add/remove flag.
    ///            Data items that are not IP addresses are ignored.
    /// @param[in] existing_ip_data_items
    ///            the set of IP data items already associated with a
    ///            particular peer or destination.  This is only used
    ///            to validate IP data item removals.
    /// @return "" if no inconsistencies were found, else a message
    ///         describing the inconsistency.
    std::string validate_ip_data_items(const DataItems & new_data_items,
                                       const DataItems & existing_ip_data_items);

    /// Unique string identifying this peer.
    std::string peer_id;

private:

    void handle_send(const boost::system::error_code & error);
    void send_session_message(const uint8_t * packet, int size);
    void handle_session_receive(const boost::system::error_code & error,
                                size_t bytes_recvd);

    void schedule_heartbeat();
    void handle_heartbeat_timeout(const boost::system::error_code & error);

    void schedule_acktivity_timer();
    void handle_acktivity_timeout(const boost::system::error_code & error);
    bool check_for_activity(std::time_t current_time);
    void check_for_retransmits(std::time_t current_time);

    bool is_not_interested(const DlepMac & destination) const;
    void not_interested(const DlepMac & destination);
    void send_message_expecting_response(ResponsePendingPtr rp);
    bool handle_response(const ProtocolMessage & pm);
    bool should_send_response(const std::string & response_name) const;
    void send_simple_response(const std::string & response_name,
                         const std::string & status_name,
                         const std::string & status_message = "",
                         const LLDLEP::DlepMac * mac = nullptr);

    void stop_timers();

    bool check_status_code_failure(ProtocolMessage & pm);

    // handlers for individual received signals

    void store_heartbeat_interval(ProtocolMessage & pm);
    void handle_peer_initialization(ProtocolMessage & pm);
    void handle_peer_initialization_response(ProtocolMessage & pm);
    void handle_peer_update(ProtocolMessage & pm);
    void handle_peer_update_response(ProtocolMessage & pm);
    void handle_peer_termination(ProtocolMessage & pm);
    void handle_peer_termination_response(ProtocolMessage & pm);
    void handle_destination_up(ProtocolMessage & pm);
    void handle_destination_up_response(ProtocolMessage & pm);
    void handle_destination_announce(ProtocolMessage & pm);
    void handle_destination_announce_response(ProtocolMessage & pm);
    void handle_destination_update(ProtocolMessage & pm);
    void handle_destination_down(ProtocolMessage & pm);
    void handle_destination_down_response(ProtocolMessage & pm);
    void handle_link_characteristics_request(ProtocolMessage & pm);
    void handle_link_characteristics_response(ProtocolMessage & pm);
    void handle_heartbeat(ProtocolMessage & pm);

    void handle_peer_signal(uint8_t * buf, std::size_t buflen);

    void send_peer_initialization_response();

    /// Set current state.
    void set_state(PeerState);
    void set_state_terminating();

    // Destination endpoint to send to peer
    boost::asio::ip::tcp::endpoint peer_endpoint_tcp;

    // For InfoBase
    boost::shared_ptr<PeerData> peer_pdp;

    // Pointer back to this device's Dlep object
    DlepPtr dlep;

    std::string peer_type;
    std::vector<std::string> experiment_names;

    PeerState pstate;

    /// Socket for our TCP session with the peer.
    boost::asio::ip::tcp::socket * session_socket_;

    /// Responses that we are waiting for from this peer.  DLEP's
    /// transaction model says that a peer can only have one message
    /// in flight for each destination.  So, there is a queue of
    /// messages for each destination.  The message at the head of the
    /// queue has been sent and is awaiting a response.  Additional
    /// messages in the queue are still waiting to be sent.
    std::map<LLDLEP::DlepMac,
             std::queue<ResponsePendingPtr>> responses_pending;

    /// Heartbeat interval that the peer says it will use to send
    /// heartbeats to us.  This is the exact value sent by the peer;
    /// no unit conversion has been performed.  If 0, the peer will
    /// not send heartbeats.
    unsigned int peer_heartbeat_interval;

    /// peer_heartbeat_interval converted from the configured units to
    /// seconds
    unsigned int peer_heartbeat_interval_sec;

    /// Timer used to send heartbeats to the peer.
    boost::asio::deadline_timer heartbeat_timer;

    /// Heartbeat message.  We construct this once and use it for every
    /// heartbeat, since they're all the same.
    std::unique_ptr<ProtocolMessage> heartbeat_msg;

    /// Timer to handle resending of messages that haven't beed ACK'ed
    /// AND to terminate peers for which there has been no activity (i.e.,
    /// no data received from the peer) for too long.
    /// Thus the intentional misspelling: Ack + activity = acktivity
    boost::asio::deadline_timer acktivity_timer;

    /// Time we last received any data from this peer.
    std::time_t last_receive_time;

    /// Data received from the peer is buffered here until there is
    // a complete signal to process.
    uint8_t signal_recv_buffer[ProtocolMessage::MAX_SIGNAL_SIZE];

    /// Number of bytes received from the peer and stored in
    /// signal_recv_buffer.
    size_t signal_recv_len;

    /// Set of extensions that can be used with this peer.
    /// This is the intersection of the extensions that the local Dlep
    /// supports with those that the peer supports.
    std::vector<LLDLEP::ExtensionIdType> mutual_extensions;

    /// Destinations for which this peer does not want to receive any
    /// messages.  A destination is added to this set when a peer
    /// responds with a status of Not Interested to a Destination Up.
    /// A destination is removed from this set when a peer includes
    /// the destination in a Destination Announce message (router only).
    std::set<LLDLEP::DlepMac> not_interested_destinations;

    /// logger that gets shared throughout the library
    DlepLoggerPtr logger;
};

typedef boost::shared_ptr<Peer> PeerPtr;

} // namespace internal
} // namespace LLDLEP

#endif // PEER_H
