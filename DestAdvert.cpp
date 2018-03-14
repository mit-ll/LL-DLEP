/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016 Massachusetts Institute of Technology
 *
 * Contributor: Adjacent Link LLC, Bridgewater, NJ
 */

/// @file
/// The DestAdvert class implements the DLEP destination advertisement
/// service.

#include "DestAdvert.h"
#include "DestAdvertMessage.h"

using namespace LLDLEP;
using namespace LLDLEP::internal;

DestAdvert::DestAdvert(
    DlepPtr dlep,
    boost::asio::io_service & io_service,
    std::string interface_name,
    uint16_t udp_port,
    boost::asio::ip::address & multicast_address,
    unsigned int send_interval,
    const DlepMac & local_rfid,
    DlepLoggerPtr logger) :
    PeriodicMcastSendRcv(dlep,
                         io_service,
                         interface_name,
                         udp_port,
                         multicast_address,
                         0, // default TTL
                         send_interval,
                         send_interval > 0, // sending if interval is > 0
                         true,              // always receiving when enabled
                         logger),
    seq_num(0),
    send_interval(send_interval),
    local_rfid(local_rfid),
    purge_advert_timer(io_service)
{
    begin_time = time(NULL);

    start_purge_advert_timer();
}


DestAdvert::~DestAdvert()
{
    purge_advert_timer.cancel();
}


//*****************************************************************************

// methods for implementing the PeriodicMcastSendRcv interface

DlepMessageBuffer DestAdvert::get_message_to_send(unsigned int * msg_len)
{
    DestAdvertInfo info(send_interval,            // interval in sec
                        time(NULL) - begin_time,  // uptime
                        ++seq_num,                // seqnum
                        local_rfid,               // our radio id
                        destinations,             // advertised destinations
                        localPeerIpv4DataItems,   // advertised ipv4 dataitems
                        localPeerIpv4SnDataItems, // advertised ipv4 subnet dataitems
                        localPeerIpv6DataItems,   // advertised ipv6 dataitems
                        localPeerIpv6SnDataItems);// advertised ipv6 subnet dataitems         

    auto result = build_destination_advert(info);

    // msg build success
    if (result.first)
    {
        *msg_len = result.second.size();

        DlepMessageBuffer msg_buf(new std::uint8_t[result.second.size()]);

        memcpy(msg_buf.get(), result.second.data(), result.second.size());

        LOG(DLEP_LOG_INFO, std::ostringstream(info.to_string()));

        return msg_buf;
    }
    else
    {
        *msg_len = 0;

        LOG(DLEP_LOG_ERROR, std::ostringstream("msg build error"));

        return DlepMessageBuffer();
    }
}


