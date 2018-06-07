/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2013, 2015, 2016, 2018 Massachusetts Institute of Technology
 */
#include "InfoBaseMgr.h"

using namespace std;
using namespace LLDLEP;
using namespace LLDLEP::internal;


/// Add or drop an IP address or subnet to the list of addresses/subnets
/// for this owner (peer or destination).  This is a free-standing
/// function because it is used for both destinations and peers.
///
/// @param[in] owner_name
///            name of destination or peer whose IP addresses are
///            being updated.  For logging.
/// @param[in,out] ip_data_items
///            List of IP addresses to be updated.
/// @param[in] new_ip_data_item
///            IP address or subnet to be added or dropped
/// @param[in] logger
///            logger, for LOG macro
static void
update_ip_data_items(const std::string & owner_name,
                     DataItems & ip_data_items,
                     const DataItem & new_ip_data_item,
                     DlepLoggerPtr logger)
{
    ostringstream msg;
    // Is the IP address being added or dropped?
    bool adding = new_ip_data_item.ip_flags() & DataItem::IPFlags::add;

    // Look for new_ip_data_item in this destination's ip_data_items

    for (auto it = ip_data_items.begin(); it != ip_data_items.end(); it++)
    {
        const DataItem & di = *it;

        if (di.ip_equal(new_ip_data_item))
        {
            // This destination already has new_ip_data_item.  If it's
            // being added, there's nothing left to do.  If it's being
            // removed, then erase it from ip_data_items.  Either way,
            // we're done processing this update.

            if (! adding)
            {
                msg << owner_name << " remove " << new_ip_data_item.to_string();
                LOG(DLEP_LOG_DEBUG, msg);
                ip_data_items.erase(it);
            }
            return;
        }
    }

    // If we get here, the IP address in new_ip_data_item wasn't
    // found, so append it to ip_data_items.

    if (adding)
    {
        msg << owner_name << " add " << new_ip_data_item.to_string();
        LOG(DLEP_LOG_DEBUG, msg);
        ip_data_items.push_back(new_ip_data_item);
    }
}

//-----------------------------------------------------------------------------

DestinationData::DestinationData(const DlepMac & mac,
                                 const DataItems & initial_data_items,
                                 DlepPtr dlep) :
    mac_address(mac),
    dlep(dlep),
    logger(dlep->logger)
{
    ostringstream msg;

    msg << "Mac Address of destination is " << mac_address;
    LOG(DLEP_LOG_INFO, msg);

    for (auto const & di : initial_data_items)
    {
        // Only store data items with this destination that are
        // metrics or contain IP addresses.  This filters out unwanted
        // data items like Status.

        if (dlep->protocfg->is_metric(di.id))
        {
            metric_data_items[di.id] = di;
        }
        else if (dlep->protocfg->is_ipaddr(di.id))
        {
            update_ip_data_items("destination=" + mac_address.to_string(),
                                 ip_data_items, di, logger);
        }
    } // for each data item
}

DestinationData::~DestinationData()
{
    // Using smart pointers...
}


unsigned int
DestinationData::update(const DataItems & updates, bool tell_peers)
{
    unsigned int num_updates = 0;

    for (auto const & di : updates)
    {
        if (dlep->protocfg->is_metric(di.id))
        {
            metric_data_items[di.id] = di;
            num_updates++;
        }
        else if (dlep->protocfg->is_ipaddr(di.id))
        {
            update_ip_data_items("destination=" + mac_address.to_string(),
                                 ip_data_items, di, logger);
            num_updates++;
        }
    }

    if (tell_peers)
    {
        for (auto & it : dlep->peers)
        {
            PeerPtr p = it.second;
            p->destination_update(mac_address, updates);
        }
    }

    return num_updates;
}

void
DestinationData::get_all_data_items(DataItems & data_items_out) const
{
    // Copy of all of the data items in the map to the caller's vector

    for (auto const & dipair : metric_data_items)
    {
        data_items_out.push_back(dipair.second);
    }

    for (auto const & di : ip_data_items)
    {
        data_items_out.push_back(di);
    }
}

