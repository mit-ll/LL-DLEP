/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2018 Massachusetts Institute of Technology
 */

/// @file
/// Boost Unit Tests for Serialize.h

#include <boost/test/unit_test.hpp>

#include "Serialize.h"
#include <limits>

using namespace LLDLEP;


BOOST_AUTO_TEST_SUITE(Serialize)

template <typename T>
void
test_serialize()
{
    // values to attempt to serialize/deserialize
    std::vector<T> test_vals =
        { 0,
          std::numeric_limits<T>::max()/2,
          std::numeric_limits<T>::max()/2 + 1,
          std::numeric_limits<T>::max()/3,
          std::numeric_limits<T>::max()
        };

    // try serializing into all possible field sizes

    for (std::size_t field_size = 1; field_size <= 8; field_size++)
    {
        for (auto val : test_vals )
        {
           bool serialize_should_succeed = true;

           // If the field size we're serializing into is smaller than
           // the type T, the value may not fit.  Compare val with the
           // largest value that can be represented in field_size
           // bytes to see if it will fit.

           if (field_size < sizeof(T))
           {
               std::uint64_t max_val_in_field_size = (1ULL << field_size*8) - 1;
               BOOST_TEST_MESSAGE(__FILE__ << ":" << __LINE__ << " " << max_val_in_field_size);
               if (val > max_val_in_field_size)
               {
                   serialize_should_succeed = false;
               }
           }

           std::size_t sz;
           std::vector<std::uint8_t> buf; // buffer to serialize into

           // Say what we're about to test

           BOOST_TEST_MESSAGE(__FILE__ << ":" << __LINE__
                              << ": serialize value " << std::uint64_t(val)
                              << " in field size " << field_size
                              << " should succeed " << serialize_should_succeed);

           // attempt serialization

           try
           {
               sz = serialize(val, field_size, buf);
               BOOST_CHECK(serialize_should_succeed);
               BOOST_CHECK_EQUAL(sz, field_size);
           }
           catch (std::invalid_argument)
           {
               BOOST_CHECK( ! serialize_should_succeed);

               // If it didn't serialize, don't bother trying deserialization

               continue;
           }

           // attempt deserialization

           auto it_begin = buf.begin();
           auto it_end   = buf.end();

           T val2 = 0;
           sz = deserialize(val2, field_size, it_begin, it_end);
           BOOST_CHECK_EQUAL(sz, field_size);
           BOOST_CHECK_EQUAL(val, val2);

        } // for each value to serialize/deserialize
    } // for each field size
}

BOOST_AUTO_TEST_CASE(serialize_u8)
{
    test_serialize<std::uint8_t>();
}

BOOST_AUTO_TEST_CASE(serialize_u16)
{
    test_serialize<std::uint16_t>();
}

BOOST_AUTO_TEST_CASE(serialize_u32)
{
    test_serialize<std::uint32_t>();
}

BOOST_AUTO_TEST_CASE(serialize_u64)
{
    test_serialize<std::uint64_t>();
}

BOOST_AUTO_TEST_SUITE_END()
