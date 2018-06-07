/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016 Massachusetts Institute of Technology
 */

/// @file
/// This is the abstract interface that the DLEP client (library user)
/// must present to the DLEP service (library).
/// I.e., the DLEP client implements this interface and the
/// DLEP service calls it.
/// Clients should include this file indirectly by including DlepInit.h.

#ifndef DLEP_CLIENT_H
#define DLEP_CLIENT_H

#include <boost/variant.hpp>
#include <boost/asio.hpp> // for ip::address
#include <vector>
#include "DlepCommon.h"

namespace LLDLEP
{

/// This is the abstract interface that the DLEP client (library user)
///  must present to the DLEP service (library).
///
/// At library initialization time (DlepInit), the client passes a
/// pointer to an object that implements the DlepClient interface.
/// The DLEP library calls these methods to get information from the
/// client or notify the client of events related to the DLEP
/// protocol.  The DLEP client subclasses from this class and
/// implements all of the required methods.  The DLEP library calls
/// these methods from its own internal threads, distinct from the
/// client's threads.  The client must be prepared for this, possibly
/// implementing mutex mechanisms as appropriate.
class DlepClient
{
public:

    /// Possible types for configuration parameter values.
    typedef boost::variant < bool,
            unsigned int,
            std::string,
            boost::asio::ip::address,
            std::vector<unsigned int>
            > ConfigValue;

    // There is an overloaded get_config_parameter method for each parameter
    // type that can be returned.  I could probably do this more elegantly
    // with templates.

    /// Get a configuration parameter of any type defined in ConfigValue.
    /// @param[in] parameter_name
    ///            the name of the parameter to retrieve
    /// @param[out] value
    ///             place to store the parameter's value
    /// @throw BadParameterName if parameter_name does not exist
    virtual void get_config_parameter(const std::string & parameter_name,
                                      ConfigValue * value) = 0;

    /// Get a boolean configuration parameter.
    /// @param[in] parameter_name
    ///            the name of the parameter to retrieve
    /// @param[out] value
    ///             place to store the parameter's value
    /// @throw BadParameterName if parameter_name does not exist
    virtual void get_config_parameter(const std::string & parameter_name,
                                      bool * value) = 0;

    /// Get an unsigned int configuration parameter.
    /// @param[in] parameter_name
    ///            the name of the parameter to retrieve
    /// @param[out] value
    ///             place to store the parameter's value
    /// @throw BadParameterName if parameter_name does not exist
    virtual void get_config_parameter(const std::string & parameter_name,
                                      unsigned int * value) = 0;

    /// Get a string configuration parameter.
    /// @param[in] parameter_name
    ///            the name of the parameter to retrieve
    /// @param[out] value
    ///             place to store the parameter's value
    /// @throw BadParameterName if parameter_name does not exist
    virtual void get_config_parameter(const std::string & parameter_name,
                                      std::string * value) = 0;

    /// Get an IP address (v4 or v6) configuration parameter.
    /// @param[in] parameter_name
    ///            the name of the parameter to retrieve
    /// @param[out] value
    ///             place to store the parameter's value
    /// @throw BadParameterName if parameter_name does not exist
    virtual void get_config_parameter(const std::string & parameter_name,
                                      boost::asio::ip::address * value) = 0;

    /// Get a vector<unsigned int> configuration parameter.
    /// @param[in] parameter_name
    ///            the name of the parameter to retrieve
    /// @param[out] value
    ///             place to store the parameter's value
    /// @throw BadParameterName if parameter_name does not exist
    virtual void get_config_parameter(const std::string & parameter_name,
                                      std::vector<unsigned int> * value) = 0;

    /// Exception class for get_config_parameter methods.
    class BadParameterName : public std::runtime_error
    {
    public:
        /// Name of the parameter that did not exist.
        std::string parameter_name;
        explicit BadParameterName(const std::string & pn) :
            runtime_error("Missing value for parameter " + pn),
            parameter_name(pn) {}
    };

    /// Notify the client that a new peer session is up.
    /// @param[in] peer_info
    ///            contains detailed information about the peer
    virtual void peer_up(const LLDLEP::PeerInfo & peer_info) = 0;

    /// Notify the client that a peer has been updated.
    /// @param[in] peer_id
    ///            uniquely identifies the peer
    /// @param[in] data_items
    ///            Data items that have changed for this peer
    virtual void peer_update(const std::string & peer_id,
                             const DataItems & data_items) = 0;

    /// Notify the client that the peer session is down.
    /// @param[in] peer_id
    ///            uniquely identifies the peer
    virtual void peer_down(const std::string & peer_id) = 0;

