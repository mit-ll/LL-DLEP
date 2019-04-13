/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2018 Massachusetts Institute of Technology
 * Modified by LabN Consulting, L.L.C.
 */

/// @file
/// Declarations for the Multi Hop extension

#ifndef MULTI_HOP_H
#define MULTI_HOP_H

namespace LLDLEP
{
namespace ProtocolStrings
{

// data item strings

const std::string Multi_Hop = "Multi_Hop";

} // namespace ProtocolStrings
} // namespace LLDLEP

/// Flag definitions for the Hop_Count
enum HopCountFlags : std::uint8_t
{
  none = 0,           ///< no flags set
  p_bit = (1 << 7)    ///< P-bit
};
#endif // MULTI_HOP_H
