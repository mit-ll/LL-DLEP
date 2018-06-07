/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016 Massachusetts Institute of Technology
 */

/// @file
/// Boost Unit Tests for the DLEP library interface.

#include <boost/test/unit_test.hpp>

#include "DlepInit.h"
#include "TestClientImpl.h"

using namespace LLDLEP;

BOOST_AUTO_TEST_SUITE(lib_interface)

// Test fixtures for starting/stopping Dlep clients and services.  I
// started to make these as BOOST test fixtures, but I found them easier
// to use them un-BOOST-ified.

struct DlepServiceFixture
{
    explicit DlepServiceFixture(TestClientImpl & client)
    {
        dlep_service = DlepInit(client);
        BOOST_REQUIRE(dlep_service != nullptr);
    }

    ~DlepServiceFixture()
    {
        BOOST_TEST_MESSAGE(__FILE__ << ":" << __LINE__
                           << ": terminating DlepService");

        BOOST_TEST_PASSPOINT();
        dlep_service->terminate();

        BOOST_TEST_PASSPOINT();
        delete dlep_service;
    }

    DlepService * dlep_service;
};

struct DlepClientAndServiceFixture
{
    explicit DlepClientAndServiceFixture(const std::string & config_file_name)
    {
        BOOST_TEST_MESSAGE(__FILE__ << ":" << __LINE__
                           << ": client config file=" << config_file_name);
        BOOST_REQUIRE(client.parse_config_file(config_file_name.c_str()));
        client.print_config();
        dlep_service = DlepInit(client);
        BOOST_REQUIRE(dlep_service != nullptr);
    }

    ~DlepClientAndServiceFixture()
    {
        BOOST_TEST_MESSAGE(__FILE__ << ":" << __LINE__
                           << ": terminating DlepService");

        BOOST_TEST_PASSPOINT();
        dlep_service->terminate();

        BOOST_TEST_PASSPOINT();
        delete dlep_service;
    }

    TestClientImpl client;
    DlepService * dlep_service;
};

struct DlepModemRouterFixture
{
    explicit DlepModemRouterFixture(const std::string & modem_config_file,
                           const std::string & router_config_file)
    {
        BOOST_TEST_MESSAGE(__FILE__ << ":" << __LINE__
                           << ": modem config file=" << modem_config_file
                           << " router config file=" << router_config_file);

        // create our modem client
        BOOST_REQUIRE(modem_client.parse_config_file(
                          modem_config_file.c_str()));
        modem_client.print_config();

        // create our router client
        BOOST_REQUIRE(router_client.parse_config_file(
                          router_config_file.c_str()));
        router_client.print_config();

        // create modem service
        modem_client.peer_up_waiter.prepare_to_wait();
        modem_service = DlepInit(modem_client);
        BOOST_REQUIRE(modem_service != nullptr);

        // create router service
        router_client.peer_up_waiter.prepare_to_wait();
        router_service = DlepInit(router_client);
        BOOST_REQUIRE(router_service != nullptr);

        // check that the modem and router peer up
        BOOST_CHECK(modem_client.peer_up_waiter.wait_for_client_call());
        BOOST_CHECK(router_client.peer_up_waiter.wait_for_client_call());

        // check that the router has one peer
        std::vector<std::string> peers;
        DlepService::ReturnStatus r;
        r = router_service->get_peers(peers);
        BOOST_REQUIRE(r == DlepService::ReturnStatus::ok);
        BOOST_CHECK_EQUAL(peers.size(), 1);

        // check that the modem has one peer
        peers.clear();
        r = modem_service->get_peers(peers);
        BOOST_REQUIRE(r == DlepService::ReturnStatus::ok);
        BOOST_CHECK_EQUAL(peers.size(), 1);
    }

    ~DlepModemRouterFixture()
    {
        BOOST_TEST_MESSAGE(__FILE__ << ":" << __LINE__
                           << ": terminating modem DlepService");

        BOOST_TEST_PASSPOINT();
        modem_service->terminate();

        BOOST_TEST_PASSPOINT();
        delete modem_service;

        BOOST_TEST_MESSAGE(__FILE__ << ":" << __LINE__
                           << ": terminating router DlepService");

        BOOST_TEST_PASSPOINT();
        router_service->terminate();

        BOOST_TEST_PASSPOINT();
        delete router_service;
    }

