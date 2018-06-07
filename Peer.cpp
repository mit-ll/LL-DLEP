/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2013, 2015, 2016, 2018 Massachusetts Institute of Technology
 */

/// @file
/// The Peer class implements the DLEP protocol starting after the peers
/// have a TCP connection established.

#include "Peer.h"
#include "NetUtils.h"
#include "DestAdvert.h"

#include <time.h>
#include <ctype.h>

using namespace std;
using namespace LLDLEP;
using namespace LLDLEP::internal;

Peer::Peer(boost::asio::ip::tcp::socket * peer_socket, DlepPtr dlep) :
    dlep(dlep),
    peer_type(string("unknown")),
    pstate(PeerState::connected),
    session_socket_(peer_socket),
    peer_heartbeat_interval(0),
    peer_heartbeat_interval_sec(0),
    heartbeat_timer(dlep->io_service_),
    heartbeat_msg(new ProtocolMessage(dlep->protocfg, dlep->logger)),
    acktivity_timer(dlep->io_service_),
    last_receive_time(std::time(nullptr)),
    signal_recv_buffer{0},
    signal_recv_len(0),
    logger(dlep->logger)
{
    ostringstream msg;

    peer_endpoint_tcp = peer_socket->remote_endpoint();
    peer_id = dlep->get_peer_id_from_endpoint(peer_endpoint_tcp);

    msg << "Peer ID is " << peer_id ;
    LOG(DLEP_LOG_DEBUG, msg);
}

Peer::~Peer()
{
    ostringstream msg;
    msg << "peer=" << peer_id;
    LOG(DLEP_LOG_DEBUG, msg);

    delete session_socket_;
}

void
Peer::handle_send(const boost::system::error_code & error)
{
    ostringstream msg ;
    boost::recursive_mutex::scoped_lock lock(dlep->mutex);

    msg << "peer=" << peer_id << " error=" << error;
    LOG(DLEP_LOG_INFO, msg);
}

void
Peer::send_session_message(const uint8_t * packet, int size)
{
    ostringstream msg;

    msg << "to peer=" << peer_id << " size=" << size;
    LOG(DLEP_LOG_INFO, msg);

    boost::asio::async_write(*session_socket_,
                             boost::asio::buffer(packet, size),
                             boost::bind(&Peer::handle_send,
                                         shared_from_this(),
                                         boost::asio::placeholders::error));
}

//-----------------------------------------------------------------------------

// Response (ACK) sending and receiving

ResponsePending::ResponsePending(const ProtocolConfig * protocfg,
                                 const ProtocolMessage & pm) :
    queued(false),
    send_time(0),
    send_tries(0)
{
    // Get the signal id of the expected response

    ProtocolConfig::SignalInfo  siginfo =
        protocfg->get_signal_info(pm.get_signal_name());
    response_id = siginfo.response_id;
    assert(response_id != 0);

    if (pm.is_signal())
    {
        response_name = protocfg->get_signal_name(response_id);
    }
    else
    {
        response_name = protocfg->get_message_name(response_id);
    }

    // Copy the serialized message to msg_buffer

    std::size_t pm_length = pm.get_length();
    msg_buffer = DlepMessageBuffer(new std::uint8_t[pm_length]);
    memcpy(msg_buffer.get(), pm.get_buffer(), pm_length);
    msg_buffer_len = pm_length;

    // If the protocol message contains a mac address, use it as the
    // destination.  Otherwise leave destination at its default value
    // to indicate that this is a session-level message.

    try
    {
        destination = pm.get_mac();
    }
    catch (ProtocolMessage::DataItemNotPresent)
    {
        /* no-op */
    }
}

std::string
ResponsePending::queue_name() const
{
    if (destination.mac_addr.empty())
    {
        return "session queue";
    }
    else
    {
        return "destination queue=" + destination.to_string();
    }

}

bool
Peer::should_send_response(const std::string & response_name) const
{
    ostringstream msg;

    unsigned int random_percent = std::rand() % 100;
    unsigned int ack_probability;
    dlep->dlep_client.get_config_parameter("ack-probability",
                                           &ack_probability);
    bool sendit = (random_percent < ack_probability);

    if (! sendit)
    {
        msg << "suppressing " << response_name << " to peer=" << peer_id;
        LOG(DLEP_LOG_INFO, msg);
    }
    return sendit;
}

void
Peer::send_simple_response(const std::string & response_name,
                           const std::string & status_name,
                           const std::string & status_message,
                           const DlepMac * mac)
{
    if (! should_send_response(response_name))
    {
        return;
    }

    ProtocolMessage pm {dlep->protocfg, dlep->logger};

    pm.add_header(response_name);
    if (status_name != "")
    {
        pm.add_status(status_name, status_message);
    }
    if (mac != nullptr)
    {
        pm.add_mac(*mac);
    }

    // A freshly built message should be parsable.

    std::string err = pm.parse_and_validate(dlep->is_modem(), __func__);
    assert(err == "");

    send_session_message(pm.get_buffer(), pm.get_length());
}

bool
Peer::is_not_interested(const DlepMac & destination) const
{
    return (not_interested_destinations.find(destination) !=
            not_interested_destinations.end() );
}

void
Peer::not_interested(const DlepMac & destination)
{

    ostringstream msg;
    msg << "peer=" << peer_id
        << " is not interested in destination=" << destination;
    LOG(DLEP_LOG_INFO, msg);

    (void)not_interested_destinations.insert(destination);
}

void
Peer::send_message_expecting_response(ResponsePendingPtr rp)
{
    ostringstream msg;

    msg << " to peer " << peer_id << " expecting " << rp->response_name
        << " " << rp->queue_name();
    LOG(DLEP_LOG_INFO, msg);

    if (is_not_interested(rp->destination))
    {
        msg << "not sending signal/message expecting " << rp->response_name
            << " because peer is not interested in destination="
            << rp->destination;
        LOG(DLEP_LOG_INFO, msg);
        return;
    }

    // If this message is not on the appropriate queue, add it.

    if (! rp->queued)
    {
        // Get the queue for this destination, creating it if it
        // wasn't there.
        auto & dest_queue = responses_pending[rp->destination];

        // Add the pending response to the queue for this destination.
        dest_queue.push(rp);
        rp->queued = true;

        msg << rp->queue_name() << " size=" << dest_queue.size()
            << " queues now active=" << responses_pending.size();
        LOG(DLEP_LOG_DEBUG, msg);
    }

    // if rp is at the head of its queue, send it

    if (responses_pending[rp->destination].front() == rp)
    {
        int plen = rp->msg_buffer_len;
        uint8_t * pptr = rp->msg_buffer.get();

        send_session_message(pptr, plen);
        rp->send_time = std::time(nullptr);

        rp->send_tries++;

        msg << "expecting " << rp->response_name
            << " from peer=" << peer_id
            << " tries=" << rp->send_tries;
        LOG(DLEP_LOG_DEBUG, msg);
    }
}

