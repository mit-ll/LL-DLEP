/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016 Massachusetts Institute of Technology
 */

/// @file
/// Boost Unit Tests for Destination Advertisement.

#include <boost/test/unit_test.hpp>

#include "DlepInit.h"
#include "TestClientImpl.h"
#include "LogFile.h"

using namespace LLDLEP;

BOOST_AUTO_TEST_SUITE(destination_advertisement)

// config parameter name
std::string dest_advert_iface = "destination-advert-iface";

// Tests using a DLEP client in isolation with destination advertisement enabled
BOOST_AUTO_TEST_CASE(isolated_client)
{
    TestClientImpl modem_client;

    BOOST_REQUIRE(modem_client.parse_config_file("modem1_dest_advert.xml"));
    modem_client.set_loopback_iface(dest_advert_iface);

    // start up the DLEP service

    DlepService * dlep_service = DlepInit(modem_client);
    BOOST_REQUIRE(dlep_service != nullptr);

    BOOST_TEST_PASSPOINT();
    dlep_service->terminate();
    delete dlep_service;
    BOOST_TEST_PASSPOINT();
}

// Tests using two DLEP clients (both modems)
BOOST_AUTO_TEST_CASE(two_modems)
{
    // create and start modem1

    TestClientImpl modem1_client;

    BOOST_REQUIRE(modem1_client.parse_config_file("modem1_dest_advert.xml"));
    modem1_client.set_loopback_iface(dest_advert_iface);

    // start modem1 DLEP service

    DlepService * modem1_service = DlepInit(modem1_client);
    BOOST_REQUIRE(modem1_service != nullptr);

    // create and start modem2

    TestClientImpl modem2_client;

    BOOST_REQUIRE(modem2_client.parse_config_file("modem2_dest_advert.xml"));
    modem2_client.set_loopback_iface(dest_advert_iface);

    // start modem2 DLEP service

    DlepService * modem2_service = DlepInit(modem2_client);
    BOOST_REQUIRE(modem2_service != nullptr);

    // Give the protocol some time to work

    sleep(10);

    std::string nda = "new destination advertisement entry";

    // Load modem1's log file into memory

    std::string modem1_logname;
    modem1_client.get_config_parameter("log-file", &modem1_logname);
    LogFile modem1_log(modem1_logname);

    // check that the new destination advertisement message (from modem2)
    // is present in modem1's log

    BOOST_CHECK(modem1_log.find(0, nda) != -1);

    // Load modem2's log file into memory

    std::string modem2_logname;
    modem2_client.get_config_parameter("log-file", &modem2_logname);
    LogFile modem2_log(modem2_logname);

    // check that the new destination advertisement message (from modem1)
    // is present in modem2's log

    BOOST_CHECK(modem2_log.find(0, nda) != -1);

    // Tear down everything

    BOOST_TEST_PASSPOINT();
    modem1_service->terminate();
    delete modem1_service;

    BOOST_TEST_PASSPOINT();

    modem2_service->terminate();
    delete modem2_service;
    BOOST_TEST_PASSPOINT();
}

DlepMac
get_rfid(TestClientImpl & modem_client)
{
    std::vector<unsigned int> rfid;
    modem_client.get_config_parameter("destination-advert-rf-id", &rfid);
    DlepMac mac_rfid;

    for (unsigned int x : rfid)
    {
        mac_rfid.mac_addr.push_back(x);
    }

    return mac_rfid;
}

