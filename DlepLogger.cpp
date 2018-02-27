/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2013 Massachusetts Institute of Technology
 */
#include "DlepLogger.h"
#include <time.h>
#include <ctype.h>
#include <iostream>

using namespace std;
using namespace LLDLEP::internal;

DlepLogger::DlepLogger():
    pstream(&std::cout)
{
    this->run_level = DLEP_LOG_INFO;
    //this->run_level = DEBUG;
    level_name[1] = string("DEBUG: ");
    level_name[2] = string("INFO: ");
    level_name[3] = string("NOTICE: ");
    level_name[4] = string("ERROR: ");
    level_name[5] = string("FATAL: ");
}

DlepLogger::DlepLogger(int run_level):
    pstream(&std::cout)
{
    if (run_level < DLEP_LOG_DEBUG)
    {
        this->run_level = DLEP_LOG_DEBUG;
    }
    else if (run_level > DLEP_LOG_FATAL)
    {
        this->run_level = DLEP_LOG_FATAL;
    }
    else
    {
        this->run_level = run_level;
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
DlepLogger::log(int level, std::string str)
{
    boost::mutex::scoped_lock lock(mutex);
    if (level >= run_level)
    {
        *pstream << level_name[level] << str << endl;
    }
}

void
DlepLogger::log(int level, std::ostringstream & msg)
{
    log(level, msg.str());
    msg.str("");
}

void
DlepLogger::log_time(int level, std::string str)
{
    boost::mutex::scoped_lock lock(mutex);
    if (level >= run_level)
    {
        *pstream << time_string_get() << level_name[level] << str << endl;
    }
}

void
DlepLogger::log_time(int level, std::ostringstream & msg)
{
    log_time(level, msg.str());
    msg.str("");
}

void
DlepLogger::set_run_level(int run_level)
{
    if (run_level < DLEP_LOG_DEBUG)
    {
        this->run_level = DLEP_LOG_DEBUG;
    }
    else if (run_level > DLEP_LOG_FATAL)
    {
        this->run_level = DLEP_LOG_FATAL;
    }
    else
    {
        this->run_level = run_level;
    }
}

void
DlepLogger::set_log_file(const char * name)
{
    if (logfile.is_open())
    {
       logfile.close();
    }
    logfile.open(name);

    if (!logfile.fail())
    {
        file_name = name;
        pstream = &logfile;
    }
    else
    {
      file_name.clear();
      pstream = &std::cout;
      ostringstream msg;
      msg <<  "Unable to open log file: " << name;
      DlepLogger * logger(this);
      LOG(DLEP_LOG_ERROR, msg);
    }
}

string
DlepLogger::time_string_get()
{
    char buf[128];
    timeval tp;
    gettimeofday(&tp, 0);
    time_t tim = time(NULL);
    tm now;
    localtime_r(&tim, &now);
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03ld ",
             now.tm_hour,
             now.tm_min,
             now.tm_sec,
             tp.tv_usec / 1000L);
    return string(buf);
}
