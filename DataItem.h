/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016 Massachusetts Institute of Technology
 */

/// @file
/// DLEP Data Item class and supporting declarations
///
/// DataItem uses boost::variant as a type-safe union to hold all of
/// the possible types of data item values.  This implementation aims
/// to support multiple DLEP drafts, so we need to support all of the
/// different variations of data item values that have appeared in any
/// draft.  Unfortunately, boost::variant only supports 20 different
/// types by default, and there are more than 20 different data item
/// types.  Fortunately, boost::variant provides a way to increase the
/// number of types it can handle by defining some preprocessor
/// symbols:
///
/// - -DBOOST_MPL_CFG_NO_PREPROCESSED_HEADERS
/// - -DBOOST_MPL_LIMIT_LIST_SIZE=30
/// - -DBOOST_MPL_LIMIT_VECTOR_SIZE=30
///
/// ALL CODE THAT USES THE DLEP LIBRARY MUST DEFINE THESE PREPROCESSOR SYMBOLS.
///
/// The safest way to define them is on the compiler command line to
/// make sure they're defined from the very start.  You can try
/// putting them in a header or directly in your .cpp file, but this
/// can fail if boost headers are included before the header that
/// defines these symbols.  That is why they are not defined in this
/// header.  Sometimes that would work, sometimes it wouldn't, and it
/// would depend on whether boost headers happened to be included before
/// this one, which is out of our control.
///
/// Use `pkg-config --cflags libdlep` in your build to avoid hardcoding
/// these defines.

#ifndef DATA_ITEM_H
#define DATA_ITEM_H

#include <cstdint>
#include <boost/asio/ip/address.hpp>
#include <boost/variant.hpp>
#include "DlepMac.h"
#include "Serialize.h"
#include "IdTypes.h"

namespace LLDLEP
{

class ProtocolConfig;

/// Each possible data item value that isn't trivially representable
/// with a built-in type has a structure defined below.  These
/// structures do NOT represent the exact bits-on-the-wire form of the
/// data item; they are for internal use only.  DataItem.serialize()
/// converts these structures to wire format.
///
/// The field names in the structures are generic (field1, field2, etc.)
/// because each value type could be used for more than one data item
/// type.
///
/// The prefix Div on these type names stands for Data Item Value.

/// Status
struct Div_u8_string_t
{
    std::uint8_t field1;
    std::string field2;

    bool operator==(const Div_u8_string_t & other) const
    {
        return (field1 == other.field1) && (field2 == other.field2);
    }
};

/// IPv4 Address
struct Div_u8_ipv4_t
{
    std::uint8_t field1;
    boost::asio::ip::address_v4 field2;

    bool operator==(const Div_u8_ipv4_t & other) const
    {
        return (field1 == other.field1) && (field2 == other.field2);
    }
};

/// IPv4 Attached Subnet (draft 8)
struct Div_ipv4_u8_t
{
    boost::asio::ip::address_v4 field1;
    std::uint8_t field2;

    bool operator==(const Div_ipv4_u8_t & other) const
    {
        return (field1 == other.field1) && (field2 == other.field2);
    }
};

/// IPv6 Address
struct Div_u8_ipv6_t
{
    std::uint8_t field1;
    boost::asio::ip::address_v6 field2;

    bool operator==(const Div_u8_ipv6_t & other) const
    {
        return (field1 == other.field1) && (field2 == other.field2);
    }
};

/// IPv6 Attached Subnet (draft 8)
struct Div_ipv6_u8_t
{
    boost::asio::ip::address_v6 field1;
    std::uint8_t field2;

    bool operator==(const Div_ipv6_u8_t & other) const
    {
        return (field1 == other.field1) && (field2 == other.field2);
    }
};

/// extension metric
struct Div_u64_u8_t
{
    std::uint64_t field1;
    std::uint8_t  field2;

    bool operator==(const Div_u64_u8_t & other) const
    {
        return (field1 == other.field1) && (field2 == other.field2);
    }
};

/// extension metric
struct Div_u16_vu8_t
{
    std::uint16_t field1;
    std::vector<std::uint8_t> field2;