bool
Peer::handle_response(const ProtocolMessage & pm)
{
    ostringstream msg;
    const std::string received_response_name = pm.get_signal_name();
    bool response_ok = false;

    msg << "from peer=" << peer_id << " " << received_response_name;

    // Extract destination from the response if there is one.

    DlepMac destination;
    try
    {
        destination = pm.get_mac();
        msg << " destination=" << destination.to_string();
    }
    catch (ProtocolMessage::DataItemNotPresent)
    {
        /* no-op */
    }

    LOG(DLEP_LOG_INFO, msg);

    try
    {
        std::queue<ResponsePendingPtr> & dest_queue =
            responses_pending.at(destination);
        ResponsePendingPtr expected_response = dest_queue.front();

        std::string queue_name = expected_response->queue_name();

        if (expected_response->response_id == pm.get_signal_id())
        {
            dest_queue.pop();
            response_ok = true;
            msg << "got expected " << queue_name << " response "
                << received_response_name;
            LOG(DLEP_LOG_INFO, msg);

            msg << queue_name << " size=" << dest_queue.size();

            // If this queue is empty, remove it from the map.
            // Otherwise, send the next message on the queue.

            if (dest_queue.empty())
            {
                responses_pending.erase(destination);
            }
            else
            {
                send_message_expecting_response(dest_queue.front());
            }

            msg << " queues now active=" << responses_pending.size();
            LOG(DLEP_LOG_DEBUG, msg);
        }
        else
        {
            msg << queue_name << " response mismatch: expected "
                << expected_response->response_name
                << " got " << received_response_name;
            LOG(DLEP_LOG_ERROR, msg);
        }
    }
    catch (std::exception)
    {
        // We come here if there was no queue in the map for the
        // destination, or if the queue was empty.  We don't
        // need to do anything because response_ok will still
        // be false.
    }

    if (! response_ok)
    {
        msg << "unexpected " << received_response_name;
        LOG(DLEP_LOG_ERROR, msg);
        terminate(ProtocolStrings::Unexpected_Message,
                  received_response_name);
    }
    
    return response_ok;
}

void
Peer::schedule_acktivity_timer()
{
    // This timer always fires once a second

    acktivity_timer.expires_from_now(boost::posix_time::seconds(1));
    acktivity_timer.async_wait(
        boost::bind(&Peer::handle_acktivity_timeout, shared_from_this(),
                    boost::asio::placeholders::error));
}

bool
Peer::check_for_activity(std::time_t current_time)
{
    ostringstream msg;

    // If the peer isn't using heartbeats, there's no good way to tell
    // if it's still alive.  It could legitimately be quiet (not send
    // any DLEP messages) for a very long time.  So in this case we
    // just always say the peer is active.

    if (peer_heartbeat_interval_sec == 0)
    {
        return true;
    }

    unsigned int heartbeat_threshold;
    dlep->dlep_client.get_config_parameter("heartbeat-threshold",
                                           &heartbeat_threshold);

    // The peer is considered active for up to (heartbeat_threshold)
    // of its heartbeat intervals after the last time it sent us a
    // signal.

    unsigned int active_time =
        last_receive_time + peer_heartbeat_interval_sec * heartbeat_threshold;

    if (active_time <= current_time)
    {
        msg << "peer=" << peer_id
            << " has been inactive for " << current_time - active_time
            << " seconds; terminating peer";
        LOG(DLEP_LOG_ERROR, msg);
        terminate(ProtocolStrings::Timed_Out, msg.str());
        return false;
    }

    return true;
}

void
Peer::check_for_retransmits(std::time_t current_time)
{
    ostringstream msg;

    // Quick exit if we aren't waiting for any responses.
    // This lets us avoid constantly asking the client for
    // config parameters (below) unless they're needed.

    if (responses_pending.empty())
    {
        return;
    }

    unsigned int response_timeout;
    dlep->dlep_client.get_config_parameter("ack-timeout",
                                           &response_timeout);
    unsigned int send_tries;
    dlep->dlep_client.get_config_parameter("send-tries",
                                           &send_tries);

    // Look at all of the active queues to see if any messages
    // need to be retransmitted.

    msg << "queues now active=" << responses_pending.size();
    LOG(DLEP_LOG_DEBUG, msg);

    for (auto & kvpair : responses_pending)
    {
        auto & dest_queue = kvpair.second;

        if (! dest_queue.empty())
        {
            ResponsePendingPtr expected_response = dest_queue.front();

            if (expected_response->send_time + response_timeout < current_time)
            {
                // We should have seen the response by now.
                // If there are more tries left, send the message again.

                if (expected_response->send_tries < send_tries)
                {
                    send_message_expecting_response(expected_response);
                }
                else
                {
                    msg << "Max send tries " << send_tries
                        << " to peer=" << peer_id
                        << " reached for signal/message that expects "
                        << expected_response->response_name
                        << ", terminating peer";
                    LOG(DLEP_LOG_ERROR, msg);
                    terminate(ProtocolStrings::Timed_Out, msg.str());
                    break;
                }
            } // if response is late
        } // if destination queue is not empty
    } // for each destination queue
}

void
Peer::handle_acktivity_timeout(const boost::system::error_code & error)
{
    ostringstream msg;
    boost::recursive_mutex::scoped_lock lock(dlep->mutex);

    if (error)
    {
        msg << "boost timer error " << error;
        LOG(DLEP_LOG_ERROR, msg);
        if (error == boost::asio::error::operation_aborted)
        {
            // The timer was cancelled, which means this peer
            // is shutting down.  We don't want to restart the
            // timer at the end of this function, so return here.
            return;
        }
    }
    else
    {
        // First see if this peer has been inactive for too long.  If
        // so, terminate the peer and don't bother doing Response
        // processing.

        std::time_t current_time = std::time(nullptr);

        if (check_for_activity(current_time))
        {
            // The peer is still active.  Now see if any messages need
            // to be retransmitted because of the absence of a
            // response from the peer.

            check_for_retransmits(current_time);

        } // peer is still active
    } // timeout was not an error

    // Set up for next time unless we are terminating.

    if (pstate != PeerState::terminating)
    {
        schedule_acktivity_timer();
    }
}

//-----------------------------------------------------------------------------

// Message sending and handling methods

