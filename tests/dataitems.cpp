/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016, 2018 Massachusetts Institute of Technology
 */

/// @file
/// Boost Unit Tests for DataItems

#define BOOST_TEST_MODULE lib_tests
#include <boost/test/unit_test.hpp>

#include "ProtocolConfigImpl.h"

using namespace LLDLEP;
using namespace LLDLEP::internal;

BOOST_AUTO_TEST_SUITE(dataitems)

static ProtocolConfig *
get_protocol_config()
{
    DlepLoggerPtr logger(new DlepLogger());
    return new ProtocolConfigImpl("../config/protocol/protocol-config.xsd",
                                  "test-protocol-config.xml",
                                  logger);
}

template <typename T>
void
test_dataitem(const std::string & di_name, const DataItemValueType di_type,
              const std::size_t serialized_value_size,
              const T & test_value,
              bool expect_valid = true)
{
    ProtocolConfig * protocfg = get_protocol_config();

    const DataItemIdType di_id = protocfg->get_data_item_id(di_name);
    const std::size_t header_size =
        protocfg->get_data_item_id_size() +
        protocfg->get_data_item_length_size();
    const std::size_t expected_serialized_size =
        header_size + serialized_value_size;

    BOOST_TEST_MESSAGE(__FILE__ << ":" << __LINE__
                       << ": data item name=" << di_name
                       << " id=" << di_id
                       << " type=" << LLDLEP::to_string(di_type)
                       << " serialized value size=" << serialized_value_size
                       << " expect valid=" << expect_valid);

    // check that di_name has the expected type

    BOOST_CHECK_EQUAL(LLDLEP::to_string(protocfg->get_data_item_value_type(di_name)),
                      LLDLEP::to_string(di_type));

    // make the data item

    DataItemValue div = test_value;
    DataItem di(di_name, div, protocfg);

    BOOST_TEST_MESSAGE(__FILE__ << ":" << __LINE__ << ": " << di.to_string());

    // check that the data item has the expected type

    BOOST_CHECK(di.get_type() == di_type);

    // check that the serialized data item has the expected size

    std::vector<std::uint8_t> di_serialized = di.serialize();
    BOOST_CHECK_EQUAL(di_serialized.size(), expected_serialized_size);

    // reconstitute the data item in di2 from the serialized form

    DataItem di2(protocfg);
    auto it_begin = di_serialized.cbegin();
    di2.deserialize(it_begin, di_serialized.cend());

    // check that the deserialization consumed the whole buffer

    BOOST_CHECK(it_begin == di_serialized.cend());

    // check that the reconstituted data item looks like the original

    BOOST_CHECK_EQUAL(di.id, di2.id);
    T di2_value = boost::get<T>(di2.value);

    // We don't use BOOST_CHECK_EQUAL here because it requires operator<< to
    // be defined (to output the values), but it might not be for type T.
    BOOST_CHECK(di2_value == test_value);

    // check that to_string returns a non-empty string

    std::string di_string = di.to_string();
    BOOST_CHECK_GT(di_string.length(), 0);

    // reconstitute the value in di2 from di_string

    di2.from_string(di_string);

    // check that di and di2 are the same when converted to strings

    BOOST_CHECK_EQUAL(di.to_string(), di2.to_string());

    // check that di and di2 are equal

    BOOST_CHECK(di == di2);

    // validate both data items and check that the results are as
    // expected

    std::string err = di.validate();
    std::string err2 = di2.validate();
    BOOST_TEST_MESSAGE(__FILE__ << ":" << __LINE__
                       << ": validate err=" << err);
    BOOST_TEST_MESSAGE(__FILE__ << ":" << __LINE__
                       << ": validate err2=" << err2);
    if (expect_valid)
    {
        BOOST_CHECK_EQUAL(err, "");
        BOOST_CHECK_EQUAL(err2, "");
    }
    else
    {
        BOOST_CHECK_NE(err, "");
        BOOST_CHECK_NE(err2, "");
    }

    delete protocfg;
}

// test values for different data types

std::vector<std::uint8_t>  vu8  = {0, 31, 32, 33, 127, 128, 129, 255};
std::vector<std::uint16_t> vu16 = {0, 255, 32767, 32768, 32769, 65535};
std::vector<std::uint32_t> vu32 = {0, std::uint32_t(~0) / 2, std::uint32_t(~0)};
std::vector<std::uint64_t> vu64 = {0, std::uint64_t(~0) / 2, std::uint64_t(~0)};
std::vector<std::string> vstrings = {"", "X", "teststring"};
std::vector<boost::asio::ip::address_v4> vipv4 =
{
    boost::asio::ip::address_v4::from_string("0.0.0.0"),
    boost::asio::ip::address_v4::from_string("127.0.0.1")
};
std::vector<boost::asio::ip::address_v6> vipv6 =
{
    boost::asio::ip::address_v6::from_string("fe80::20c:29ff:fe84:fcba"),
    boost::asio::ip::address_v6::from_string("::1")
};

