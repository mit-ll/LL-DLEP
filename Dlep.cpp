/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016, 2018 Massachusetts Institute of Technology
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <boost/lexical_cast.hpp>
#include "Dlep.h"
#include "PeerDiscovery.h"
#include "DestAdvert.h"
#include "ProtocolConfigImpl.h"
#include "NetUtils.h"

using namespace std;
using namespace LLDLEP;
using namespace LLDLEP::internal;

Dlep::Dlep(bool is_modem, DlepClient & dlep_client, DlepLoggerPtr logger) :
    dlep_client(dlep_client),
    dest_advert_enabled(false),
    protocfg(nullptr),
    logger(logger),
    modem(is_modem),
    initialization_done(false),
    initialization_success(false),
    using_ipv6(false),
    cleanup_ex_peers_timer(io_service_),
    session_acceptor(nullptr)
{
    ostringstream msg;

    if (is_modem)
    {
        msg << "I am a Modem";
        LOG(DLEP_LOG_INFO, msg);
    }
    else
    {
        msg << "I am a Router";
        LOG(DLEP_LOG_INFO, msg);
    }
};

Dlep::~Dlep()
{
    if (session_acceptor != nullptr)
    {
        delete session_acceptor;
    }

    delete protocfg;
}

void
Dlep::initialize()
{
    ostringstream msg;
    msg << "entered";
    LOG(DLEP_LOG_DEBUG, msg);

    try
    {
        InfoBaseMgrPtr ib_ptr(new InfoBaseMgr(this));
        info_base_manager = ib_ptr;

        // Initialize protocol configuration

        std::string protocol_config_schema;
        dlep_client.get_config_parameter("protocol-config-schema",
                                         &protocol_config_schema);
        std::string protocol_config_file;
        dlep_client.get_config_parameter("protocol-config-file",
                                         &protocol_config_file);

        protocfg = new ProtocolConfigImpl(protocol_config_schema,
                                          protocol_config_file,
                                          logger);

        // Initialize local_pdp, which stores two things: the default
        // metrics and IP addresses we send during peer
        // initialization, and locally discovered destinations that we
        // tell our peers about.  Note that the local_pdp "peer" is
        // NOT in the InfoBase (as a peer) because it is not really a
        // peer; it is ourself.
        //
        // We add a default data item to local_pdp for each configured
        // metric, with a bogus value of 0.  These data items are sent
        // to peers during session initialization, so this effectively
        // declares all of our supported metrics to our peers.  The
        // client can modify the metric values later if it wants with
        // DlepService->peer_update().

        DataItems initial_local_data_items;

        std::vector<DataItemInfo> data_item_info =
            protocfg->get_data_item_info();

        for (auto minfo : data_item_info)
        {
            if (! protocfg->is_metric(minfo.id))
            {
                continue;
            }

            // This will give di a default value appropriate for its type
            DataItem di(minfo.name, protocfg);

            msg << "add peer default " << di.to_string();
            LOG(DLEP_LOG_DEBUG, msg);

            initial_local_data_items.push_back(di);

        }  // end for each configured data item

        // XXX here, we could give the client a chance to modify
        // initial_local_data_items, e.g. by adding/removing some data
        // items or changing their values.

        PeerDataPtr pdp(new PeerData("local_pdp",
                                     initial_local_data_items, this));
        local_pdp = pdp;

        // Initialize peer discovery

        std::string discovery_iface;
        unsigned int discovery_port = 0;
        unsigned int discovery_interval = 0;
        unsigned int discovery_ttl = 0;
        boost::asio::ip::address discovery_multicast_address;
        bool sending = false;
        bool receiving = false;

        dlep_client.get_config_parameter("discovery-enable",
                                         &discovery_enable);
        if (discovery_enable)
        {
            dlep_client.get_config_parameter("discovery-iface",
                                             &discovery_iface);
            dlep_client.get_config_parameter("discovery-port",
                                             &discovery_port);
            dlep_client.get_config_parameter("discovery-mcast-address",
                                             &discovery_multicast_address);
            using_ipv6 = discovery_multicast_address.is_v6();

            if (modem)
            {
                // modem receives multicast packets, but doesn't send
                receiving = true;
            }
            else
            {
                // router sends multicast packets, but doesn't receive
                sending = true;
                dlep_client.get_config_parameter("discovery-interval",
                                                 &discovery_interval);
            }

            // Now get optional parameters

            try
            {
                dlep_client.get_config_parameter("discovery-ttl",
                                                 &discovery_ttl);
            }
            catch (LLDLEP::DlepClient::BadParameterName)
            {
            }
        } //  discovery enabled

        PeerDiscovery pd(this, io_service_, discovery_iface,
                         discovery_port, discovery_multicast_address,
                         discovery_ttl,
                         discovery_interval, sending, receiving);

        if (! pd.start())
        {
            msg << "Problem starting peer discovery";
            LOG(DLEP_LOG_FATAL, msg);
            notify_initialization_done(false);
            return;
        }


        if (modem)
        {
            dlep_client.get_config_parameter("destination-advert-enable",
                                             &dest_advert_enabled);

            if (dest_advert_enabled)
            {
                std::string dest_advert_iface;
                dlep_client.get_config_parameter("destination-advert-iface",
                                                 &dest_advert_iface);

                unsigned int dest_advert_port;
                dlep_client.get_config_parameter("destination-advert-port",
                                                 &dest_advert_port);

                boost::asio::ip::address dest_advert_multicast_address;
                dlep_client.get_config_parameter("destination-advert-mcast-address",
                                                 &dest_advert_multicast_address);

                unsigned int dest_advert_interval;
                dlep_client.get_config_parameter("destination-advert-send-interval",
                                                 &dest_advert_interval);


                std::vector<unsigned int> dest_advert_rfid;
                dlep_client.get_config_parameter("destination-advert-rf-id",
                                                 &dest_advert_rfid);

                DlepMac mac;

                for (auto rfid_byte : dest_advert_rfid)
                {
                    mac.mac_addr.push_back(rfid_byte);
                }

                dest_advert.reset(new DestAdvert(this,
                                                 io_service_,
                                                 dest_advert_iface,
                                                 dest_advert_port,
                                                 dest_advert_multicast_address,
                                                 dest_advert_interval,
                                                 mac,
                                                 logger));

                if (! dest_advert->start())
                {
                    msg << "Problem starting destination advertisement";
                    LOG(DLEP_LOG_FATAL, msg);
                    notify_initialization_done(false);
                    return;
                }
            }
        }

        if (start_dlep())
        {
            // start_dlep succeeded

            notify_initialization_done(true);

            // turn over control to boost::asio

            io_service_.run();
        }
        else
        {
            // start_dlep failed

            notify_initialization_done(false);
        }
    }
    catch (LLDLEP::DlepClient::BadParameterName & bpn)
    {
        msg << bpn.what();
        LOG(DLEP_LOG_FATAL, msg);
        notify_initialization_done(false);
    }
}

