/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2013, 2015, 2016 Massachusetts Institute of Technology
 */
#ifndef _INFO_BASE_MGR_
#define _INFO_BASE_MGR_

#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <strings.h>
#include <time.h>
#include <map>
#include <vector>
#include <iterator>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <boost/thread/recursive_mutex.hpp>

#include "Dlep.h"
#include "DlepLogger.h"
#include "DataItem.h"

namespace LLDLEP
{
namespace internal
{

class Dlep;
class Peer;

// Map data item ID to its value
typedef std::map<LLDLEP::DataItemIdType, LLDLEP::DataItem> DataItemMap;

/// Information about a destination
class DestinationData
{
public:
    /// Constructor.
    /// @param[in] mac  MAC address of new destination
    /// @param[in] initial_data_items  data items associated with mac
    /// @param[in] dlep back-pointer to dlep instance
    DestinationData(const LLDLEP::DlepMac & mac,
                    const LLDLEP::DataItems & initial_data_items,
                    DlepPtr dlep);
    ~DestinationData();

    /// Record updated data items for this destination.
    /// @param[in] updates    new data items to record
    /// @param[in] tell_peers should we send peers a DestinationUpdate signal
    ///                       with these updates?
    /// @return the number of updates recorded
    unsigned int update(const LLDLEP::DataItems & updates, bool tell_peers);

    /// Get a list of all of this destination's current data items.
    /// @param[out] data_items_out vector to store the data items in
    void get_all_data_items(LLDLEP::DataItems & data_items_out) const;

    /// Get a list of all of this destination's data items containing
    /// IP addresses.
    LLDLEP::DataItems get_ip_data_items() const;

    /// Log information about this destination.
    /// @param[in] prefix    string to put at the beginning of the log message
    /// @param[in] log_level log level to use for LOG
    void log(const std::string & prefix, int log_level) const;

    void needs_response(const std::string & response_name);
    std::string needs_response() const;

    /// Search for an IP address on this destination.
    /// @param[in] ip_data_item
    ///            data item containing an IP address to search for in this
    ///            destination's IP addresses
    /// @return "" if not found, else a non-empty string identifying this
    ///         destination.
    std::string find_ip_data_item(const DataItem & ip_data_item) const;

private:
    /// MAC address of this destination
    LLDLEP::DlepMac mac_address;

    /// Metric data items associated with this destination.  Only one
    /// data item per metric type is allowed, so this is a map.
    DataItemMap metric_data_items;

    /// IP address data items associated with this destination.
    /// There can be multiple IP addresses of each type, so this
    /// is a vector.
    DataItems ip_data_items;

    /// Is the peer expecting a Destination Announce Response or Destination
    /// Up Response for this destination?
    std::string needs_response_;

    /// back-pointer to dlep instance
    DlepPtr dlep;

    /// logger, for LOG macro
    DlepLoggerPtr logger;
};

typedef boost::shared_ptr<DestinationData> DestinationDataPtr;

class PeerData
{
public:
    /// Constructor.
    /// @param[in] id   peer id
    /// @param[in] initial_data_items  data items associated with this peer
    /// @param[in] dlep back-pointer to dlep instance
    PeerData(const std::string & id, const LLDLEP::DataItems & initial_data_items,
             DlepPtr dlep);
    ~PeerData();

    bool addDestination(const LLDLEP::DlepMac & mac,
                        const LLDLEP::DataItems & initial_data_items,
                        bool tell_peers);
    void sendAllDestinations(boost::shared_ptr<Peer> peer);
    bool updateDestination(const LLDLEP::DlepMac & mac,
                           const LLDLEP::DataItems & updates,
                           bool tell_peers);
    bool removeDestination(const LLDLEP::DlepMac & mac, bool tell_peers);
    bool getDestinationData(const LLDLEP::DlepMac & mac, DestinationDataPtr * ddpp);
    void logDestinations(bool include_metrics);
    void getDestinations(std::vector<LLDLEP::DlepMac> & destinations);

    bool validDestination(const LLDLEP::DlepMac & mac);

    std::string update_data_items(const LLDLEP::DataItems & updates,
                                  bool tell_peers);
    LLDLEP::DataItems get_data_items();

    /// Get a list of all of this peer's data items containing
    /// IP addresses.
    LLDLEP::DataItems get_ip_data_items() const;

    void log_data_items();
    void needs_response(const LLDLEP::DlepMac & mac, const std::string & response_name);
    std::string needs_response(const LLDLEP::DlepMac & mac);

    /// Search for an IP address on this peer.
    /// @param[in] ip_data_item
    ///            data item containing an IP address to search for in this
    ///            peer's IP addresses
    /// @return "" if not found, else a non-empty string identifying where the
    ///         IP address was found: this peer, or one of its destinations.
    std::string find_ip_data_item(const DataItem & ip_data_item) const;

private:
    /// peer_id is a combination of the TCP remote IP and port of the session
    std::string peer_id;

    /// Map destination mac to Destination Data
    std::map<LLDLEP::DlepMac, DestinationDataPtr> destination_data;

    /// Metric data items that this peer supports, and their default
    /// values.  Only one data item per metric type is allowed, so
    /// this is a map.
    DataItemMap metric_data_items;

    /// IP address data items associated with this destination.
    /// There can be multiple IP addresses of each type, so this
    /// is a vector.
    DataItems ip_data_items;

    /// back-pointer to dlep instance
    DlepPtr dlep;

    /// logger, for LOG macro
    DlepLoggerPtr logger;
};

typedef boost::shared_ptr<PeerData> PeerDataPtr;


class InfoBaseMgr
{
public:
    explicit InfoBaseMgr(DlepPtr dlep);
    ~InfoBaseMgr();

    PeerDataPtr addPeer(const std::string & peer_id,
                        const LLDLEP::DataItems & initial_values);
    bool removePeer(const std::string & peer_id);
    bool getPeerData(const std::string & peer_id, PeerDataPtr * pdpp);
    void logInfoBase(bool include_metrics);

    bool validPeer(const std::string & peer_id);
    bool validDestination(const std::string & peer_id, const LLDLEP::DlepMac & mac);

private:

    // Peers, by peer ID
    std::map<std::string, PeerDataPtr> peer_data;

    DlepPtr dlep;
    DlepLoggerPtr logger;
};

typedef boost::shared_ptr<InfoBaseMgr> InfoBaseMgrPtr;

} // namespace internal
} // namespace LLDLEP

#endif // _INFO_BASE_MGR_