BOOST_AUTO_TEST_CASE(dataitem_blank)
{
    const std::string di_name = "Credit_Request";
    DataItemValueType di_type = DataItemValueType::blank;
    const std::size_t serialized_value_size = 0;

    test_dataitem<boost::blank>(di_name, di_type, serialized_value_size,
                                boost::blank());
}

BOOST_AUTO_TEST_CASE(dataitem_u8)
{
    const std::string di_name = ProtocolStrings::Resources_Receive;
    DataItemValueType di_type = DataItemValueType::div_u8;
    const std::size_t serialized_value_size = sizeof(std::uint8_t);

    for (auto u8 : vu8)
    {
        test_dataitem<std::uint8_t>(di_name, di_type, serialized_value_size,
                                    u8, (u8 <= 100));
    }
}

BOOST_AUTO_TEST_CASE(dataitem_u16)
{
    const std::string di_name = ProtocolStrings::Port;
    DataItemValueType di_type = DataItemValueType::div_u16;
    const std::size_t serialized_value_size = sizeof(std::uint16_t);

    for (auto u16 : vu16)
    {
        test_dataitem<std::uint16_t>(di_name, di_type, serialized_value_size,
                                     u16);
    }
}

BOOST_AUTO_TEST_CASE(dataitem_u32)
{
    const std::string di_name = ProtocolStrings::Heartbeat_Interval;
    DataItemValueType di_type = DataItemValueType::div_u32;
    const std::size_t serialized_value_size = sizeof(std::uint32_t);

    for (auto u32 : vu32)
    {
        test_dataitem<std::uint32_t>(di_name, di_type, serialized_value_size,
                                     u32);
    }
}

BOOST_AUTO_TEST_CASE(dataitem_u64)
{
    const std::string di_name = ProtocolStrings::Maximum_Data_Rate_Receive;
    DataItemValueType di_type = DataItemValueType::div_u64;
    const std::size_t serialized_value_size = sizeof(std::uint64_t);

    for (auto u64 : vu64)
    {
        test_dataitem<std::uint64_t>(di_name, di_type, serialized_value_size,
                                     u64);
    }
}

BOOST_AUTO_TEST_CASE(dataitem_string)
{
    const std::string di_name = ProtocolStrings::Peer_Type;
    DataItemValueType di_type = DataItemValueType::div_string;

    test_dataitem<std::string>(di_name, di_type, 0, "");

    std::string s("teststring");
    test_dataitem<std::string>(di_name, di_type, s.length(), s);
}

BOOST_AUTO_TEST_CASE(dataitem_dlepmac)
{
    const std::string di_name = ProtocolStrings::MAC_Address;
    DataItemValueType di_type = DataItemValueType::div_dlepmac;
    DlepMac mac;

    for (unsigned int i = 0; i < 6; i++)
    {
        mac.mac_addr.push_back(i);
        test_dataitem<DlepMac>(di_name, di_type, i + 1, mac);
    }
}

BOOST_AUTO_TEST_CASE(dataitem_v_u8)
{
    const std::string di_name = "Test_v_u8";
    DataItemValueType di_type = DataItemValueType::div_v_u8;
    std::vector<std::uint8_t> vu8;

    for (unsigned int i = 0; i < 10; i++)
    {
        test_dataitem<std::vector<std::uint8_t>>(di_name, di_type, i, vu8);
        vu8.push_back(i);
    }
}

BOOST_AUTO_TEST_CASE(dataitem_a2_u16)
{
    const std::string di_name = ProtocolStrings::Version;
    DataItemValueType di_type = DataItemValueType::div_a2_u16;

    for (auto i : vu16)
    {
        for (auto j : vu16)
        {
            std::array<std::uint16_t, 2> a2u16 { {i, j} };
            test_dataitem<std::array<std::uint16_t, 2>>(di_name, di_type,
                                                        4, a2u16);
        }
    }
}

BOOST_AUTO_TEST_CASE(dataitem_a2_u64)
{
    const std::string di_name = "Credit_Window_Status";
    DataItemValueType di_type = DataItemValueType::div_a2_u64;

    for (auto i : vu64)
    {
        for (auto j : vu64)
        {
            std::array<std::uint64_t, 2> a2u64 { {i, j} };
            test_dataitem<std::array<std::uint64_t, 2>>(di_name, di_type,
                                                        16, a2u64);
        }
    }
}