    TestClientImpl modem_client;
    DlepService * modem_service;

    TestClientImpl router_client;
    DlepService * router_service;
};

// Tests using a DLEP client in isolation, i.e., without starting DlepService
// cppcheck-suppress unusedFunction
BOOST_AUTO_TEST_CASE(isolated_client)
{
    // create our client
    TestClientImpl client;

    // parse a config file with a bad parameter name
    BOOST_CHECK(! client.parse_config_file("bad_paramname_config.xml"));

    // parse a config file with a bad parameter value (string instead of int)
    BOOST_CHECK(! client.parse_config_file("bad_paramval1_config.xml"));

    // parse a config file with a bad parameter value
    // (string instead of multicast address)
    BOOST_CHECK(! client.parse_config_file("bad_paramval2_config.xml"));

    // parse a config file that is not in xml format
    BOOST_CHECK(! client.parse_config_file("Makefile"));

    // parse a good config file
    BOOST_CHECK(client.parse_config_file("test_modem_config.xml"));

    // parse another good config file
    BOOST_CHECK(client.parse_config_file("test_router_config.xml"));
}

// Tests using a DLEP modem in isolation, i.e., without a router peer
BOOST_AUTO_TEST_CASE(isolated_modem)
{
    DlepClientAndServiceFixture fixture("test_modem_config.xml");
}

// Tests using a DLEP router in isolation, i.e., without a modem peer
BOOST_AUTO_TEST_CASE(isolated_router)
{
    DlepClientAndServiceFixture fixture("test_router_config.xml");
}

struct ConfigFiles
{
    std::string modem_config_file;
    std::string router_config_file;
};

std::vector<ConfigFiles> config_files
{
    { "test_modem_config.xml", "test_router_config.xml" }
};

// Test peering when modem comes up first, then router
BOOST_AUTO_TEST_CASE(peering_modem_then_router)
{
    for (const auto & cf : config_files)
    {
        DlepModemRouterFixture mrf(cf.modem_config_file,
                                   cf.router_config_file);
    }
}

// Test peering when router comes up first, then modem
BOOST_AUTO_TEST_CASE(peering_router_then_modem)
{
    for (const auto & cf : config_files)
    {
        BOOST_TEST_MESSAGE(__FILE__ << ":" << __LINE__
                           << ": modem config file=" << cf.modem_config_file
                           << " router config file=" << cf.router_config_file);

        // create our modem client
        TestClientImpl modem_client;
        BOOST_REQUIRE(modem_client.parse_config_file(
                          cf.modem_config_file.c_str()));

        // create our router client
        TestClientImpl router_client;
        BOOST_REQUIRE(router_client.parse_config_file(
                          cf.router_config_file.c_str()));

        router_client.peer_up_waiter.prepare_to_wait();
        DlepServiceFixture router_fixture(router_client);

        // give the router time to start
        sleep(3);

        modem_client.peer_up_waiter.prepare_to_wait();
        DlepServiceFixture modem_fixture(modem_client);

        // check that the modem and router peer up
        BOOST_CHECK(modem_client.peer_up_waiter.wait_for_client_call());
        BOOST_CHECK(router_client.peer_up_waiter.wait_for_client_call());
    }
}

// Tests of peer update sent by modem
BOOST_AUTO_TEST_CASE(peer_update_from_modem)
{
    for (const auto & cf : config_files)
    {
        DlepModemRouterFixture mrf(cf.modem_config_file,
                                   cf.router_config_file);

        // Now update the peer with a new metric
        mrf.router_client.peer_update_waiter.prepare_to_wait();
        DataItems data_items;
        DataItem di(ProtocolStrings::Latency, std::uint64_t(1000),
                    mrf.modem_service->get_protocol_config());
        data_items.push_back(di);
        DlepService::ReturnStatus r =
           mrf.modem_service->peer_update(data_items);
        BOOST_REQUIRE(r == DlepService::ReturnStatus::ok);

        // check that the router sees the peer update
        BOOST_REQUIRE(mrf.router_client.peer_update_waiter.wait_for_client_call());
    }
}

