/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016 Massachusetts Institute of Technology
 */

/// @file
/// Utility for examining DLEP log files.


#include "LogFile.h"
#include <iostream>
#include <fstream>

LogFile::LogFile(std::string & filename)
{
    std::ifstream file(filename);
    std::string oneline;

    // Read in all of the lines of the file
    while (std::getline(file, oneline))
    {
        lines.push_back(oneline);
    }
}

int
LogFile::find(int start_line, const std::string & search_string)
{
    for (unsigned int i = start_line; i < lines.size(); i++)
    {
        if (lines[i].find(search_string) != std::string::npos)
        {
            return i;
        }
    }
    return -1;
}
