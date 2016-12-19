/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016 Massachusetts Institute of Technology
 */

/// @file
/// Utility for examining DLEP log files.

#ifndef LOG_FILE_H
#define LOG_FILE_H

#include <string>
#include <vector>

class LogFile
{
public:
    explicit LogFile(std::string & filename);
    ~LogFile() {};

    int find(int start_line, const std::string & search_string);

private:
    std::vector<std::string> lines;
};


#endif
