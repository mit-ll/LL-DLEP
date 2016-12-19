/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016 Massachusetts Institute of Technology
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

DataItem::DataItem(const std::string & di_type,
                   const DataItemValue & di_value,
                   const ProtocolConfig * protocfg) :
    value(di_value),
    protocfg(protocfg)
{
    assert(protocfg != nullptr);
    id = protocfg->get_data_item_id(di_type);
}

DataItem::DataItem(const std::string & di_type,
                   const ProtocolConfig * protocfg) :
    protocfg(protocfg)
{
    assert(protocfg != nullptr);
    id = protocfg->get_data_item_id(di_type);

    // Now set a default value according to the value type for this
    // data item id.

    DataItemValueType value_type = protocfg->get_data_item_value_type(id);

    switch (value_type)
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
    std::vector<std::uint8_t> operator()(const boost::blank & operand) const
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
                      std::vector<std::uint8_t>::const_iterator it_end)
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

    DataItemValueType di_type = protocfg->get_data_item_value_type(id);

    switch (di_type)
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

    // to_string blank
    std::string operator()(const boost::blank & operand) const
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

}; // class DataItemToStringVisitor

std::string
DataItem::to_string() const
{
    std::ostringstream ss;

    ss << "data item id=" << id
       << "(" << protocfg->get_data_item_name(id) << ") "
       << "value=" << boost::apply_visitor(DataItemToStringVisitor(), value);
    return ss.str();
}

std::string
DataItem::name() const
{
    return protocfg->get_data_item_name(id);
}

std::string
DataItem::value_to_string() const
{
    return boost::apply_visitor(DataItemToStringVisitor(), value);
}

//-----------------------------------------------------------------------------
//
// from_string support

