/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016, 2018 Massachusetts Institute of Technology
 */

#include <sstream>
#include <boost/bimap.hpp>
#include "DataItem.h"
#include "ProtocolConfig.h"

namespace LLDLEP
{

//-----------------------------------------------------------------------------
//
// DataItem constructors

DataItem::DataItem(const ProtocolConfig * protocfg) :
    id(0),
    protocfg(protocfg)
{
}

DataItem::DataItem(const std::string & di_name,
                   const DataItemValue & di_value,
                   const ProtocolConfig * protocfg,
                   const DataItemInfo * parent_di_info) :
    value(di_value),
    protocfg(protocfg)
{
    assert(protocfg != nullptr);
    id = protocfg->get_data_item_id(di_name, parent_di_info);
}

DataItem::DataItem(const std::string & di_name,
                   const ProtocolConfig * protocfg,
                   const DataItemInfo * parent_di_info) :
    protocfg(protocfg)
{
    assert(protocfg != nullptr);
    id = protocfg->get_data_item_id(di_name, parent_di_info);

    // Now set a default value according to the value type for this
    // data item id.

    set_default_value(protocfg->get_data_item_value_type(di_name));
}

void
DataItem::set_default_value(DataItemValueType di_value_type)
{
    switch (di_value_type)
    {
        case DataItemValueType::blank:
            value = boost::blank();
            break;
        case DataItemValueType::div_u8:
            value = std::uint8_t(0);
            break;
        case DataItemValueType::div_u16:
            value = std::uint16_t(0);
            break;
        case DataItemValueType::div_u32:
            value = std::uint32_t(0);
            break;
        case DataItemValueType::div_u64:
            value = std::uint64_t(0);
            break;
        case DataItemValueType::div_v_u8:
            value = std::vector<std::uint8_t>();
            break;
        case DataItemValueType::div_a2_u16:
            value = std::array<std::uint16_t, 2> { {0, 0} };
            break;
        case DataItemValueType::div_a2_u64:
            value = std::array<std::uint64_t, 2> { {0, 0} };
            break;
        case DataItemValueType::div_string:
            value = "";
            break;
        case DataItemValueType::div_dlepmac:
            value = DlepMac();
            break;
        case DataItemValueType::div_u8_string:
            value = Div_u8_string_t {0, ""};
            break;
        case DataItemValueType::div_u8_ipv4:
            value = Div_u8_ipv4_t {0, boost::asio::ip::address_v4()};
            break;
        case DataItemValueType::div_ipv4_u8:
            value = Div_ipv4_u8_t {boost::asio::ip::address_v4(), 0};
            break;
        case DataItemValueType::div_u8_ipv6:
            value = Div_u8_ipv6_t {0, boost::asio::ip::address_v6()};
            break;
        case DataItemValueType::div_ipv6_u8:
            value = Div_ipv6_u8_t {boost::asio::ip::address_v6(), 0};
            break;
        case DataItemValueType::div_u64_u8:
            value = Div_u64_u8_t {0, 0};
            break;
        case DataItemValueType::div_u16_vu8:
            value = Div_u16_vu8_t {0, std::vector<std::uint8_t>()};
            break;
        case DataItemValueType::div_v_extid:
            value = Div_v_extid_t {std::vector<ExtensionIdType>()};
            break;
        case DataItemValueType::div_u8_ipv4_u16:
            value = Div_u8_ipv4_u16_t {0, boost::asio::ip::address_v4(), 0};
            break;
        case DataItemValueType::div_u8_ipv6_u16:
            value = Div_u8_ipv6_u16_t {0, boost::asio::ip::address_v6(), 0};
            break;
        case DataItemValueType::div_u8_ipv4_u8:
            value = Div_u8_ipv4_u8_t {0, boost::asio::ip::address_v4(), 0};
            break;
        case DataItemValueType::div_u8_ipv6_u8:
            value = Div_u8_ipv6_u8_t {0, boost::asio::ip::address_v6(), 0};
            break;
        case DataItemValueType::div_u64_u64:
            value = Div_u64_u64_t {0, 0};
            break;
        case DataItemValueType::div_sub_data_items:
            value = Div_sub_data_items_t {std::vector<DataItem>()};
            break;
    }
}

//-----------------------------------------------------------------------------
//
// get_type support

DataItemValueType
DataItem::get_type() const
{
    // For this to work, in DataItem.h the order of the types in the
    // boost::variant DataItemValue must match precisely with the
    // order of the enum values in DataItemValueType.

    return static_cast<DataItemValueType>(value.which());
}

//-----------------------------------------------------------------------------
//
// serialize support

class DataItemSerializeVisitor :
    public boost::static_visitor< std::vector<std::uint8_t> >
{
public:

    explicit DataItemSerializeVisitor(const ProtocolConfig * protocfg) :
        protocfg(protocfg) {}

    // serialize blank
    std::vector<std::uint8_t> operator()(const boost::blank &  /*operand*/) const
    {
        std::vector<std::uint8_t> buf;
        return buf;
    }

    // serialize uintX_t
    template <typename T>
    std::vector<std::uint8_t> operator()(const T & operand) const
    {
        std::vector<std::uint8_t> buf;
        LLDLEP::serialize(operand, buf);
        return buf;
    }

    // serialize std::vector<uintX_t>
    template <typename T>
    std::vector<std::uint8_t> operator()(const std::vector<T> & operand) const
    {
        std::vector<std::uint8_t> buf;

        for (auto & x : operand)
        {
            LLDLEP::serialize(x, buf);
        }
        return buf;
    }

    // serialize std::array<uintX_t>
    template <typename T, std::size_t N>
    std::vector<std::uint8_t> operator()(const std::array<T, N> & operand) const
    {
        std::vector<std::uint8_t> buf;

        for (auto & x : operand)
        {
            LLDLEP::serialize(x, buf);
        }
        return buf;
    }

    // serialize Div_v_extid
    std::vector<std::uint8_t> operator()(const Div_v_extid_t & operand) const
    {
        std::vector<std::uint8_t> buf;

        for (auto & x : operand.field1)
        {
            LLDLEP::serialize(x, protocfg->get_extension_id_size(), buf);
        }
        return buf;
    }

    // serialize std::string
    std::vector<std::uint8_t> operator()(const std::string & operand) const
    {
        std::vector<std::uint8_t> buf;

        buf.insert(buf.end(), operand.begin(), operand.end());

        return buf;
    }

    // serialize DlepMac
    std::vector<std::uint8_t> operator()(const LLDLEP::DlepMac & operand) const
    {
        std::vector<std::uint8_t> buf;

        buf.insert(buf.end(), operand.mac_addr.begin(), operand.mac_addr.end());

        return buf;
    }

    // serialize Div_u8_string_t
    std::vector<std::uint8_t> operator()(const Div_u8_string_t & operand) const
    {
        std::vector<std::uint8_t> buf;

        LLDLEP::serialize(operand.field1, buf);
        buf.insert(buf.end(), operand.field2.begin(), operand.field2.end());

        return buf;
    }

    // serialize Div_u8_ipv4_t
    std::vector<std::uint8_t> operator()(const Div_u8_ipv4_t & operand) const
    {
        std::vector<std::uint8_t> buf;

        buf.push_back(std::uint8_t(operand.field1));

        // to_bytes() returns the bytes in network byte order
        boost::asio::ip::address_v4::bytes_type ipv4_bytes =
            operand.field2.to_bytes();
        buf.insert(buf.end(), ipv4_bytes.begin(), ipv4_bytes.end());

        return buf;
    }

    // serialize Div_ipv4_u8_t
    std::vector<std::uint8_t> operator()(const Div_ipv4_u8_t & operand) const
    {
        std::vector<std::uint8_t> buf;

        // to_bytes() returns the bytes in network byte order
        boost::asio::ip::address_v4::bytes_type ipv4_bytes =
            operand.field1.to_bytes();
        buf.insert(buf.end(), ipv4_bytes.begin(), ipv4_bytes.end());

        buf.push_back(operand.field2);

        return buf;
    }

    // serialize Div_u8_ipv6_t
    std::vector<std::uint8_t> operator()(const Div_u8_ipv6_t & operand) const
    {
        std::vector<std::uint8_t> buf;

        buf.push_back(std::uint8_t(operand.field1));

        // to_bytes() returns the bytes in network byte order
        boost::asio::ip::address_v6::bytes_type ipv6_bytes =
            operand.field2.to_bytes();
        buf.insert(buf.end(), ipv6_bytes.begin(), ipv6_bytes.end());

        return buf;
    }

    // serialize Div_ipv6_u8_t
    std::vector<std::uint8_t> operator()(const Div_ipv6_u8_t & operand) const
    {
        std::vector<std::uint8_t> buf;

        // to_bytes() returns the bytes in network byte order
        boost::asio::ip::address_v6::bytes_type ipv6_bytes =
            operand.field1.to_bytes();
        buf.insert(buf.end(), ipv6_bytes.begin(), ipv6_bytes.end());

        buf.push_back(operand.field2);

        return buf;
    }

    // serialize Div_u64_u8_t
    std::vector<std::uint8_t> operator()(const Div_u64_u8_t & operand) const
    {
        std::vector<std::uint8_t> buf;

        LLDLEP::serialize(operand.field1, buf);
        LLDLEP::serialize(operand.field2, buf);

        return buf;
    }

    // serialize Div_u16_vu8_t
    std::vector<std::uint8_t> operator()(const Div_u16_vu8_t & operand) const
    {
        std::vector<std::uint8_t> buf;

        LLDLEP::serialize(operand.field1, buf);
        buf.insert(buf.end(), operand.field2.begin(), operand.field2.end());

        return buf;
    }

    // serialize Div_u8_ipv4_u16_t
    std::vector<std::uint8_t> operator()(const Div_u8_ipv4_u16_t & operand) const
    {
        std::vector<std::uint8_t> buf;

        buf.push_back(std::uint8_t(operand.field1));

        // to_bytes() returns the bytes in network byte order
        boost::asio::ip::address_v4::bytes_type ipv4_bytes =
            operand.field2.to_bytes();
        buf.insert(buf.end(), ipv4_bytes.begin(), ipv4_bytes.end());

        // port is optional; if 0, don't put it
        if (operand.field3)
        {
            LLDLEP::serialize(operand.field3, buf);
        }
        return buf;
    }

    // serialize Div_u8_ipv6_u16_t
    std::vector<std::uint8_t> operator()(const Div_u8_ipv6_u16_t & operand) const
    {
        std::vector<std::uint8_t> buf;

        buf.push_back(std::uint8_t(operand.field1));

        // to_bytes() returns the bytes in network byte order
        boost::asio::ip::address_v6::bytes_type ipv6_bytes =
            operand.field2.to_bytes();
        buf.insert(buf.end(), ipv6_bytes.begin(), ipv6_bytes.end());

        // port is optional; if 0, don't put it
        if (operand.field3)
        {
            LLDLEP::serialize(operand.field3, buf);
        }
        return buf;
    }

    // serialize Div_u8_ipv4_u8_t
    std::vector<std::uint8_t> operator()(const Div_u8_ipv4_u8_t & operand) const
    {
        std::vector<std::uint8_t> buf;

        buf.push_back(operand.field1);

        // to_bytes() returns the bytes in network byte order
        boost::asio::ip::address_v4::bytes_type ipv4_bytes =
            operand.field2.to_bytes();
        buf.insert(buf.end(), ipv4_bytes.begin(), ipv4_bytes.end());

        buf.push_back(operand.field3);

        return buf;
    }

    // serialize Div_u8_ipv6_u8_t
    std::vector<std::uint8_t> operator()(const Div_u8_ipv6_u8_t & operand) const
    {
        std::vector<std::uint8_t> buf;

        buf.push_back(operand.field1);

        // to_bytes() returns the bytes in network byte order
        boost::asio::ip::address_v6::bytes_type ipv6_bytes =
            operand.field2.to_bytes();
        buf.insert(buf.end(), ipv6_bytes.begin(), ipv6_bytes.end());

        buf.push_back(operand.field3);

        return buf;
    }

    // serialize Div_u64_u64_t
    std::vector<std::uint8_t> operator()(const Div_u64_u64_t & operand) const
    {
        std::vector<std::uint8_t> buf;

        LLDLEP::serialize(operand.field1, buf);
        LLDLEP::serialize(operand.field2, buf);

        return buf;
    }

    // serialize Div_sub_data_items_t
    std::vector<std::uint8_t> operator()(const Div_sub_data_items_t & operand) const
    {
        std::vector<std::uint8_t> buf;

        for (const DataItem & sdi : operand.sub_data_items)
        {
            std::vector<std::uint8_t> one_serialized_sdi = sdi.serialize();
            buf.insert(buf.end(), 
                       one_serialized_sdi.begin(), one_serialized_sdi.end());
        }
        return buf;
    }

private:
    const ProtocolConfig * protocfg;
}; // DataItemSerializeVisitor

std::vector<std::uint8_t>
DataItem::serialize() const
{
    std::vector<std::uint8_t> dibuf;

    std::vector<std::uint8_t> valbuf =
        boost::apply_visitor(DataItemSerializeVisitor(protocfg), value);

    // We have to serialize the data item header after serializing the
    // value because the header needs to know the value's length.

    LLDLEP::serialize(id, protocfg->get_data_item_id_size(), dibuf);
    LLDLEP::serialize(valbuf.size(), protocfg->get_data_item_length_size(),
                      dibuf);

    // append the serialized value to the header
    dibuf.insert(dibuf.end(), valbuf.begin(), valbuf.end());

    return dibuf;
}

//-----------------------------------------------------------------------------
//
// deserialize support

boost::asio::ip::address_v4
DataItem::deserialize_ipv4(std::vector<std::uint8_t>::const_iterator & it,
                           std::vector<std::uint8_t>::const_iterator di_end)
{
    boost::asio::ip::address_v4::bytes_type ipbytes;

    std::size_t nbytes = 0;
    for (;
            (nbytes < ipbytes.size()) && (it < di_end);
            nbytes++, it++)
    {
        ipbytes[nbytes] = *it;
    }
    if (nbytes < ipbytes.size())
    {
        throw std::length_error(
            "data item has too few bytes for an IPv4 address");
    }
    return boost::asio::ip::address_v4(ipbytes);
}

boost::asio::ip::address_v6
DataItem::deserialize_ipv6(std::vector<std::uint8_t>::const_iterator & it,
                           std::vector<std::uint8_t>::const_iterator di_end)
{
    boost::asio::ip::address_v6::bytes_type ipbytes;

    std::size_t nbytes = 0;
    for (;
            (nbytes < ipbytes.size()) && (it < di_end);
            nbytes++, it++)
    {
        ipbytes[nbytes] = *it;
    }
    if (nbytes < ipbytes.size())
    {
        throw std::length_error(
            "data item has too few bytes for an IPv6 address");
    }
    return boost::asio::ip::address_v6(ipbytes);
}

template<typename T, std::size_t N>
std::array<T, N>
DataItem::deserialize_array(std::vector<std::uint8_t>::const_iterator & it,
                            std::vector<std::uint8_t>::const_iterator di_end)
{
    std::array<T, N> val;
    for (std::size_t i = 0; i < val.size(); i++)
    {
        LLDLEP::deserialize(val[i], it, di_end);
    }
    return val;
}

void
DataItem::deserialize(std::vector<std::uint8_t>::const_iterator & it,
                      std::vector<std::uint8_t>::const_iterator it_end,
                      const DataItemInfo * parent_di_info)
{
    // get the data item ID

    LLDLEP::deserialize(id, protocfg->get_data_item_id_size(), it, it_end);

    // get the data item length

    std::size_t di_length;
    LLDLEP::deserialize(di_length, protocfg->get_data_item_length_size(),
                        it, it_end);

    // it should now point to the start of the serialized data item's value.
    // di_end is where the serialized data item should end.

    auto di_end = it + di_length;

    if (di_end > it_end)
    {
        throw std::length_error(
            "data item=" + std::to_string(id) +
            " length=" + std::to_string(di_length) +
            " extends beyond the end of the message");
    }

    DataItemInfo di_info = protocfg->get_data_item_info(id, parent_di_info);

    switch (di_info.value_type)
    {
        case DataItemValueType::blank:
        {
            value = boost::blank();
            break;
        }
        case DataItemValueType::div_u8:
        {
            std::uint8_t val;
            LLDLEP::deserialize(val, it, di_end);
            value = val;
            break;
        }
        case DataItemValueType::div_u16:
        {
            std::uint16_t val;
            LLDLEP::deserialize(val, it, di_end);
            value = val;
            break;
        }
        case DataItemValueType::div_u32:
        {
            std::uint32_t val;
            LLDLEP::deserialize(val, it, di_end);
            value = val;
            break;
        }
        case DataItemValueType::div_u64:
        {
            std::uint64_t val;
            LLDLEP::deserialize(val, it, di_end);
            value = val;
            break;
        }
        case DataItemValueType::div_v_u8:
        {
            std::vector<std::uint8_t> val;
            while (it < di_end)
            {
                std::uint8_t ui;
                LLDLEP::deserialize(ui, it, di_end);
                val.push_back(ui);
            }
            value = val;
            break;
        }
        case DataItemValueType::div_a2_u16:
        {
            value = deserialize_array<std::uint16_t, 2>(it, di_end);
            break;
        }
        case DataItemValueType::div_a2_u64:
        {
            value = deserialize_array<std::uint64_t, 2>(it, di_end);
            break;
        }
        case DataItemValueType::div_string:
        {
            std::string val;
            val.insert(val.end(), it, di_end);
            it = di_end;
            value = val;
            break;
        }
        case DataItemValueType::div_dlepmac:
        {
            LLDLEP::DlepMac val;
            val.mac_addr.insert(val.mac_addr.end(), it, di_end);
            it = di_end;
            value = val;
            break;
        }
        case DataItemValueType::div_u8_string:
        {
            Div_u8_string_t val;
            LLDLEP::deserialize(val.field1, it, di_end);
            val.field2.insert(val.field2.end(), it, di_end);
            it = di_end;
            value = val;
            break;
        }
        case DataItemValueType::div_u8_ipv4:
        {
            Div_u8_ipv4_t val;
            LLDLEP::deserialize(val.field1, it, di_end);
            val.field2 = deserialize_ipv4(it, di_end);
            value = val;
            break;
        }
        case DataItemValueType::div_ipv4_u8:
        {
            Div_ipv4_u8_t val;
            val.field1 = deserialize_ipv4(it, di_end);
            LLDLEP::deserialize(val.field2, it, di_end);
            value = val;
            break;
        }
        case DataItemValueType::div_u8_ipv6:
        {
            Div_u8_ipv6_t val;
            LLDLEP::deserialize(val.field1, it, di_end);
            val.field2 = deserialize_ipv6(it, di_end);
            value = val;
            break;
        }
        case DataItemValueType::div_ipv6_u8:
        {
            Div_ipv6_u8_t val;
            val.field1 = deserialize_ipv6(it, di_end);
            LLDLEP::deserialize(val.field2, it, di_end);
            value = val;
            break;
        }
        case DataItemValueType::div_u64_u8:
        {
            Div_u64_u8_t val;
            LLDLEP::deserialize(val.field1, it, di_end);
            LLDLEP::deserialize(val.field2, it, di_end);
            value = val;
            break;
        }
        case DataItemValueType::div_u16_vu8:
        {
            Div_u16_vu8_t val;
            LLDLEP::deserialize(val.field1, it, di_end);
            val.field2.insert(val.field2.end(), it, di_end);
            value = val;
            break;
        }
        case DataItemValueType::div_v_extid:
        {
            Div_v_extid_t val;

            while (it < di_end)
            {
                ExtensionIdType xid;
                LLDLEP::deserialize(xid, protocfg->get_extension_id_size(),
                                    it, di_end);
                val.field1.push_back(xid);
            }
            value = val;
            break;
        }
        case DataItemValueType::div_u8_ipv4_u16:
        {
            Div_u8_ipv4_u16_t val;
            LLDLEP::deserialize(val.field1, it, di_end);
            val.field2 = deserialize_ipv4(it, di_end);

            // port is optional; if there are more bytes, get it
            if (it != di_end)
            {
                LLDLEP::deserialize(val.field3, it, di_end);
            }
            else
            {
                val.field3 = 0;
            }
            value = val;
            break;
        }
        case DataItemValueType::div_u8_ipv6_u16:
        {
            Div_u8_ipv6_u16_t val;
            LLDLEP::deserialize(val.field1, it, di_end);
            val.field2 = deserialize_ipv6(it, di_end);

            // port is optional; if there are more bytes, get it
            if (it != di_end)
            {
                LLDLEP::deserialize(val.field3, it, di_end);
            }
            else
            {
                val.field3 = 0;
            }
            value = val;
            break;
        }
        case DataItemValueType::div_u8_ipv4_u8:
        {
            Div_u8_ipv4_u8_t val;
            LLDLEP::deserialize(val.field1, it, di_end);
            val.field2 = deserialize_ipv4(it, di_end);
            LLDLEP::deserialize(val.field3, it, di_end);
            value = val;
            break;
        }
        case DataItemValueType::div_u8_ipv6_u8:
        {
            Div_u8_ipv6_u8_t val;
            LLDLEP::deserialize(val.field1, it, di_end);
            val.field2 = deserialize_ipv6(it, di_end);
            LLDLEP::deserialize(val.field3, it, di_end);
            value = val;
            break;
        }
        case DataItemValueType::div_u64_u64:
        {
            Div_u64_u64_t val;
            LLDLEP::deserialize(val.field1, it, di_end);
            LLDLEP::deserialize(val.field2, it, di_end);
            value = val;
            break;
        }

        case DataItemValueType::div_sub_data_items:
        {
            Div_sub_data_items_t val;
            while (it < di_end)
            {
                DataItem sdi(protocfg);
                sdi.deserialize(it, di_end, &di_info);
                val.sub_data_items.push_back(sdi);
            }
            value = val;
            break;
        }

        // Do not add a default: case to this switch!  We need every
        // enum value to have a corresponding case so that values of
        // that type are properly deserialized.  By not having a
        // default, if a new enum value is added to DataItemValueType,
        // the compiler will warn us if the new value isn't handled in
        // this switch.  If there is a default case, we won't get that
        // warning.

    } // end switch on data item type

    // Deserialization should have consumed all of the bytes belonging to
    // this data item, as given by its length field.  If the iterator
    // didn't land exactly at the end of the data item (start of the next
    // one, really), throw a length error.

    if (it != di_end)
    {
        throw std::length_error(
            "data item=" + std::to_string(id) +
            " length=" + std::to_string(di_length) +
            " deserialized length was only " + std::to_string(di_end - it));
    }
}

//-----------------------------------------------------------------------------
//
// to_string support

class DataItemToStringVisitor :
    public boost::static_visitor< std::string >
{
public:

    explicit DataItemToStringVisitor(const DataItemInfo * parent_di_info) :
        parent_di_info(parent_di_info) {}

    // to_string blank
    std::string operator()(const boost::blank &  /*operand*/) const
    {
        return "";
    }

    // In many places below, we cast an integer argument to uint64_t
    // before passing it to to_string().  The reason is that uint8_t
    // will print as a character instead of an integer without a cast
    // to an integer type, and we don't want that.  Some of these are
    // template functions, so we cast to uint64_t to accommodate the
    // largest integer type that the template might be used for.

    // to_string uintX_t
    template <typename T>
    std::string operator()(const T & operand) const
    {
        return std::to_string(std::uint64_t(operand));
    }

    // to_string std::vector<uintX_t>
    template <typename T>
    std::string operator()(const std::vector<T> & operand) const
    {
        std::ostringstream ss;

        std::string comma = "";
        for (auto & x : operand)
        {
            ss << comma << std::to_string(std::uint64_t(x));
            comma = ",";
        }
        return ss.str();
    }

    // to_string std::array<uintX_t>
    template <typename T, std::size_t N>
    std::string operator()(const std::array<T, N> & operand) const
    {
        std::ostringstream ss;

        std::string comma = "";
        for (auto & x : operand)
        {
            ss << comma << std::to_string(std::uint64_t(x));
            comma = ",";
        }
        return ss.str();
    }

    // to_string Div_v_extid
    std::string operator()(const Div_v_extid_t & operand) const
    {
        std::ostringstream ss;

        std::string comma = "";
        for (auto & x : operand.field1)
        {
            ss << comma << std::to_string(std::uint64_t(x));
            comma = ",";
        }
        return ss.str();
    }

    // to_string std::string
    std::string operator()(const std::string & operand) const
    {
        return operand;
    }

    // to_string DlepMac
    std::string operator()(const LLDLEP::DlepMac & operand) const
    {
        return operand.to_string();
    }

    // to_string Div_u8_string_t
    std::string operator()(const Div_u8_string_t & operand) const
    {
        std::ostringstream ss;

        ss << std::uint64_t(operand.field1) << ";" << operand.field2;
        return ss.str();
    }

    // to_string Div_u8_ipv4_t
    std::string operator()(const Div_u8_ipv4_t & operand) const
    {
        std::ostringstream ss;

        ss << std::uint64_t(operand.field1) << ";"
           << operand.field2.to_string();
        return ss.str();
    }

    // to_string Div_ipv4_u8_t
    std::string operator()(const Div_ipv4_u8_t & operand) const
    {
        std::ostringstream ss;

        ss << operand.field1.to_string() << "/"
           << std::uint64_t(operand.field2);
        return ss.str();
    }

    // to_string Div_u8_ipv6_t
    std::string operator()(const Div_u8_ipv6_t & operand) const
    {
        std::ostringstream ss;

        ss << std::uint64_t(operand.field1) << ";"
           << operand.field2.to_string();
        return ss.str();
    }

    // to_string Div_ipv6_u8_t
    std::string operator()(const Div_ipv6_u8_t & operand) const
    {
        std::ostringstream ss;

        ss << operand.field1.to_string() << "/"
           << std::uint64_t(operand.field2);
        return ss.str();
    }

    // to_string Div_u64_u8_t
    std::string operator()(const Div_u64_u8_t & operand) const
    {
        std::ostringstream ss;

        ss << operand.field1 << ";" << std::uint64_t(operand.field2);
        return ss.str();
    }

    // to_string Div_u16_vu8_t
    std::string operator()(const Div_u16_vu8_t & operand) const
    {
        std::ostringstream ss;

        ss << operand.field1 << ";";
        std::string comma = "";
        for (auto & x : operand.field2)
        {
            ss << comma << std::to_string(std::uint64_t(x));
            comma = ",";
        }
        return ss.str();
    }

    // to_string Div_u8_ipv4_u16_t
    std::string operator()(const Div_u8_ipv4_u16_t & operand) const
    {
        std::ostringstream ss;

        ss << std::uint64_t(operand.field1) << ";"
           << operand.field2.to_string()  << ";"
           << operand.field3;
        return ss.str();
    }

    // to_string Div_u8_ipv6_u16_t
    std::string operator()(const Div_u8_ipv6_u16_t & operand) const
    {
        std::ostringstream ss;

        ss << std::uint64_t(operand.field1) << ";"
           << operand.field2.to_string()  << ";"
           << operand.field3;
        return ss.str();
    }

    // to_string Div_u8_ipv4_u8_t
    std::string operator()(const Div_u8_ipv4_u8_t & operand) const
    {
        std::ostringstream ss;

        ss << std::uint64_t(operand.field1) << ";"
           << operand.field2.to_string()  << ";"
           << std::uint64_t(operand.field3);
        return ss.str();
    }

    // to_string Div_u8_ipv6_u8_t
    std::string operator()(const Div_u8_ipv6_u8_t & operand) const
    {
        std::ostringstream ss;

        ss << std::uint64_t(operand.field1) << ";"
           << operand.field2.to_string()  << ";"
           << std::uint64_t(operand.field3);
        return ss.str();
    }

    // to_string Div_u64_u64_t
    std::string operator()(const Div_u64_u64_t & operand) const
    {
        std::ostringstream ss;

        ss << operand.field1 << ";" << operand.field2;
        return ss.str();
    }

    // to_string Div_sub_data_items_t
    std::string operator()(const Div_sub_data_items_t & operand) const
    {
        std::ostringstream ss;

        ss << "{ ";
        for (const DataItem & sdi : operand.sub_data_items)
        {
            ss << sdi.to_string(parent_di_info) << " ";
        }
        ss << "} ";

        return ss.str();
    }

private:
    const DataItemInfo * parent_di_info;
}; // class DataItemToStringVisitor

std::string
DataItem::to_string(const DataItemInfo * parent_di_info) const
{
    std::ostringstream ss;
    std::string di_name = protocfg->get_data_item_name(id, parent_di_info);
    DataItemInfo di_info = protocfg->get_data_item_info(di_name);

    ss << di_name << " ";

    if (di_info.value_type == DataItemValueType::div_sub_data_items)
    {
        parent_di_info = &di_info;
    }

    ss << boost::apply_visitor(DataItemToStringVisitor(parent_di_info), value);

    return ss.str();
}

std::string
DataItem::name(const DataItemInfo * parent_di_info) const
{
    return protocfg->get_data_item_name(id, parent_di_info);
}

std::string
DataItem::value_to_string(const DataItemInfo * parent_di_info) const
{
    return boost::apply_visitor(DataItemToStringVisitor(parent_di_info), value);
}

//-----------------------------------------------------------------------------
//
// from_string support

// advance sstream past leading whitespace
static void
skip_whitespace(std::istringstream & ss)
{
    while (ss && isspace(ss.peek()))
    {
        ss.ignore();
    }
}

class DataItemValueFromStringStreamVisitor :
    public boost::static_visitor<DataItemValue>
{
public:

    explicit DataItemValueFromStringStreamVisitor(std::istringstream & ss,
                                                  const std::string & value_type_name) :
        ss(ss), value_type_name(value_type_name) {}

    bool is_field_separator(char c) const
    {
        return ((c == ';') || (c == '/'));
    }

    // Make sure the next character in ss is a field separator and
    // that there are more characters after it.
    void check_field_separator() const
    {
        if (! is_field_separator(ss.get()) || ! ss)
        {
            throw std::invalid_argument("expected field separator");
        }
    }

    template <typename T>
    T parse_uint() const
    {
        std::uint64_t u64;

        ss >> u64;
        if (ss.fail())
        {
            // parse failed because there were no characters left in
            // the stream

            if (ss.eof())
            {
                throw std::invalid_argument("missing value of type " +
                                            value_type_name);
            }

            // parse failed because of bad characters in the value

            ss.clear();
            std::string badvalue;
            std::getline(ss, badvalue, ' ');

            throw std::invalid_argument(badvalue +
                                        " is not a valid value of type " +
                                        value_type_name);
        }

        // Make sure the parsed value fits in type T
        T v = u64;
        if (v != u64)
        {
            throw std::invalid_argument("value " +
                                        std::to_string(u64) +
                                        " is too large for type " +
                                        value_type_name);
        }
        return v;
    }

    // from_stringstream to uintX_t
    template <typename T>
    DataItemValue operator()(const T &  /*operand*/) const
    {
        return parse_uint<T>();
    }

    template <typename T>
    std::vector<T> parse_vector() const
    {
        std::vector<T> vec;

        // We have to allow for an empty vector, so it's possible that
        // there is not even one valid value to be parsed.  Therefore,
        // we don't use parse_uint<T> because it throws an exception
        // if it doesn't find a valid value.

        std::uint64_t u64;
        while (ss >> u64)
        {
            // Make sure the parsed value fits in type T
            T v = u64;
            if (v != u64)
            {
                throw std::invalid_argument("value " +
                                            std::to_string(u64) +
                                            " is too large for type " +
                                            value_type_name);
            }

            vec.push_back(v);
            // skip over the , between the numbers
            if (ss.peek() == ',')
            {
                ss.ignore();
            }
        }

        // set up for parsing more data items after this one
        if (ss.fail())
        {
            ss.clear();
        }
        return vec;
    }

    // from_stringstream to vector<uintX_t>
    template <typename T>
    DataItemValue operator()(const std::vector<T> &  /*operand*/) const
    {
        return parse_vector<T>();
    }

    // from_stringstream to array<uintX_t, N>
    template <typename T, std::size_t N>
    DataItemValue operator()(const std::array<T, N> &  /*operand*/) const
    {
        std::array<T, N> arr;
        unsigned int i;

        for (i = 0; (ss && i < N); i++)
        {
            arr[i] = parse_uint<T>();
            // skip over the , between the numbers
            if (ss.peek() == ',')
            {
                ss.ignore();
            }
        }

        if (i < N)
        {
            throw std::invalid_argument("array requires " +
                                        std::to_string(N) +
                                        " elements, but got only " +
                                        std::to_string(i));
        }
        return arr;
    }

    // from_stringstream to blank
    DataItemValue operator()(const boost::blank &  /*operand*/) const
    {
        return boost::blank();
    }

    // from_stringstream to string
    DataItemValue operator()(const std::string &  /*operand*/) const
    {
        std::string s;
        ss >> s;
        return s;
    }

    // from_stringstream to DlepMac
    DataItemValue operator()(const LLDLEP::DlepMac &  /*operand*/) const
    {
        LLDLEP::DlepMac mac;

        // expect hex bytes for mac address
        ss >> std::hex;
        do
        {
            std::uint8_t byte = parse_uint<std::uint8_t>();
            mac.mac_addr.push_back(byte);
            if (ss.peek() == ':')
            {
                // move past : and expect another byte
                ss.ignore();
            }
            else
            {
                // no more mac addr bytes
                break;
            }
        } while (true);

        // restore default expectation of decimal numbers
        ss >> std::dec;
        return mac;
    }

    // from_stringstream to Div_u8_string_t
    DataItemValue operator()(const Div_u8_string_t &  /*operand*/) const
    {
        Div_u8_string_t u8str;

        u8str.field1 = parse_uint<decltype(u8str.field1)>();
        check_field_separator();
        // Get the rest of the stream as one string, possibly empty
        ss >> u8str.field2;
        return u8str;
    }

    template <typename IPAddrType>
    IPAddrType parse_ip_addr() const
    {
        // The boost IP address parser does want to see leading spaces
        skip_whitespace(ss);

        // Collect characters up to the field separator or the end of
        // the stream ss.
        std::string ip_str;
        while (ss && ! is_field_separator(ss.peek()))
        {
            char ch = ss.get();
            if (! ss.eof())
            {
                ip_str += ch;
            }
        }
        boost::system::error_code ec;
        IPAddrType ipaddr = IPAddrType::from_string(ip_str, ec);
        if (ec)
        {
            throw std::invalid_argument("invalid IP address " + ip_str);
        }
        return ipaddr;
    }

    // from_stringstream to Div_u8_ipv4_t
    DataItemValue operator()(const Div_u8_ipv4_t &  /*operand*/) const
    {
        Div_u8_ipv4_t u8ipv4;

        u8ipv4.field1 = parse_uint<decltype(u8ipv4.field1)>();
        check_field_separator();
        u8ipv4.field2 = parse_ip_addr<boost::asio::ip::address_v4>();
        return u8ipv4;
    }

    // from_stringstream to Div_ipv4_u8_t
    DataItemValue operator()(const Div_ipv4_u8_t &  /*operand*/) const
    {
        Div_ipv4_u8_t ipv4u8;

        ipv4u8.field1 = parse_ip_addr<boost::asio::ip::address_v4>();
        check_field_separator();
        ipv4u8.field2 = parse_uint<decltype(ipv4u8.field2)>();
        return ipv4u8;
    }

    // from_stringstream to Div_u8_ipv6_t
    DataItemValue operator()(const Div_u8_ipv6_t &  /*operand*/) const
    {
        Div_u8_ipv6_t u8ipv6;

        u8ipv6.field1 = parse_uint<decltype(u8ipv6.field1)>();
        check_field_separator();
        u8ipv6.field2 = parse_ip_addr<boost::asio::ip::address_v6>();
        return u8ipv6;
    }

    // from_stringstream to Div_ipv6_u8_t
    DataItemValue operator()(const Div_ipv6_u8_t &  /*operand*/) const
    {
        Div_ipv6_u8_t ipv6u8;

        ipv6u8.field1 = parse_ip_addr<boost::asio::ip::address_v6>();
        check_field_separator();
        ipv6u8.field2 = parse_uint<decltype(ipv6u8.field2)>();
        return ipv6u8;
    }

    // from_stringstream to Div_u64_u8_t
    DataItemValue operator()(const Div_u64_u8_t &  /*operand*/) const
    {
        Div_u64_u8_t u64u8;

        u64u8.field1 = parse_uint<decltype(u64u8.field1)>();
        check_field_separator();
        u64u8.field2 = parse_uint<decltype(u64u8.field2)>();
        return u64u8;
    }

    // from_stringstream to Div_u16_vu8_t
    DataItemValue operator()(const Div_u16_vu8_t &  /*operand*/) const
    {
        Div_u16_vu8_t u16vu8;

        u16vu8.field1 = parse_uint<decltype(u16vu8.field1)>();
        check_field_separator();
        u16vu8.field2 = parse_vector<std::uint8_t>();
        return u16vu8;
    }

    // from_stringstream to Div_v_extid_t
    DataItemValue operator()(const Div_v_extid_t &  /*operand*/) const
    {
        Div_v_extid_t extid;

        extid.field1 = parse_vector<ExtensionIdType>();

        return extid;
    }

    // from_stringstream to Div_u8_ipv4_u16_t
    DataItemValue operator()(const Div_u8_ipv4_u16_t &  /*operand*/) const
    {
        Div_u8_ipv4_u16_t u8ipv4u16;

        u8ipv4u16.field1 = parse_uint<decltype(u8ipv4u16.field1)>();
        check_field_separator();
        u8ipv4u16.field2 = parse_ip_addr<boost::asio::ip::address_v4>();
        check_field_separator();
        u8ipv4u16.field3 = parse_uint<decltype(u8ipv4u16.field3)>();
        return u8ipv4u16;
    }

    // from_stringstream to Div_u8_ipv6_u16_t
    DataItemValue operator()(const Div_u8_ipv6_u16_t &  /*operand*/) const
    {
        Div_u8_ipv6_u16_t u8ipv6u16;

        u8ipv6u16.field1 = parse_uint<decltype(u8ipv6u16.field1)>();
        check_field_separator();
        u8ipv6u16.field2 = parse_ip_addr<boost::asio::ip::address_v6>();
        check_field_separator();
        u8ipv6u16.field3 = parse_uint<decltype(u8ipv6u16.field3)>();
        return u8ipv6u16;
    }

    // from_stringstream to Div_u8_ipv4_u8_t
    DataItemValue operator()(const Div_u8_ipv4_u8_t &  /*operand*/) const
    {
        Div_u8_ipv4_u8_t u8ipv4u8;

        u8ipv4u8.field1 = parse_uint<decltype(u8ipv4u8.field1)>();
        check_field_separator();
        u8ipv4u8.field2 = parse_ip_addr<boost::asio::ip::address_v4>();
        check_field_separator();
        u8ipv4u8.field3 = parse_uint<decltype(u8ipv4u8.field3)>();
        return u8ipv4u8;
    }

    // from_stringstream to Div_u8_ipv6_u8_t
    DataItemValue operator()(const Div_u8_ipv6_u8_t &  /*operand*/) const
    {
        Div_u8_ipv6_u8_t u8ipv6u8;

        u8ipv6u8.field1 = parse_uint<decltype(u8ipv6u8.field1)>();
        check_field_separator();
        u8ipv6u8.field2 = parse_ip_addr<boost::asio::ip::address_v6>();
        check_field_separator();
        u8ipv6u8.field3 = parse_uint<decltype(u8ipv6u8.field3)>();
        return u8ipv6u8;
    }

    // from_stringstream to Div_u64_u64_t
    DataItemValue operator()(const Div_u64_u64_t &  /*operand*/) const
    {
        Div_u64_u64_t u64u64;

        u64u64.field1 = parse_uint<decltype(u64u64.field1)>();
        check_field_separator();
        u64u64.field2 = parse_uint<decltype(u64u64.field2)>();
        return u64u64;
    }

    // from_stringstream to Div_sub_data_items_t
    // This should never get called because sub data items are handled
    // directly in from_istringstream()
    DataItemValue operator()(const Div_sub_data_items_t &  /*operand*/) const
    {
        Div_sub_data_items_t sdi;
        return sdi;
    }

private:
    // stringstream to parse
    std::istringstream & ss;

    // string type name so we can give better error messages
    const std::string & value_type_name;
}; // DataItemValueFromStringStreamVisitor

void
DataItem::value_from_istringstream(std::istringstream & ss)
{
    // set default numeric input to base 10
    ss >> std::dec;
    std::string value_type_name = LLDLEP::to_string(get_type());
    value = boost::apply_visitor(
        DataItemValueFromStringStreamVisitor(ss, value_type_name),
        value);
}

void
DataItem::value_from_string(const std::string & str)
{
    std::istringstream ss(str);
    value_from_istringstream(ss);
}

void
DataItem::from_istringstream(std::istringstream & ss,
                             const DataItemInfo * parent_di_info)
{
    std::string di_name;

    ss >> std::skipws >> di_name;
    if ( ! ss)
    {
        throw std::invalid_argument("expected data item name, not end of line");
    }

    id = protocfg->get_data_item_id(di_name, parent_di_info);
    DataItemInfo di_info = protocfg->get_data_item_info(di_name);
    if (di_info.value_type == DataItemValueType::div_sub_data_items)
    {
        Div_sub_data_items_t div;
        std::string token;

        ss >> token;
        if (token != "{" )
        {
            if (ss.eof())
            {
                token = "end of line";
            }
            throw std::invalid_argument("expected {, not " + token);
        }

        skip_whitespace(ss);
        while (ss.peek() != '}')
        {
            DataItem subdi(protocfg);
            subdi.from_istringstream(ss, &di_info);
            div.sub_data_items.push_back(subdi);
            skip_whitespace(ss);
        }
        // skip the closing }
        ss.ignore();
        value = div;
    }
    else // value does not contain sub data items
    {
        set_default_value(di_info.value_type);
        value_from_istringstream(ss);
    }
}

void
DataItem::from_string(const std::string & str,
                      const DataItemInfo * parent_di_info)
{
    std::istringstream ss(str);
    from_istringstream(ss, parent_di_info);
}

//-----------------------------------------------------------------------------
//
// validate support

class DataItemValidateVisitor :
    public boost::static_visitor<std::string>
{
public:
    DataItemValidateVisitor(const ProtocolConfig * protocfg,
                            const DataItemInfo * di_info) :
        protocfg(protocfg),
        di_info(di_info) { }

    // default method does no checking
    template <typename T>
    std::string operator()(const T &  /*operand*/) const
    {
        return "";
    }

    std::string operator()(const Div_u8_ipv4_t & operand) const
    {
        if (operand.field1 > 1)
        {
            return "add/drop is " +
                   std::to_string(std::uint64_t(operand.field1)) +
                   ", must be 0 or 1";
        }
        return "";
    }

    std::string operator()(const Div_u8_ipv6_t & operand) const
    {
        if (operand.field1 > 1)
        {
            return "add/drop is " +
                   std::to_string(std::uint64_t(operand.field1)) +
                   ", must be 0 or 1";
        }
        return "";
    }

    std::string validate_status(StatusCodeIdType status_id) const
    {
        // If we can convert the status ID to a string, it's valid
        // (according to the currrent protocol configuration), else
        // it's not.

        try
        {
            (void)protocfg->get_status_code_name(status_id);
        }
        catch (ProtocolConfig::BadStatusCodeId)
        {
            return std::to_string(status_id) + " is invalid";
        }
        return "";
    }

    std::string operator()(const std::uint8_t & operand) const
    {
        if (di_info->units == "percentage")
        {
            if (operand > 100)
            {
                return "is " + std::to_string(std::uint64_t(operand)) +
                       ", must be <= 100";
            }
        }
        else if (di_info->name == ProtocolStrings::Status)
        {
            return validate_status(operand);
        }
        return "";
    }

    std::string operator()(const Div_u8_string_t & operand) const
    {
        if (di_info->name == ProtocolStrings::Status)
        {
            return validate_status(operand.field1);
        }
        return "";
    }

    std::string operator()(const Div_ipv4_u8_t & operand) const
    {
        if (operand.field2 > 32)
        {
            return "subnet mask is " +
                   std::to_string(std::uint64_t(operand.field2)) +
                   ", must be <= 32";
        }
        return "";
    }

    std::string operator()(const Div_ipv6_u8_t & operand) const
    {
        if (operand.field2 > 128)
        {
            return "subnet mask is " +
                   std::to_string(std::uint64_t(operand.field2)) +
                   ", must be <= 128";
        }
        return "";
    }

    std::string operator()(const Div_u8_ipv4_u8_t & operand) const
    {
        if (operand.field1 > 1)
        {
            return "add/drop is " +
                   std::to_string(std::uint64_t(operand.field1)) +
                   ", must be 0 or 1";
        }
        if (operand.field3 > 32)
        {
            return "subnet mask is " +
                   std::to_string(std::uint64_t(operand.field3)) +
                   ", must be <= 32";
        }
        return "";
    }

    std::string operator()(const Div_u8_ipv6_u8_t & operand) const
    {
        if (operand.field1 > 1)
        {
            return "add/drop is " +
                   std::to_string(std::uint64_t(operand.field1)) +
                   ", must be 0 or 1";
        }
        if (operand.field3 > 128)
        {
            return "subnet mask is " +
                   std::to_string(std::uint64_t(operand.field3)) +
                   ", must be <= 128";
        }
        return "";
    }

    std::string operator()(const Div_sub_data_items_t & operand) const
    {
        std::string err;

        // validate each of the sub data items

        for (const DataItem & sdi : operand.sub_data_items)
        {
            err = sdi.validate(di_info);
            if (err != "")
            {
                return err;
            }
        }

    // If we haven't found an error, check the number and kind of sub data
    // items in the data item against what is allowed for this data item.

        err = DataItem::validate_occurrences(operand.sub_data_items,
                                             di_info->sub_data_items,
                                             protocfg,
                                             di_info);
        return err;
    }

private:
    const ProtocolConfig * protocfg;
    const DataItemInfo * di_info;
};

std::string
DataItem::validate(const DataItemInfo * parent_di_info) const
{
    std::string di_name = protocfg->get_data_item_name(id, parent_di_info);
    DataItemInfo di_info = protocfg->get_data_item_info(di_name);

    // check that the data item value has the correct type

    DataItemValueType div_type = get_type();

    if (di_info.value_type != div_type)
    {
        return di_name + " type is " + LLDLEP::to_string(div_type) +
               ", must be " + LLDLEP::to_string(di_info.value_type);
    }

    std::string err =
        boost::apply_visitor(DataItemValidateVisitor(protocfg, &di_info),
                             value);
    if (err != "")
    {
        err = di_name + " " + err;
    }

    return err;
}

template <typename DataItemContainer>
std::string 
DataItem::validate_occurrences(const DataItemContainer & data_items,
                               const std::vector<SubDataItem> & v_di_info,
                               const ProtocolConfig * protocfg,
                               const DataItemInfo * parent_di_info)
{
    std::string err = "";
    std::map<DataItemIdType, unsigned int> di_occurrences;

    // Count up how many of each data item there are in this message.

    for (const auto & di : data_items)
    {
        if (di_occurrences.find(di.id) == di_occurrences.end())
        {
            di_occurrences[di.id] = 1;
        }
        else
        {
            di_occurrences[di.id]++;
        }
    }

    // Go through each data item that is allowed for this data item
    // and check if the actual count of data items in this message
    // conform to the configured the "occurs" constraints.

    for (const auto & di_info : v_di_info)
    {
        std::string di_name = di_info.name;

        // number of times this data item id occurs in the message
        unsigned int di_occurs_actual = 0;
        try
        {
            di_occurs_actual = di_occurrences.at(di_info.id);

            // remove this data item id from di_occurrences to signfiy that
            // we have already checked it
            di_occurrences.erase(di_info.id);
        }
        catch (std::out_of_range)
        {
            /* di_info.id is not in this message */
        }

        // check expected number of occurs vs. actual

        if (di_info.occurs == "1")
        {
            if (di_occurs_actual != 1)
            {
                err = "exactly one of " + di_name + " required, but got " +
                      std::to_string(di_occurs_actual);
                break;
            }
        }
        else if (di_info.occurs == "1+")
        {
            if (di_occurs_actual < 1)
            {
                err = "at least one of " + di_name + " required, but got none";
                break;
            }
        }
        else if (di_info.occurs == "0-1")
        {
            if (di_occurs_actual > 1)
            {
                err = "no more than one of " + di_name + " required, but got " +
                      std::to_string(di_occurs_actual);
                break;
            }
        }
        else
        {
            assert(di_info.occurs == "0+");
            // Any number of this data item is allowed, so there's no
            // error case to check for.
        }
    } // for each data item defined for this signal

    // If we haven't found an error yet, look for any data items that
    // are still left; they are unexpected.

    if (err == "")
    {
        // If any data items remain in di_occurrences, it means the
        // message contained data items that are not allowed by the
        // protocol configuration.

        if (! di_occurrences.empty())
        {
            err = "unexpected data items: ";
            for (const auto & kvpair : di_occurrences)
            {
                // this translates to DataItemName(count)
                err += protocfg->get_data_item_name(kvpair.first, parent_di_info) +
                       "(" + std::to_string(kvpair.second) + ") ";
            }
        }
    } // if no error yet

    return err;
}

//-----------------------------------------------------------------------------
//
// ip_equal support

class DataItemIpEqualVisitor :
    public boost::static_visitor<bool>
{
public:
    explicit DataItemIpEqualVisitor(const DataItemValue & other) :
        other_div(other) {}

    // default method returns false
    template <typename T>
    bool operator()(const T &  /*operand*/) const
    {
        return false;
    }

    bool operator()(const Div_u8_ipv4_t & operand) const
    {
        // extract the other value and compare it
        Div_u8_ipv4_t other_val = boost::get<Div_u8_ipv4_t>(other_div);
        return (operand.field2 == other_val.field2);
    }

    bool operator()(const Div_ipv4_u8_t & operand) const
    {
        // extract the other value and compare it
        Div_ipv4_u8_t other_val = boost::get<Div_ipv4_u8_t>(other_div);
        return (operand.field1 == other_val.field1);
    }

    bool operator()(const Div_u8_ipv6_t & operand) const
    {
        // extract the other value and compare it
        Div_u8_ipv6_t other_val = boost::get<Div_u8_ipv6_t>(other_div);
        return (operand.field2 == other_val.field2);
    }

    bool operator()(const Div_ipv6_u8_t & operand) const
    {
        // extract the other value and compare it
        Div_ipv6_u8_t other_val = boost::get<Div_ipv6_u8_t>(other_div);
        return (operand.field1 == other_val.field1);
    }

    bool operator()(const Div_u8_ipv4_u8_t & operand) const
    {
        // extract the other value and compare it
        Div_u8_ipv4_u8_t other_val = boost::get<Div_u8_ipv4_u8_t>(other_div);
        return ( (operand.field2 == other_val.field2) &&
                 (operand.field3 == other_val.field3));
    }

    bool operator()(const Div_u8_ipv6_u8_t & operand) const
    {
        // extract the other value and compare it
        Div_u8_ipv6_u8_t other_val = boost::get<Div_u8_ipv6_u8_t>(other_div);
        return ( (operand.field2 == other_val.field2) &&
                 (operand.field3 == other_val.field3));
    }

private:
    const DataItemValue & other_div;
};

bool
DataItem::ip_equal(const DataItem & other) const
{
    // If the type of the other data item value doesn't match,
    // it can't be equal.
    if (get_type() != other.get_type())
    {
        return false;
    }

    return boost::apply_visitor(DataItemIpEqualVisitor(other.value), value);
}

//-----------------------------------------------------------------------------
//
// ip_flags support

class DataItemIpFlagsVisitor :
    public boost::static_visitor<DataItem::IPFlags>
{
public:

    // default method returns none
    template <typename T>
    DataItem::IPFlags operator()(const T &  /*operand*/) const
    {
        return DataItem::IPFlags::none;
    }

    DataItem::IPFlags operator()(const Div_u8_ipv4_t & operand) const
    {
        return static_cast<DataItem::IPFlags>(operand.field1);
    }

    DataItem::IPFlags operator()(const Div_ipv4_u8_t &  /*operand*/) const
    {
        // This data item value doesn't have an explicit flags field,
        // so we'll fake the "add" flag.
        return DataItem::IPFlags::add;
    }

    DataItem::IPFlags operator()(const Div_u8_ipv6_t & operand) const
    {
        return static_cast<DataItem::IPFlags>(operand.field1);
    }

    DataItem::IPFlags operator()(const Div_ipv6_u8_t &  /*operand*/) const
    {
        // This data item value doesn't have an explicit flags field,
        // so we'll fake the "add" flag.
        return DataItem::IPFlags::add;
    }

    DataItem::IPFlags operator()(const Div_u8_ipv4_u8_t & operand) const
    {
        return static_cast<DataItem::IPFlags>(operand.field1);
    }

    DataItem::IPFlags operator()(const Div_u8_ipv6_u8_t & operand) const
    {
        return static_cast<DataItem::IPFlags>(operand.field1);
    }
};

DataItem::IPFlags
DataItem::ip_flags() const
{
    return boost::apply_visitor(DataItemIpFlagsVisitor(), value);
}

DataItems::const_iterator
DataItem::find_ip_data_item(const DataItems & search_data_items) const
{
    auto it = search_data_items.begin();
    for ( ; it != search_data_items.end(); it++)
    {
        if (ip_equal(*it))
        {
            break;
        }
    }
    return it;
}

//-----------------------------------------------------------------------------
//
// Convert between strings and DataItemValueType

// I couldn't get an initializer list to work with boost::bimap, so
// the initialization is somewhat uglier, and happens at runtime.
// Once the map is initialized, it is read-only, so multithreaded
// access shouldn't be a problem without any need for locking.
//
// If the list below changes, you must also update DataItemValueType
// in protocol-config.xsd.

typedef boost::bimap<DataItemValueType, std::string> DataItemValueMap;
static std::vector<DataItemValueMap::value_type> mapvals
{
    { DataItemValueType::blank, "blank" },
    { DataItemValueType::div_u8, "u8" },
    { DataItemValueType::div_u16, "u16" },
    { DataItemValueType::div_u32, "u32" },
    { DataItemValueType::div_u64, "u64" },
    { DataItemValueType::div_v_u8, "v_u8" },
    { DataItemValueType::div_a2_u16, "a2_u16" },
    { DataItemValueType::div_a2_u64, "a2_u64" },
    { DataItemValueType::div_string, "string" },
    { DataItemValueType::div_dlepmac, "dlepmac" },
    { DataItemValueType::div_u8_string, "u8_string" },
    { DataItemValueType::div_u8_ipv4, "u8_ipv4" },
    { DataItemValueType::div_ipv4_u8, "ipv4_u8" },
    { DataItemValueType::div_u8_ipv6, "u8_ipv6" },
    { DataItemValueType::div_ipv6_u8, "ipv6_u8" },
    { DataItemValueType::div_u64_u8, "u64_u8" },
    { DataItemValueType::div_u16_vu8, "u16_vu8" },
    { DataItemValueType::div_v_extid, "v_extid" },
    { DataItemValueType::div_u8_ipv4_u16, "u8_ipv4_u16" },
    { DataItemValueType::div_u8_ipv6_u16, "u8_ipv6_u16" },
    { DataItemValueType::div_u8_ipv4_u8, "u8_ipv4_u8" },
    { DataItemValueType::div_u8_ipv6_u8, "u8_ipv6_u8" },
    { DataItemValueType::div_u64_u64, "u64_u64" },
    { DataItemValueType::div_sub_data_items, "sub_data_items" }
};

static DataItemValueMap data_item_value_type_map(mapvals.begin(),
                                                 mapvals.end());

std::string to_string(DataItemValueType t)
{
    return data_item_value_type_map.left.at(t);
}

DataItemValueType from_string(const std::string & s)
{
    return data_item_value_type_map.right.at(s);
}

} // namespace LLDLEP