/// This method can send one of four different messages depending on
/// the circumstances: Destination Up, Destination Up Response,
/// Destination Announce, or Destination Announce Response.
///
/// When called by the router:
/// If the protocol is configured to have the Destination Announce message,
/// send that, else send Destination Up.
///
/// When called by the modem:
/// If the router has previously sent a Destination Announce or Destination Up
/// message for this destination, then send the corresponding response message.
/// Otherwise, send Destination Up.
void
Peer::destination_up(const DlepMac & destination_mac,
                     const DataItems & initial_data_items)
{
    ostringstream msg;

    msg << "to peer=" << peer_id << " destination mac=" << destination_mac;
    LOG(DLEP_LOG_INFO, msg);

    ProtocolMessage pm {dlep->protocfg, dlep->logger};

    std::string msg_name = peer_pdp->needs_response(destination_mac);
    bool is_response = false;

    if (msg_name != "")
    {
        is_response = true;

        // After we finish sending this message, remember that this
        // peer/destination no longer needs a response.

        peer_pdp->needs_response(destination_mac, "");
    }
    else
    {
        msg_name = ProtocolStrings::Destination_Up;

        // If we're the router, and the protocol is configured with the
        // Destination Announce message, then use that instead of
        // Destination Up.

        if (! dlep->is_modem())
        {
            try
            {
                std::string m = ProtocolStrings::Destination_Announce;
                (void)dlep->protocfg->get_signal_id(m);
                msg_name = m;
            }
            catch (ProtocolConfig::BadSignalName)
            {
                // Destination_Announce is not in the protocol
                // configuration, so leave msg_name as Destination_Up
            }
        }
    }

    pm.add_header(msg_name);
    pm.add_mac(destination_mac);
    pm.add_data_items(initial_data_items);
    pm.add_common_data_items(dlep->dlep_client);

    // A freshly built message should be parsable.  However, this
    // message contains data items that originated from the client, and
    // they could be invalid.  So we parse and validate the message before
    // sending so that problems get logged, but we don't do anything
    // drastic (assert/throw/exit) if it fails.

    pm.parse_and_validate(dlep->is_modem(), __func__);

    if (is_response)
    {
        // Response messages don't need responses.
        send_session_message(pm.get_buffer(), pm.get_length());
    }
    else
    {
        ResponsePendingPtr rp(new ResponsePending(dlep->protocfg, pm));
        send_message_expecting_response(rp);
    }
}

void
Peer::destination_down(const DlepMac & destination_mac)
{
    ostringstream msg;

    msg << "to peer=" << peer_id << " destination mac=" << destination_mac;
    LOG(DLEP_LOG_INFO, msg);

    ProtocolMessage pm {dlep->protocfg, dlep->logger};

    pm.add_header(ProtocolStrings::Destination_Down);
    pm.add_mac(destination_mac);

    // A freshly built message should be parsable.

    std::string err = pm.parse_and_validate(dlep->is_modem(), __func__);
    assert(err == "");

    ResponsePendingPtr rp(new ResponsePending(dlep->protocfg, pm));
    send_message_expecting_response(rp);
}

void
Peer::destination_update(const DlepMac & mac, const DataItems & updates)
{
    ostringstream msg;

    msg << "to peer=" << peer_id << " destination mac=" << mac;
    LOG(DLEP_LOG_INFO, msg);

    if (is_not_interested(mac))
    {
        msg << "not sending " << ProtocolStrings::Destination_Update
            << " because peer is not interested in destination=" << mac;
        LOG(DLEP_LOG_INFO, msg);
        return;
    }

    ProtocolMessage pm {dlep->protocfg, dlep->logger};

    pm.add_header(ProtocolStrings::Destination_Update);
    pm.add_mac(mac);
    pm.add_data_items(updates);

    // A freshly built message should be parsable.  However, this
    // message contains data items that originated from the client, and
    // they could be invalid.  So we parse and validate the message before
    // sending so that problems get logged, but we don't do anything
    // drastic (assert/throw/exit) if it fails.

    pm.parse_and_validate(dlep->is_modem(), __func__);

    send_session_message(pm.get_buffer(), pm.get_length());
}

bool
Peer::peer_update(const DataItems & updates)
{
    ostringstream msg;

    // XXX_ser why do we check the state here, but not in other places?

    if (pstate == PeerState::in_session)
    {
        ProtocolMessage pm {dlep->protocfg, dlep->logger};

        pm.add_header(ProtocolStrings::Session_Update);
        pm.add_data_items(updates);

        // A freshly built message should be parsable.  However, this
        // message contains data items that originated from the client, and
        // they could be invalid.  So we parse and validate the message before
        // sending so that problems get logged, but we don't do anything
        // drastic (assert/throw/exit) if it fails.

        pm.parse_and_validate(dlep->is_modem(), __func__);

        ResponsePendingPtr rp(new ResponsePending(dlep->protocfg, pm));
        send_message_expecting_response(rp);
    }
    else
    {
        msg << "Peer update not issued because peer not in session";
        LOG(DLEP_LOG_ERROR, msg);
        return false;
    }
    return true;
}

void
Peer::link_characteristics_request(const DlepMac & mac,
                                   const DataItems & requests)
{
    ostringstream msg;

    msg << "to peer=" << peer_id << " mac=" << mac;
    LOG(DLEP_LOG_INFO, msg);

    ProtocolMessage pm {dlep->protocfg, dlep->logger};

    pm.add_header(ProtocolStrings::Link_Characteristics_Request);
    pm.add_mac(mac);
    pm.add_data_items(requests);

    // A freshly built message should be parsable.  However, this
    // message contains data items that originated from the client, and
    // they could be invalid.  So we parse and validate the message before
    // sending so that problems get logged, but we don't do anything
    // drastic (assert/throw/exit) if it fails.

    pm.parse_and_validate(dlep->is_modem(), __func__);

    ResponsePendingPtr rp(new ResponsePending(dlep->protocfg, pm));
    send_message_expecting_response(rp);
}

void
Peer::link_characteristics_response(const DlepMac & mac, const DataItems & updates)
{
    ostringstream msg;

    msg << "to peer=" << peer_id << " mac=" << mac;
    LOG(DLEP_LOG_INFO, msg);

    if (! should_send_response(ProtocolStrings::Link_Characteristics_Response))
    {
        return;
    }

    ProtocolMessage pm {dlep->protocfg, dlep->logger};

    pm.add_header(ProtocolStrings::Link_Characteristics_Response);
    pm.add_mac(mac);
    pm.add_data_items(updates);

    // A freshly built message should be parsable.  However, this
    // message contains data items that originated from the client, and
    // they could be invalid.  So we parse and validate the message before
    // sending so that problems get logged, but we don't do anything
    // drastic (assert/throw/exit) if it fails.

    pm.parse_and_validate(dlep->is_modem(), __func__);

    send_session_message(pm.get_buffer(), pm.get_length());
}


void
Peer::start_peer()
{
    ostringstream msg;

    // Create the heartbeat message one time here so that it can be
    // reused each time we send one.

    heartbeat_msg->add_header(ProtocolStrings::Heartbeat);
    heartbeat_msg->add_common_data_items(dlep->dlep_client);

    std::string err = heartbeat_msg->parse_and_validate(dlep->is_modem(),
                                                        __func__);
    assert(err == "");

    msg << "Heartbeat length is " << heartbeat_msg->get_length();
    LOG(DLEP_LOG_DEBUG, msg);

    // Start up the periodic acktivity timer.  We need this running now
    // because the next thing to happen in the protocol is the Peer Init/
    // Peer Init Ack handshake, and we need to be able to service the ACKs
    // (or lack thereof).

    schedule_acktivity_timer();

    // Disable Nagle's algorithm to prevent delay
    boost::asio::ip::tcp::no_delay no_delay_option(true);
    session_socket_->set_option(no_delay_option);

    session_socket_->async_read_some(boost::asio::buffer(signal_recv_buffer,
                                                         sizeof(signal_recv_buffer)),
                                     boost::bind(&Peer::handle_session_receive,
                                                 shared_from_this(),
                                                 boost::asio::placeholders::error,
                                                 boost::asio::placeholders::bytes_transferred));

    // If we are the router, send the PEER_INITIALIZATION signal
    if (! dlep->is_modem())
    {
        ProtocolMessage pm {dlep->protocfg, dlep->logger};

        pm.add_header(ProtocolStrings::Session_Initialization);
        pm.add_common_data_items(dlep->dlep_client);

        // fill in extensions if we have any

        std::vector<ExtensionIdType> v_extid =
            dlep->protocfg->get_extension_ids();
        if (!v_extid.empty())
        {
            pm.add_extensions(v_extid);
        }

        // A freshly built message should be parsable.

        std::string err = pm.parse_and_validate(dlep->is_modem(), __func__);
        assert(err == "");

        ResponsePendingPtr rp(new ResponsePending(dlep->protocfg, pm));
        send_message_expecting_response(rp);
    }
}

