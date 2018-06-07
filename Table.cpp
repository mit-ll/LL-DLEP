/*
 * Dynamic Link Exchange Protocol (DLEP)
 *
 * Copyright (C) 2015, 2016, 2018 Massachusetts Institute of Technology
 */

#include <iomanip>
#include "Table.h"

Table::Table(const std::vector<std::string> & headings)
{
    table.push_back(headings);
    current_row = 1;
    current_column = 0;
}

void
Table::add_field(const std::string & value)
{
    while (current_row >= table.size())
    {
        Row new_row;
        new_row.resize(table[0].size());
        table.push_back(new_row);
    }

    table[current_row][current_column++] = value;
}

void
Table::add_field(const std::string & field_name, const std::string & value)
{
    const Row & headings = table[0];
    // XXX vector doesn't have find() ???
    for (current_column = 0; current_column < headings.size(); current_column++)
    {
        if (headings[current_column] == field_name)
        {
            break;
        }
    }
    add_field(value);
}

void
Table::finish_row(bool force_empty_row)
{
    if ( (current_column > 0) || force_empty_row)
    {
        current_row++;
        current_column = 0;
    }
}

unsigned int
Table::get_row_index()
{
    return current_row;
}

void
Table::set_row_index(unsigned int ri)
{
    current_row = ri;
}

void
Table::set_row_index_end()
{
    current_row = table.size();
}

void Table::print(std::ostream & os)
{
    // The first row has the column headings.  There must be one
    // for each possible column.
    std::vector<std::size_t> column_widths(table[0].size(), 0);

    // Should we skip printing this column?  Yes (true) if the column
    // was completely empty except for the column name in the first
    // row.
    std::vector<bool> skip_column(table[0].size(), true);

    // Figure out how wide columns need to be to print nicely.

    bool heading_row = true;  // true only for first row
    for (const auto & row : table)
    {
        for (unsigned int ci = 0; ci < row.size(); ci++)
        {
            column_widths[ci] = std::max(column_widths[ci],
                                         row[ci].length());
            if ( ! heading_row && (row[ci].length() > 0))
            {
                // If column ci on this row is non-blank, don't
                // skip printing this column
                skip_column[ci] = false;
            }
        }

        heading_row = false;
    }

    // Increase each column width by 1 to leave a space between columns.

    for (unsigned int ci = 0; ci < column_widths.size(); ci++)
    {
        column_widths[ci]++;
    }

    // Print out all of the rows.

    os << std::left;
    for (const auto & row : table)
    {
        for (unsigned int ci = 0; ci < row.size(); ci++)
        {
            if ( ! skip_column[ci])
            {
                os << std::setw(column_widths[ci]) << row[ci];
            }
        }
        os << std::endl;
    }
}
