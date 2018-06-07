/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016, 2018 Massachusetts Institute of Technology
 */

/// @file
/// class DlepServiceImpl is the library implementation of the DlepService interface.
///
/// The methods in this class are called by the DLEP client to provide
/// information, mostly about destinations, for the DLEP service to
/// convey to its peers using the DLEP protocol.
///
/// The methods in this class are called by the library client, from
/// the client's threads which are distinct from the library's
/// threads.  Therefore, any library-owned data that is accessed by
/// these methods needs appropriate locking, both here and anywhere
/// else in the library that accesses the same data.

#include "DlepServiceImpl.h"
#include "DestAdvert.h"

using namespace LLDLEP;
using namespace LLDLEP::internal;

DlepServiceImpl::DlepServiceImpl(DlepPtr dlep, DlepLoggerPtr logger) :
    dlep(dlep),
    logger(logger)
{
    init_thread = Thread(&Dlep::initialize, dlep);
    if (! dlep->wait_for_initialization())
    {
        // clean up the thread before its destructor is called
        init_thread.join();

        // At this point, we don't have specific information about
        // what caused initialization to fail.
        throw DlepServiceImpl::InitializationError("see log file");
    }
}

DlepServiceImpl::ReturnStatus
DlepServiceImpl::destination_up(const DlepMac & mac_address,
                                const DataItems & data_items)
{
    std::ostringstream msg;
    boost::recursive_mutex::scoped_lock lock(dlep->mutex);

    msg << "mac=" << mac_address;
    LOG(DLEP_LOG_INFO, msg);

    ReturnStatus rs = ReturnStatus::ok;

    if (dlep->dest_advert_enabled)
    {
        auto lock = dlep->dest_advert->advert_db_lock();

        auto result = dlep->dest_advert->find_advert_entry(mac_address);

        if (result.first)
        {
            // advert entry for mac_address was found
            const DestAdvertDBEntry & entry = result.second;

            dlep->dest_advert->update_advert_entry_data_items(mac_address,
                                                              data_items);
            // If the entry was down, send Destination Up signals to
            // the peer and set the entry to up.

            if (entry.estate == DestAdvertDBEntry::EntryState::down)
            {
                for (const DlepMac & dest : entry.info.destinations)
                {
                    dlep->local_pdp->addDestination(dest, data_items, true);
                }
                dlep->dest_advert->update_advert_entry_state(mac_address,
                                                             DestAdvertDBEntry::EntryState::up);
            }
            else
            {
                rs = ReturnStatus::destination_exists;
            }
        }
        else
        {
            // advert entry for mac_address was NOT found

            // add a placeholder entry for mac_address

            dlep->dest_advert->add_advert_entry(mac_address,
                                                DestAdvertDBEntry(
                                                    time(nullptr),
                                                    DestAdvertDBEntry::EntryState::up,
                                                    true,
                                                    DestAdvertInfo(),
                                                    data_items));
        }
    }
    else
    {
        // destination advertisement is not enabled
        if (! dlep->local_pdp->addDestination(mac_address, data_items, true))
        {
            rs = ReturnStatus::destination_exists;
        }
    }

    return rs;
}

DlepServiceImpl::ReturnStatus
DlepServiceImpl::destination_update(const DlepMac & mac_address,
                                    const DataItems & data_items)
{
    std::ostringstream msg;
    boost::recursive_mutex::scoped_lock lock(dlep->mutex);

    msg << "mac=" << mac_address;
    LOG(DLEP_LOG_INFO, msg);

    ReturnStatus rs = ReturnStatus::ok;
    bool maybe_not_our_mac_address = false;

    if (dlep->dest_advert_enabled)
    {
        auto lock = dlep->dest_advert->advert_db_lock();

        auto result = dlep->dest_advert->find_advert_entry(mac_address);

        if (result.first)
        {
            // advert entry for mac_address was found
            const DestAdvertDBEntry & entry = result.second;

            dlep->dest_advert->update_advert_entry_data_items(mac_address,
                                                              data_items);
            if (entry.estate == DestAdvertDBEntry::EntryState::up)
            {
                for (const DlepMac & dest : entry.info.destinations)
                {
                    dlep->local_pdp->updateDestination(dest, data_items, true);
                }
            }
        }
        else
        {
            // advert entry for mac_address was NOT found
            maybe_not_our_mac_address = true;
        }
    }
    else
    {
        // destination advertisement is not enabled
        if (! dlep->local_pdp->updateDestination(mac_address, data_items, true))
        {
            maybe_not_our_mac_address = true;
        }
    }

    // It may be that this update is meant for a destination that
    // doesn't belong to the local node.  See if we can find the peer
    // that owns this destination, and if so, send the update to them.

    if (maybe_not_our_mac_address)
    {
        PeerPtr peer = dlep->find_peer(mac_address);

        if (peer == nullptr)
        {
            rs = ReturnStatus::destination_does_not_exist;
            msg << "peer for " << mac_address << " was not found";
            LOG(DLEP_LOG_ERROR, msg);
        }
        else
        {
            peer->destination_update(mac_address, data_items);
        }
    }

    return rs;
}