    /// Notify the client that a new destination from the peer is up.
    /// @param[in] peer_id
    ///            the peer reporting on the destination
    /// @param[in] mac_address
    ///            the destination that is up.  If this represents a
    ///            multicast address, the modem should begin sending
    ///            traffic for this address to the router.
    /// @param[in] data_items
    ///            data_items associated with the destination
    ///
    /// @return
    /// A ProtocolString from ProtocolConfig.h specifying the status to
    /// put in the Destination Up Response message.  Plausible values:
    /// - "" (empty string) to omit sending status (interpreted as Success)
    /// - ProtocolStrings::Success if all is well
    /// - ProtocolStrings::Not_Interested to tell the peer to refrain
    ///   from sending further messages about this \p mac_address
    virtual std::string destination_up(const std::string & peer_id,
                                       const DlepMac & mac_address,
                                       const DataItems & data_items) = 0;

    /// Notify the client that an existing destination's
    /// attributes have changed.
    ///
    /// @param[in] peer_id
    ///            the peer reporting on the destination
    /// @param[in] mac_address
    ///            the destination that is being updated
    /// @param[in] data_items
    ///            data_items associated with the destination
    virtual void destination_update(const std::string & peer_id,
                                    const DlepMac & mac_address,
                                    const DataItems & data_items) = 0;

    /// Notify the client that an existing destination from the peer is down.
    /// @param[in] peer_id
    ///            the peer reporting on the destination
    /// @param[in] mac_address
    ///            the destination that is down.  If this represents a
    ///            multicast address, the modem should cease sending
    ///            traffic for this address to the router.
    virtual void destination_down(const std::string & peer_id,
                                  const DlepMac & mac_address) = 0;

    /// Request that the client establish specific link characteristics
    /// for a destination.
    ///
    /// This is called when a Link Characteristics Request signal is
    /// received with a non-empty list of metrics.  If a Link
    /// Characteristics Request signal is received that contains an
    /// empty list of metrics, it is interpreted as a request for all
    /// of the current metrics for \p mac_address.  In this case, the
    /// response is sent automatically by DlepService without calling
    /// this method.
    ///
    /// See DlepClient::linkchar_reply() for the complete sequence of events
    /// in a link characteristics request/response transacation.
    ///
    /// @param[in] peer_id
    ///            the peer making the request
    /// @param[in] mac_address
    ///            the destination for which link characteristics are requested.
    ///            This must be a destination originating from the client
    ///            side, not from the peer.
    /// @param[in] data_items
    ///            Contains any subset of the metrics defined by the
    ///            Link Characteristics Request signal of DLEP draft
    ///            or any configured extensions.  The client should
    ///            reconfigure the link to \p mac_address to have
    ///            these requested metric values.  When the new metric
    ///            values have taken effect, the client inform the
    ///            peer by calling DlepService::linkchar_reply().
    ///
    virtual void linkchar_request(const std::string & peer_id,
                                  const DlepMac & mac_address,
                                  const DataItems & data_items) = 0;

    /// Notify the client that a Link Characteristics Response signal was
    /// received.
    ///
    /// This is the final API call in the Link Characteristics
    /// transaction.  The complete sequence looks like this:
    ///
    /// - Client calls DlepService::linkchar_request() to send the
    ///   request to the peer
    /// - Link Characteristics Request goes over the wire and into the
    ///   peer's DlepService
    /// - Peer is notified by a call to DlepClient::linkchar_request()
    /// - Peer processes the linkchar request to assign the new metrics
    /// - Peer calls DlepService::linkchar_reply() to send the response
    ///   to the client
    /// - Link Characteristics Response goes over the wire and into the
    ///   client's DlepService
    /// - Client is notified by a call to DlepClient::linkchar_reply()
    ///   (this method)
    ///
    /// @param[in] peer_id
    ///            the peer sending the response
    /// @param[in] mac_address
    ///            the destination to which the link characteristics in
    ///            \p data_items apply.
    ///            This must be a destination originating from the peer.
    /// @param[in] data_items
    ///            Contains either:
    ///            - a full set of metrics for \p mac_address, if the original
    ///              linkchar request contained no data items, or
    ///            - the same subset of metrics that appeared in the Link
    ///              Characteristics Request signal to which this is a response.
    ///              The metric values are updated to reflect the state after
    ///              the peer processed the Link Characteristics Request.
    ///              If the peer was unable to establish one or more of the
    ///              requested metrics, the list will include a Status data
    ///              item with the status code Request Denied.
    virtual void linkchar_reply(const std::string & peer_id,
                                const DlepMac & mac_address,
                                const DataItems & data_items) = 0;

protected:
    DlepClient() {};
    virtual ~DlepClient() {};
};

} // namespace LLDLEP

#endif // DLEP_CLIENT_H
