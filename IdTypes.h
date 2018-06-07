/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2013 Massachusetts Institute of Technology
 */

/// @file
/// Types for various kinds of DLEP IDs.
///
/// These are NOT the types that are used on the wire, i.e., in the
/// actual protocol.  The wire representation varies according to the
/// DLEP draft and is handled via configuration; see ProtocolConfig.
/// These types are used internally and in the library interface to
/// represent the IDs.  They should be large enough to accommodate the
/// largest expected wire representation for their respective ID type.

#ifndef IDTYPES_H
#define IDTYPES_H

namespace LLDLEP
{

typedef std::uint32_t SignalIdType;
typedef std::uint32_t DataItemIdType;
typedef std::uint32_t ExtensionIdType;
typedef std::uint32_t StatusCodeIdType;

// value to use if an ID is unknown or undefined
constexpr std::uint32_t IdUndefined = ~0U;

}  // namespace LLDLEP

#endif // IDTYPES_H