DlepServiceImpl::ReturnStatus
DlepServiceImpl::destination_down(const DlepMac & mac_address)
{
    std::ostringstream msg;
    boost::recursive_mutex::scoped_lock lock(dlep->mutex);

    msg << "mac=" << mac_address;
    LOG(DLEP_LOG_INFO, msg);
    bool dest_down_mac_address = true;

    ReturnStatus rs = ReturnStatus::ok;

    if (dlep->dest_advert_enabled)
    {
        auto lock = dlep->dest_advert->advert_db_lock();

        auto result = dlep->dest_advert->find_advert_entry(mac_address);

        if (result.first)
        {
            dest_down_mac_address = false;

            // advert entry for mac_address was found
            const DestAdvertDBEntry & entry = result.second;

            if (entry.estate == DestAdvertDBEntry::EntryState::up)
            {
                for (const DlepMac & dest : entry.info.destinations)
                {
                    if (! dlep->local_pdp->removeDestination(dest, true))
                    {
                        msg << "dest advert destination " << dest
                            << " was not found";
                        LOG(DLEP_LOG_ERROR, msg);
                        rs = ReturnStatus::destination_does_not_exist;
                    }
                }
                dlep->dest_advert->update_advert_entry_state(mac_address,
                                          DestAdvertDBEntry::EntryState::down);
            }
        } // mac_address found in destination advertisements
    } // destination advertisement enabled

    if (dest_down_mac_address)
    {
        // Did we originate this mac_address (send the initial Up/Announce)?

        if (! dlep->local_pdp->removeDestination(mac_address, true))
        {
            // mac_address isn't ours.  Try to find its originating peer.

            PeerPtr peer = dlep->find_peer(mac_address);
            if (peer == nullptr)
            {
                msg << "peer for " << mac_address << " was not found";
                LOG(DLEP_LOG_ERROR, msg);
                return ReturnStatus::destination_does_not_exist;
            }
            else
            {
                // find_peer() above said that mac_address belongs to peer,
                // so this shouldn't fail.
                bool ok = peer->remove_destination(mac_address);
                assert(ok);
            }
        }
    }

    return rs;
}

DlepServiceImpl::ReturnStatus
DlepServiceImpl::peer_update(const LLDLEP::DataItems & data_items)
{
    std::ostringstream msg;
    boost::recursive_mutex::scoped_lock lock(dlep->mutex);

    msg << "entered";
    LOG(DLEP_LOG_INFO, msg);

    std::string err = dlep->local_pdp->update_data_items(data_items, true);
    msg << err;
    LOG(DLEP_LOG_INFO, msg);
    if (err == ProtocolStrings::Success)
    {
        return ReturnStatus::ok;
    }
    else
    {
        return ReturnStatus::invalid_data_item;
    }
}

DlepServiceImpl::ReturnStatus
DlepServiceImpl::get_peers(std::vector<std::string> & peers)
{
    boost::recursive_mutex::scoped_lock lock(dlep->mutex);

    for (auto & it : dlep->peers)
    {
        PeerPtr p = it.second;

        peers.push_back(p->peer_id);
    }
    return ReturnStatus::ok;
}

