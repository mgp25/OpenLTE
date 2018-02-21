/*******************************************************************************

    Copyright 2017 Ben Wojtowicz

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*******************************************************************************

    File: libtools_helpers.h

    Description: Contains all the definitions for helper functions.

    Revision History
    ----------    -------------    --------------------------------------------
    07/29/2017    Ben Wojtowicz    Created file

*******************************************************************************/

#ifndef __LIBTOOLS_HELPERS_H__
#define __LIBTOOLS_HELPERS_H__

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "typedefs.h"
#include <string>

/*******************************************************************************
                              DEFINES
*******************************************************************************/


/*******************************************************************************
                              FORWARD DECLARATIONS
*******************************************************************************/


/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/


/*******************************************************************************
                              FUNCTION DECLARATIONS
*******************************************************************************/

/*********************************************************************
    Name: to_string

    Description: Converts various types to a string.
*********************************************************************/
std::string to_string(int32 value);
std::string to_string(uint32 value);
std::string to_string(int64 value);
std::string to_string(uint64 value);
std::string to_string(uint64 value, uint32 num_digits);
std::string to_string(uint8 *value, uint32 num_bytes);
std::string to_string(double value);
std::string to_string(const char *value);

/*********************************************************************
    Name: to_number

    Description: Converts a string to various types.
*********************************************************************/
// FIXME: Use template
bool to_number(std::string str, uint16 *number);
bool to_number(std::string str, uint64 *number);
bool to_number(std::string str, uint32 num_digits, uint16 *number);
bool to_number(std::string str, uint32 num_digits, uint64 *number);
bool to_number(std::string str, uint32 num_bytes, uint8 *number);

/*********************************************************************
    Name: get_formatted_time

    Description: Populates a string with a formatted time.
*********************************************************************/
void get_formatted_time(std::string &time_string);

/*********************************************************************
    Name: is_string_valid_as_number

    Description: Checks if the string is valid as a number (base 10
                 or 16) and is of a specific number of digits.
*********************************************************************/
bool is_string_valid_as_number(std::string str, uint32 num_digits, uint8 base);

#endif /* __LIBTOOLS_HELPERS_H__ */