void DestAdvert::handle_message(DlepMessageBuffer msg_buffer,
                                unsigned int msg_buffer_len,
                                boost::asio::ip::udp::endpoint from_endpoint)
{
    std::ostringstream msg;

    msg << "received message length=" << msg_buffer_len
        << " from=" << from_endpoint;
    LOG(DLEP_LOG_INFO, msg);

    if (msg_buffer_len == 0)
    {
        LOG(DLEP_LOG_ERROR, std::ostringstream("recv empty message"));
        return;
    }

    auto result = unbuild_destination_advert(msg_buffer.get(), msg_buffer_len, dlep);

    if (! result.first)
    {
        LOG(DLEP_LOG_ERROR, std::ostringstream("could not parse message"));
        return;
    }

    // msg parse success

    DestAdvertInfo & dainfo = result.second; // synonym for easier reading

    msg << "message content: " << dainfo.to_string();
    LOG(DLEP_LOG_INFO, msg);

    boost::recursive_mutex::scoped_lock lock(dest_advert_mutex);

    // lookup based on rfId
    auto iter = dest_advert_db.find(dainfo.rfId);

    // check for new/existing entry
    if (iter == dest_advert_db.end())
    {
        LOG(DLEP_LOG_INFO,
            std::ostringstream("new destination advertisement entry!"));

        // insert the ip data items to DataItems
        DataItems dataItems = dainfo.ipv4DataItems;
        dataItems.insert(dataItems.end(), dainfo.ipv4SnDataItems.begin(), dainfo.ipv4SnDataItems.end());
        dataItems.insert(dataItems.end(), dainfo.ipv6DataItems.begin(), dainfo.ipv6DataItems.end());
        dataItems.insert(dataItems.end(), dainfo.ipv6SnDataItems.begin(), dainfo.ipv6SnDataItems.end());
        

        // new entry, save timestamp and advertisement info
        dest_advert_db[dainfo.rfId] = DestAdvertDBEntry {time(NULL),
                                                         DestAdvertDBEntry::EntryState::down,
                                                         false,
                                                         dainfo,
                                                         dataItems
                                                        };
    }
    else
    {
        DestAdvertDBEntry & entry = iter->second;  // synonym for easier reading

        msg << "existing destination advertisement entry was "
            << entry.to_string();
        LOG(DLEP_LOG_INFO, msg);

        // update timestamp
        entry.timestamp = time(NULL);
        //entry.data_items.clear();
        if (entry.estate == DestAdvertDBEntry::EntryState::up)
        {
            DlepMacAddrs added, deleted;

            // handle destinations that were added by this advertisement

            // new - old = added
            getDifference(dainfo.destinations, entry.info.destinations,
                          added);

            msg << "added size " << added.size();
            for (auto it = dainfo.destinations.begin(); it != dainfo.destinations.end(); ++it)
            {
                msg <<  " dainfo.destinations" << *it;
            }
            for (auto it = entry.info.destinations.begin(); it != entry.info.destinations.end(); ++it)
            {
                msg <<  " entry.info.destinations" << *it;
            }
            LOG(DLEP_LOG_DEBUG, msg);

            for (const DlepMac & mac : added)
            {
                if (! dlep->local_pdp->addDestination(mac, entry.data_items,
                                                      true))
                {
                    msg << "destination " << mac << " already exists";
                    LOG(DLEP_LOG_ERROR, msg);
                }
            }

            //update the entry info dataitems with ip dataitems that were just received
            std::vector<DataItems> ipParameters = {dainfo.ipv4DataItems, dainfo.ipv4SnDataItems, dainfo.ipv6DataItems, dainfo.ipv6SnDataItems};
            for (auto dataItems : ipParameters)
            {
                DataItems ipdisAdd, ipdisRemove;
                for (auto ipdi : dataItems)
                {
                    DataItem::IPFlags ipAdded = ipdi.ip_flags();
                    //if ip add, then we need to add all new and unique id data items
                    if (ipAdded == DataItem::IPFlags::add)
                    {
                        bool isNewIpDi = true;
                        for (auto di : entry.data_items)
                        {
                            if (di == ipdi)
                            {
                                isNewIpDi = false;
                            }
                        }
                        if (isNewIpDi)
                        {
                            //add only new ip dataitems
                            ipdisAdd.push_back(ipdi);
                        }
                    }
                    else //if ip drop remove this ip data item
                    {
                        for (auto di : entry.data_items)
                        {
                            if (ipdi.ip_equal(di))
                            {
                                ipdisRemove.push_back(di);
                            }
                        }
                    }
                }
                entry.data_items.insert(entry.data_items.end(), ipdisAdd.begin(), ipdisAdd.end());
                for (auto ipdir : ipdisRemove)
                {
                    entry.data_items.erase(std::remove(entry.data_items.begin(), entry.data_items.end(), ipdir), entry.data_items.end());
                }
            }

            // handle destinations that were deleted by this advertisement

            // old - new = deleted
            getDifference(entry.info.destinations, dainfo.destinations,
                          deleted);

            msg << "deleted size " << deleted.size();
            LOG(DLEP_LOG_DEBUG, msg);


            for (const DlepMac & mac : deleted)
            {
                entry.data_items.clear();
                if (! dlep->local_pdp->removeDestination(mac, true))
                {
                    msg << "destination " << mac << " does not exist";
                    LOG(DLEP_LOG_ERROR, msg);
                }
            }
        }
        // update the entry info to what was just received
        entry.info = dainfo;
    }
}

//*****************************************************************************

// methods for manipulating the set of destinations included in our own
// Destination Advertisement


void DestAdvert::add_destination(const LLDLEP::DlepMac & mac)
{
    LOG(DLEP_LOG_INFO, std::ostringstream(mac.to_string()));

    destinations.insert(mac);
}