BOOST_AUTO_TEST_CASE(dataitem_u8_string)
{
    const std::string di_name = ProtocolStrings::Status;
    DataItemValueType di_type = DataItemValueType::div_u8_string;
    ProtocolConfig * protocfg = get_protocol_config();

    for (auto i : vu8)
    {
        // Figure out if i is a valid status code. If we can convert
        // the status ID to a string, it's valid (according to the
        // currrent protocol configuration), else it's not.

        bool expect_valid = true;
        try
        {
            (void)protocfg->get_status_code_name(i);
        }
        catch (ProtocolConfig::BadStatusCodeId)
        {
            expect_valid = false;
        }

        for (auto j : vstrings)
        {
            Div_u8_string_t div {i, j};
            std::size_t expected_size = 1 + j.length();
            test_dataitem<Div_u8_string_t>(di_name, di_type,
                                           expected_size, div, expect_valid);
        }
    }

    delete protocfg;
}

BOOST_AUTO_TEST_CASE(dataitem_u8_ipv4)
{
    const std::string di_name = ProtocolStrings::IPv4_Address;
    DataItemValueType di_type = DataItemValueType::div_u8_ipv4;

    for (auto u8 : vu8)
    {
        for (auto ip : vipv4)
        {
            Div_u8_ipv4_t div {u8, ip};
            test_dataitem<Div_u8_ipv4_t>(di_name, di_type,
                                         5, div, (u8 < 2));
        }
    }
}

BOOST_AUTO_TEST_CASE(dataitem_ipv4_u8)
{
    const std::string di_name = ProtocolStrings::IPv4_Attached_Subnet;
    DataItemValueType di_type = DataItemValueType::div_ipv4_u8;

    for (auto u8 : vu8)
    {
        for (auto ip : vipv4)
        {
            Div_ipv4_u8_t div {ip, u8};
            test_dataitem<Div_ipv4_u8_t>(di_name, di_type,
                                         5, div, (u8 <= 32));
        }
    }
}

BOOST_AUTO_TEST_CASE(dataitem_u8_ipv6)
{
    const std::string di_name = ProtocolStrings::IPv6_Address;
    DataItemValueType di_type = DataItemValueType::div_u8_ipv6;

    for (auto u8 : vu8)
    {
        for (auto ip : vipv6)
        {
            Div_u8_ipv6_t div {u8, ip};
            test_dataitem<Div_u8_ipv6_t>(di_name, di_type,
                                         17, div, (u8 < 2));
        }
    }
}

BOOST_AUTO_TEST_CASE(dataitem_ipv6_u8)
{
    const std::string di_name = ProtocolStrings::IPv6_Attached_Subnet;
    DataItemValueType di_type = DataItemValueType::div_ipv6_u8;

    for (auto u8 : vu8)
    {
        for (auto ip : vipv6)
        {
            Div_ipv6_u8_t div {ip, u8};
            test_dataitem<Div_ipv6_u8_t>(di_name, di_type,
                                         17, div, (u8 <= 128));
        }
    }
}

BOOST_AUTO_TEST_CASE(dataitem_v_extid)
{
    const std::string di_name = ProtocolStrings::Extensions_Supported;
    DataItemValueType di_type = DataItemValueType::div_v_extid;
    ProtocolConfig * protocfg = get_protocol_config();

    std::size_t extid_size = protocfg->get_extension_id_size();

    Div_v_extid_t div;
    for (unsigned int i = 0; i < 10; i++)
    {
        test_dataitem<Div_v_extid_t>(di_name, di_type,
                                     i * extid_size, div);
        div.field1.push_back(i);
    }

    delete protocfg;
}