DataItems
DestinationData::get_ip_data_items() const
{
    return ip_data_items;
}


void
DestinationData::log(const std::string & prefix, int log_level) const
{
    ostringstream msg;

    msg << prefix << " destination=" << mac_address
        << " needs response=" << needs_response_;
    LOG(log_level, msg);

    for (auto const & dipair : metric_data_items)
    {
        msg << dipair.second.to_string();
        LOG(log_level, msg);
    }

    for (auto const & di : ip_data_items)
    {
        msg << di.to_string();
        LOG(log_level, msg);
    }
}

void
DestinationData::needs_response(const std::string & response_name)
{
    needs_response_ = response_name;
}

std::string
DestinationData::needs_response() const
{ 
    return needs_response_;
}

std::string
DestinationData::find_ip_data_item(const DataItem & ip_data_item) const
{
    if (ip_data_item.find_ip_data_item(ip_data_items) != ip_data_items.end())
    {
        return "destination=" + mac_address.to_string();
    }
    return "";
}

//-----------------------------------------------------------------------------

PeerData::PeerData(const std::string & id, const DataItems & initial_data_items,
                   DlepPtr dlep) :
    peer_id(id),
    dlep(dlep),
    logger(dlep->logger)
{
    for (auto const & di : initial_data_items)
    {
        if (dlep->protocfg->is_metric(di.id))
        {
            metric_data_items[di.id] = di;
        }
        else if (dlep->protocfg->is_ipaddr(di.id))
        {
            update_ip_data_items("peer=" + peer_id,
                                 ip_data_items, di, logger);
        }
    }
}

PeerData::~PeerData()
{
}

bool
PeerData::addDestination(const DlepMac & mac,
                         const DataItems & initial_data_items,
                         bool tell_peers)
{
    ostringstream msg;

    auto it = destination_data.find(mac);
    if (it != destination_data.end())
    {
        msg << "adding existing destination mac=" << mac
            << " already exists for peer=" << peer_id;
        LOG(DLEP_LOG_ERROR, msg);
        return false;
    }

    DestinationDataPtr ddp(new DestinationData(mac, initial_data_items, dlep));
    destination_data[mac] = ddp;
    if (tell_peers)
    {
        for (auto & it : dlep->peers)
        {
            PeerPtr p = it.second;
            p->destination_up(mac, initial_data_items);
        }
    }

    return true;
}

void
PeerData::sendAllDestinations(PeerPtr peer)
{
    for (auto const & destpair : destination_data)
    {
        DlepMac mac = destpair.first;
        DestinationDataPtr ddp = destpair.second;
        DataItems dest_data_items;

        ddp->get_all_data_items(dest_data_items);
        peer->destination_up(mac, dest_data_items);
    }
}

bool
PeerData::updateDestination(const DlepMac & mac,
                            const DataItems & updates,
                            bool tell_peers)
{
    DestinationDataPtr ddp;
    bool good_destination = getDestinationData(mac, &ddp);

    if (good_destination)
    {
        ddp->update(updates, tell_peers);
        return true;
    }
    else
    {
        return false;
    }
}

bool
PeerData::removeDestination(const DlepMac & mac, bool tell_peers)
{
    ostringstream msg;

    auto it = destination_data.find(mac);

    if (it == destination_data.end())
    {
        msg << "removing destination mac=" << mac
            << " does not exist for peer=" << peer_id;
        LOG(DLEP_LOG_ERROR, msg);
        return false;
    }
    else
    {
        destination_data.erase(it->first);
        if (tell_peers)
        {
            for (auto & it : dlep->peers)
            {
                PeerPtr p = it.second;
                p->destination_down(mac);
            }
        }
    }

    return true;
}