    bool operator==(const Div_u16_vu8_t & other) const
    {
        return (field1 == other.field1) && (field2 == other.field2);
    }
};

/// Extensions Supported
struct Div_v_extid_t
{
    std::vector<ExtensionIdType> field1;

    bool operator==(const Div_v_extid_t & other) const
    {
        return (field1 == other.field1);
    }
};

/// IPv4 Connection Point
struct Div_u8_ipv4_u16_t
{
    std::uint8_t field1;
    boost::asio::ip::address_v4 field2;
    std::uint16_t field3;

    bool operator==(const Div_u8_ipv4_u16_t & other) const
    {
        return ((field1 == other.field1) &&
                (field2 == other.field2) &&
                (field3 == other.field3));
    }

};

/// IPv6 Connection Point
struct Div_u8_ipv6_u16_t
{
    std::uint8_t field1;
    boost::asio::ip::address_v6 field2;
    std::uint16_t field3;

    bool operator==(const Div_u8_ipv6_u16_t & other) const
    {
        return ((field1 == other.field1) &&
                (field2 == other.field2) &&
                (field3 == other.field3));
    }

};

/// IPv4 Attached Subnet (draft 17)
struct Div_u8_ipv4_u8_t
{
    std::uint8_t field1;
    boost::asio::ip::address_v4 field2;
    std::uint8_t field3;

    bool operator==(const Div_u8_ipv4_u8_t & other) const
    {
        return ((field1 == other.field1) &&
                (field2 == other.field2) &&
                (field3 == other.field3));
    }
};

/// IPv6 Attached Subnet (draft 17)
struct Div_u8_ipv6_u8_t
{
    std::uint8_t field1;
    boost::asio::ip::address_v6 field2;
    std::uint8_t field3;

    bool operator==(const Div_u8_ipv6_u8_t & other) const
    {
        return ((field1 == other.field1) &&
                (field2 == other.field2) &&
                (field3 == other.field3));
    }
};

/// Latency Range extension
struct Div_u64_u64_t
{
    std::uint64_t field1;
    std::uint64_t field2;

