/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2013, 2018 Massachusetts Institute of Technology
 */
#ifndef _DLEP_LOGGER_
#define _DLEP_LOGGER_

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <strings.h>
#include <time.h>
#include <sys/time.h>
#include <ctype.h>
#include <assert.h>
#include <map>
#include <boost/thread/mutex.hpp>

namespace LLDLEP
{
namespace internal
{

#define MAX_LEVEL 5
#define MIN_LEVEL 1
#define DLEP_LOG_FATAL 5
#define DLEP_LOG_ERROR 4
#define DLEP_LOG_NOTICE 3
#define DLEP_LOG_INFO 2
#define DLEP_LOG_DEBUG 1

#define DEBUG_FILE       __FILE__
#define DEBUG_FUNCTION   __func__
#define DEBUG_LINE       __LINE__
#define DEBUG_DATE       __DATE__
#define DEBUG_TIME       __TIME__

class DlepLogger
{
public:
    DlepLogger();
    explicit DlepLogger(int run_level);
    ~DlepLogger();
    void log(int level, const std::string & str);
    void log(int level, std::ostringstream & msg);
    void log_time(int level, const std::string & str);
    void log_time(int level, std::ostringstream & msg);
    void set_run_level(int run_level);
    void set_log_file(const char * name);
    std::string get_log_file()
    {
        return file_name;
    }
private:
    std::string file_name;
    int run_level;
    std::ofstream logfile;
    std::string time_string_get();
    std::map <int, std::string> level_name;
    boost::mutex mutex;
};

typedef  boost::shared_ptr<DlepLogger> DlepLoggerPtr;

#define LOG(_level, _msg) {                                          \
    std::ostringstream m;                                            \
    m << DEBUG_FILE << ":" << DEBUG_LINE << ":" << DEBUG_FUNCTION << \
        "(): " << (_msg).str() ;                                     \
    logger->log_time(_level, m);                                     \
    (_msg).str("");                                                  \
}

} // namespace internal
} // namespace LLDLEP

#endif // _DLEP_LOGGER_
