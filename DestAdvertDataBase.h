/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016, 2018 Massachusetts Institute of Technology
 *
 * Contributor: Adjacent Link LLC, Bridgewater, NJ
 */


#ifndef DEST_ADVERT_DATABASE_H
#define DEST_ADVERT_DATABASE_H

#include <cstdint>
#include <string>
#include <sstream>
#include <map>
#include "DestAdvertInfo.h"

namespace LLDLEP
{
namespace internal
{

struct DestAdvertDBEntry
{
    /// time that this entry was created or last updated
    time_t   timestamp;

    /// state of this entry
    enum class EntryState
    {
        /// client says RF ID for this entry is down
        down,

        /// client says RF ID for this entry is up
        up
    };

    EntryState estate;

    /// Is this a placeholder entry?  When true, it means that the
    /// client has declared the entry up, but we do not (yet) have
    /// a destination advertisement from this RF ID.
    bool placeholder;

    /// information from the DestAdvert message
    DestAdvertInfo  info;

    /// client-supplied metrics for this RF ID when it
    /// was declared up, or when last updated
    LLDLEP::DataItems data_items;

    DestAdvertDBEntry() :
        timestamp {0},
              estate {EntryState::down},
              placeholder {false}
    { }

    DestAdvertDBEntry(time_t ts,
                      EntryState s,
                      bool p,
                      const DestAdvertInfo & e,
                      const LLDLEP::DataItems & di) :
        timestamp {ts},
              estate {s},
              placeholder {p},
              info {e},
              data_items {di}
    { }


    std::string to_string() const
    {
        std::stringstream ss;

        ss << " age="  << time(nullptr) - timestamp;
        ss << " state=" << (int)estate;
        ss << " placeholder=" << placeholder << " ";
        ss << info.to_string();
        return ss.str();
    }
};

/// map from RF ID (DlepMac) to an entry
using DestAdvertDB = std::map<LLDLEP::DlepMac, DestAdvertDBEntry>;

} // namespace internal
} // namespace LLDLEP

#endif
