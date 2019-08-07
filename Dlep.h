/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016 Massachusetts Institute of Technology
 */

#ifndef DLEP_H
#define DLEP_H

#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string>
#include <iterator>
#include <map>
#include <vector>
#include <set>
#include <deque>

#include <time.h>
#include <math.h>

#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

namespace LLDLEP
{

/// Library-internal code that should not be visible to clients
/// goes in this namespace.
namespace internal
{

class Dlep;

// DlepPtr is intentionally not a shared_ptr.  The Dlep object
// contains pointers to many subcomponents, and many of those
// subcomponents contain pointers back to Dlep, creating the dreaded
// cycle that effectively leads to a memory leak when shared pointers
// are used.  Using weak pointers to break the cycles would have
// resulted in many more code changes.  A bare pointer works just fine
// in this case.
typedef Dlep * DlepPtr;

// DlepMessageBuffer holds a serialized message to be sent to or
// received from the network.
typedef boost::shared_ptr<std::vector<std::uint8_t>> DlepMessageBuffer;

} // namespace internal
} // namespace LLDLEP

#include "Peer.h"
#include "InfoBaseMgr.h"
#include "DlepClient.h"
#include "Thread.h"
#include "ProtocolConfig.h"

namespace LLDLEP
{
namespace internal
{

class Peer;
class InfoBaseMgr;
class DeviceInterface;
class DestAdvert;


enum class PeerState;

class Dlep : public boost::enable_shared_from_this<Dlep>
{
public:

    Dlep(bool is_modem, LLDLEP::DlepClient & dlep_client,
         DlepLoggerPtr logger);

    ~Dlep();

    void initialize();

    /// Wait for initialize(), which runs in its own thread, to complete
    /// @return true if initialization succeeded; the library thread(s) have
    ///         started normal operation (peer discovery, etc.)
    ///         false if it failed; the library thread(s) have exited.
    bool wait_for_initialization();

    void start_async_connect(const boost::asio::ip::address & dest_ip,
                             uint16_t port);

    // Remove Peer objects that have been terminated
    void cleanup_ex_peers();

    /// Get the current state of a peer.
    /// @param peer_id the ID of the peer
    /// @return the state of peer_id
    PeerState peer_state(std::string peer_id);

    /// Find the peer that advertised a given destination.
    /// @param mac_address
    ///        the destination for which to find the peer
    /// @return the requested peer, or nullptr if mac_address did not
    ///         belong to any peer
    boost::shared_ptr<Peer> find_peer(const LLDLEP::DlepMac & mac_address);

    /// Are we configured as the modem or the router?
    /// @return true if running as the modem, else false (router)
    bool is_modem()
    {
        return modem;
    }

    boost::asio::io_service io_service_;

    /// Default metrics, and destinations that we own.
    /// The modem sends these default metrics sent in a
    /// Peer Initialization Ack signal.
    /// These destinations are located on the same side
    /// as this Dlep process (modem or router); they will
    /// be communicated via Dlep to the other side.
    boost::shared_ptr<PeerData> local_pdp;

    // Peers:  router typically has >1,  modem just one
    std::map <std::string, boost::shared_ptr<Peer> > peers;

    std::string get_peer_id_from_endpoint(boost::asio::ip::tcp::endpoint
                                          from_endpoint);

    boost::shared_ptr<InfoBaseMgr>  info_base_manager;

    LLDLEP::DlepClient & dlep_client;

    std::unique_ptr<DestAdvert> dest_advert;
    bool dest_advert_enabled;

    LLDLEP::ProtocolConfig * protocfg;

    DlepLoggerPtr logger;
    boost::recursive_mutex mutex;

private:

    /// Are we the modem (true) or the router (false)?
    bool modem;

    /// indicates that Dlep::initialize has finished
    bool initialization_done;

    /// tells whether Dlep::initialize succeeded (true) or failed (false)
    bool initialization_success;

    /// Are we using IPv6 (true) or IPv4 (false)?
    bool using_ipv6;

    /// Is peer discovery enabled (true) or disabled (false)?  The
    /// value comes from the discovery-enable config parameter.
    bool discovery_enable;

    /// inter-thread synchroniztion objects for initialization_done
    boost::condition_variable initialization_condvar;
    boost::mutex initialization_mutex;

    /// The library thread calls this to let DlepInit (running in the client thread)
    /// know that initialization has completed and it is safe to proceed.
    /// @param[in] success
    ///            tells whether initialize succeeded (true) or failed (false).
    ///            This value is stored in initialization_success for DlepInit
    ///            to examine.
    void notify_initialization_done(bool success);

    boost::asio::deadline_timer cleanup_ex_peers_timer;

    // for accepting async tcp connections
    boost::asio::ip::tcp::acceptor * session_acceptor;

    bool start_dlep();

    void handle_ex_peers_timeout(const boost::system:: error_code & error);

    void handle_async_make_peer(boost::asio::ip::tcp::socket * peer_socket,
                                const boost::system::error_code & error);
    void start_async_accept();
};

} // namespace internal
} // namespace LLDLEP

#endif // DLEP_H