class DataItemFromStringVisitor :
    public boost::static_visitor<DataItemValue>
{
public:

    explicit DataItemFromStringVisitor(const std::string & val_str) :
        val_str(val_str) {}

    bool is_field_separator(char c) const
    {
        return ((c == ';') || (c == '/'));
    }

    // Make sure the next character in ss is a field separator and
    // that there are more characters after it.
    void check_field_separator(std::istringstream & ss) const
    {

        if (! is_field_separator(ss.get()) || ! ss)
        {
            throw std::invalid_argument(val_str);
        }
    }

    // from_string to uintX_t
    template <typename T>
    DataItemValue  operator()(const T & operand) const
    {
        std::istringstream ss(val_str);
        std::uint64_t u64;

        ss >> u64;
        if (ss.fail())
        {
            throw std::invalid_argument(val_str);
        }

        // Make sure the parsed value fits in type T
        T v = u64;
        if (v != u64)
        {
            throw std::invalid_argument(val_str);
        }
        return v;
    }

    template <typename T>
    std::vector<T> parse_vector(std::istringstream & ss) const
    {
        std::vector<T> vec;

        while (ss && (ss.rdbuf()->in_avail() > 0))
        {
            std::uint64_t u64;
            ss >> u64;
            if (ss.fail())
            {
                throw std::invalid_argument(val_str);
            }

            // Make sure the parsed value fits in type T
            T v = u64;
            if (v != u64)
            {
                throw std::invalid_argument(val_str);
            }

            vec.push_back(v);
            // skip over the , between the numbers
            if (ss.peek() == ',')
            {
                ss.ignore();
            }
        }
        return vec;
    }

    std::uint8_t
    parse_uint8(std::istringstream & ss) const
    {
        unsigned int ui;

        ss >> ui;
        if (! ss || (ui > 255))
        {
            throw std::invalid_argument(val_str);
        }
        return std::uint8_t(ui);
    }

    // from_string to vector<uintX_t>
    template <typename T>
    DataItemValue operator()(const std::vector<T> & operand) const
    {
        std::istringstream ss(val_str);

        return parse_vector<T>(ss);
    }

    // from_string to array<uintX_t, N>
    template <typename T, std::size_t N>
    DataItemValue operator()(const std::array<T, N> & operand) const
    {
        std::array<T, N> arr;
        std::istringstream ss(val_str);
        unsigned int i;

        for (i = 0; (ss && i < N); i++)
        {
            ss >> arr[i];
            if (ss.fail())
            {
                throw std::invalid_argument(val_str);
            }
            // skip over the , between the numbers
            if (ss.peek() == ',')
            {
                ss.ignore();
            }
        }

        if (i < N)
        {
            throw std::invalid_argument(val_str);
        }
        return arr;
    }

    // from_string to blank
    DataItemValue operator()(const boost::blank & operand) const
    {
        return boost::blank();
    }

    // from_string to string
    DataItemValue operator()(const std::string & operand) const
    {
        return val_str;
    }

    // from_string to DlepMac
    DataItemValue operator()(const LLDLEP::DlepMac & operand) const
    {
        std::istringstream ss(val_str);
        LLDLEP::DlepMac mac;
        unsigned int ui;
        bool valid = true;

        while (ss >> std::hex >> ui)
        {
            if (ui > 255)
            {
                valid = false;
                break;
            }
            mac.mac_addr.push_back(ui);
            // skip over : between numbers
            if (ss.peek() == ':')
            {
                ss.ignore();
            }
        }

        // If we didn't parse all the way to the end of the string,
        // there must have been non-numeric characters in the string,
        // and that's an error.
        if (! ss.eof())
        {
            valid = false;
        }

        if (! valid)
        {
            throw std::invalid_argument(val_str);
        }

        return mac;
    }

    // from_string to Div_u8_string_t
    DataItemValue operator()(const Div_u8_string_t & operand) const
    {
        std::istringstream ss(val_str);
        Div_u8_string_t u8str;

        u8str.field1 = parse_uint8(ss);

        check_field_separator(ss);

        // Get the rest of the stream as one string
        ss >> u8str.field2;

        return u8str;
    }

    template <typename IPAddrType>
    IPAddrType parse_ip_addr(std::istringstream & ss) const
    {
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

    // from_string to Div_u8_ipv4_t
    DataItemValue operator()(const Div_u8_ipv4_t & operand) const
    {
        std::istringstream ss(val_str);
        Div_u8_ipv4_t u8ipv4;

        u8ipv4.field1 = parse_uint8(ss);
        check_field_separator(ss);
        u8ipv4.field2 = parse_ip_addr<boost::asio::ip::address_v4>(ss);
        return u8ipv4;
    }

    // from_string to Div_ipv4_u8_t
    DataItemValue operator()(const Div_ipv4_u8_t & operand) const
    {
        std::istringstream ss(val_str);
        Div_ipv4_u8_t ipv4u8;

        ipv4u8.field1 = parse_ip_addr<boost::asio::ip::address_v4>(ss);
        check_field_separator(ss);
        ipv4u8.field2 = parse_uint8(ss);
        return ipv4u8;
    }

    // from_string to Div_u8_ipv6_t
    DataItemValue operator()(const Div_u8_ipv6_t & operand) const
    {
        std::istringstream ss(val_str);
        Div_u8_ipv6_t u8ipv6;

        u8ipv6.field1 = parse_uint8(ss);
        check_field_separator(ss);
        u8ipv6.field2 = parse_ip_addr<boost::asio::ip::address_v6>(ss);
        return u8ipv6;
    }

    // from_string to Div_ipv6_u8_t
    DataItemValue operator()(const Div_ipv6_u8_t & operand) const
    {
        std::istringstream ss(val_str);
        Div_ipv6_u8_t ipv6u8;

        ipv6u8.field1 = parse_ip_addr<boost::asio::ip::address_v6>(ss);
        check_field_separator(ss);
        ipv6u8.field2 = parse_uint8(ss);
        return ipv6u8;
    }

    // from_string to Div_u64_u8_t
    DataItemValue operator()(const Div_u64_u8_t & operand) const
    {
        std::istringstream ss(val_str);
        Div_u64_u8_t u64u8;

        ss >> u64u8.field1;
        if (! ss)
        {
            throw std::invalid_argument(val_str);
        }

        check_field_separator(ss);

        u64u8.field2 = parse_uint8(ss);

        return u64u8;
    }

    // from_string to Div_u16_vu8_t
    DataItemValue operator()(const Div_u16_vu8_t & operand) const
    {
        std::istringstream ss(val_str);
        Div_u16_vu8_t u16vu8;

        ss >> u16vu8.field1;
        if (! ss)
        {
            throw std::invalid_argument(val_str);
        }

        check_field_separator(ss);

        u16vu8.field2 = parse_vector<std::uint8_t>(ss);

        return u16vu8;
    }

    // from_string to Div_v_extid_t
    DataItemValue operator()(const Div_v_extid_t & operand) const
    {
        std::istringstream ss(val_str);
        Div_v_extid_t extid;

        extid.field1 = parse_vector<ExtensionIdType>(ss);

        return extid;
    }

    // from_string to Div_u8_ipv4_u16_t
    DataItemValue operator()(const Div_u8_ipv4_u16_t & operand) const
    {
        std::istringstream ss(val_str);
        Div_u8_ipv4_u16_t u8ipv4u16;

        u8ipv4u16.field1 = parse_uint8(ss);
        check_field_separator(ss);
        u8ipv4u16.field2 = parse_ip_addr<boost::asio::ip::address_v4>(ss);
        check_field_separator(ss);
        ss >> u8ipv4u16.field3;
        return u8ipv4u16;
    }

    // from_string to Div_u8_ipv6_u16_t
    DataItemValue operator()(const Div_u8_ipv6_u16_t & operand) const
    {
        std::istringstream ss(val_str);
        Div_u8_ipv6_u16_t u8ipv6u16;

        u8ipv6u16.field1 = parse_uint8(ss);
        check_field_separator(ss);
        u8ipv6u16.field2 = parse_ip_addr<boost::asio::ip::address_v6>(ss);
        check_field_separator(ss);
        ss >> u8ipv6u16.field3;
        return u8ipv6u16;
    }

    // from_string to Div_u8_ipv4_u8_t
    DataItemValue operator()(const Div_u8_ipv4_u8_t & operand) const
    {
        std::istringstream ss(val_str);
        Div_u8_ipv4_u8_t u8ipv4u8;

        u8ipv4u8.field1 = parse_uint8(ss);
        check_field_separator(ss);
        u8ipv4u8.field2 = parse_ip_addr<boost::asio::ip::address_v4>(ss);
        check_field_separator(ss);
        u8ipv4u8.field3 = parse_uint8(ss);
        return u8ipv4u8;
    }

    // from_string to Div_u8_ipv6_u8_t
    DataItemValue operator()(const Div_u8_ipv6_u8_t & operand) const
    {
        std::istringstream ss(val_str);
        Div_u8_ipv6_u8_t u8ipv6u8;

        u8ipv6u8.field1 = parse_uint8(ss);
        check_field_separator(ss);
        u8ipv6u8.field2 = parse_ip_addr<boost::asio::ip::address_v6>(ss);
        check_field_separator(ss);
        u8ipv6u8.field3 = parse_uint8(ss);

        return u8ipv6u8;
    }

private:
    // string to parse
    const std::string & val_str;
}; // DataItemFromStringVisitor

void
DataItem::from_string(const std::string & val_str)
{
    value = boost::apply_visitor(DataItemFromStringVisitor(val_str), value);
}

//-----------------------------------------------------------------------------
//
// validate support

class DataItemValidateVisitor :
    public boost::static_visitor<std::string>
{
public:
    DataItemValidateVisitor(const ProtocolConfig * protocfg,
                            const ProtocolConfig::DataItemInfo & di_info) :
        protocfg(protocfg), di_info(di_info) { }

    // default method does no checking
    template <typename T>
    std::string operator()(const T & operand) const
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
        if (di_info.units == "percentage")
        {
            if (operand > 100)
            {
                return "is " + std::to_string(std::uint64_t(operand)) +
                       ", must be <= 100";
            }
        }
        else if (di_info.name == ProtocolStrings::Status)
        {
            return validate_status(operand);
        }
        return "";
    }

    std::string operator()(const Div_u8_string_t & operand) const
    {
        if (di_info.name == ProtocolStrings::Status)
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

private:
    const ProtocolConfig * protocfg;
    const ProtocolConfig::DataItemInfo & di_info;
};

std::string
DataItem::validate() const
{
    std::string di_name = protocfg->get_data_item_name(id);
    ProtocolConfig::DataItemInfo di_info =
        protocfg->get_data_item_info(di_name);

    // check that the data item value has the correct type

    DataItemValueType div_type = get_type();

    if (di_info.value_type != div_type)
    {
        return di_name + " type is " + LLDLEP::to_string(div_type) +
               ", must be " + LLDLEP::to_string(di_info.value_type);
    }

    std::string err =
        boost::apply_visitor(DataItemValidateVisitor(protocfg, di_info),
                             value);
    if (err != "")
    {
        err = di_name + " " + err;
    }

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
    bool operator()(const T & operand) const
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
    DataItem::IPFlags operator()(const T & operand) const
    {
        return DataItem::IPFlags::none;
    }

    DataItem::IPFlags operator()(const Div_u8_ipv4_t & operand) const
    {
        return static_cast<DataItem::IPFlags>(operand.field1);
    }

    DataItem::IPFlags operator()(const Div_ipv4_u8_t & operand) const
    {
        // This data item value doesn't have an explicit flags field,
        // so we'll fake the "add" flag.
        return DataItem::IPFlags::add;
    }

    DataItem::IPFlags operator()(const Div_u8_ipv6_t & operand) const
    {
        return static_cast<DataItem::IPFlags>(operand.field1);
    }

    DataItem::IPFlags operator()(const Div_ipv6_u8_t & operand) const
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
    { DataItemValueType::div_u8_ipv6_u8, "u8_ipv6_u8" }
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
