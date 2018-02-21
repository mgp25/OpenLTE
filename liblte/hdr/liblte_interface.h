/*******************************************************************************

    Copyright 2013, 2016 Ben Wojtowicz

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

    File: liblte_interface.h

    Description: Contains all the definitions for the LTE interface library.

    Revision History
    ----------    -------------    --------------------------------------------
    02/23/2013    Ben Wojtowicz    Created file
    11/13/2013    Ben Wojtowicz    Added functions for getting corresponding
                                   EARFCNs for FDD configuration
    12/18/2016    Ben Wojtowicz    Added support for band 26, 26, and 28 (thanks
                                   to Jeremy Quirke).

*******************************************************************************/

#ifndef __LIBLTE_INTERFACE_H__
#define __LIBLTE_INTERFACE_H__

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "typedefs.h"

/*******************************************************************************
                              DEFINES
*******************************************************************************/


/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/


/*******************************************************************************
                              DECLARATIONS
*******************************************************************************/

/*********************************************************************
    Parameter Name: Band

    Description: Defines the operating frequency bands.

    Document Reference: 36.101 v11.2.0 Section 5.5
*********************************************************************/
// Defines
// Enums
typedef enum{
    LIBLTE_INTERFACE_BAND_1 = 0,
    LIBLTE_INTERFACE_BAND_2,
    LIBLTE_INTERFACE_BAND_3,
    LIBLTE_INTERFACE_BAND_4,
    LIBLTE_INTERFACE_BAND_5,
    LIBLTE_INTERFACE_BAND_6,
    LIBLTE_INTERFACE_BAND_7,
    LIBLTE_INTERFACE_BAND_8,
    LIBLTE_INTERFACE_BAND_9,
    LIBLTE_INTERFACE_BAND_10,
    LIBLTE_INTERFACE_BAND_11,
    LIBLTE_INTERFACE_BAND_12,
    LIBLTE_INTERFACE_BAND_13,
    LIBLTE_INTERFACE_BAND_14,
    LIBLTE_INTERFACE_BAND_17,
    LIBLTE_INTERFACE_BAND_18,
    LIBLTE_INTERFACE_BAND_19,
    LIBLTE_INTERFACE_BAND_20,
    LIBLTE_INTERFACE_BAND_21,
    LIBLTE_INTERFACE_BAND_22,
    LIBLTE_INTERFACE_BAND_23,
    LIBLTE_INTERFACE_BAND_24,
    LIBLTE_INTERFACE_BAND_25,
    LIBLTE_INTERFACE_BAND_26,
    LIBLTE_INTERFACE_BAND_27,
    LIBLTE_INTERFACE_BAND_28,
    LIBLTE_INTERFACE_BAND_33,
    LIBLTE_INTERFACE_BAND_34,
    LIBLTE_INTERFACE_BAND_35,
    LIBLTE_INTERFACE_BAND_36,
    LIBLTE_INTERFACE_BAND_37,
    LIBLTE_INTERFACE_BAND_38,
    LIBLTE_INTERFACE_BAND_39,
    LIBLTE_INTERFACE_BAND_40,
    LIBLTE_INTERFACE_BAND_41,
    LIBLTE_INTERFACE_BAND_42,
    LIBLTE_INTERFACE_BAND_43,
    LIBLTE_INTERFACE_BAND_N_ITEMS,
}LIBLTE_INTERFACE_BAND_ENUM;
static const char liblte_interface_band_text[LIBLTE_INTERFACE_BAND_N_ITEMS][20] = { "1",  "2",  "3",  "4",
                                                                                    "5",  "6",  "7",  "8",
                                                                                    "9", "10", "11", "12",
                                                                                   "13", "14", "17", "18",
                                                                                   "19", "20", "21", "22",
                                                                                   "23", "24", "25", "26",
                                                                                   "27", "28", "33", "34",
                                                                                   "35", "36", "37", "38",
                                                                                   "39", "40", "41", "42",
                                                                                   "43"};
static const uint8 liblte_interface_band_num[LIBLTE_INTERFACE_BAND_N_ITEMS] = { 1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 17, 18, 19,
                                                                               20, 21, 22, 23, 24, 25, 26, 27, 28, 33, 34, 35, 36, 37, 38, 39, 40,
                                                                               41, 42, 43};
// Structs
// Functions

