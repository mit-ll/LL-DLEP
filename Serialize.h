/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016 Massachusetts Institute of Technology
 */

#ifndef SERIALIZE_H
#define SERIALIZE_H

#include <stdexcept>
#include <vector>
#include <type_traits>

namespace LLDLEP
{

//-----------------------------------------------------------------------------

// Serialize integers of varying sizes

template <typename T>
std::size_t serialize(const T & val, const std::size_t field_size,
                      std::vector<std::uint8_t> & buf,
                      std::vector<std::uint8_t>::iterator it)
{
    static_assert(std::is_integral<T>::value, "T must be an integer type");

    if (sizeof(T) > field_size)
    {
        T mask = (~0) << field_size * 8;

        if (val & mask)
        {
            throw std::invalid_argument(
                std::to_string(val) + " cannot fit in " +
                std::to_string(field_size) + " bytes");
        }
    }

    bool append_bytes = (it == buf.end());

    for (std::size_t i = field_size; i > 0; i--)
    {
        std::uint8_t onebyte = val >> ((i - 1) * 8) & 0xff;
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

template <typename T>
std::size_t serialize(const T & val, const std::size_t field_size,
                      std::vector<std::uint8_t> & buf)
{
    static_assert(std::is_integral<T>::value, "T must be an integer type");

    std::vector<std::uint8_t>::iterator it = buf.end();
    return serialize(val, field_size, buf, it);
}

template <typename T>
std::size_t serialize(const T & value, std::vector<std::uint8_t> & buf)
{
    static_assert(std::is_integral<T>::value, "T must be an integer type");

    for (std::size_t i = sizeof(T); i > 0; i--)
    {
        buf.push_back((value >> ((i - 1) * 8)) & 0xff);
    }

    return sizeof(T);
}

//-----------------------------------------------------------------------------

// Deserialize integers of varying sizes

// specialization (overload, really) for uint8
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


template <typename T, typename Iterator>
std::size_t deserialize(T & value, Iterator & it, Iterator it_end)
{
    static_assert(std::is_integral<T>::value, "T must be an integer type");

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


template <typename T, typename Iterator>
std::size_t deserialize(T & value, const std::size_t field_size,
                        Iterator & it, Iterator it_end)
{
    static_assert(std::is_integral<T>::value, "T must be an integer type");

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
