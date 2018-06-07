/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016 Massachusetts Institute of Technology
 */

/// @file
/// Declaration of Table class

#ifndef TABLE_H
#define TABLE_H

#include <string>
#include <vector>
#include <ostream>

/// This class provides a convenient way to print tables of text
/// values.  Tables can be sparse; not every row/column needs to have
/// a value.  The table's column widths auto-adjust to accommodate the
/// column's widest value.
class Table
{
public:
    explicit Table(const std::vector<std::string> & headings);

    void add_field(const std::string & value);
    void add_field(const std::string & field_name, const std::string & value);
    void finish_row(bool force_empty_row = false);
    unsigned int get_row_index();
    void set_row_index(unsigned int ri);
    void set_row_index_end();
    void print(std::ostream & os);

private:
    unsigned int current_row;
    unsigned int current_column;
    typedef std::vector<std::string> Row;
    std::vector<Row> table;
};

#endif // TABLE_H