// Tests of peer update sent by router
BOOST_AUTO_TEST_CASE(peer_update_from_router)
{
    for (const auto & cf : config_files)
    {
        DlepModemRouterFixture mrf(cf.modem_config_file,
                                   cf.router_config_file);

        // Now update the peer with a new metric
        mrf.modem_client.peer_update_waiter.prepare_to_wait();
        DataItems data_items;
        DataItem di(ProtocolStrings::Latency, std::uint64_t(1000),
                    mrf.modem_service->get_protocol_config());
        data_items.push_back(di);
        DlepService::ReturnStatus r =
           mrf.router_service->peer_update(data_items);
        BOOST_REQUIRE(r == DlepService::ReturnStatus::ok);

        // check that the router sees the peer update
        BOOST_REQUIRE(mrf.modem_client.peer_update_waiter.wait_for_client_call());
    }
}

// Tests of destination up/down
BOOST_AUTO_TEST_CASE(destination_up_down)
{
    for (const auto & cf : config_files)
    {
        DlepModemRouterFixture mrf(cf.modem_config_file,
                                   cf.router_config_file);

        // Give the modem service a destination_up
        DlepMac mac = {{1, 2, 3, 4, 5, 6}};
        DlepService::ReturnStatus r;
        DataItems data_items; // empty

        mrf.router_client.destination_up_waiter.prepare_to_wait();
        r = mrf.modem_service->destination_up(mac, data_items);
        BOOST_REQUIRE(r == DlepService::ReturnStatus::ok);

        // check that the router sees the destination up
        BOOST_REQUIRE(mrf.router_client.destination_up_waiter.wait_for_client_call());
        BOOST_CHECK(mrf.router_client.destination_up_waiter.check_result(mac));

        // Now take the destination down
        mrf.router_client.destination_down_waiter.prepare_to_wait();
        r = mrf.modem_service->destination_down(mac);
        BOOST_REQUIRE(r == DlepService::ReturnStatus::ok);

        // check that the router sees the destination down
        BOOST_REQUIRE(mrf.router_client.destination_down_waiter.wait_for_client_call());
        BOOST_CHECK(mrf.router_client.destination_down_waiter.check_result(mac));

        // Now take the destination down AGAIN, which is an error that should
        // be caught by the library.
        r = mrf.modem_service->destination_down(mac);
        BOOST_REQUIRE(r == DlepService::ReturnStatus::destination_does_not_exist);
    }
}

// Tests of destination update
BOOST_AUTO_TEST_CASE(destination_update)
{
    for (const auto & cf : config_files)
    {
        DlepModemRouterFixture mrf(cf.modem_config_file,
                                   cf.router_config_file);

        // Give the modem service a destination_up
        DlepMac mac = {{61, 62, 63, 64, 65, 66}};
        DlepService::ReturnStatus r;
        DataItems data_items;

        mrf.router_client.destination_up_waiter.prepare_to_wait();
        r = mrf.modem_service->destination_up(mac, data_items);
        BOOST_REQUIRE(r == DlepService::ReturnStatus::ok);

        // check that the router sees the destination up
        BOOST_REQUIRE(mrf.router_client.destination_up_waiter.wait_for_client_call());
        BOOST_CHECK(mrf.router_client.destination_up_waiter.check_result(mac));

        // Now update the destination with a new metric
        mrf.router_client.destination_update_waiter.prepare_to_wait();
        DataItem di(ProtocolStrings::Latency, std::uint64_t(1000),
                    mrf.modem_service->get_protocol_config());
        data_items.push_back(di);
        r = mrf.modem_service->destination_update(mac, data_items);
        BOOST_REQUIRE(r == DlepService::ReturnStatus::ok);

        // check that the router sees the destination update
        BOOST_REQUIRE(mrf.router_client.destination_update_waiter.wait_for_client_call());
        BOOST_CHECK(mrf.router_client.destination_update_waiter.check_result(mac));

    }
}

static bool
compare_data_item_lists(const DataItems & dilist1, const DataItems & dilist2)
{
    return std::is_permutation(dilist1.begin(), dilist1.end(), dilist2.begin());
}