BOOST_AUTO_TEST_CASE(dataitem_u8_ipv4_u16)
{
    const std::string di_name = "Test_div_u8_ipv4_u16";
    DataItemValueType di_type = DataItemValueType::div_u8_ipv4_u16;

    for (auto u8 : vu8)
    {
        for (auto ip : vipv4)
        {
            for (auto u16 : vu16)
            {
                // if u16 is zero, it won't be serialized at the end of
                // the data item (the "port" is optional), so account
                // for the difference in the data item's length
                size_t u16_len = (u16 == 0) ? 0 : 2;

                Div_u8_ipv4_u16_t div {u8, ip, u16};
                test_dataitem<Div_u8_ipv4_u16_t>(di_name, di_type,
                                                 5 + u16_len, div);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(dataitem_u8_ipv6_u16)
{
    const std::string di_name = "Test_div_u8_ipv6_u16";
    DataItemValueType di_type = DataItemValueType::div_u8_ipv6_u16;

    for (auto u8 : vu8)
    {
        for (auto ip : vipv6)
        {
            for (auto u16 : vu16)
            {
                // if u16 is zero, it won't be serialized at the end of
                // the data item (the "port" is optional), so account
                // for the difference in the data item's length
                size_t u16_len = (u16 == 0) ? 0 : 2;

                Div_u8_ipv6_u16_t div {u8, ip, u16};
                test_dataitem<Div_u8_ipv6_u16_t>(di_name, di_type,
                                                 17 + u16_len, div);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(dataitem_u8_ipv4_u8)
{

    const std::string di_name = "Test_div_u8_ipv4_u8";
    DataItemValueType di_type = DataItemValueType::div_u8_ipv4_u8;

    for (auto u8 : vu8)
    {
        for (auto ip : vipv4)
        {
            for (auto u8_2 : vu8)
            {
                Div_u8_ipv4_u8_t div {u8, ip, u8_2};
                test_dataitem<Div_u8_ipv4_u8_t>(di_name, di_type,
                                                6, div,
                                                ((u8 < 2) && (u8_2 <= 32)));
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(dataitem_u8_ipv6_u8)
{
    const std::string di_name = "Test_div_u8_ipv6_u8";
    DataItemValueType di_type = DataItemValueType::div_u8_ipv6_u8;

    for (auto u8 : vu8)
    {
        for (auto ip : vipv6)
        {
            for (auto u8_2 : vu8)
            {
                Div_u8_ipv6_u8_t div {u8, ip, u8_2};
                test_dataitem<Div_u8_ipv6_u8_t>(di_name, di_type,
                                                18, div,
                                                ((u8 < 2) && (u8_2 <= 128)));
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(dataitem_u64_u64)
{
    const std::string di_name = "Test_u64_u64";
    DataItemValueType di_type = DataItemValueType::div_u64_u64;
    const std::size_t serialized_value_size = sizeof(std::uint64_t) * 2;

    for (auto u64_1 : vu64)
    {
        for (auto u64_2 : vu64)
        {
            Div_u64_u64_t div {u64_1, u64_2};
            test_dataitem<Div_u64_u64_t>(di_name, di_type, serialized_value_size,
                                          div);
        }
    }
}

BOOST_AUTO_TEST_CASE(dataitem_with_sub_data_items)
{
    const std::string di_name = "Test_parent_data_item";
    DataItemValueType di_type = DataItemValueType::div_sub_data_items;
    ProtocolConfig * protocfg = get_protocol_config();
    DataItemInfo di_info = protocfg->get_data_item_info(di_name);

    std::uint64_t div_u64{0};
    DataItem di_u64_1("Test_u64_1", div_u64, protocfg, &di_info);
    DataItem di_u64_1p("Test_u64_1+", div_u64, protocfg, &di_info);
    DataItem di_u64_01("Test_u64_0-1", div_u64, protocfg, &di_info);
    DataItem di_u64_0p("Test_u64_0+", div_u64, protocfg, &di_info);
    DataItem di_peer("Peer_Type", "12345678", protocfg);

    struct DataItemListModifications
    {
        // "add" to add data item
        // "remfirst" to remove first data item
        // "remlast"  to remove last  data item
        std::string operation;
        DataItem di; // if adding, data item to add
        bool valid; // after this add/remove, is the list expected to validate?
    };

    std::vector<DataItemListModifications> mods = {
        { "add",      di_u64_1,   false },
        { "add",      di_u64_1p,  true },
        { "add",      di_u64_1p,  true },
        { "add",      di_u64_0p,  true },
        { "add",      di_u64_0p,  true },
        { "add",      di_u64_01,  true },
        { "add",      di_u64_01,  false },
        { "remlast",  di_u64_01,  true },
        { "add",      di_u64_1,   false },
        { "remlast",  di_u64_1,   true },
        { "add",      di_peer,    false },
        { "remlast",  di_peer,    true },
        { "remfirst", di_u64_1,   false },
    };

    Div_sub_data_items_t div;
    const std::size_t serialized_value_header_size =
        protocfg->get_data_item_id_size() +
        protocfg->get_data_item_length_size();

    for (auto const & m : mods)
    {
        if (m.operation == "add")
        {
            div.sub_data_items.push_back(m.di);
        }
        else if (m.operation == "remlast")
        {
            div.sub_data_items.pop_back();
        }
        else if (m.operation == "remfirst")
        {
            div.sub_data_items.erase(div.sub_data_items.begin());
        }

        std::size_t s = div.sub_data_items.size() *
            (serialized_value_header_size + sizeof(std::uint64_t));

        test_dataitem<Div_sub_data_items_t>(di_name, di_type, s, div, m.valid);
    }

    delete protocfg;
}

BOOST_AUTO_TEST_SUITE_END()