void
Peer::stop_peer()
{
    ostringstream msg;

    msg << "peer=" << peer_id;
    LOG(DLEP_LOG_INFO, msg);

    stop_timers();
    set_state_terminating();
}


void
Peer::terminate(const std::string & status_name, const std::string & reason)
{
    ostringstream msg;

    msg << "peer=" << peer_id << " status=" << status_name
        << " reason=" << reason;
    LOG(DLEP_LOG_INFO, msg);

    if (pstate == PeerState::terminating)
    {
        msg << "Already in state " << pstate;
        return;
    }

    stop_peer();

    // Create and serialize a Termination message

    ProtocolMessage pm {dlep->protocfg, dlep->logger};

    pm.add_header(ProtocolStrings::Session_Termination);
    pm.add_status(status_name, reason);

    // A freshly built message should be parsable.

    std::string err = pm.parse_and_validate(dlep->is_modem(), __func__);
    assert(err == "");

    ResponsePendingPtr rp(new ResponsePending(dlep->protocfg, pm));
    send_message_expecting_response(rp);

    dlep->info_base_manager->removePeer(peer_id);

    // This peer will now be in state terminating, and the cleanup timer in
    // Dlep.cpp will eventually remove it.
}

void
Peer::cancel_session()
{
    ostringstream msg;
    msg << "peer=" << peer_id;
    LOG(DLEP_LOG_DEBUG, msg)
    session_socket_->cancel();
}

void
Peer::schedule_heartbeat()
{
    // If heartbeat-interval is 0, there will be no more heartbeats
    // sent to this peer, ever.  In other words, once disabled, they
    // cannot be re-enabled.  Changing the interval from non-zero to
    // zero is probably only useful for testing.

    unsigned int heartbeat_interval;
    dlep->dlep_client.get_config_parameter("heartbeat-interval",
                                           &heartbeat_interval);
    if (heartbeat_interval > 0)
    {
        heartbeat_timer.expires_from_now(
            boost::posix_time::seconds(heartbeat_interval));
        heartbeat_timer.async_wait(
            boost::bind(&Peer::handle_heartbeat_timeout,
                        shared_from_this(),
                        boost::asio::placeholders::error));
    }
}

void
Peer::handle_heartbeat_timeout(const boost::system::error_code & error)
{
    ostringstream msg;
    boost::recursive_mutex::scoped_lock lock(dlep->mutex);

    if (error)
    {
        msg << "boost timer error " << error;
        LOG(DLEP_LOG_ERROR, msg);
        if (error == boost::asio::error::operation_aborted)
        {
            // The timer was cancelled, which means this peer
            // is shutting down.  We don't want to restart the
            // timer at the end of this function, so return here.
            return;
        }
    }
    else if (pstate == PeerState::in_session)
    {
        msg << "Send Heartbeat to peer ID=" << peer_id;
        LOG(DLEP_LOG_INFO, msg);

        send_session_message(heartbeat_msg->get_buffer(),
                             heartbeat_msg->get_length());
    }

    schedule_heartbeat();
}

void
Peer::stop_timers()
{
    ostringstream msg;
    msg << "stopping heartbeats to peer=" << peer_id;
    LOG(DLEP_LOG_DEBUG, msg);
    heartbeat_timer.cancel();

    msg << "stopping acktivity timer for peer=" << peer_id;
    LOG(DLEP_LOG_DEBUG, msg);
    acktivity_timer.cancel();
}


