/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016 Massachusetts Institute of Technology
 */

/// @file
/// DLEP Service library entry point

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <boost/lexical_cast.hpp>
#include "Dlep.h"
#include "DlepInit.h"
#include "DlepServiceImpl.h"
#include "DlepClient.h"

using namespace std;

namespace LLDLEP
{

DlepService *
DlepInit(DlepClient & dlep_client)
{
    ostringstream msg;

    // Set up the logger that will be used by this instance of
    // DlepService.  This pointer will be passed to the constructors
    // of many of the other classes so that they can all use the same
    // logger without needing a global variable.  It is important
    // that the logger always be named "logger" everywhere because
    // that's what the LOG macro expects.

    internal::DlepLoggerPtr logger(new internal::DlepLogger());

    try
    {
        std::string logfile;
        dlep_client.get_config_parameter("log-file", &logfile);
        logger->set_log_file(logfile.c_str());

        unsigned int log_level;
        dlep_client.get_config_parameter("log-level", &log_level);
        logger->set_run_level(log_level);
    }
    catch (LLDLEP::DlepClient::BadParameterName & bpn)
    {
        // The log file isn't set up, so print the message to stderr.
        std::cerr << bpn.what() << std::endl;
        return nullptr;
    }

    // Now that logging is set up, fetch some more parameters we need
    // to get started.  If they're missing, the error will go to the
    // log file.

    std::string local_type;

    try
    {
        dlep_client.get_config_parameter("local-type", &local_type);
    }
    catch (LLDLEP::DlepClient::BadParameterName & bpn)
    {
        msg << bpn.what();
        LOG(DLEP_LOG_ERROR, msg);
        return nullptr;
    }

    DlepService * dlep_service = nullptr;
    try
    {
        bool dlep_modem = (local_type == "modem");

        internal::DlepPtr dlep_ptr(new internal::Dlep(dlep_modem,
                                                      dlep_client, logger));
        dlep_service = new internal::DlepServiceImpl(dlep_ptr, logger);
    }
    catch (internal::DlepServiceImpl::InitializationError)
    {
        return nullptr;
    }

    return dlep_service;
}

} // namespace LLDLEP
