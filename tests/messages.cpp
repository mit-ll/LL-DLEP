/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016 Massachusetts Institute of Technology
 */

/// @file
/// Boost Unit Tests for ProtocolMessages.

#include <boost/test/unit_test.hpp>

#include "ProtocolMessage.h"
#include "ProtocolConfigImpl.h"
#include "TestClientImpl.h"

using namespace LLDLEP;
using namespace LLDLEP::internal;

static DlepLoggerPtr logger(new DlepLogger("test-messages-dlep.log",
                                           DLEP_LOG_ERROR));

// All tests will be run with each of the following protocol config files
static std::vector<std::string> proto_config_files =
{
    "../config/protocol/dlep-draft-29.xml",
    "../config/protocol/dlep-rfc-8175.xml",
};

ProtocolConfig * get_protocfg(const std::string & pcf)
{
    BOOST_TEST_MESSAGE("Using protocol config file " << pcf);

    return new ProtocolConfigImpl("../config/protocol/protocol-config.xsd",
                                  pcf, logger);
}

BOOST_AUTO_TEST_SUITE(messages)

static void
check_header_size(ProtocolMessage & pm, ProtocolConfig * protocfg)
{
    std::size_t expected_header_size =
        protocfg->get_signal_id_size() + protocfg->get_signal_length_size();

    if (pm.is_signal())
    {
        std::string prefix = protocfg->get_signal_prefix();
        expected_header_size += prefix.length();
    }

    BOOST_CHECK_EQUAL(expected_header_size, pm.get_length());
}


static void
check_serialization(ProtocolMessage & pm, const DataItems & data_items,
                    bool modem_sender,
                    ProtocolConfig * protocfg)
{
    std::uint32_t sender_flag = 0;
    std::string sender_name;
    if (modem_sender)
    {
        sender_flag = ProtocolConfig::SignalInfo::Flags::modem_sends;
        sender_name = "modem";
    }
    else
    {
        sender_flag = ProtocolConfig::SignalInfo::Flags::router_sends;
        sender_name = "router";
    }

    BOOST_TEST_MESSAGE(__func__ << " sent by " << sender_name);

    pm.clear_data_items();
    pm.add_data_items(data_items);

    // Check that the original message can parse

    std::string err = pm.parse(__func__);
    BOOST_REQUIRE_EQUAL(err, "");

    ProtocolConfig::SignalInfo siginfo =
        protocfg->get_signal_info(pm.get_signal_name());

    // Check that the original message is valid

    err = pm.validate(modem_sender);

    // if the side specified by modem_sender allowed to send this
    // message, validation should succeed, else it should fail.  If it
    // fails, we do no further checking.

    if (siginfo.flags & sender_flag)
    {
        BOOST_REQUIRE_EQUAL(err, "");
    }
    else
    {
        BOOST_REQUIRE_NE(err, "");
        return;
    }

    // Reconstitute the message in rpm from its serialized form and
    // check that the reconstituted message is valid

    ProtocolMessage rpm {protocfg, logger};

    err = rpm.parse_and_validate(pm.get_buffer(), pm.get_length(),
                                 pm.is_signal(), modem_sender, __func__);
    BOOST_REQUIRE_EQUAL(err, "");

    // Check that the reconstituted message type, number of data
    // items, size, and set of data items are the same as the original
    // message

    BOOST_CHECK_EQUAL(pm.get_signal_id(), rpm.get_signal_id());
    BOOST_CHECK_EQUAL(pm.get_length(), rpm.get_length());

    DataItems pm_data_items = pm.get_data_items();
    DataItems rpm_data_items = rpm.get_data_items();
    BOOST_CHECK_EQUAL(pm_data_items.size(), rpm_data_items.size());

    // Find the set of all data item ids in pm
    std::set<DataItemIdType> pm_dataitem_ids;
    for (const DataItem & di : pm_data_items)
    {
        pm_dataitem_ids.insert(di.id);
    }

    // Find the set of all data item ids in rpm
    std::set<DataItemIdType> rpm_dataitem_ids;
    for (const DataItem & di : rpm_data_items)
    {
        rpm_dataitem_ids.insert(di.id);
    }

    // Check that the data item id sets are the same in pm and rpm

    BOOST_CHECK(pm_dataitem_ids == rpm_dataitem_ids);

    // A more extensive check would make sure that all of the
    // individual data items are equal.
}

