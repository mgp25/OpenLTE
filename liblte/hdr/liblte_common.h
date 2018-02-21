/*******************************************************************************

    Copyright 2012-2016 Ben Wojtowicz

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

    File: liblte_common.h

    Description: Contains all the common definitions for the LTE library.

    Revision History
    ----------    -------------    --------------------------------------------
    02/26/2012    Ben Wojtowicz    Created file.
    07/21/2013    Ben Wojtowicz    Added a common message structure.
    06/15/2014    Ben Wojtowicz    Split LIBLTE_MSG_STRUCT into bit and byte
                                   aligned messages.
    08/03/2014    Ben Wojtowicz    Commonized value_2_bits and bits_2_value.
    11/29/2014    Ben Wojtowicz    Added liblte prefix to value_2_bits and
                                   bits_2_value.
    07/14/2015    Ben Wojtowicz    Added an error code for DCIs with invalid
                                   contents.
    07/03/2016    Ben Wojtowicz    Increased the maximum message size.

*******************************************************************************/

#ifndef __LIBLTE_COMMON_H__
#define __LIBLTE_COMMON_H__

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "typedefs.h"
#include <string.h>

/*******************************************************************************
                              DEFINES
*******************************************************************************/

// FIXME: This was chosen arbitrarily
#define LIBLTE_MAX_MSG_SIZE 5512

/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/

typedef enum{
    LIBLTE_SUCCESS = 0,
    LIBLTE_ERROR_INVALID_INPUTS,
    LIBLTE_ERROR_DECODE_FAIL,
    LIBLTE_ERROR_INVALID_CRC,
    LIBLTE_ERROR_INVALID_CONTENTS,
}LIBLTE_ERROR_ENUM;

typedef struct{
    uint32 N_bits;
    uint8  msg[LIBLTE_MAX_MSG_SIZE];
}LIBLTE_BIT_MSG_STRUCT;

typedef struct{
    uint32 N_bytes;
    uint8  msg[LIBLTE_MAX_MSG_SIZE];
}LIBLTE_BYTE_MSG_STRUCT;

/*******************************************************************************
                              DECLARATIONS
*******************************************************************************/

/*********************************************************************
    Name: liblte_value_2_bits

    Description: Converts a value to a bit string
*********************************************************************/
void liblte_value_2_bits(uint32   value,
                         uint8  **bits,
                         uint32   N_bits);

/*********************************************************************
    Name: liblte_bits_2_value

    Description: Converts a bit string to a value
*********************************************************************/
uint32 liblte_bits_2_value(uint8  **bits,
                           uint32   N_bits);

#endif /* __LIBLTE_COMMON_H__ */
