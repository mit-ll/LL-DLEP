/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016, 2018 Massachusetts Institute of Technology
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
    begin_time = time(nullptr);

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
                        time(nullptr) - begin_time,  // uptime
                        ++seq_num,                // seqnum
                        local_rfid,               // our radio id
                        destinations);            // advertised destinations

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

    auto result = unbuild_destination_advert(msg_buffer.get(), msg_buffer_len);

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

        DataItems empty_data_items;

        // new entry, save timestamp and advertisement info
        dest_advert_db[dainfo.rfId] = DestAdvertDBEntry {time(nullptr),
                                                         DestAdvertDBEntry::EntryState::down,
                                                         false,
                                                         dainfo,
                                                         empty_data_items
                                                        };
    }
    else
    {
        DestAdvertDBEntry & entry = iter->second;  // synonym for easier reading

        msg << "existing destination advertisement entry was "
            << entry.to_string();
        LOG(DLEP_LOG_INFO, msg);

        // update timestamp
        entry.timestamp = time(nullptr);

        if (entry.estate == DestAdvertDBEntry::EntryState::up)
        {
            DlepMacAddrs added, deleted;

            // handle destinations that were added by this advertisement

            // new - old = added
            getDifference(dainfo.destinations, entry.info.destinations,
                          added);

            for (const DlepMac & mac : added)
            {
                if (! dlep->local_pdp->addDestination(mac, entry.data_items,
                                                      true))
                {
                    msg << "destination " << mac << " already exists";
                    LOG(DLEP_LOG_ERROR, msg);
                }
            }

            // handle destinations that were deleted by this advertisement

            // old - new = deleted
            getDifference(entry.info.destinations, dainfo.destinations,
                          deleted);

            for (const DlepMac & mac : deleted)
            {
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
        entry.data_items = data_items;
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

    time_t current_time = time(nullptr);
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