void
destination_update_ip_list(DlepModemRouterFixture & mrf,
                           const DlepMac & mac,
                           const DataItems & add_data_items,
                           const DataItems & drop_data_items)
{
    BOOST_TEST_MESSAGE(__FILE__ << ":" << __LINE__ << " "
                       << __FUNCTION__ << " mac=" << mac
                       << " # data items=" << add_data_items.size());

    mrf.router_client.destination_up_waiter.prepare_to_wait();
    DataItems empty_data_items;
    DlepService::ReturnStatus r = mrf.modem_service->destination_up(mac,
                                                            empty_data_items);
    BOOST_REQUIRE(r == DlepService::ReturnStatus::ok);

    // check that the router sees the destination up
    BOOST_REQUIRE(mrf.router_client.destination_up_waiter.wait_for_client_call());
    BOOST_CHECK(mrf.router_client.destination_up_waiter.check_result(mac));

    // Now update the destination with add_data_items
    mrf.router_client.destination_update_waiter.prepare_to_wait();
    r = mrf.modem_service->destination_update(mac, add_data_items);
    BOOST_REQUIRE(r == DlepService::ReturnStatus::ok);

    // check that the router sees the destination update
    BOOST_REQUIRE(mrf.router_client.destination_update_waiter.wait_for_client_call());
    BOOST_CHECK(mrf.router_client.destination_update_waiter.check_result(mac));

    // check that the router has the expected IP data items on the destination
    std::vector<std::string> peers;
    r = mrf.router_service->get_peers(peers);
    BOOST_REQUIRE(r == DlepService::ReturnStatus::ok);
    BOOST_CHECK_EQUAL(peers.size(), 1);
    DestinationInfo dest_info;
    r = mrf.router_service->get_destination_info(peers[0], mac, dest_info);
    BOOST_REQUIRE(r == DlepService::ReturnStatus::ok);
    BOOST_CHECK(compare_data_item_lists(add_data_items, dest_info.data_items));

    // update the destination with a removal of the IP data items
    mrf.router_client.destination_update_waiter.prepare_to_wait();
    r = mrf.modem_service->destination_update(mac, drop_data_items);
    BOOST_REQUIRE(r == DlepService::ReturnStatus::ok);

    // check that the router sees the destination update
    BOOST_REQUIRE(mrf.router_client.destination_update_waiter.wait_for_client_call());
    BOOST_CHECK(mrf.router_client.destination_update_waiter.check_result(mac));

    // check that the destination has no data items now
    dest_info.data_items.clear();
    r = mrf.router_service->get_destination_info(peers[0], mac, dest_info);
    BOOST_REQUIRE(r == DlepService::ReturnStatus::ok);
    BOOST_REQUIRE(dest_info.data_items.empty());

    // bring the destination down
    mrf.router_client.destination_down_waiter.prepare_to_wait();
    r = mrf.modem_service->destination_down(mac);
    BOOST_REQUIRE(r == DlepService::ReturnStatus::ok);

    // check that the router sees the destination down
    BOOST_REQUIRE(mrf.router_client.destination_down_waiter.wait_for_client_call());
    BOOST_CHECK(mrf.router_client.destination_down_waiter.check_result(mac));
}

static std::vector<boost::asio::ip::address_v4> vipv4 =
{
    boost::asio::ip::address_v4::from_string("10.0.0.0"),
    boost::asio::ip::address_v4::from_string("168.0.0.1")
};

static std::vector<boost::asio::ip::address_v6> vipv6 =
{
    boost::asio::ip::address_v6::from_string("fe80::20c:29ff:fe84:fcba"),
    boost::asio::ip::address_v6::from_string("::1")
};