void
Peer::handle_session_receive(const boost::system::error_code & error,
                             size_t bytes_recvd)
{
    ostringstream msg;
    boost::recursive_mutex::scoped_lock lock(dlep->mutex);

    msg << "from peer=" << peer_id << " error=" << error
        << " bytes_recvd=" << bytes_recvd;
    LOG(DLEP_LOG_INFO, msg);

    if (error && error != boost::asio::error::message_size)
    {
        msg << "bailing out because error=" << error;
        LOG(DLEP_LOG_ERROR, msg);

        if (error == boost::asio::error::operation_aborted)
        {
            // The timer was cancelled, which means this peer is
            // shutting down.  We don't want to restart the async read
            // at the end of this function, so return here.
            return;
        }

        if (error == boost::asio::error::misc_errors::eof)
        {
            stop_peer();
            // By exiting here, we skip setting up for the next read from
            // this peer, which is done near the end of this method.
            // We've already gotten EOF on this peer, so attempting to read
            // more from it would be fruitless.
            return;
        }
    }
    else
    {
        // We received data from the peer, so remember the current
        // time for the acktivity timer.

        last_receive_time = std::time(nullptr);

        // Keep track of how many bytes are now in the receive buffer.
        signal_recv_len += bytes_recvd;

        msg << "signal buffer now holds " << signal_recv_len
            << " bytes from the peer";
        LOG(DLEP_LOG_DEBUG, msg);

        // Handle all complete signals now in signal_recv_buffer.

        for (;;)
        {
            // Overall length of the signal including the header.
            std::size_t expected_msg_len;

            // If we don't have enough data for a complete DLEP
            // message, exit the loop.  This method will
            // be re-entered when we receive more data from the peer.

            if (! ProtocolMessage::is_complete_message(dlep->protocfg,
                                                       signal_recv_buffer, signal_recv_len, expected_msg_len))
            {
                break;
            }

            // We have at least one complete signal in the buffer.

            handle_peer_signal(signal_recv_buffer, expected_msg_len);

            // How many extra bytes were there in the buffer after the
            // signal we just handled?
            size_t remaining_bytes = signal_recv_len - expected_msg_len;

            msg << "signal buffer has " << remaining_bytes
                << " extra bytes after the signal just handled";
            LOG(DLEP_LOG_DEBUG, msg);

            // If there was more data in the signal buffer after the
            // complete signal we just handled, move it to the front
            // of the buffer.
            if (remaining_bytes > 0)
            {
                memmove(signal_recv_buffer,
                       signal_recv_buffer + expected_msg_len,
                       remaining_bytes);
            }

            // Now we're just left with those extra bytes, if any.
            signal_recv_len = remaining_bytes;

            // If signal_recv_len is now zero, we could break out of
            // the loop here, but the check at the top of the loop
            // will take care of it.

        } // end handle all complete signals
    } // end read was not an error

    // Set up to receive more data from the peer

    session_socket_->async_read_some(
        boost::asio::buffer(signal_recv_buffer + signal_recv_len,
                            sizeof(signal_recv_buffer) - signal_recv_len),
        boost::bind(&Peer::handle_session_receive,
                    shared_from_this(),
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
}


void
Peer::get_info(PeerInfo & peer_info)
{
    peer_info.peer_id = peer_id;
    peer_info.peer_type = peer_type;
    peer_info.extensions = mutual_extensions;
    peer_info.experiment_names = experiment_names;
    peer_info.data_items = peer_pdp->get_data_items();
    peer_pdp->getDestinations(peer_info.destinations);
    peer_info.heartbeat_interval = peer_heartbeat_interval;
}

void
Peer::send_peer_initialization_response()
{
    ProtocolMessage pm {dlep->protocfg, dlep->logger};

    pm.add_header(ProtocolStrings::Session_Initialization_Response);
    pm.add_common_data_items(dlep->dlep_client);

    // Add extensions if any

    if (!mutual_extensions.empty())
    {
        pm.add_extensions(mutual_extensions);
    }

    // Add default metrics

    pm.add_allowed_data_items(dlep->local_pdp->get_data_items());

    // A freshly built message should be parsable.

    std::string err = pm.parse_and_validate(dlep->is_modem(), __func__);
    assert(err == "");

    if (should_send_response(ProtocolStrings::Session_Initialization_Response))
    {
        send_session_message(pm.get_buffer(), pm.get_length());
    }
}


void Peer::store_heartbeat_interval(ProtocolMessage & pm)
{
    peer_heartbeat_interval = pm.get_heartbeat_interval();

    // Convert peer_heartbeat_interval to seconds if it is in some
    // other units.  peer_heartbeat_interval will continue to hold the
    // exact value sent by the peer, and peer_heartbeat_interval_sec
    // will be the converted value.

    peer_heartbeat_interval_sec = peer_heartbeat_interval;

    // We assume that the peer is configured with the same units as we are

    DataItemInfo di_info =
        dlep->protocfg->get_data_item_info(ProtocolStrings::Heartbeat_Interval);

    unsigned int divisor = 1;
    if (di_info.units == "milliseconds")
    {
        divisor = 1000;
    }
    else if (di_info.units == "microseconds")
    {
        divisor = 1000000;
    }

    if (divisor > 1)
    {
        // round to nearest second
        peer_heartbeat_interval_sec += (divisor / 2);
        peer_heartbeat_interval_sec /= divisor;
    }
}

// Peer Initialization message sent from router to modem
void
Peer::handle_peer_initialization(ProtocolMessage & pm)
{
    ostringstream msg;
    DataItems data_items = pm.get_data_items();
    LLDLEP::DataItems empty_data_items;
    std::string status_message =
        validate_ip_data_items(data_items, empty_data_items);

    if (status_message != "")
    {
        terminate(ProtocolStrings::Inconsistent_Data, status_message);
        return;
    }

    // get optional peer type from the message

    try
    {
        peer_type = pm.get_peer_type();
    }
    catch (ProtocolMessage::DataItemNotPresent)
    {
        /* no-op */
    }

    // get optional experiment name from the message

    try
    {
        experiment_names = pm.get_experiment_names();
    }
    catch (ProtocolMessage::DataItemNotPresent)
    {
        /* no-op */
    }
    catch (ProtocolConfig::BadDataItemName)
    {
        /* no-op */
    }

    store_heartbeat_interval(pm);

    // Get optional extensions from the message and compute the
    // intersection with our extensions.

    try
    {
        std::vector<ExtensionIdType> peer_extensions = pm.get_extensions();

        // Figure out which extension IDs we have in common with this peer.

        std::vector<ExtensionIdType> my_extensions =
            dlep->protocfg->get_extension_ids();

        for (auto extid : peer_extensions)
        {
            // Is extid (from the peer) also in my_extensions?
            auto it = std::find(std::begin(my_extensions),
                                std::end(my_extensions),
                                extid);
            if (it != my_extensions.end())
            {
                // Record this as an extension we have in common.
                mutual_extensions.push_back(extid);
            }
        }
    }
    catch (ProtocolMessage::DataItemNotPresent)
    {
        /* no-op */
    }

    peer_pdp = dlep->info_base_manager->addPeer(peer_id, empty_data_items);

    send_peer_initialization_response();

    // Now that we've sent a PEER_INITIALIZATION_Response, we consider
    // this session to be up.
    set_state(PeerState::in_session);

    // Gather up all the information about the peer

    LLDLEP::PeerInfo peer_info;
    get_info(peer_info);
    dlep->dlep_client.peer_up(peer_info);

    // Send the peer all of our destinations
    dlep->local_pdp->sendAllDestinations(shared_from_this());

    // Start heartbeats if required
    schedule_heartbeat();

    // If Destination Advertisement is enabled, get our router peer's
    // MAC address and add it to the list of destinations to advertise.

    if (dlep->dest_advert_enabled)
    {
        std::string discovery_iface;

        dlep->dlep_client.get_config_parameter("discovery-iface",
                                               &discovery_iface);

        DlepMac  peerMac;

        if (NetUtils::ipv4ToEtherMacAddr(peer_endpoint_tcp.address(),
                                         peerMac,
                                         discovery_iface,
                                         msg))
        {
            msg << "got peer mac address " << peerMac.to_string();
            LOG(DLEP_LOG_INFO, msg);

            // add peer mac address as a destination
            dlep->dest_advert->add_destination(peerMac);
        }
        else
        {
            LOG(DLEP_LOG_ERROR, msg);
        }
    }
}

void
Peer::handle_peer_initialization_response(ProtocolMessage & pm)
{
    // If we weren't expecting this response, bail out now.
    // The peer will be terminated in handle_response().
    if (! handle_response(pm))
    {
        return;
    }

    // get optional peer type from the message

    try
    {
        peer_type = pm.get_peer_type();
    }
    catch (ProtocolMessage::DataItemNotPresent)
    {
        /* no-op */
    }

    // get optional experiment name from the message

    try
    {
        experiment_names = pm.get_experiment_names();
    }
    catch (ProtocolMessage::DataItemNotPresent)
    {
        /* no-op */
    }
    catch (ProtocolConfig::BadDataItemName)
    {
        /* no-op */
    }

    store_heartbeat_interval(pm);

    // get optional extensions from the message

    try
    {
        mutual_extensions = pm.get_extensions();
    }
    catch (ProtocolMessage::DataItemNotPresent)
    {
        /* no-op */
    }

    DataItems metrics_and_ip_items = pm.get_metrics_and_ipaddrs();

    peer_pdp = dlep->info_base_manager->addPeer(peer_id,
                                                metrics_and_ip_items);

    // Now that we've received a PEER_INITIALIZATION_Response,
    // we consider this session to be up.
    set_state(PeerState::in_session);

    // Gather up all the information about the peer to tell the client

    LLDLEP::PeerInfo peer_info;
    get_info(peer_info);
    peer_info.data_items = metrics_and_ip_items;
    dlep->dlep_client.peer_up(peer_info);

    // Send the peer all of our destinations
    dlep->local_pdp->sendAllDestinations(shared_from_this());

    // Start heartbeats if required
    schedule_heartbeat();
}

void
Peer::handle_peer_update(ProtocolMessage & pm)
{
    DataItems data_items = pm.get_data_items();
    std::string status_message =
        validate_ip_data_items(data_items, peer_pdp->get_ip_data_items());

    if (status_message != "")
    {
        terminate(ProtocolStrings::Inconsistent_Data, status_message);
        return;
    }

    std::string update_status = peer_pdp->update_data_items(data_items, false);

    // We always send the update to the client, even if there was an error.
    // Is that OK?
    dlep->dlep_client.peer_update(peer_id, data_items);

    send_simple_response(ProtocolStrings::Session_Update_Response,
                         update_status, "");
}

void
Peer::handle_peer_update_response(ProtocolMessage & pm)
{
    (void)handle_response(pm);
}

void
Peer::handle_peer_termination(ProtocolMessage &  /*pm*/)
{
    send_simple_response(ProtocolStrings::Session_Termination_Response, "");

    // Stop this peer session - we are terminating
    stop_peer();
}

void
Peer::handle_peer_termination_response(ProtocolMessage & pm)
{
    if (pstate == PeerState::terminating)
    {
        handle_response(pm);
        stop_peer();
    } // if (pstate is PeerState::terminating)
}

/// This method handles Destination Up, Destination Announce Response,
/// and some cases of Destination Up Response:
/// - Destination Announce Response is equivalent to Destination Up, thus
///   handled here.
/// - In older DLEP drafts that didn't have Destination Announce Response,
///   Destination Up Response is used instead from modem to router, so
///   that case is also handled here.
void
Peer::handle_destination_up(ProtocolMessage & pm)
{
    ostringstream msg;

    DlepMac destination_mac = pm.get_mac();

    msg << pm.get_signal_name() << " from peer=" << peer_id
        << " destination=" << destination_mac;
    LOG(DLEP_LOG_INFO, msg);

    // already_have_this_dest will be true if we've already gotten a
    // Destination Up for destination_mac from this peer.  In that
    // case, we shouldn't be getting another one; that's a protocol
    // error.

    bool already_have_this_dest = peer_pdp->validDestination(destination_mac);

    std::string statusname;
    if (! already_have_this_dest)
    {
        DataItems data_items = pm.get_data_items();
        DataItems empty_data_items;
        std::string status_message =
            validate_ip_data_items(data_items, empty_data_items);

        if (status_message != "")
        {
            terminate(ProtocolStrings::Inconsistent_Data, status_message);
            return;
        }

        // If the peer had previously expressed Not Interested in
        // this destination, reinstate interest by removing it from
        // the not_interested set.

        if (not_interested_destinations.erase(destination_mac) > 0)
        {
            msg << "peer=" << peer_id
                << " regains interest in destination=" << destination_mac;
            LOG(DLEP_LOG_INFO, msg);
        }

        statusname = dlep->dlep_client.destination_up(peer_id, destination_mac,
                                                      data_items);

        // convert empty status name to Success
        if (statusname == "")
        {
            statusname = ProtocolStrings::Success;
        }

        // If our client returns a successful status for this destination,
        // we'll add it to the destinations from this peer.

        if (statusname == ProtocolStrings::Success)
        {
            bool ok = peer_pdp->addDestination(destination_mac, data_items,
                                               false);

            // The only possible error should be a duplicate
            // destination, and we've already checked for that, so
            // this should always succeed.

            assert(ok);
        }
    } // destination did not already exist
    else
    {
        // What's the right status for a redundant Destination Up?
        statusname = ProtocolStrings::Invalid_Message;
    }

    // Send response message if needed

    string response_name =
        dlep->protocfg->get_message_response_name(pm.get_signal_name());

    if (response_name != "")
    {
        send_simple_response(response_name, statusname, "", &destination_mac);
    }
}

/// This method handles both Destination Up Response and Destination
/// Announce Response.
void
Peer::handle_destination_up_response(ProtocolMessage & pm)
{
    ostringstream msg;

    DlepMac destination_mac = pm.get_mac();

    msg << pm.get_signal_name() << " from peer=" << peer_id
        << " destination=" << destination_mac;
    LOG(DLEP_LOG_INFO, msg);

    if (handle_response(pm))
    {
        try
        {
            std::string status_name = pm.get_status();
            msg << "status=" << status_name;
            LOG(DLEP_LOG_INFO, msg);

            // If there is a status data item of Not Interested,
            // add this destination to the not_interested set.

            if (status_name == ProtocolStrings::Not_Interested)
            {
                not_interested(destination_mac);
            }

            // If the status was anything besides Success, then we're
            // done.

            if (status_name != ProtocolStrings::Success)
            {
                return;
            }
        }
        catch (ProtocolMessage::DataItemNotPresent)
        {
            /* If there is no status data item, it's a no-op */
        }

        // If we're the router, then the modem sent this Destination
        // Up Response (or Destination Announce Response).  The router
        // treats that as a Destination Up.

        if (! dlep->is_modem())
        {
            handle_destination_up(pm);
        }

    } // response was expected
}

/// This method should only be called on the modem side.  It is used
/// when the router sends a Destination Announce or Destination Up
/// message.  The router only sends Destination Up when configured
/// with older DLEP drafts that did not yet have Destination Announce.
void
Peer::handle_destination_announce(ProtocolMessage & pm)
{
    ostringstream msg;

    DlepMac destination_mac = pm.get_mac();

    msg << pm.get_signal_name() << " from peer=" << peer_id
        << " destination=" << destination_mac;
    LOG(DLEP_LOG_INFO, msg);

    // response_name will be Destination Up Response or Destination
    // Announce Response, depending on what the received message was.

    std::string response_name =
        dlep->protocfg->get_message_response_name(pm.get_signal_name());
    assert(response_name != "");

    // already_have_this_dest will be true if we've already gotten a
    // Destination Up/Announce for destination_mac from this peer.  In
    // that case, we shouldn't be getting another one; that's a
    // protocol error.

    bool already_have_this_dest = peer_pdp->validDestination(destination_mac);

    std::string statusname;
    if (! already_have_this_dest)
    {
        // If the peer had previously expressed Not Interested in
        // this destination, reinstate interest by removing it from
        // the not_interested set.

        if (not_interested_destinations.erase(destination_mac) > 0)
        {
            msg << "peer=" << peer_id
                << " regains interest in destination=" << destination_mac;
            LOG(DLEP_LOG_INFO, msg);
        }

        // Remember that this destination was announced by the peer.
        
        DataItems data_items = pm.get_data_items();
        bool ok = peer_pdp->addDestination(destination_mac, data_items,
                                           false);

        // The only possible error should be a duplicate destination,
        // and we've already checked for that, so this should always
        // succeed.

        assert(ok);

        // If we can satisfy the announce immediately because we
        // already have this destination, send the response without
        // interacting with the client, and we're done.

        DestinationDataPtr ddp;
        if (dlep->local_pdp->getDestinationData(destination_mac, &ddp))
        {
            DataItems response_data_items;
            ddp->get_all_data_items(response_data_items);

            // Make sure destination_up sends the intended response
            // message.

            peer_pdp->needs_response(destination_mac, response_name);
            destination_up(destination_mac, response_data_items);
            return;
        }

        // If we get here, we did not already have the announced
        // destination, so ask the client what to do with the
        // destination.
        
        statusname = dlep->dlep_client.destination_up(peer_id, destination_mac,
                                                      data_items);

        // convert empty status name to Success
        if (statusname == "")
        {
           statusname = ProtocolStrings::Success;
        }
    } // destination did not already exist from this peer
    else
    {
        // What's the right status for a redundant Destination Announce?
        statusname = ProtocolStrings::Invalid_Message;
    } // destination already exists from this peer

    if (statusname == ProtocolStrings::Success)
    {
        // Leave without sending a response yet.  It is the client's
        // responsbility to call DlepService->destination_up() later
        // when the destination becomes available.  destination_up()
        // will know that it needs to send a Destinatin Up or Announce
        // Response because of this call to needs_response().

        peer_pdp->needs_response(destination_mac, response_name);
    }
    else
    {
        // Error responses can be sent right away.
        
        send_simple_response(response_name, statusname, "", &destination_mac);
    }
}

void
Peer::handle_destination_announce_response(ProtocolMessage & pm)
{
    handle_destination_up_response(pm);
}

void
Peer::handle_destination_update(ProtocolMessage & pm)
{
    ostringstream msg;

    DlepMac destination_mac = pm.get_mac();

    DestinationDataPtr ddp;
    bool good_destination = peer_pdp->getDestinationData(destination_mac, &ddp);

    if (good_destination)
    {
        DataItems update_data_items = pm.get_data_items_no_mac();
        std::string status_message =
            validate_ip_data_items(update_data_items, ddp->get_ip_data_items());
        if (status_message != "")
        {
            terminate(ProtocolStrings::Inconsistent_Data, status_message);
            return;
        }

        ddp->update(update_data_items, false);
        dlep->dlep_client.destination_update(peer_id, destination_mac,
                                             update_data_items);
    }
    else
    {
        msg << " unknown mac=" << destination_mac;
        LOG(DLEP_LOG_ERROR, msg);
        terminate(ProtocolStrings::Invalid_Message);
    }
}

void
Peer::handle_destination_down(ProtocolMessage & pm)
{
    ostringstream msg;

    DlepMac destination_mac = pm.get_mac();

    msg << "from peer=" << peer_id << " destination=" << destination_mac;
    LOG(DLEP_LOG_INFO, msg);

    bool ok = peer_pdp->removeDestination(destination_mac, false);
    if (! ok)
    {
        // destination_mac did not originate from the peer.  If it
        // originated from us (local DLEP), then we interpret the Down
        // as a declaration from the peer that it is not interested in
        // this destination.

        if (dlep->local_pdp->validDestination(destination_mac))
        {
            not_interested(destination_mac);
        }
        else
        {
            // destination_mac originated neither from the peer nor
            // from us.  Since we have no idea where it came from, we
            // consider this an error.

            msg << "destination=" << destination_mac
                << " does not exist, terminating peer=" << peer_id;
            LOG(DLEP_LOG_ERROR, msg);
            terminate(ProtocolStrings::Invalid_Destination, msg.str());
            return;
        }
    }

    dlep->dlep_client.destination_down(peer_id, destination_mac);

    send_simple_response(ProtocolStrings::Destination_Down_Response,
                         ProtocolStrings::Success, "", &destination_mac);
}

void
Peer::handle_destination_down_response(ProtocolMessage & pm)
{
    ostringstream msg;

    DlepMac destination_mac = pm.get_mac();

    msg << "from peer=" << peer_id << " destination=" << destination_mac;
    LOG(DLEP_LOG_INFO, msg);

    handle_response(pm);
}

void
Peer::handle_link_characteristics_request(ProtocolMessage & pm)
{
    ostringstream msg;

    DlepMac destination_mac = pm.get_mac();
    msg << "from peer=" << peer_id << " destination=" << destination_mac;
    LOG(DLEP_LOG_INFO, msg);

    DataItems data_items = pm.get_data_items_no_mac();

    DestinationDataPtr ddp;
    bool good_destination =
        dlep->local_pdp->getDestinationData(destination_mac, &ddp);
    if ( ! good_destination)
    {
        msg << "destination " << destination_mac << " is invalid";
        LOG(DLEP_LOG_ERROR, msg);
        terminate(ProtocolStrings::Invalid_Destination, msg.str());
        return;
    }

    if (data_items.empty())
    {
        DataItems all_data_items;
        DataItems metric_data_items;
        ddp->get_all_data_items(all_data_items);

        // We just want the metric data items.

        for (const auto & di : all_data_items)
        {
            if (dlep->protocfg->is_metric(di.id))
            {
                metric_data_items.push_back(di);
            }
        }
        link_characteristics_response(destination_mac, metric_data_items);
    }
    else
    {
        dlep->dlep_client.linkchar_request(peer_id, destination_mac, data_items);
    }
}

void
Peer::handle_link_characteristics_response(ProtocolMessage & pm)
{
    ostringstream msg;

    DlepMac destination_mac = pm.get_mac();

    msg << "from peer=" << peer_id << " destination=" << destination_mac;
    LOG(DLEP_LOG_INFO, msg);

    // If we weren't expecting this response, bail out now.
    // The peer will be terminated in handle_response().
    if (! handle_response(pm))
    {
        return;
    }

    // Update the destination info
    DestinationDataPtr ddp;
    bool good_destination = peer_pdp->getDestinationData(destination_mac, &ddp);
    if (good_destination)
    {
        const DataItems & data_items = pm.get_data_items_no_mac();
        ddp->update(data_items, false);
        ddp->log(__func__, DLEP_LOG_INFO);
        dlep->dlep_client.linkchar_reply(peer_id, destination_mac, data_items);
    }
    else
    {
        msg << "destination " << destination_mac << " is invalid";
        LOG(DLEP_LOG_ERROR, msg);
        terminate(ProtocolStrings::Invalid_Destination, msg.str());
    }
}

void
Peer::handle_heartbeat(ProtocolMessage &  /*pm*/)
{
    ostringstream msg;
    msg << "from peer=" << peer_id;
    LOG(DLEP_LOG_DEBUG, msg);

    // There is nothing else to do!  The heartbeat contains no data
    // items and requires no response.  The practical effect of the
    // heartbeat is to set the peer's last_receive_time, and that has
    // already been done in handle_session_receive().
}

bool
Peer::check_status_code_failure(ProtocolMessage & pm)
{
    ostringstream msg;
    std::string msgname = pm.get_signal_name();

    if ( (msgname != ProtocolStrings::Session_Termination) &&
         (msgname != ProtocolStrings::Session_Termination_Response) )
    {
        // If the message contains a status code that has a failure mode
        // of terminate, terminate the peer and return true.

        try
        {
            std::string status_name = pm.get_status();
            ProtocolConfig::StatusCodeInfo sc_info =
                dlep->protocfg->get_status_code_info(status_name);
            if (sc_info.failure_mode == "terminate")
            {
                msg << pm.get_signal_name()
                    << " from peer=" << peer_id
                    << " contained termination status=" << status_name;
                LOG(DLEP_LOG_ERROR, msg);

                // the same status code gets echoed back to the peer
                terminate(status_name, msg.str());
                return true;
            }
        }
        catch (ProtocolMessage::DataItemNotPresent)
        {
            /* if no status data item, this is a no-op */
        }
    }

    return false;
}

void
Peer::handle_peer_signal(uint8_t * buf, std::size_t buflen)
{
    ostringstream msg;

    ProtocolMessage pm {dlep->protocfg, dlep->logger};

    msg << "from=" << peer_id << " size=" << buflen << " :";
    LOG(DLEP_LOG_INFO, msg);

    std::string err = pm.parse_and_validate(buf, buflen, false,
                                            ! dlep->is_modem(), __func__);
    if (err != "")
    {
        msg << "invalid message: " << err << ", terminating peer=" << peer_id;
        LOG(DLEP_LOG_ERROR, msg);
        terminate(ProtocolStrings::Invalid_Message, err);
        return;
    }

    // Handle status codes that might terminate the peer

    if (check_status_code_failure(pm))
    {
        return;
    }

    // dispatch the message to the right handler

    std::string msgname = pm.get_signal_name();

    if (msgname == ProtocolStrings::Heartbeat)
    {
        handle_heartbeat(pm);
    }
    else if (msgname == ProtocolStrings::Destination_Update)
    {
        handle_destination_update(pm);
    }
    else if (msgname == ProtocolStrings::Destination_Up)
    {
        if (dlep->is_modem())
        {
            // This message came from the router.  Handle it as
            // Destination Announce; older DLEP drafts used
            // Destination Up for this.
            handle_destination_announce(pm);
        }
        else
        {
            handle_destination_up(pm);
        }
    }
    else if (msgname == ProtocolStrings::Destination_Up_Response)
    {
        handle_destination_up_response(pm);
    }
    else if (msgname == ProtocolStrings::Destination_Announce)
    {
        handle_destination_announce(pm);
    }
    else if (msgname == ProtocolStrings::Destination_Announce_Response)
    {
        handle_destination_announce_response(pm);
    }
    else if (msgname == ProtocolStrings::Destination_Down)
    {
        handle_destination_down(pm);
    }
    else if (msgname == ProtocolStrings::Destination_Down_Response)
    {
        handle_destination_down_response(pm);
    }
    else if (msgname == ProtocolStrings::Link_Characteristics_Request)
    {
        handle_link_characteristics_request(pm);
    }
    else if (msgname == ProtocolStrings::Link_Characteristics_Response)
    {
        handle_link_characteristics_response(pm);
    }
    else if (msgname == ProtocolStrings::Session_Update)
    {
        handle_peer_update(pm);
    }
    else if (msgname == ProtocolStrings::Session_Update_Response)
    {
        handle_peer_update_response(pm);
    }
    else if (msgname == ProtocolStrings::Session_Initialization)
    {
        handle_peer_initialization(pm);
    }
    else if (msgname == ProtocolStrings::Session_Initialization_Response)
    {
        handle_peer_initialization_response(pm);
    }
    else if (msgname == ProtocolStrings::Session_Termination)
    {
        handle_peer_termination(pm);
    }
    else if (msgname == ProtocolStrings::Session_Termination_Response)
    {
        handle_peer_termination_response(pm);
    }
    else
    {
        msg << "unhandled signal " << msgname;
        LOG(DLEP_LOG_ERROR, msg);
    }
}

PeerState
Peer::get_state()
{
    return pstate;
}

void
Peer::set_state(PeerState newstate)
{
    ostringstream msg;

    if (newstate == pstate)
    {
        msg << "peer=" << peer_id << " is already in state " << pstate;
    }
    else
    {
        msg << "peer=" << peer_id << " old state=" << pstate <<
            " new state=" << newstate;
        pstate = newstate;
    }

    LOG(DLEP_LOG_INFO, msg);
}

void
Peer::set_state_terminating()
{
    if (pstate != PeerState::terminating)
    {
        set_state(PeerState::terminating);

        dlep->dlep_client.peer_down(peer_id);

        if (dlep->dest_advert_enabled)
        {
            // clear all destinations associated with this peer
            dlep->dest_advert->clear_destinations();
        }
    }
}

std::ostream & LLDLEP::internal::operator<<(std::ostream & os,
                                            PeerState peerstate)
{
    switch (peerstate)
    {
        case PeerState::nonexistent:
            os << "nonexistent";
            break;
        case PeerState::connected:
            os << "connected";
            break;
        case PeerState::in_session:
            os << "in session";
            break;
        case PeerState::terminating:
            os << "terminating";
            break;
        default:
            os << "unknown";
            break;
    }
    return os;
}


bool
Peer::get_destination(const DlepMac & mac,
                      boost::shared_ptr<DestinationData> & destination)
{
    return peer_pdp->getDestinationData(mac, &destination);
}

bool
Peer::remove_destination(const LLDLEP::DlepMac & mac)
{
    return peer_pdp->removeDestination(mac, true);
}

std::string
Peer::find_ip_data_item(const DataItem & ip_data_item) const
{
    return peer_pdp->find_ip_data_item(ip_data_item);
}


std::string
Peer::validate_ip_data_items(const DataItems & new_data_items,
                             const DataItems & existing_ip_data_items)
{
    for (const DataItem & ndi : new_data_items)
    {
        if (! dlep->protocfg->is_ipaddr(ndi.id))
        {
            continue;
        }

        if (ndi.ip_flags() & DataItem::IPFlags::add)
        {
            // See if the local node already has this IP address
            std::string ip_owner = dlep->local_pdp->find_ip_data_item(ndi);
            if (ip_owner == "")
            {
                // See if any peer node already has this IP address
                for (auto & itp : dlep->peers)
                {
                    PeerPtr peer = itp.second;

                    ip_owner = peer->find_ip_data_item(ndi);
                    if (ip_owner != "")
                    {
                        break;
                    }
                } // end for each peer
            } // end IP address not found on local node

            if (ip_owner != "")
            {
                return "cannot add "+ ndi.to_string() + ", "
                       + ip_owner + " already has it";
            }
        }
        else // the IP address/subnet in ndi is being removed
        {
            if (ndi.find_ip_data_item(existing_ip_data_items) ==
                existing_ip_data_items.end() )
            {
                return "cannot remove " + ndi.to_string() + ", it is not there";
            }
        }
    } // end for each new_data_item

    return "";
}