    bool operator==(const Div_u64_u64_t & other) const
    {
        return ((field1 == other.field1) &&
                (field2 == other.field2));
    }
};

/// Holds a data item value of any type.
///
/// If a new data item must be supported that has a value type that is
/// different from all of the existing ones, you will have to add a
/// new type to this variant.  In this case, you must also update:
/// - enum DataItemValueType below
/// - to/from_string support for the new DataItemValueType enum value
/// - any switch statements that use DataItemValueType as the control variable
/// - boost::variant visitor classes that use this variant (DataItem.cpp)
/// - DataItemValueType in config/protocol/protocol-config.xsd
/// - if the data item contains an IP address field,
///   ProtocolConfigImpl::is_ipaddr()
/// - add a test case to tests/dataitems.cpp
typedef boost::variant <
      boost::blank,
      std::uint8_t,
      std::uint16_t,
      std::uint32_t,
      std::uint64_t,
      std::vector<std::uint8_t>,
      // std::vector<std::uint16_t>,
      // std::vector<std::uint32_t>,
      // std::vector<std::uint64_t>,
      // std::array<std::uint8_t, 2>,
      std::array<std::uint16_t, 2>,
      // std::array<std::uint32_t, 2>,
      std::array<std::uint64_t, 2>,
      std::string,
      LLDLEP::DlepMac,
      Div_u8_string_t,
      Div_u8_ipv4_t,
      Div_ipv4_u8_t,
      Div_u8_ipv6_t,
      Div_ipv6_u8_t,
      Div_u64_u8_t,
      Div_u16_vu8_t,
      Div_v_extid_t,
      Div_u8_ipv4_u16_t,
      Div_u8_ipv6_u16_t,
      Div_u8_ipv4_u8_t,
      Div_u8_ipv6_u8_t,
      Div_u64_u64_t
      > DataItemValue;

/// DataItemValueType has one enum value for each type of value that
/// can go in the DataItemValue variant above.  The order of these
/// enum values must match precisely with the order of the types
/// declared in DataItemValue, else DataItem::get_type() will break.
enum class DataItemValueType
{
    blank,           ///< no value (like void)
    div_u8,          ///< unsigned 8 bit integer
    div_u16,         ///< unsigned 16 bit integer
    div_u32,         ///< unsigned 32 bit integer
    div_u64,         ///< unsigned 64 bit integer
    div_v_u8,        ///< variable length list of unsigned 8 bit integer
    div_a2_u16,      ///< array of 2 unsigned 16 bit integers
    div_a2_u64,      ///< array of 2 unsigned 64 bit integers
    div_string,      ///< string
    div_dlepmac,     ///< MAC address
    div_u8_string,   ///< unsigned 8 bit integer followed by string
    div_u8_ipv4,     ///< unsigned 8 bit integer followed by IPv4 address
    div_ipv4_u8,     ///< IPv4 address followed by unsigned 8 bit integer
    div_u8_ipv6,     ///< unsigned 8 bit integer followed by IPv6 address
    div_ipv6_u8,     ///< IPv6 address followed by unsigned 8 bit integer
    div_u64_u8,      ///< unsigned 64 bit int followed by unsigned 8 bit int
    div_u16_vu8,     ///< unsigned 16 bit integer followed by variable length
                     ///< list of unsigned 8 bit integer
    div_v_extid,     ///< variable length list of extension IDs
    div_u8_ipv4_u16, ///< unsigned 8 bit integer, followed by IPv4 address,
                     ///< followed by unsigned 16 bit integer
    div_u8_ipv6_u16, ///< unsigned 8 bit integer, followed by IPv6 address,
                     ///< followed by unsigned 16 bit integer
    div_u8_ipv4_u8,  ///< unsigned 8 bit integer, followed by IPv4 address,
                     ///< followed by unsigned 8 bit integer
    div_u8_ipv6_u8,  ///< unsigned 8 bit integer, followed by IPv6 address,
                     ///< followed by unsigned 8 bit integer
    div_u64_u64      ///< two unsigned 64 bit integers
};

/// @return string representation of the given data item value
std::string to_string(DataItemValueType);

/// @return data item value represented by the given string
DataItemValueType from_string(const std::string &);

/// internal representation of a data item id
typedef std::uint32_t DataItemIdType;

/// shorthand for a vector of DataItem
class DataItem;
typedef std::vector<DataItem> DataItems;

/// Holds one DLEP Data Item (TLV that goes in a signal/message)
class DataItem
{
public:
    DataItemIdType id;   ///< numeric id of this data item
    DataItemValue value; ///< value of this data item

    /// compare two DataItems
    bool operator==(const DataItem & other) const
    {
        return ((id == other.id) && (value == other.value));
    }

    // Constructors

    /// Default constructor.  Do not use.  It is only here because STL
    /// containers require it.
    explicit DataItem(const ProtocolConfig * protocfg = nullptr);

    /// Recommended constructor.
    ///
    /// @param[in] di_type
    ///            data item type, one of the data item strings listed in
    ///            LLDLEP::ProtocolStrings, or one added by an extension.
    ///            Alternatively, ProtocolConfig::get_data_item_info()
    ///            returns a list of all configured data items; the value
    ///            of the field ProtocolConfig::DataItemInfo::name can be
    ///            used for this parameter.
    /// @param[in] di_value
    ///            initial value of the data item
    /// @param[in] protocfg
    ///            protocol configuration object, usually obtained from
    ///            DlepService::get_protocol_config().  Do not use nullptr.
    /// @throw ProtocolConfig::BadDataItemName if \p di_type is not a valid
    ///        data item name

    DataItem(const std::string & di_type, const DataItemValue & di_value,
             const ProtocolConfig * protocfg);

    /// Alternate constructor.
    /// This defaults the data item's value, so you should set the value
    /// after the DataItem is constructed.
    ///
    /// @param[in] di_type
    ///            data item type, one of the data item strings listed in
    ///            LLDLEP::ProtocolStrings, or one added by an extension.
    ///            Alternatively, ProtocolConfig::get_data_item_info()
    ///            returns a list of all configured data items; the value
    ///            of the field ProtocolConfig::DataItemInfo::name can be
    ///            used for this parameter.
    /// @param[in] protocfg
    ///            protocol configuration object, usually obtained from
    ///            DlepService::get_protocol_config().  Do not use nullptr.
    /// @throw ProtocolConfig::BadDataItemName if \p di_type is not a valid
    ///        data item name
    DataItem(const std::string & di_type, const ProtocolConfig * protocfg);