bool
PeerData::getDestinationData(const DlepMac & mac, DestinationDataPtr * ddpp)
{
    ostringstream msg;
    bool good = false;

    auto it = destination_data.find(mac);

    if (it == destination_data.end())
    {
        msg << "destination mac=" << mac << " not found for peer="
            << peer_id;
        LOG(DLEP_LOG_ERROR, msg);
    }
    else
    {
        *ddpp = it->second;
        good = true;
    }

    return good;
}


void
PeerData::logDestinations(bool include_metrics)
{
    ostringstream msg;

    for (auto const & destpair : destination_data)
    {
        DlepMac mac = destpair.first;
        DestinationDataPtr ddp = destpair.second;

        if (include_metrics)
        {
            ddp->log(__func__, DLEP_LOG_INFO);
        }
        else
        {
            msg << "destination mac= " << mac;
            LOG(DLEP_LOG_INFO, msg);
        }
    }
}

void
PeerData::getDestinations(std::vector<DlepMac> & destinations)
{

    for (auto const & destpair : destination_data)
    {
        destinations.push_back(destpair.first);
    }
}

bool
PeerData::validDestination(const DlepMac & mac)
{
    return (destination_data.find(mac) != destination_data.end());
}

std::string
PeerData::update_data_items(const DataItems & updates, bool tell_peers)
{
    ostringstream msg;

    // See if there are any metrics in this update that we haven't
    // seen before from this peer.  If so, don't apply any changes,
    // and return an error.
    // XXX destination_up/update also need to do this

    for (auto const & di : updates)
    {
        if (dlep->protocfg->is_metric(di.id))
        {
            if (metric_data_items.find(di.id) == metric_data_items.end())
            {
                // metric from the update was not found in the peer's
                // preexisting list of metrics
                msg << "peer=" << peer_id
                    << " metric " << dlep->protocfg->get_data_item_name(di.id)
                    << " in update is previously unknown";
                LOG(DLEP_LOG_ERROR, msg);
                return ProtocolStrings::Invalid_Message;
            }
        }
    } // for each data item in updates

    // If we get here, the data items are all OK, so we can apply the changes.

    // Collect updates to apply to all destinations here
    DataItems destination_updates;

    for (auto const & di : updates)
    {
        if (dlep->protocfg->is_metric(di.id))
        {
            metric_data_items[di.id] = di;
            destination_updates.push_back(di);
        }
        else if (dlep->protocfg->is_ipaddr(di.id))
        {
            update_ip_data_items("peer=" + peer_id,
                                 ip_data_items, di, logger);

            // IP addresses don't propagate to all destinations
        }
    } // for each data item in updates

    if (!destination_updates.empty())
    {
        // Update all destinations' data metrics
        for (auto const & destpair : destination_data)
        {
            DestinationDataPtr ddp = destpair.second;

            // Don't generate a Destination Update message for this
            // destination.  If this was a Peer Update received from a
            // peer, that peer is responsible for sending Destination
            // Updates.  If this update is originating on our side,
            // then when we send the Peer Update(s) below
            // (tell_peers), the peer will apply the changes to all
            // destinations from us when it receives the update.  So
            // there is no need for us to send individual Destination
            // Updates.
            ddp->update(destination_updates, false);
        }
    }

    if (tell_peers)
    {
        for (auto & it : dlep->peers)
        {
            PeerPtr p = it.second;
            p->peer_update(updates);
        }
    }

    return ProtocolStrings::Success;
}

DataItems
PeerData::get_data_items()
{
    DataItems return_data_items;

    for (auto const & dipair : metric_data_items)
    {
        return_data_items.push_back(dipair.second);
    }

    for (auto const & di : ip_data_items)
    {
        return_data_items.push_back(di);
    }

    return return_data_items;
}

DataItems
PeerData::get_ip_data_items() const
{
    return ip_data_items;
}


