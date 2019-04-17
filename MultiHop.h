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

const std::string Hop_Count   = "Hop_Count";
const std::string Hop_Control = "Hop_Control";

// extension name string
//
// string must match the module name defined in the protocol xml file
const std::string Multi_Hop = "Multi-Hop";

} // namespace ProtocolStrings

/// Flag definitions for the Hop_Count
enum HopCountFlags : std::uint8_t
{
  none = 0,           ///< no flags set
  p_bit = (1 << 7)    ///< P-bit
};
} // namespace LLDLEP
#endif // MULTI_HOP_H
