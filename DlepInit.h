/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016 Massachusetts Institute of Technology
 */

/// @file
/// DLEP Service library entry point declaration

#ifndef DLEP_INIT_H
#define DLEP_INIT_H

#include "DlepClient.h"
#include "DlepService.h"

/// All client-visible identifiers necessary to use the DLEP Service Library
/// are in this namespace.
/// If you find it necessary to use identifiers in the Dlep source that are
/// not in this namespace, you are most likely doing something wrong.
namespace LLDLEP
{
/// Main entry point into the DLEP Service library.
///
/// DLEP service library users call this function first to start the
/// DLEP service.  After performing some initialization, this function
/// returns, but one or more threads remain active in the library to
/// manage the DLEP protocol.
///
/// @param[in] dlep_client
///            used for the library to communicate to the caller
///
/// @return
/// - nullptr if the library failed to initialize
/// - otherwise, a pointer to a DlepService object that the caller
///   uses to communicate to the DLEP service library
DlepService * DlepInit(DlepClient & dlep_client);

} // namespace LLDLEP

#endif // DLEP_INIT_H