void
PeerData::log_data_items()
{
    ostringstream msg;

    for (auto const & dipair : metric_data_items)
    {
        msg << dipair.second.to_string();
        LOG(DLEP_LOG_DEBUG, msg);
    }

    for (auto const & di : ip_data_items)
    {
        msg << di.to_string();
        LOG(DLEP_LOG_DEBUG, msg);
    }
}

void
PeerData::needs_response(const DlepMac & mac,
                         const std::string & response_name)
{
    auto it = destination_data.find(mac);

    if (it != destination_data.end())
    {
        it->second->needs_response(response_name);
    }
}

std::string
PeerData::needs_response(const DlepMac & mac)
{
    auto it = destination_data.find(mac);

    if (it == destination_data.end())
    {
        return "";
    }
    else
    {
        return it->second->needs_response();
    }
}

std::string
PeerData::find_ip_data_item(const DataItem & ip_data_item) const
{
    if (ip_data_item.find_ip_data_item(ip_data_items) != ip_data_items.end() )
    {
        return "peer=" + peer_id;
    }
    else
    {
        for (auto const & destpair : destination_data)
        {
            DlepMac mac = destpair.first;
            DestinationDataPtr ddp = destpair.second;
            std::string ip_owner = ddp->find_ip_data_item(ip_data_item);
            if (ip_owner != "")
            {
                return ip_owner;
            }
        }
    }
    return "";
}

//-----------------------------------------------------------------------------

InfoBaseMgr::InfoBaseMgr(DlepPtr dlep) :
    dlep(dlep),
    logger(dlep->logger)
{
}

InfoBaseMgr::~InfoBaseMgr()
{
    // Using smart pointers
}

PeerDataPtr
InfoBaseMgr::addPeer(const std::string & peer_id,
                     const DataItems & initial_values)
{
    PeerDataPtr pdp(new PeerData(peer_id, initial_values, dlep));
    peer_data[peer_id] = pdp;
    return pdp;

}

bool
InfoBaseMgr::removePeer(const std::string & peer_id)
{
    ostringstream msg;

    auto it = peer_data.find(peer_id);

    if (it == peer_data.end())
    {
        msg << "Removing peer=" << peer_id
            << " Peer not found";
        LOG(DLEP_LOG_ERROR, msg);
        return false;
    }
    else
    {
        peer_data.erase(it->first);
    }

    return true;
}

bool
InfoBaseMgr::getPeerData(const std::string & peer_id, PeerDataPtr * pdpp)
{
    ostringstream msg;
    bool good = false;

    auto it = peer_data.find(peer_id);

    if (it == peer_data.end())
    {
        msg << "peer=" << peer_id << " not found";
        LOG(DLEP_LOG_ERROR, msg);
    }
    else
    {
        *pdpp = it->second;
        good = true;
    }
    return good;
}

void
InfoBaseMgr::logInfoBase(bool include_metrics)
{
    ostringstream msg;

    for (auto const & peerpair : peer_data)
    {
        std::string peer_id = peerpair.first;
        PeerDataPtr pdp = peerpair.second;

        msg << "peer=" << peer_id << " Destinations:";
        LOG(DLEP_LOG_INFO, msg);

        pdp->logDestinations(include_metrics);

        if (include_metrics)
        {
            msg << "peer=" << peer_id << " Peer Metrics:";
            LOG(DLEP_LOG_INFO, msg);

            pdp->log_data_items();
        }
    }
}

bool
InfoBaseMgr::validPeer(const std::string & peer_id)
{
    bool rnc = true;

    if (peer_data.find(peer_id) == peer_data.end())
    {
        rnc = false;
    }
    return rnc;

}

bool
InfoBaseMgr::validDestination(const std::string & peer_id, const DlepMac & mac)
{
    bool rnc = true;

    if (peer_data.find(peer_id) != peer_data.end())
    {
        PeerDataPtr pdp = peer_data[peer_id];

        if (!pdp->validDestination(mac))
        {
            rnc = false;
        }
    }
    else
    {
        rnc = false;
    }
    return rnc;
}
