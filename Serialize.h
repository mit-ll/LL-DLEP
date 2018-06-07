/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016, 2018 Massachusetts Institute of Technology
 */

/// @file
/// Serialize and deserialize integers of varying sizes (1,2,4,8 bytes)
/// into fields of varying sizes (1-8 bytes).
/// Use network byte order (MSB first).
/// Handles serializing a type that is larger than the field size,
/// e.g., a uint32 into a 2-byte field, as long as the value can be
/// represented in a field of that size.
///
/// These functions are not expected to be used directly by DLEP
/// clients, but since this header is included by other DLEP public
/// header files, it has to be installed.
///
/// Currently this only works on unsigned integers.  To handle signed
/// integers, modifications are required to perform sign extension.


#ifndef SERIALIZE_H
#define SERIALIZE_H

#include <stdexcept>
#include <vector>
#include <type_traits>

namespace LLDLEP
{

//-----------------------------------------------------------------------------

// Serialize integers of varying sizes


/// Serialize a value into a specific size and location in a buffer.
///
/// @param[in]     val
///                the value to serialize
/// @param[in]     field_size
///                the number of bytes the serialized form must consume
/// @param[in,out] buf
///                the buffer to store the serialized bytes
/// @param[in]     it
///                location in buf at which to store the serialized bytes.
///                If this is buf.end(), append bytes to the end of buf.
template <typename T>
std::size_t serialize(const T & val, const std::size_t field_size,
                      std::vector<std::uint8_t> & buf,
                      std::vector<std::uint8_t>::iterator it)
{
    static_assert(std::is_integral<T>::value, "T must be an integer type");
    static_assert(! std::is_signed<T>::value, "T must be an unsigned type");

    // Verify that val will fit in field_size bytes.

    if (sizeof(T) > field_size)
    {
        T mask = T(~0) << field_size * 8;

        if (val & mask)
        {
            throw std::invalid_argument(
                std::to_string(val) + " cannot fit in " +
                std::to_string(field_size) + " bytes" + " mask " + std::to_string(mask));
        }
    }

    bool append_bytes = (it == buf.end());

    for (std::size_t i = field_size; i > 0; i--)
    {
        std::uint8_t onebyte = 0;
        if (i <= sizeof(T))
        {
            onebyte = val >> ((i - 1) * 8) & 0xff;
        }

        if (append_bytes)
        {
            buf.push_back(onebyte);
        }
        else
        {
            if (it == buf.end())
            {
                throw std::length_error("reached end of output buffer");
            }
            *it++ = onebyte;
        }
    }

    return field_size;
}

/// Serialize a value into a specific size and append it to a buffer.
///
/// @param[in]     val
///                the value to serialize
/// @param[in]     field_size
///                the number of bytes the serialized form must consume
/// @param[in,out] buf
///                the buffer to append the serialized bytes to
template <typename T>
std::size_t serialize(const T & val, const std::size_t field_size,
                      std::vector<std::uint8_t> & buf)
{
    static_assert(std::is_integral<T>::value, "T must be an integer type");
    static_assert(! std::is_signed<T>::value, "T must be an unsigned type");

    return serialize(val, field_size, buf, buf.end());
}

/// Serialize a value and append it to a buffer.
/// This always adds sizeof(T) bytes to the buffer.
///
/// @param[in]     value
///                the value to serialize
/// @param[in,out] buf
///                the buffer to append the serialized bytes to
template <typename T>
std::size_t serialize(const T & value, std::vector<std::uint8_t> & buf)
{
    static_assert(std::is_integral<T>::value, "T must be an integer type");
    static_assert(! std::is_signed<T>::value, "T must be an unsigned type");

    for (std::size_t i = sizeof(T); i > 0; i--)
    {
        buf.push_back((value >> ((i - 1) * 8)) & 0xff);
    }

    return sizeof(T);
}

//-----------------------------------------------------------------------------

// Deserialize integers of varying sizes

// specialization (overload, really) of the next function for uint8
template <typename Iterator>
std::size_t deserialize(std::uint8_t & value, Iterator & it, Iterator it_end)
{
    if (it == it_end)
    {
        throw std::length_error("reached end of input buffer");
    }
    value = *it++;
    return 1;
}


/// Deserialize a value from a specific location in a buffer.
///
/// @param[out]    value
///                upon return, the deserialized value
/// @param[in,out] it
///                location in the buffer at which the serialized bytes begin.
///                Advances to reflect bytes consumed by deserialization.
/// @param[in]     it_end
///                end of the buffer.  Deserialization must not go beyond this.
template <typename T, typename Iterator>
std::size_t deserialize(T & value, Iterator & it, Iterator it_end)
{
    static_assert(std::is_integral<T>::value, "T must be an integer type");
    static_assert(! std::is_signed<T>::value, "T must be an unsigned type");

    value = 0;

    std::size_t i;
    for (i = sizeof(T);
            i > 0 && it != it_end;
            i--, it++)
    {
        value <<= 8;
        value |= *it;
    }

    if (i > 0)
    {
        throw std::length_error("reached end of input buffer");
    }

    return sizeof(T);
}


/// Deserialize a value from a specific size and location in a buffer.
///
/// @param[out]    value
///                upon return, the deserialized value
/// @param[in]     field_size
///                the number of bytes the serialized form consumes
/// @param[in,out] it
///                location in the buffer at which the serialized bytes begin.
///                Advances to reflect bytes consumed by deserialization.
/// @param[in]     it_end
///                end of the buffer.  Deserialization must not go beyond this.
template <typename T, typename Iterator>
std::size_t deserialize(T & value, const std::size_t field_size,
                        Iterator & it, Iterator it_end)
{
    static_assert(std::is_integral<T>::value, "T must be an integer type");
    static_assert(! std::is_signed<T>::value, "T must be an unsigned type");

    std::uint64_t val64 = 0;

    std::size_t i;
    for (i = field_size;
            (i > 0) && (it != it_end);
            i--, it++)
    {
        val64 <<= 8;
        val64 |= *it;
    }

    if (i > 0)
    {
        throw std::length_error("reached end of input buffer");
    }

    T mask = ~0;
    if ((val64 & mask) != val64)
    {
        throw std::invalid_argument(
            std::to_string(val64) + " cannot fit in " +
            std::to_string(sizeof(T)) + " bytes");
    }

    value = T(val64);
    return field_size;
}

} // namespace LLDLEP

#endif // SERIALIZE_H