// Populate add_data_items and drop_data_items
static void
destination_update_populate_ip_data_items(DlepModemRouterFixture & mrf,
                                          DataItems & add_data_items,
                                          DataItems & drop_data_items)
{
    // add IPv4 data items

    for (const auto & ipv4addr : vipv4)
    {
        // IPv4 Address
        Div_u8_ipv4_t div_ipv4 {DataItem::IPFlags::add, ipv4addr};
        DataItem di_ipv4 (ProtocolStrings::IPv4_Address, div_ipv4,
                          mrf.modem_service->get_protocol_config());
        add_data_items.push_back(di_ipv4);

        // add the corresponding drop data item
        div_ipv4.field1 = DataItem::IPFlags::none;
        di_ipv4.value = div_ipv4;
        drop_data_items.push_back(di_ipv4);

        // IPv4 Attached Subnet
        Div_u8_ipv4_u8_t div_ipv4subnet  {DataItem::IPFlags::add, ipv4addr, 24};
        DataItem di_ipv4subnet (ProtocolStrings::IPv4_Attached_Subnet, div_ipv4subnet,
                          mrf.modem_service->get_protocol_config());
        add_data_items.push_back(di_ipv4subnet);

        // add the corresponding drop data item
        div_ipv4subnet.field1 = DataItem::IPFlags::none;
        di_ipv4subnet.value = div_ipv4subnet;
        drop_data_items.push_back(di_ipv4subnet);
    }

    // add IPv6 data items

    for (const auto & ipv6addr : vipv6)
    {
        // IPv6 Address
        Div_u8_ipv6_t div_ipv6 {DataItem::IPFlags::add, ipv6addr};
        DataItem di_ipv6 (ProtocolStrings::IPv6_Address, div_ipv6,
                          mrf.modem_service->get_protocol_config());
        add_data_items.push_back(di_ipv6);

        // add the corresponding drop data item
        div_ipv6.field1 = DataItem::IPFlags::none;
        di_ipv6.value = div_ipv6;
        drop_data_items.push_back(di_ipv6);

        // IPv6 Attached Subnet
        Div_u8_ipv6_u8_t div_ipv6subnet  {DataItem::IPFlags::add, ipv6addr, 64};
        DataItem di_ipv6subnet (ProtocolStrings::IPv6_Attached_Subnet, div_ipv6subnet,
                          mrf.modem_service->get_protocol_config());
        add_data_items.push_back(di_ipv6subnet);

        // add the corresponding drop data item
        div_ipv6subnet.field1 = DataItem::IPFlags::none;
        di_ipv6subnet.value = div_ipv6subnet;
        drop_data_items.push_back(di_ipv6subnet);
    }

    BOOST_CHECK_EQUAL(add_data_items.size(), drop_data_items.size());
}

BOOST_AUTO_TEST_CASE(destination_update_ip)
{
    DataItems add_data_items, drop_data_items;
    DlepMac mac = {{0x10, 0x22, 0x33, 0x44, 0x55, 0x66}};
    bool first_time = true;

    for (const auto & cf : config_files)
    {
        DlepModemRouterFixture mrf(cf.modem_config_file,
                                   cf.router_config_file);
        if (first_time)
        {
            destination_update_populate_ip_data_items(mrf,
                                                      add_data_items,
                                                      drop_data_items);
            first_time = false;
        }

        unsigned int num_data_items = add_data_items.size();
        unsigned int num_combinations = (1 << num_data_items) - 1;
        for (unsigned int combination = 0;
             combination < num_combinations;
             combination++)
        {
            // Use a new MAC address each time to make it easier to find
            // in the logs
            mac.mac_addr[0]++;
            if (mac.mac_addr[0] == 0)
            {
                mac.mac_addr[1]++;
            }

            // Use combination to pick a subset of add/drop data items to use
            DataItems add_di, drop_di;

            for (unsigned int data_item_index = 0;
                 data_item_index < num_data_items;
                 data_item_index++)
            {
                if ( (1 << data_item_index) & combination)
                {
                    add_di.push_back (add_data_items[data_item_index]);
                    drop_di.push_back(drop_data_items[data_item_index]);
                }
            }
            destination_update_ip_list(mrf, mac, add_di, drop_di);
        }
    }
}

