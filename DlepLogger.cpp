/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2013, 2018, 2019 Massachusetts Institute of Technology
 */
#include "DlepLogger.h"
#include <time.h>
#include <ctype.h>

using namespace std;
using namespace LLDLEP::internal;

DlepLogger::DlepLogger(const std::string & filename, unsigned int level)
{
    set_log_level(level);

    logfile.open(filename);
    if (logfile.fail())
    {
        throw std::invalid_argument("could not open log file " + filename);
    }
}

DlepLogger::~DlepLogger()
{
    if (logfile.is_open())
    {
        logfile.close();
    }
}

void
DlepLogger::log(unsigned int level, std::ostringstream & msg)
{
    boost::mutex::scoped_lock lock(mutex);

    level = clamp_log_level(level);
    if (level >= log_level)
    {
        logfile << time_string_get() << level_name[level] << msg.str()
                << endl;
    }
}

unsigned int
DlepLogger::clamp_log_level(unsigned int level)
{
    if (level < DLEP_LOG_DEBUG)
    {
        return DLEP_LOG_DEBUG;
    }
    else if (level > DLEP_LOG_FATAL)
    {
        return DLEP_LOG_FATAL;
    }
    else
    {
        return level;
    }
}

void
DlepLogger::set_log_level(unsigned int level)
{
    boost::mutex::scoped_lock lock(mutex);
    log_level = clamp_log_level(level);
}

void
DlepLogger::set_log_file(const std::string & filename)
{
    boost::mutex::scoped_lock lock(mutex);
    if (logfile.is_open())
    {
        logfile.close();
    }

    logfile.open(filename);
    if (logfile.fail())
    {
        throw std::invalid_argument(filename);
    }
}

string
DlepLogger::time_string_get()
{
    char buf[128];
    timeval tp;
    gettimeofday(&tp, nullptr);
    time_t tim = time(nullptr);
    tm now;
    localtime_r(&tim, &now);
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03ld ",
             now.tm_hour,
             now.tm_min,
             now.tm_sec,
             tp.tv_usec / 1000L);
    return string(buf);
}