void
Dlep::notify_initialization_done(bool success)
{
    // tell other threads that initialization is done
    boost::lock_guard<boost::mutex> lock(initialization_mutex);
    initialization_done = true;
    initialization_success = success;
    initialization_condvar.notify_one();
}

bool
Dlep::wait_for_initialization()
{
    boost::unique_lock<boost::mutex> lock(initialization_mutex);

    while (! initialization_done)
    {
        initialization_condvar.wait(lock);
    }

    return initialization_success;
}

/// Given a socket connection to a new peer, create the peer.
/// This is the boost::asio callback for connect AND accept.
///
/// @param peer_socket open socket connection to the new peer
/// @param error       boost::asio error, if any
void
Dlep::handle_async_make_peer(boost::asio::ip::tcp::socket * peer_socket,
                             const boost::system::error_code & error)
{
    ostringstream msg;
    boost::recursive_mutex::scoped_lock lock(mutex);

    if (!error)
    {
        msg << "New peer!";
        LOG(DLEP_LOG_INFO, msg);

        // Set the peer socket's TTL to the session-ttl config parameter
        // if it exists.

        try
        {
            unsigned int ttl;
            dlep_client.get_config_parameter("session-ttl", &ttl);
            peer_socket->set_option(boost::asio::ip::unicast::hops(ttl));
        }
        catch (LLDLEP::DlepClient::BadParameterName)
        {
            // Let the default TTL take effect.
        }

        // Create the new peer
        PeerPtr peer(new Peer(peer_socket, this));

        {
            // Associate the peer pointer with peer_id in the peers map
            peers[peer->peer_id] = peer;
        }

        // Fire up the peer
        peer->start_peer();
    }
    else
    {
        msg << "failed " << error;
        LOG(DLEP_LOG_ERROR, msg);
        delete peer_socket;
    }

    if (modem)
    {
        // This is the modem side, so set up to accept next connection
        start_async_accept();
    }
}


