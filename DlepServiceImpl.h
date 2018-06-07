/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016, 2018 Massachusetts Institute of Technology
 */

/// @file
/// DLEP service implementation class declaration.

#ifndef DLEP_SERVICE_IMPL_H
#define DLEP_SERVICE_IMPL_H

#include "DlepService.h" // for base class
#include "Dlep.h" // for DlepPtr

namespace LLDLEP
{
namespace internal
{

/// DLEP service implementation class declaration.
class DlepServiceImpl : public LLDLEP::DlepService
{
public:
    DlepServiceImpl(DlepPtr dlep, DlepLoggerPtr logger);

    // exception class thrown by constructor when initialization fails
    struct InitializationError : public std::runtime_error
    {
        explicit InitializationError(const std::string & m) :
            std::runtime_error(m) { }
    };

    ReturnStatus destination_up(const LLDLEP::DlepMac & mac_address,
                                const LLDLEP::DataItems & data_items) override;

    ReturnStatus destination_update(const LLDLEP::DlepMac & mac_address,
                                    const LLDLEP::DataItems & data_items) override;

    ReturnStatus destination_down(const LLDLEP::DlepMac & mac_address) override;

    ReturnStatus peer_update(const LLDLEP::DataItems & data_items) override;

    ReturnStatus get_peers(std::vector<std::string> & peers) override;

    ReturnStatus get_peer_info(const std::string & peer_id,
                               LLDLEP::PeerInfo & peer_info) override;

    ReturnStatus get_destination_info(const std::string & peer_id,
                                      const LLDLEP::DlepMac & mac_address,
                                      LLDLEP::DestinationInfo & dest_info) override;

    LLDLEP::ProtocolConfig * get_protocol_config() override;

    ReturnStatus linkchar_request(const DlepMac & mac_address,
                                  const DataItems & data_items) override;
    ReturnStatus linkchar_reply(const std::string & peer_id,
                                const DlepMac & mac_address,
                                const DataItems & data_items) override;

    void terminate() override;

private:
    // Pointer to the top-level dlep object so we can get to everything we
    // need when called from the client's thread.
    DlepPtr dlep;

    DlepLoggerPtr logger;

    // Pointer to the top-level thread that the library creates for its
    // internal operations
    Thread init_thread;
};

} // namespace internal
} // namespace LLDLEP

#endif // DLEP_SERVICE_IMPL_H