DlepServiceImpl::ReturnStatus
DlepServiceImpl::get_peer_info(const std::string & peer_id,
                               LLDLEP::PeerInfo & peer_info)
{
    boost::recursive_mutex::scoped_lock lock(dlep->mutex);

    auto it = dlep->peers.find(peer_id);
    if (it == dlep->peers.end())
    {
        return ReturnStatus::peer_does_not_exist;
    }

    PeerPtr peer = it->second;
    peer->get_info(peer_info);

    return ReturnStatus::ok;
}

DlepServiceImpl::ReturnStatus
DlepServiceImpl::get_destination_info(const std::string & peer_id,
                                      const DlepMac & mac_address,
                                      LLDLEP::DestinationInfo & dest_info)
{
    boost::recursive_mutex::scoped_lock lock(dlep->mutex);

    auto it = dlep->peers.find(peer_id);
    if (it == dlep->peers.end())
    {
        return ReturnStatus::peer_does_not_exist;
    }

    PeerPtr peer = it->second;
    DestinationDataPtr ddp;
    if (! peer->get_destination(mac_address, ddp))
    {
        return ReturnStatus::destination_does_not_exist;
    }

    dest_info.mac_address = mac_address;
    dest_info.peer_id = peer_id;
    dest_info.flags = 0;
    ddp->get_all_data_items(dest_info.data_items);
    return ReturnStatus::ok;
}

ProtocolConfig *
DlepServiceImpl::get_protocol_config()
{
    boost::recursive_mutex::scoped_lock lock(dlep->mutex);
    assert(dlep->protocfg != nullptr);
    return dlep->protocfg;
}

DlepServiceImpl::ReturnStatus
DlepServiceImpl::linkchar_request(const DlepMac & mac_address,
                                  const DataItems & data_items)
{
    boost::recursive_mutex::scoped_lock lock(dlep->mutex);
    ReturnStatus rs = ReturnStatus::ok;
    PeerPtr peer = dlep->find_peer(mac_address);
    std::ostringstream msg;

    // Destination advertisement should not come into play here
    // because this should be the router sending a request to the
    // modem, and the router never sees untranslated MAC addresses (RF
    // IDs).

    if (peer == nullptr)
    {
        rs = ReturnStatus::peer_does_not_exist;
        msg << "peer for " << mac_address << " was not found";
        LOG(DLEP_LOG_ERROR, msg);
    }
    else
    {
        peer->link_characteristics_request(mac_address, data_items);
    }

    return rs;
}

DlepServiceImpl::ReturnStatus
DlepServiceImpl::linkchar_reply(const std::string & peer_id,
                                const DlepMac & mac_address,
                                const DataItems & data_items)
{
    boost::recursive_mutex::scoped_lock lock(dlep->mutex);
    ReturnStatus rs = ReturnStatus::ok;

    auto it = dlep->peers.find(peer_id);
    if (it == dlep->peers.end())
    {
        return ReturnStatus::peer_does_not_exist;
    }

    PeerPtr peer = it->second;

    peer->link_characteristics_response(mac_address, data_items);

    // XXX should we send destination updates to all of the other peers
    // (other than peer)?
    if (! dlep->local_pdp->updateDestination(mac_address, data_items, false))
    {
        rs = ReturnStatus::destination_does_not_exist;
    }

    return rs;
}

void
DlepServiceImpl::terminate()
{
    {
        boost::recursive_mutex::scoped_lock lock(dlep->mutex);
        std::ostringstream msg;
        msg << "entered";
        LOG(DLEP_LOG_INFO, msg);

        for (auto & it : dlep->peers)
        {
            PeerPtr p = it.second;

            // None of the status codes seem to fit
            p->terminate(ProtocolStrings::Success);
        }

        // Wipe out all of the peers at once
        dlep->peers.clear();

        // This should shut down all the timers.  We don't want any timer
        // handlers being called after we've terminated.

        dlep->io_service_.stop();
    }

    // Wait for the DlepService thread to exit.
    // This is necessary to make sure the DlepService thread doesn't try to
    // access the pointer to its DlepClient object after returning from this
    // method.  DlepService has no control over the lifetime of its DlepClient,
    // and it is very possible that its DlepClient will be deleted soon after
    // this method returns.

    init_thread.join();

    delete dlep;
    dlep = nullptr;
}
