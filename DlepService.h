/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016 Massachusetts Institute of Technology
 */

/// @file
/// This is the abstract interface that the DLEP service
/// (library) must present to the DLEP client (library user).
/// I.e., the DLEP service implements this interface and the
/// DLEP client calls it.
/// Clients should include this file indirectly by including DlepInit.h.

#ifndef DLEP_SERVICE_H
#define DLEP_SERVICE_H

#include <vector>
#include "DlepCommon.h"
#include "ProtocolConfig.h"

namespace LLDLEP
{

/// This is the abstract interface that the DLEP service (library)
/// must present to the DLEP client (library user).
///
/// At library initialization time (DlepInit), the library returns a
/// pointer to an object that implements the DlepService interface.
/// The DLEP client calls these methods to provide information,
/// primarily about destinations, to the DLEP service.  The DLEP
/// service subclasses from this class and implements all of the
/// required methods.  The DLEP client calls these methods from its
/// own internal threads, distinct from the service's threads.  The
/// service must be prepared for this, possibly implementing mutex
/// mechanisms as appropriate.
class DlepService
{
public:
    /// Possible return values for our methods.
    enum class ReturnStatus
    {
        /// The operation completed successfully.
        ok,

        /// One or more of the data items were invalid.
        invalid_data_item,

        /// The MAC address was malformed or not recognized.
        invalid_mac_address,

        /// Attempt to add a destination MAC address that already exists.
        destination_exists,

        /// An operation was attempted on an unknown destination MAC address.
        destination_does_not_exist,

        /// An operation was attempted on an unknown peer id.
        peer_does_not_exist
    };

    /// Notify the DLEP service that a new destination is available.
    ///
    /// The DLEP service will issue a Destination Up message to the
    /// peer(s).  If there is no extant peer session, the DLEP service
    /// adds the destination to its local database so that it can be
    /// sent to a future peer.
    ///
    /// Actually, this method can send one of four different messages
    /// depending on the circumstances: Destination Up, Destination Up
    /// Response, Destination Announce, or Destination Announce
    /// Response.
    ///
    /// If the DLEP service is configured as a router (see the
    /// local-type configuration parameter), and the protocol
    /// configuration defines the Destination Announce message, then
    /// this method will generate a Destination Announce message
    /// instead of a Destination Up message.  The modem peer will be
    /// expected to send a Destination Announce Response message.
    ///
    /// If the DLEP service is configured as a modem, and the router
    /// has previously sent a Destination Announce or Destination Up
    /// message for this destination, then this method will send the
    /// corresponding response message (Destination Up Response or
    /// Destination Announce Response).  Otherwise, it will send
    /// Destination Up.
    ///
    /// The intention is that the client doesn't have to care what
    /// type of DLEP message is generated underneath.  It just
    /// declares destinations to be up as appropriate, and DlepService
    /// takes care of sending the right message.
    ///
    /// @param[in] mac_address
    ///            the destination that is up
    /// @param[in] data_items
    ///            data items (metrics and IP addresses) associated with
    ///            the destination
    ///
    /// @return ok if no error, else as described in DlepService::ReturnStatus
    virtual ReturnStatus destination_up(const DlepMac & mac_address,
                                        const DataItems & data_items) = 0;

    /// Notify the DLEP service that an existing destination's
    /// attributes have changed.
    ///
    /// The DLEP service will issue a Destination Update signal to the
    /// peer(s).  If there is no extant peer session, the DLEP service
    /// records the new attributes for the destination in its local
    /// database so that it can be sent to a future peer.
    ///
    /// If the client has never declared the destination
    /// mac_address up via DlepService::destination_up(), then the
    /// destination is assumed to belong to a peer.  The DLEP
    /// service will send the request to the peer that declared
    /// the destination to be up if one exists.  In this case, the
    /// specified data items are NOT stored locally, since it is
    /// not a locally-owned destination.
    ///
    /// @param[in] mac_address
    ///            the destination being updated
    /// @param[in] data_items
    ///            data items (metrics and IP addresses) associated with
    ///            the destination
    ///
    /// @return ok if no error, else as described in DlepService::ReturnStatus
    virtual ReturnStatus destination_update(const DlepMac & mac_address,
                                            const DataItems & data_items) = 0;

    /// Notify the DLEP service that a previously existing destination
    /// is down.
    ///
    /// The DLEP service will issue a Destination Down for the given
    /// destination to the peer.  If there is no extant peer session,
    /// the DLEP service library removes the destination from its
    /// local database.  A future peer will not see this destination
    /// unless it is subsequently re-added via destination_up().
    ///
    /// @param[in] mac_address
    ///            the destination that is down
    ///
    /// @return ok if no error, else as described in DlepService::ReturnStatus
    virtual ReturnStatus destination_down(const DlepMac & mac_address) = 0;

    /// Notify the DLEP service that the local peer's metrics or IP
    /// addresses have changed.
    ///
    /// The DLEP service will issue a Peer Update to the peer with the
    /// new information.  If there is no extant peer session, the DLEP
    /// protocol remembers the metrics/addresses so that they can be
    /// sent to a future peer.
    ///
    /// @param[in] data_items
    ///            data items (metrics and IP addresses) associated with
    ///            the local peer
    ///
    /// @return ok if no error, else as described in DlepService::ReturnStatus
    virtual ReturnStatus peer_update(const DataItems & data_items) = 0;