template <typename Div_SubDataItem_t>
static void
add_sub_data_items(DataItem & di,
                   const DataItemInfo & di_info,
                   ProtocolConfig * protocfg,
                   const std::string & occurs)
{
    for (const auto & subdi_for_di : di_info.sub_data_items)
    {
        // We have to add the required data items (1, 1+) else the
        // parent data item won't validate.

        if ( (subdi_for_di.occurs == occurs) ||
             (subdi_for_di.occurs == "1") ||
             (subdi_for_di.occurs == "1+") )
        {
            // create the data item
            DataItem subdi {subdi_for_di.name, protocfg, &di_info};

            // Extract the parent data item value, add the sub data
            // item to it, and reassign the parent data item value.
            // This is the only way I've found to change the value of
            // a boost variant.

            Div_SubDataItem_t val = boost::get<Div_SubDataItem_t>(di.value);
            val.sub_data_items.push_back(subdi);
            di.value = val;

            // Sub data items can have sub data items.  If so, call
            // recursively.

            DataItemInfo subdi_info =
                protocfg->get_data_item_info(subdi_for_di.name);

            if (subdi_info.sub_data_items.size() > 0)
            {
                switch (subdi.get_type())
                {
                    case DataItemValueType::div_sub_data_items:
                        add_sub_data_items<Div_sub_data_items_t>(
                            subdi, subdi_info, protocfg, occurs);
                    break;

                    case DataItemValueType::div_u16_sub_data_items:
                        add_sub_data_items<Div_u16_sub_data_items_t>(
                            subdi, subdi_info, protocfg, occurs);
                    break;

                    default:
                        BOOST_TEST_MESSAGE("Unknown sub data item type");
                        return;
                    break;
                }
            } // end this sub data item has its own sub data items
        } // end add this sub data item to the data item
    } // end for each possible sub data item
}


static void
add_data_items(ProtocolMessage & pm,
               DataItems & data_items,
               ProtocolConfig * protocfg,
               const std::string & occurs)
{
    std::string signal_name = pm.get_signal_name();
    ProtocolConfig::SignalInfo siginfo =
        protocfg->get_signal_info(signal_name);

    for (auto const & difs : siginfo.data_items)
    {
        if (difs.occurs == occurs)
        {
            std::string di_name = protocfg->get_data_item_name(difs.id);
            DataItemInfo di_info = protocfg->get_data_item_info(di_name);

            // This will give us a default value for the data item,
            // probably all 0's.
            DataItem di {di_name, protocfg};

            // If the data item has sub data items, add those too.

            if (di_info.sub_data_items.size() > 0)
            {
                switch (di.get_type())
                {
                    case DataItemValueType::div_sub_data_items:
                        add_sub_data_items<Div_sub_data_items_t>(
                            di, di_info, protocfg, occurs);
                    break;

                    case DataItemValueType::div_u16_sub_data_items:
                        add_sub_data_items<Div_u16_sub_data_items_t>(
                            di, di_info, protocfg, occurs);
                    break;

                    default:
                        BOOST_TEST_MESSAGE("Unknown sub data item type");
                        return;
                    break;
                }
            }

            // Add the data item to the list *after* any sub data
            // items have been added.

            data_items.push_back(di);
            BOOST_TEST_MESSAGE("add " << di.to_string()
                               << " occurs=" << occurs
                               << " to signal " << signal_name);
        } // if data item occurs the desired number of times
    } // for each data item allowed on this signal
}