// Tests using four DLEP clients: router1---modem1-----modem2---router2
BOOST_AUTO_TEST_CASE(two_modems_two_routers)
{
    // create and start modem1
    TestClientImpl modem1_client;

    BOOST_REQUIRE(modem1_client.parse_config_file("modem1_dest_advert.xml"));
    modem1_client.set_loopback_iface(dest_advert_iface);

    // start modem1 DLEP service
    DlepService * modem1_service = DlepInit(modem1_client);
    BOOST_REQUIRE(modem1_service != nullptr);

    // create and start modem2
    TestClientImpl modem2_client;

    BOOST_REQUIRE(modem2_client.parse_config_file("modem2_dest_advert.xml"));
    modem2_client.set_loopback_iface(dest_advert_iface);

    // start modem2 DLEP service
    DlepService * modem2_service = DlepInit(modem2_client);
    BOOST_REQUIRE(modem2_service != nullptr);

    // create and start router1
    TestClientImpl router1_client;
    BOOST_REQUIRE(router1_client.parse_config_file("router1_config.xml"));
    router1_client.set_loopback_iface(dest_advert_iface);

    modem1_client.peer_up_waiter.prepare_to_wait();
    router1_client.peer_up_waiter.prepare_to_wait();

    DlepService * router1_service = DlepInit(router1_client);
    BOOST_REQUIRE(router1_service != nullptr);

    // check that modem1 and router1 peer up
    BOOST_CHECK(modem1_client.peer_up_waiter.wait_for_client_call());
    BOOST_CHECK(router1_client.peer_up_waiter.wait_for_client_call());

    // create and start router2
    TestClientImpl router2_client;
    BOOST_REQUIRE(router2_client.parse_config_file("router2_config.xml"));
    router2_client.set_loopback_iface(dest_advert_iface);

    modem2_client.peer_up_waiter.prepare_to_wait();
    router2_client.peer_up_waiter.prepare_to_wait();

    DlepService * router2_service = DlepInit(router2_client);
    BOOST_REQUIRE(router2_service != nullptr);

    // check that modem2 and router2 peer up
    BOOST_CHECK(modem2_client.peer_up_waiter.wait_for_client_call());
    BOOST_CHECK(router2_client.peer_up_waiter.wait_for_client_call());

    // get the rf ids of the modems
    DlepMac modem1_rfid = get_rfid(modem1_client);
    DlepMac modem2_rfid = get_rfid(modem2_client);

    DataItems data_items;
    DlepService::ReturnStatus r;

    // modem1 declares modem2's rfid to be up
    router1_client.destination_up_waiter.prepare_to_wait();
    r = modem1_service->destination_up(modem2_rfid, data_items);
    BOOST_REQUIRE(r == DlepService::ReturnStatus::ok);

    // check that router1 sees the destination up
    BOOST_REQUIRE(router1_client.destination_up_waiter.wait_for_client_call());

    // The MAC address should NOT be modem2's rf id because destination
    // advertisement should have translated it to the router's MAC address
    // (we should enhance this check to figure out the router's MAC address
    // look for for that instead)
    BOOST_CHECK(! router1_client.destination_up_waiter.check_result(modem2_rfid));

    // now do the same checks on the other side (modem2/router2)

    // modem2 declares modem1's rfid to be up
    router2_client.destination_up_waiter.prepare_to_wait();
    r = modem2_service->destination_up(modem1_rfid, data_items);
    BOOST_REQUIRE(r == DlepService::ReturnStatus::ok);

    // check that router1 sees the destination up
    BOOST_REQUIRE(router2_client.destination_up_waiter.wait_for_client_call());

    // The MAC address should NOT be modem2's rf id because destination
    // advertisement should have translated it to the router's MAC address
    // (we should enhance this check to figure out the router's MAC address
    // look for for that instead)
    BOOST_CHECK(! router2_client.destination_up_waiter.check_result(modem1_rfid));

    // Tear down everything
    BOOST_TEST_PASSPOINT();

    modem1_service->terminate();
    delete modem1_service;

    BOOST_TEST_PASSPOINT();

    modem2_service->terminate();
    delete modem2_service;

    BOOST_TEST_PASSPOINT();

    router1_service->terminate();
    delete router1_service;

    BOOST_TEST_PASSPOINT();

    router2_service->terminate();
    delete router2_service;
}


BOOST_AUTO_TEST_SUITE_END()
