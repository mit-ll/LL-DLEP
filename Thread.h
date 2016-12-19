/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016 Massachusetts Institute of Technology
 */

/// @file
/// Portability header to make it easy to choose between Boost threads
/// and C++ standard library (std::) threads.

#ifndef THREAD_H
#define THREAD_H

#ifdef USE_BOOST_THREADS
#include <boost/thread.hpp>

namespace LLDLEP
{
namespace internal
{

typedef boost::thread Thread;

} // namespace internal
} // namespace LLDLEP

#else

// use std threads
#include <thread>

namespace LLDLEP
{
namespace internal
{

typedef std::thread Thread;

} // namespace internal
} // namespace LLDLEP

#endif

#endif // THREAD_H