static void
test_one_message(const std::string & signal_name)
{
    TestClientImpl client;

    // parse the config file
    BOOST_REQUIRE(client.parse_config_file("test_modem_config.xml"));

    // Do all of the tests for this message with all of the protocol
    // config files we have.

    for (const std::string & pcf : proto_config_files)
    {
        ProtocolConfig * protocfg = get_protocfg(pcf);

        ProtocolMessage pm {protocfg, logger};

        // Create a protocol message of the desired type with just the
        // header, and check its size.

        pm.add_header(signal_name);
        check_header_size(pm, protocfg);

        DataItems data_items;

        // Add all of the required data items ("1" and "1+") before
        // check_serialization because check_serialization will check
        // the message's validity.  To pass the test the message must
        // be valid, which means (among other things) it needs to have
        // all of the required data items.  After adding the required
        // items and calling check_serialization, we add the optional
        // items ("0-1", "0+") and check_serialization after each
        // batch of items is added.  The result is that multiple
        // variations of each message are tested.

        add_data_items(pm, data_items, protocfg, "1");
        add_data_items(pm, data_items, protocfg, "1+");
        check_serialization(pm, data_items, false /* router send */, protocfg);
        check_serialization(pm, data_items, true  /* modem  send */, protocfg);

        for (auto occurs : std::vector<std::string> {"0-1", "0+", "1+", "0+"})
        {
            add_data_items(pm, data_items, protocfg, occurs);
            check_serialization(pm, data_items, false /* router send */, protocfg);
            check_serialization(pm, data_items, true  /* modem  send */, protocfg);
        }
        delete protocfg;
    }
}

//---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(Peer_Discovery)
{
    test_one_message(ProtocolStrings::Peer_Discovery);
}

BOOST_AUTO_TEST_CASE(Peer_Offer)
{
    test_one_message(ProtocolStrings::Peer_Offer);
}

BOOST_AUTO_TEST_CASE(Session_Initialization)
{
    test_one_message(ProtocolStrings::Session_Initialization);
}

BOOST_AUTO_TEST_CASE(Session_Initialization_Response)
{
    test_one_message(ProtocolStrings::Session_Initialization_Response);
}

BOOST_AUTO_TEST_CASE(Session_Termination)
{
    test_one_message(ProtocolStrings::Session_Termination);
}

BOOST_AUTO_TEST_CASE(Session_Termination_Response)
{
    test_one_message(ProtocolStrings::Session_Termination_Response);
}

BOOST_AUTO_TEST_CASE(Session_Update)
{
    test_one_message(ProtocolStrings::Session_Update);
}

BOOST_AUTO_TEST_CASE(Session_Update_Response)
{
    test_one_message(ProtocolStrings::Session_Update_Response);
}

BOOST_AUTO_TEST_CASE(Destination_Up)
{
    test_one_message(ProtocolStrings::Destination_Up);
}

BOOST_AUTO_TEST_CASE(Destination_Up_Response)
{
    test_one_message(ProtocolStrings::Destination_Up_Response);
}

BOOST_AUTO_TEST_CASE(Destination_Down)
{
    test_one_message(ProtocolStrings::Destination_Down);
}

BOOST_AUTO_TEST_CASE(Destination_Down_Response)
{
    test_one_message(ProtocolStrings::Destination_Down_Response);
}

BOOST_AUTO_TEST_CASE(Destination_Update)
{
    test_one_message(ProtocolStrings::Destination_Update);
}

BOOST_AUTO_TEST_CASE(Link_Characteristics_Request)
{
    test_one_message(ProtocolStrings::Link_Characteristics_Request);
}

BOOST_AUTO_TEST_CASE(Link_Characteristics_Response)
{
    test_one_message(ProtocolStrings::Link_Characteristics_Response);
}

BOOST_AUTO_TEST_CASE(Heartbeat)
{
    test_one_message(ProtocolStrings::Heartbeat);
}

BOOST_AUTO_TEST_SUITE_END()