/*********************************************************************
    Parameter Name: DL_EARFCN

    Description: Defines the downlink E-UTRA Absolute Radio Frequency
                 Channel Number.

    Document Reference: 36.101 v11.2.0 Section 5.7.3
*********************************************************************/
// Defines
#define LIBLTE_INTERFACE_DL_EARFCN_INVALID 65535
static const uint16 liblte_interface_first_dl_earfcn[LIBLTE_INTERFACE_BAND_N_ITEMS] = {    0 + 25,   600 +  7,  1200 +  7,  1950 +  7,
                                                                                        2400 +  7,  2650 + 25,  2750 + 25,  3450 +  7,
                                                                                        3800 + 25,  4150 + 25,  4750 + 25,  5010 +  7,
                                                                                        5180 + 25,  5280 + 25,  5730 + 25,  5850 + 25,
                                                                                        6000 + 25,  6150 + 25,  6450 + 25,  6600 + 25,
                                                                                        7500 +  7,  7700 + 25,  8040 +  7,  8690 +  7,
                                                                                        9040 +  7,  9210 + 15, 36000 + 25, 36200 + 25,
                                                                                       36350 +  7, 36950 +  7, 37550 + 25, 37750 + 25,
                                                                                       38250 + 25, 38650 + 25, 39650 + 25, 41590 + 25,
                                                                                       43590 + 25};
static const uint16 liblte_interface_last_dl_earfcn[LIBLTE_INTERFACE_BAND_N_ITEMS]  = {  600 - 25,  1200 -  7,  1950 -  7,  2400 -  7,
                                                                                        2650 -  7,  2750 - 25,  3450 - 25,  3800 -  7,
                                                                                        4150 - 25,  4750 - 25,  4950 - 25,  5180 -  7,
                                                                                        5280 - 25,  5380 - 25,  5850 - 25,  6000 - 25,
                                                                                        6150 - 25,  6450 - 25,  6600 - 25,  7400 - 25,
                                                                                        7700 -  7,  8040 - 25,  8690 -  7,  9040 -  7,
                                                                                        9210 -  7,  9660 - 15, 36200 - 25, 36350 - 25,
                                                                                       36950 -  7, 37550 -  7, 37750 - 25, 38250 - 25,
                                                                                       38650 - 25, 39650 - 25, 41590 - 25, 43590 - 25,
                                                                                       45590 - 25};
// Enums
// Structs
// Functions
uint32 liblte_interface_dl_earfcn_to_frequency(uint16 dl_earfcn);
uint16 liblte_interface_get_corresponding_dl_earfcn(uint16 ul_earfcn);

/*********************************************************************
    Parameter Name: UL_EARFCN

    Description: Defines the uplink E-UTRA Absolute Radio Frequency
                 Channel Number.

    Document Reference: 36.101 v11.2.0 Section 5.7.3
*********************************************************************/
// Defines
#define LIBLTE_INTERFACE_UL_EARFCN_INVALID 65535
static const uint16 liblte_interface_first_ul_earfcn[LIBLTE_INTERFACE_BAND_N_ITEMS] = {18000 + 25, 18600 +  7, 19200 +  7, 19950 +  7,
                                                                                       20400 +  7, 20650 + 25, 20750 + 25, 21450 +  7,
                                                                                       21800 + 25, 22150 + 25, 22750 + 25, 23010 +  7,
                                                                                       23180 + 25, 23280 + 25, 23730 + 25, 23850 + 25,
                                                                                       24000 + 25, 24150 + 25, 24450 + 25, 24600 + 25,
                                                                                       25500 +  7, 25700 + 25, 26040 +  7, 26690 +  7,
                                                                                       27040 +  7, 27210 +  7, 36000 + 25, 36200 + 25,
                                                                                       36350 +  7, 36950 +  7, 37550 + 25, 37750 + 25,
                                                                                       38250 + 25, 38650 + 25, 39650 + 25, 41590 + 25,
                                                                                       43590 + 25};
static const uint16 liblte_interface_last_ul_earfcn[LIBLTE_INTERFACE_BAND_N_ITEMS]  = {18600 - 25, 19200 -  7, 19950 -  7, 20400 -  7,
                                                                                       20650 -  7, 20750 - 25, 21450 - 25, 21800 -  7,
                                                                                       22150 - 25, 22750 - 25, 22950 - 25, 23180 -  7,
                                                                                       23280 - 25, 23380 - 25, 23850 - 25, 24000 - 25,
                                                                                       24150 - 25, 24450 - 25, 24600 - 25, 25400 - 25,
                                                                                       25700 -  7, 26040 - 25, 26690 -  7, 27040 -  7,
                                                                                       27210 -  7, 27660 - 15, 36200 - 25, 36350 - 25,
                                                                                       36950 -  7, 37550 -  7, 37750 - 25, 38250 - 25,
                                                                                       38650 - 25, 39650 - 25, 41590 - 25, 43590 - 25,
                                                                                       45590 - 25};
// Enums
// Structs
// Functions
uint32 liblte_interface_ul_earfcn_to_frequency(uint16 ul_earfcn);
uint16 liblte_interface_get_corresponding_ul_earfcn(uint16 dl_earfcn);

#endif /* __LIBLTE_INTERFACE_H__ */