void DestAdvert::add_dataitem(const LLDLEP::DataItem & di)
{
    LOG(DLEP_LOG_INFO, std::ostringstream(di.to_string()));

    DataItems* selfDataItems(nullptr);
    if (di.name() == ProtocolStrings::IPv4_Address)
    {
        selfDataItems = &localPeerIpv4DataItems;
    }else if (di.name() == ProtocolStrings::IPv4_Attached_Subnet)
    {
        selfDataItems = &localPeerIpv4SnDataItems;
    }else if (di.name() == ProtocolStrings::IPv6_Address)
    {
        selfDataItems = &localPeerIpv6DataItems;
    }else if (di.name() == ProtocolStrings::IPv6_Attached_Subnet)
    {
        selfDataItems = &localPeerIpv6SnDataItems;
    }

    DataItems ipdisRemove;
    for (auto diRemove : *selfDataItems)
    {
        if (diRemove.ip_equal(di))
        {
            ipdisRemove.push_back(diRemove);
        }
    }
    for (auto diRemove : ipdisRemove)
    {
        selfDataItems->erase(std::remove(selfDataItems->begin(), selfDataItems->end(), diRemove), selfDataItems->end());
    }
    selfDataItems->push_back(di);
}

void DestAdvert::clear_ipdataitems()
{   
    localPeerIpv4DataItems.erase(localPeerIpv4DataItems.begin(), localPeerIpv4DataItems.end());
    localPeerIpv4SnDataItems.erase(localPeerIpv4SnDataItems.begin(), localPeerIpv4SnDataItems.end());
    localPeerIpv6DataItems.erase(localPeerIpv6DataItems.begin(), localPeerIpv6DataItems.end());
    localPeerIpv6SnDataItems.erase(localPeerIpv6SnDataItems.begin(), localPeerIpv6SnDataItems.end());
}

void DestAdvert::del_destination(const LLDLEP::DlepMac & mac)
{
    LOG(DLEP_LOG_INFO, std::ostringstream(mac.to_string()));

    destinations.erase(mac);
}


void DestAdvert::clear_destinations()
{
    LOG(DLEP_LOG_INFO, std::ostringstream(""));

    destinations.clear();
}

//*****************************************************************************

// methods for manipulating the DestAdvertDB (destination advertisement table)

boost::recursive_mutex::scoped_lock
DestAdvert::advert_db_lock()
{
    return boost::recursive_mutex::scoped_lock {dest_advert_mutex};
}


void DestAdvert::add_advert_entry(const DlepMac & rfId,
                                  const DestAdvertDBEntry & advert_entry)
{
    std::ostringstream msg;

    boost::recursive_mutex::scoped_lock lock(dest_advert_mutex);

    msg << "adding rfid=" << rfId.to_string()
        << " entry=" << advert_entry.to_string();
    LOG(DLEP_LOG_INFO, msg);

    dest_advert_db[rfId] = advert_entry;
}


std::pair<bool, const DestAdvertDBEntry &>
DestAdvert::find_advert_entry(const DlepMac & rfId)
{
    boost::recursive_mutex::scoped_lock lock(dest_advert_mutex);
    std::ostringstream msg;

    const auto iter = dest_advert_db.find(rfId);

    if (iter != dest_advert_db.end())
    {
        msg << "rfid " << rfId.to_string() << " found in table";
        LOG(DLEP_LOG_INFO, msg);

        return std::pair<bool, const DestAdvertDBEntry &>(true, iter->second);
    }
    else
    {
        msg << "rfid " << rfId.to_string() << " not found in table";
        LOG(DLEP_LOG_INFO, msg);

        return std::pair<bool, const DestAdvertDBEntry &>(false,
                                                          DestAdvertDBEntry());
    }
}

void
DestAdvert::update_advert_entry_data_items(const DlepMac & rfId,
                                           const DataItems & data_items)
{
    boost::recursive_mutex::scoped_lock lock(dest_advert_mutex);
    std::ostringstream msg;

    msg << "updating rfid=" << rfId.to_string();
    LOG(DLEP_LOG_INFO, msg);

    const auto iter = dest_advert_db.find(rfId);

    if (iter != dest_advert_db.end())
    {
        DestAdvertDBEntry & entry = iter->second;
        DataItems ip_data_items;
        //save aside all ip data items
        for (auto di : entry.data_items)
        {
            if (di.name() == ProtocolStrings::IPv4_Address
                || di.name() == ProtocolStrings::IPv4_Attached_Subnet
                || di.name() == ProtocolStrings::IPv6_Address
                || di.name() == ProtocolStrings::IPv6_Attached_Subnet)
                {
                    ip_data_items.push_back(di);
                    msg << "updating ipdataitems=" << di.to_string();
                    LOG(DLEP_LOG_INFO, msg);
                }
        }

        //clean entry
        entry.data_items.erase(entry.data_items.begin(), entry.data_items.end());
        
        //add all new data items wich are not ip
        for (auto di : data_items)
        {
            if (di.name() != ProtocolStrings::IPv4_Address
                && di.name() != ProtocolStrings::IPv4_Attached_Subnet
                && di.name() != ProtocolStrings::IPv6_Address
                && di.name() != ProtocolStrings::IPv6_Attached_Subnet)
                {
                    entry.data_items.push_back(di);
                }
        }

        //add ip data items that we saved aside
        entry.data_items.insert(entry.data_items.end(), ip_data_items.begin(), ip_data_items.end());
    }
    else
    {
        msg << "rfid " << rfId.to_string() << " not found in table";
        LOG(DLEP_LOG_ERROR, msg);
    }
}