    /// Get a list of all existing peer ids.
    ///
    /// @param[out] peers
    ///             Upon return, this will be filled in with the peer ids
    /// @return ok if no error, else as described in DlepService::ReturnStatus
    virtual ReturnStatus get_peers(std::vector<std::string> & peers) = 0;

    /// Get detailed information about a peer.
    ///
    /// @param[in]  peer_id
    ///             the id of the peer for which information is requested
    /// @param[out] peer_info
    ///             Upon successful return, this will be filled in with
    ///             information about the given \p peer_id
    /// @return
    /// - ok if no error
    /// - peer_does_not_exist if peer_id does not name an existing peer
    virtual ReturnStatus get_peer_info(const std::string & peer_id,
                                       LLDLEP::PeerInfo & peer_info) = 0;

    /// Get detailed information about a destination.
    ///
    /// @param[in] peer_id
    ///            the id of the peer to which the destination belongs
    /// @param[in] mac_address
    ///            the destination for which information is requested
    /// @param[out] dest_info
    ///             Upon successful return, this will be filled in with
    ///             information about the destination \p mac_address
    /// @return
    /// - ok if no error
    /// - peer_does_not_exist if peer_id does not name an existing peer
    /// - destination_does_not_exist if mac_address was not a destination
    ///   known to peer_id
    virtual ReturnStatus get_destination_info(const std::string & peer_id,
                                              const DlepMac & mac_address,
                                              LLDLEP::DestinationInfo & dest_info) = 0;

    /// Get the ProtocolConfig object, enabling the client to
    /// access protocol configuration information, e.g. supported
    /// metrics.  See the LLDLEP::ProtocolConfig class for
    /// details.
    ///
    /// @return a pointer to the ProtocolConfig object.  The client
    /// can use this pointer until this DlepService instance terminates.
    virtual LLDLEP::ProtocolConfig * get_protocol_config() = 0;

    /// Tell the DLEP service to send a Link Characteristics Request.
    ///
    /// The DLEP service will issue a Link Characteristics Request to the
    /// peer that declared the destination to be up.
    ///
    /// See DlepClient::linkchar_reply() for the complete sequence of events
    /// in a link characteristics request/reply transacation.
    ///
    /// @param[in] mac_address
    ///            the destination whose link characteristics are being
    ///            requested
    /// @param[in] data_items
    ///            data items (metrics) associated with \p mac_address.
    ///            If empty, this is a request for the peer to send the
    ///            current values of all metrics.  Otherwise, it is a
    ///            request for the peer to "make it so": the given metric
    ///            values should be established as the real metrics for
    ///            the given destination.
    /// @return
    /// - ok if no error
    /// - peer_does_not_exist if no peer could be found for
    ///   destination \p mac_address
    virtual ReturnStatus linkchar_request(const DlepMac & mac_address,
                                          const DataItems & data_items) = 0;

    /// Tell the DLEP service to send a Link Characteristics Response.
    ///
    /// The DLEP service will issue a Link Characteristics Response to
    /// the specified peer.  A DLEP client must call this in response
    /// to a Link Characteristics Request, i.e., after its
    /// DlepClient::linkchar_request() method is called.  Separating
    /// the linkchar request and reply allows the client to do
    /// arbitrary processing in between (for example, to make the new
    /// link characteristics take effect) without holding up the Dlep
    /// protocol processing thread.
    ///
    /// See DlepClient::linkchar_reply() for the complete sequence of events
    /// in a link characteristics request/reply transacation.
    ///
    /// @param[in] peer_id
    ///            the peer to send the reply to.  This is one of the
    ///            few places that DlepService requires a peer id.
    ///            The reason is that the sending of the ack (reply)
    ///            signal is usually handled internally by
    ///            DlepService, but in this case the generation of the
    ///            reply is being initiated by the client.  So the
    ///            client needs to know which peer to send it to,
    ///            since there can be multiple peers.  The client
    ///            knows which peer id should be replied to because
    ///            DlepService provides it to the client in the method
    ///            DlepClient::linkchar_request().
    /// @param[in] mac_address
    ///            the destination whose link characteristics are being
    ///            supplied
    /// @param[in] data_items
    ///            data items (metrics) associated with \p mac_address
    ///
    /// @return
    /// - ok if no error
    /// - peer_does_not_exist if peer_id does not name an existing peer
    virtual ReturnStatus linkchar_reply(const std::string & peer_id,
                                        const DlepMac & mac_address,
                                        const DataItems & data_items) = 0;

    /// Tell the DLEP service to terminate all operations.
    ///
    /// This should be done before the user of the DLEP service
    /// library exits, to enable a clean shutdown the library's
    /// threads, peer sessions, etc.
    virtual void terminate() = 0;

    virtual ~DlepService() {};

protected:
    DlepService() {};
};

} // namespace LLDLEP

#endif // DLEP_SERVICE_H