    /// @return the type of the currently-stored value
    DataItemValueType get_type() const;

    /// @return the serialized (wire-ready byte stream) form of this data
    ///         item, using information from the protocfg object supplied
    ///         at construction time
    std::vector<std::uint8_t> serialize() const;

    /// Deserialize a byte stream into this data item.  Complement of
    /// serialize().  Modifies the data item's id and value.  Uses
    /// information from the protocfg object supplied at construction
    /// time.
    ///
    /// @param[in,out] it
    ///                start of bytes to deserialize from.  This method
    ///                updates the iterator to reflect all of the consumed
    ///                bytes.
    /// @param[in] it_end
    ///                marks the end of the buffer (could be past the end
    ///                of this data item).  deserialization will not cross
    ///                this boundary.
    /// @throw std::length_error if there was a problem with the data item
    ///        length, e.g., the data item needed more bytes beyond \p it_end
    void deserialize(std::vector<std::uint8_t>::const_iterator & it,
                     std::vector<std::uint8_t>::const_iterator it_end);

    /// @return string representation of this data item.
    /// The string will contain both the name and the value.
    std::string to_string() const;

    /// @return string representation of this data item's name.
    std::string name() const;

    /// @return string representation of this data item's value.
    std::string value_to_string() const;

    /// Convert from string representation.  Use the current type of
    /// the value member to decide what kind of value to parse.  Store
    /// the result in value.  This is the inverse function of
    /// value_to_string(), NOT of to_string().
    /// @param[in] val_str string to convert from.
    void from_string(const std::string & val_str);

    /// Check this data item for validity.  The data item value's type
    /// must be correct for its id, and any restrictions on the value
    /// must be met.  Uses information from the protocfg object
    /// supplied at construction time.
    ///
    /// @return empty string "" if the data item is valid, else a string
    ///         description of why it is invalid
    std::string validate() const;

    /// Compare the IP address information in this data item with
    /// another data item.  IP address information includes IP addresses
    /// and, if present, subnet sizes.
    ///
    /// @param[in] other
    ///            the other data item to compare this one to
    ///
    /// @return true if the IP address information in the two data items is
    ///         the same, else false.
    ///         Also returns false when:
    ///         - comparing two data items that have different
    ///           data item value types
    ///         - comparing data items that do not contain IP addresses
    bool ip_equal(const DataItem & other) const;

    /// Flag definitions for Flags field of data items with IP addresses
    enum IPFlags : std::uint8_t
    {
        none = 0,       ///< no flags set
        add = (1 << 0), ///< IP address is being added, not dropped
    };

    /// @return the IPFlags associated with this data item.  If the data
    ///         item does not hold an IP address, returns IPFlags::none.
    IPFlags ip_flags() const;

    /// Search for the IP address in this data item.
    /// @param[in] search_data_items
    ///            vector of data items to search
    /// @return an iterator into search_data_items indicating where this
    ///         IP address data item was found.  If not found, returns
    ///         std::end(search_data_items)
    DataItems::const_iterator
    find_ip_data_item(const DataItems & search_data_items) const;

private:

    boost::asio::ip::address_v4
    deserialize_ipv4(std::vector<std::uint8_t>::const_iterator & it,
                     std::vector<std::uint8_t>::const_iterator di_end);

    boost::asio::ip::address_v6
    deserialize_ipv6(std::vector<std::uint8_t>::const_iterator & it,
                     std::vector<std::uint8_t>::const_iterator di_end);

    template<typename T, std::size_t N>
    std::array<T, N>
    deserialize_array(std::vector<std::uint8_t>::const_iterator & it,
                      std::vector<std::uint8_t>::const_iterator di_end);

    /// from constructor, we do not own it
    const ProtocolConfig * protocfg;
};



} // namespace LLDLEP

#endif // DATA_ITEM_H