BOOST_AUTO_TEST_CASE(destination_update_ip_redundant_add)
{
    DataItems data_items;
    DlepMac mac = {{0x20, 0x22, 0x33, 0x44, 0x55, 0x66}};

    for (const auto & cf : config_files)
    {
        DlepModemRouterFixture mrf(cf.modem_config_file,
                                   cf.router_config_file);

        // IPv4 Address
        Div_u8_ipv4_t div_ipv4 {DataItem::IPFlags::add, vipv4[0]};
        DataItem di_ipv4 (ProtocolStrings::IPv4_Address, div_ipv4,
                          mrf.modem_service->get_protocol_config());
        data_items.push_back(di_ipv4);

        // send destination_up to the router
        mrf.router_client.destination_up_waiter.prepare_to_wait();
        DlepService::ReturnStatus r = mrf.modem_service->destination_up(mac,
                                                                        data_items);
        BOOST_REQUIRE(r == DlepService::ReturnStatus::ok);

        // check that the router sees the destination up
        BOOST_REQUIRE(mrf.router_client.destination_up_waiter.wait_for_client_call());
        BOOST_CHECK(mrf.router_client.destination_up_waiter.check_result(mac));

        // Now update the destination with the same data_items
        // this is an error that should tear down the DLEP session
        mrf.router_client.peer_down_waiter.prepare_to_wait();
        r = mrf.modem_service->destination_update(mac, data_items);
        BOOST_REQUIRE(r == DlepService::ReturnStatus::ok);

        // check that the router sees the peer down
        BOOST_REQUIRE(mrf.router_client.peer_down_waiter.wait_for_client_call());
    }
}

BOOST_AUTO_TEST_CASE(destination_update_ip_nonexistent_remove)
{
    DataItems data_items;
    DlepMac mac = {{0x30, 0x22, 0x33, 0x44, 0x55, 0x66}};

    for (const auto & cf : config_files)
    {
        DlepModemRouterFixture mrf(cf.modem_config_file,
                                   cf.router_config_file);

        // send destination_up to the router with no data items
        mrf.router_client.destination_up_waiter.prepare_to_wait();
        DlepService::ReturnStatus r = mrf.modem_service->destination_up(mac,
                                                                        data_items);
        BOOST_REQUIRE(r == DlepService::ReturnStatus::ok);

        // check that the router sees the destination up
        BOOST_REQUIRE(mrf.router_client.destination_up_waiter.wait_for_client_call());
        BOOST_CHECK(mrf.router_client.destination_up_waiter.check_result(mac));

        // create an IPv4 Address for removal
        Div_u8_ipv4_t div_ipv4 {DataItem::IPFlags::none, vipv4[0]};
        DataItem di_ipv4 (ProtocolStrings::IPv4_Address, div_ipv4,
                          mrf.modem_service->get_protocol_config());
        data_items.push_back(di_ipv4);

        // update the destination, removing a nonexistent IPv4 address
        // this is an error that should tear down the DLEP session
        mrf.router_client.peer_down_waiter.prepare_to_wait();
        r = mrf.modem_service->destination_update(mac, data_items);
        BOOST_REQUIRE(r == DlepService::ReturnStatus::ok);

        // check that the router sees the peer down
        BOOST_REQUIRE(mrf.router_client.peer_down_waiter.wait_for_client_call());
    }
}

// Tests of destination announce
BOOST_AUTO_TEST_CASE(destination_announce)
{
    for (const auto & cf : config_files)
    {
        DlepModemRouterFixture mrf(cf.modem_config_file,
                                   cf.router_config_file);

        // Give the modem service a destination announce
        DlepMac mac = {{71, 72, 73, 74, 75, 76}};
        DlepService::ReturnStatus r;
        DataItems data_items;

        mrf.modem_client.destination_up_waiter.prepare_to_wait();
        r = mrf.router_service->destination_up(mac, data_items);
        BOOST_REQUIRE(r == DlepService::ReturnStatus::ok);

        // check that the modem sees the destination announce
        BOOST_REQUIRE(mrf.modem_client.destination_up_waiter.wait_for_client_call());
        BOOST_CHECK(mrf.modem_client.destination_up_waiter.check_result(mac));

        // Now have the modem send the reply.  At the API level, this
        // looks like a destination up, but at the protocol level it
        // will be a destination announce response.
        mrf.router_client.destination_up_waiter.prepare_to_wait();
        mrf.modem_service->destination_up(mac, data_items);

        // check that the router sees the destination announce reply
        BOOST_REQUIRE(mrf.router_client.destination_up_waiter.wait_for_client_call());
        BOOST_CHECK(mrf.router_client.destination_up_waiter.check_result(mac));
    }
}

BOOST_AUTO_TEST_SUITE_END()