/// Start an asynchronous accept via boost::asio.
/// When the accept completes, handle_async_make_peer will be called.
void
Dlep::start_async_accept()
{
    // The modem is the TCP server, so only it should be accepting connections.
    assert(modem);

    auto peer_socket(new boost::asio::ip::tcp::socket(io_service_));

    session_acceptor->async_accept(*peer_socket,
                                   boost::bind(&Dlep::handle_async_make_peer,
                                               this,
                                               peer_socket,
                                               boost::asio::placeholders::error));
}


/// Start an asynchronous connect via boost::asio.
/// When the connect completes, handle_async_make_peer will be called.
/// This is the method that peer discovery uses to start up a new peer
/// that it finds.
///
/// @param dest_ip IP address of new peer
/// @param port     port on new peer to connect to
void
Dlep::start_async_connect(const boost::asio::ip::address & dest_ip,
                          uint16_t port)
{
    std::ostringstream msg;

    // The router is the TCP client, so only it should be initiating
    // connections.
    assert(!modem);

    msg << "session address=" << dest_ip << " port=" << port;
    LOG(DLEP_LOG_DEBUG, msg);

    auto peer_socket(new boost::asio::ip::tcp::socket(io_service_));

    // boost::asio really wants us to "resolve" the host we're connecting
    // to, even though we already have its IP address.
    boost::asio::ip::tcp::resolver resolver(io_service_);
    boost::asio::ip::tcp::resolver::query query(dest_ip.to_string() ,
                                                std::to_string(port));
    boost::asio::ip::tcp::resolver::iterator iter = resolver.resolve(query);

    boost::asio::async_connect(*peer_socket, iter,
                               boost::bind(&Dlep::handle_async_make_peer,
                                           this,
                                           peer_socket,
                                           boost::asio::placeholders::error));
}


bool
Dlep::start_dlep()
{
    std::ostringstream msg;
    unsigned int session_port = 0;

    if (this->modem)
    {
        // We're the modem, so start accepting TCP connections for peer
        // sessions.

        dlep_client.get_config_parameter("session-port", &session_port);

        // If a session address is configured, use it to accept connections.

        boost::asio::ip::address session_address;
        try
        {
            dlep_client.get_config_parameter("session-address",
                                             &session_address);

            // If the session address is IPv6, figure out which
            // interface has that address so we can set the session
            // address's IPv6 scope id.

            if (session_address.is_v6())
            {
                std::string session_iface =
                    NetUtils::get_iface_from_ip_addr(session_address, logger);
                if (session_iface == "")
                {
                    msg << "No interface found for address="
                        << session_address;
                    LOG(DLEP_LOG_FATAL, msg);
                    return false;
                }

                if (! NetUtils::set_ipv6_scope_id(session_address,
                                                  session_iface))
                {
                    msg << "Could not set IPv6 scope id for address="
                        << session_address
                        << " interface=" << session_iface;
                    LOG(DLEP_LOG_FATAL, msg);
                    return false;
                }
            }
        }
        catch (LLDLEP::DlepClient::BadParameterName)
        {
            // If a specific session address wasn't configured, we'll
            // just use the any address.

            if (using_ipv6)
            {
                session_address = boost::asio::ip::address_v6::any();
            }
            else
            {
                session_address = boost::asio::ip::address_v4::any();
            }
        }

        session_acceptor =
            new boost::asio::ip::tcp::acceptor(io_service_,
                                               boost::asio::ip::tcp::endpoint(session_address, session_port));
        start_async_accept();
    }
    else // we're the router
    {
        if (! discovery_enable)
        {
            // For the router, the session-address and session-port
            // config parameters are required when peer discovery is
            // disabled.  Without it, the router won't know where to
            // connect to the modem.

            dlep_client.get_config_parameter("session-port", &session_port);
            boost::asio::ip::address session_address;
            dlep_client.get_config_parameter("session-address",
                                             &session_address);

            // If the session address is IPv6, get the session-iface
            // config parameter so we can set the session address's
            // IPv6 scope id.  Unlike on the modem side, here we have
            // to be told which interface to use, because the session
            // address doesn't exist on any of our interfaces; it's a
            // modem-side address.

            if (session_address.is_v6())
            {
                std::string session_iface;
                dlep_client.get_config_parameter("session-iface",
                                                 &session_iface);
                if (! NetUtils::set_ipv6_scope_id(session_address,
                                                  session_iface))
                {
                    msg << "Could not set IPv6 scope id for for address="
                        << session_address
                        << " interface=" << session_iface;
                    LOG(DLEP_LOG_FATAL, msg);
                    return false;
                }
            }
            start_async_connect(session_address, session_port);
        }
    }

    cleanup_ex_peers_timer.expires_from_now(boost::posix_time::seconds(5));
    cleanup_ex_peers_timer.async_wait(
        boost::bind(&Dlep::handle_ex_peers_timeout, this,
                    boost::asio::placeholders::error));
    return true;
}