void
DestAdvert::clear_advert_entry_data_items(const DlepMac & rfId)
{
    boost::recursive_mutex::scoped_lock lock(dest_advert_mutex);
    std::ostringstream msg;

    msg << "clearing dataitems of rfid=" << rfId.to_string();
    LOG(DLEP_LOG_INFO, msg);

    const auto iter = dest_advert_db.find(rfId);

    if (iter != dest_advert_db.end())
    {
        DestAdvertDBEntry & entry = iter->second;

        entry.data_items.clear();
        entry.data_items.erase(entry.data_items.begin(), entry.data_items.end());
    }
    else
    {
        msg << "rfid " << rfId.to_string() << " not found in table";
        LOG(DLEP_LOG_ERROR, msg);
    }
}

void
DestAdvert::update_advert_entry_state(const DlepMac & rfId,
                                      DestAdvertDBEntry::EntryState newstate)
{
    boost::recursive_mutex::scoped_lock lock(dest_advert_mutex);
    std::ostringstream msg;

    msg << "updating rfid=" << rfId.to_string()
        << " new state=" << (int)newstate;
    LOG(DLEP_LOG_INFO, msg);

    const auto iter = dest_advert_db.find(rfId);

    if (iter != dest_advert_db.end())
    {
        DestAdvertDBEntry & entry = iter->second;
        entry.estate = newstate;
    }
    else
    {
        msg << "rfid " << rfId.to_string() << " not found in table";
        LOG(DLEP_LOG_ERROR, msg);
    }
}

//*****************************************************************************

// methods for manipulating the purge advertisement timer

void
DestAdvert::start_purge_advert_timer()
{
    purge_advert_timer.expires_from_now(boost::posix_time::seconds(1));
    purge_advert_timer.async_wait(
        boost::bind(&DestAdvert::handle_purge_advert_timeout, this,
                    boost::asio::placeholders::error));

}


void
DestAdvert::handle_purge_advert_timeout(const boost::system::error_code & error)
{
    std::ostringstream msg;
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

    time_t current_time = time(NULL);
    unsigned int hold_interval;
    dlep->dlep_client.get_config_parameter("destination-advert-hold-interval",
                                           &hold_interval);

    unsigned int expire_count;
    dlep->dlep_client.get_config_parameter("destination-advert-expire-count",
                                           &expire_count);

    boost::recursive_mutex::scoped_lock lock(dest_advert_mutex);

    for (auto iter = dest_advert_db.begin();
            iter != dest_advert_db.end();
            /* increment inside the loop */)
    {
        DestAdvertDBEntry & entry = iter->second;
        time_t entry_age = current_time - entry.timestamp;

        if ((hold_interval > 0)
                &&
                (entry.placeholder)
                &&
                (entry.estate == DestAdvertDBEntry::EntryState::up)
           )
        {
            if (entry_age >= hold_interval)
            {
                msg << "placeholder entry for " << iter->first
                    << " is " << entry_age << " seconds old, removing";
                LOG(DLEP_LOG_INFO, msg);
                iter = dest_advert_db.erase(iter);
                continue;
            }
        }

        if (expire_count > 0)
        {
            if (entry_age >= expire_count * entry.info.reportInterval)
            {
                msg << "destination advertisement for " << iter->first
                    << " is " << entry_age << " seconds old, removing";
                LOG(DLEP_LOG_INFO, msg);

                if (entry.estate == DestAdvertDBEntry::EntryState::up)
                {
                    for (const DlepMac & dest : entry.info.destinations)
                    {
                        dlep->local_pdp->removeDestination(dest, true);
                    }
                }

                iter = dest_advert_db.erase(iter);
                continue;
            }
        }

        ++iter;
    } // end for each destination advertisement

    start_purge_advert_timer();
}
