/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016 Massachusetts Institute of Technology
 *
 * Contributor: Adjacent Link LLC, Bridgewater, NJ
 */

/// @file
/// DestAdvert class declaration.

#ifndef DEST_ADVERT_H
#define DEST_ADVERT_H

#include <boost/thread/mutex.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <sys/time.h>

#include "PeriodicMcastSendRcv.h"
#include "DestAdvertDataBase.h"

namespace LLDLEP
{
namespace internal
{

class Dlep;

/// This class handles most of the Destination Advertisement part of DLEP.
class DestAdvert : public PeriodicMcastSendRcv
{
public:

    /// Constructor.
    /// @param[in] dlep           back-pointer to dlep instance
    /// @param[in] io_service     for handling network I/O and timers
    /// @param[in] interface_name name of the network interface to use
    /// @param[in] udp_port       UDP port number to put on multicast packets
    /// @param[in] multicast_address  multicast address to put on packets
    /// @param[in] send_interval  number of seconds between sending packets
    /// @param[in] local_rfid     rfid of the local node
    /// @param[in] logger         logger, for LOG macro
    DestAdvert(
        DlepPtr dlep,
        boost::asio::io_service & io_service,
        std::string interface_name,
        uint16_t udp_port,
        boost::asio::ip::address & multicast_address,
        unsigned int send_interval,
        const LLDLEP::DlepMac & local_rfid,
        DlepLoggerPtr logger);

    virtual ~DestAdvert();

    /// Add a destination to future destination advertisements
    /// sent by this process.
    ///
    /// @param[in] mac  the destination to add
    void add_destination(const LLDLEP::DlepMac & mac);

    /// Remove a destination from future destination advertisements
    /// sent by this process.
    ///
    /// @param[in] mac  the destination to remove
    void del_destination(const LLDLEP::DlepMac & mac);

    /// Remove all destinations from future destination advertisements
    /// sent by this process.
    void clear_destinations();

    /// Obtain a lock on the destination advertisement database.
    /// Use this when more than one operation will be performed on the
    /// database to prevent modifications by other threads.
    /// This is not needed if just performing one operation.
    boost::recursive_mutex::scoped_lock advert_db_lock();

    /// Add an entry to the destination advertisement database.
    /// @param[in] rfId          the RF_ID associated with the entry
    /// @param[in] advert_entry  the entry to add
    void add_advert_entry(const LLDLEP::DlepMac & rfId,
                          const DestAdvertDBEntry & advert_entry);

    /// Find an entry in the destination advertisement database.
    /// @param[in] rfId  the RF_ID for which to find the entry
    /// @return a pair:
    ///         first: true iff the entry was found
    ///         second: reference to the entry, if first is true
    std::pair<bool, const DestAdvertDBEntry &>
    find_advert_entry(const LLDLEP::DlepMac & rfId);

    /// Update the metrics on an entry in the destination advertisement
    /// database.
    /// @param[in] rfId       the RF_ID whose entry will be updated
    /// @param[in] data_items the data items associated with rfId
    void update_advert_entry_data_items(const LLDLEP::DlepMac & rfId,
                                        const LLDLEP::DataItems & data_items);

    /// Update the state of an entry in the destination advertisement
    /// database.
    /// @param[in] rfId     the RF_ID whose entry will be updated
    /// @param[in] newstate new state for the entry
    void update_advert_entry_state(const LLDLEP::DlepMac & rfId,
                                   DestAdvertDBEntry::EntryState newstate);
private:

    // Documentation for override methods is in the base class.

    void handle_message(DlepMessageBuffer msg_buffer,
                        unsigned int msg_buffer_len,
                        boost::asio::ip::udp::endpoint from_endpoint) override;

    DlepMessageBuffer get_message_to_send(unsigned int * msg_len) override;

    /// Start the destination advertisement database purge timer.
    void start_purge_advert_timer();

    /// Service a destination advertisement database purge timeout.
    void handle_purge_advert_timeout(const boost::system::error_code & error);

    /// Time when this object was constructed.  Used for computing the uptime
    /// field of the destination advertisement.
    time_t begin_time;

    /// Sequence number for destination advertisements
    std::uint32_t seq_num;

    /// Wait time between sending destination advertisements
    const std::uint32_t send_interval;

    /// RF_ID of the local OTA interface for destination advertisements
    const LLDLEP::DlepMac local_rfid;

    /// any local destinations to put in destination advertisements,
    /// including the peer router mac
    LLDLEP::DlepMacAddrs destinations;

    /// mutex that should be locked whenever dest_advert_db is accessed
    boost::recursive_mutex dest_advert_mutex;

    /// mapping of RF_ID's to advertisement data
    DestAdvertDB dest_advert_db;

    /// Timer for purging destination advertisements
    boost::asio::deadline_timer purge_advert_timer;
};

} // namespace internal
} // namespace LLDLEP

#endif // DEST_ADVERT_H