void
Dlep::handle_ex_peers_timeout(const boost::system::error_code &  /*error*/)
{
    boost::recursive_mutex::scoped_lock lock(mutex);

    cleanup_ex_peers();
    cleanup_ex_peers_timer.expires_from_now(boost::posix_time::seconds(5));
    cleanup_ex_peers_timer.async_wait(
        boost::bind(&Dlep::handle_ex_peers_timeout, this,
                    boost::asio::placeholders::error));
}

// This method goes through the peers list looking for any peers that
// are in state terminating, and performs the final step of removal.
void
Dlep::cleanup_ex_peers()
{
    ostringstream msg;

    for (auto it = peers.begin(); it != peers.end(); /* ++ inside loop */)
    {
        PeerPtr p = it->second;

        if (p->get_state() == PeerState::terminating)
        {
            msg << "deleting peer=" << p->peer_id;
            LOG(DLEP_LOG_INFO, msg);

            p->cancel_session();

            // If the last reference to the peer is in this list, then
            // this will delete the peer.  However, some of the boost
            // asio operations may still be hanging on to the peer, so
            // the peer may not actually be deleted until those
            // operations finish.

            peers.erase(it++);
        }
        else
        {
            it++;
        }
    }
}

std::string
Dlep::get_peer_id_from_endpoint(boost::asio::ip::tcp::endpoint from_endpoint)
{
    // get IP address and port associated with endpoint:
    boost::asio::ip::address session_ip = from_endpoint.address();
    unsigned short session_port = from_endpoint.port();

    std::string peer_id = session_ip.to_string() + ":" +
                          std::to_string(session_port);

    return peer_id;
}

PeerPtr
Dlep::find_peer(const DlepMac & mac_address)
{

    for (auto & it : peers)
    {
        PeerPtr peer = it.second;
        DestinationDataPtr ddp;
        if (peer->get_destination(mac_address, ddp))
        {
            return peer;
        }
    }

    // We cannot just return nullptr here because gcc 4.7 issues an
    // error about not being able to convert it to a
    // boost::shared_ptr.  The following constructs a null PeerPtr and
    // works with gcc 4.7 and 4.8 (and hopefully anything higher).

    return PeerPtr();
}

PeerState
Dlep::peer_state(std::string peer_id)
{
    // modems can only have 1 peer (router). When peer discovery packet
    //  is received, it does not contain any session IP or PORT information
    //  so you cannot create a suitable peer_id to search for
    if (modem)
    {
        if (peers.empty())
        {
            return PeerState::nonexistent;
        }
        else
        {
            return peers.begin()->second->get_state();
        }
    }
    else
    {
        auto it = peers.find(peer_id);
        if (it == peers.end())
        {
            return PeerState::nonexistent;
        }
        else
        {
            return it->second->get_state();
        }
    }
}
