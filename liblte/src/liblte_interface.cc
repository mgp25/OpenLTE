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

    File: liblte_interface.cc

    Description: Contains all the implementations for the LTE interface
                 library.

    Revision History
    ----------    -------------    --------------------------------------------
    02/26/2013    Ben Wojtowicz    Created file.
    11/13/2013    Ben Wojtowicz    Added functions for getting corresponding
                                   EARFCNs for FDD configuration
    12/18/2016    Ben Wojtowicz    Added support for band 26, 26, and 28 (thanks
                                   to Jeremy Quirke).

*******************************************************************************/

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "liblte_interface.h"

/*******************************************************************************
                              DEFINES
*******************************************************************************/


/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/


/*******************************************************************************
                              GLOBAL VARIABLES
*******************************************************************************/


/*******************************************************************************
                              LOCAL FUNCTION PROTOTYPES
*******************************************************************************/


/*******************************************************************************
                              LIBRARY FUNCTIONS
*******************************************************************************/

/*********************************************************************
    Parameter Name: Band

    Description: Defines the operating frequency bands.

    Document Reference: 36.101 v10.4.0 Section 5.5
*********************************************************************/

/*********************************************************************
    Parameter Name: DL_EARFCN

    Description: Defines the downlink E-UTRA Absolute Radio Frequency
                 Channel Number.

    Document Reference: 36.101 v10.4.0 Section 5.7.3
*********************************************************************/
uint32 liblte_interface_dl_earfcn_to_frequency(uint16 dl_earfcn)
{
    uint32 F_dl;
    uint32 F_dl_low[LIBLTE_INTERFACE_BAND_N_ITEMS] = {  2110000000,   1930000000,   1805000000,   2110000000,
                                                         869000000,    875000000, 2620000000UL,    925000000,
                                                        1844900000,   2110000000,   1475900000,    729000000,
                                                         746000000,    758000000,    734000000,    860000000,
                                                         875000000,    791000000,   1495900000, 3510000000UL,
                                                      2180000000UL,   1525000000,   1930000000,    859000000,
                                                         852000000,    758000000,   1900000000,   2010000000,
                                                        1850000000,   1930000000,   1910000000, 2570000000UL,
                                                        1880000000, 2300000000UL, 2496000000UL, 3400000000UL,
                                                      3600000000UL};
    uint32 i;
    uint16 N_offs_dl[LIBLTE_INTERFACE_BAND_N_ITEMS] = {    0,   600,  1200,  1950,  2400,  2650,  2750,  3450,
                                                        3800,  4150,  4750,  5010,  5180,  5280,  5730,  5850,
                                                        6000,  6150,  6450,  6600,  7500,  7700,  8040,  8690,
                                                        9040,  9210, 36000, 36200, 36350, 36950, 37550, 37750,
                                                       38250, 38650, 39650, 41590, 43590};

    for(i=0; i<LIBLTE_INTERFACE_BAND_N_ITEMS; i++)
    {
        if(dl_earfcn >= liblte_interface_first_dl_earfcn[i] &&
           dl_earfcn <= liblte_interface_last_dl_earfcn[i])
        {
            break;
        }
    }
    if(LIBLTE_INTERFACE_BAND_N_ITEMS != i)
    {
        F_dl = F_dl_low[i] + 100000*(dl_earfcn - N_offs_dl[i]);
    }else{
        F_dl = 0;
    }

    return(F_dl);
}
uint16 liblte_interface_get_corresponding_dl_earfcn(uint16 ul_earfcn)
{
    uint16 dl_earfcn = LIBLTE_INTERFACE_DL_EARFCN_INVALID;

    if(0 != liblte_interface_ul_earfcn_to_frequency(ul_earfcn))
    {
        if(liblte_interface_first_ul_earfcn[0] <= ul_earfcn &&
           liblte_interface_last_ul_earfcn[25] >= ul_earfcn)
        {
            // FDD
            dl_earfcn = ul_earfcn - 18000;
        }else{
            // TDD
            dl_earfcn = ul_earfcn;
        }
    }

    return(dl_earfcn);
}

/*********************************************************************
    Parameter Name: UL_EARFCN

    Description: Defines the uplink E-UTRA Absolute Radio Frequency
                 Channel Number.

    Document Reference: 36.101 v10.4.0 Section 5.7.3
*********************************************************************/
uint32 liblte_interface_ul_earfcn_to_frequency(uint16 ul_earfcn)
{
    uint32 F_ul;
    uint32 F_ul_low[LIBLTE_INTERFACE_BAND_N_ITEMS] = {  1920000000,   1850000000,   1710000000,   1710000000,
                                                         824000000,    830000000, 2500000000UL,    880000000,
                                                        1749900000,   1710000000,   1427900000,    699000000,
                                                         777000000,    788000000,    704000000,    815000000,
                                                         830000000,    832000000,   1447900000, 3410000000UL,
                                                        2000000000,   1626500000,   1850000000,    814000000,
                                                         807000000,    703000000,   1900000000,   2010000000,
                                                        1850000000,   1930000000,   1910000000, 2570000000UL,
                                                        1880000000, 2300000000UL, 2496000000UL, 3400000000UL,
                                                      3600000000UL};
    uint32 i;
    uint16 N_offs_ul[LIBLTE_INTERFACE_BAND_N_ITEMS] = {18000, 18600, 19200, 19950, 20400, 20650, 20750, 21450,
                                                       21800, 22150, 22750, 23010, 23180, 23280, 23730, 23850,
                                                       24000, 24150, 24450, 24600, 25500, 25700, 26040, 26690,
                                                       27040, 27210, 36000, 36200, 36350, 36950, 37550, 37750,
                                                       38250, 38650, 39650, 41590, 43590};

    for(i=0; i<LIBLTE_INTERFACE_BAND_N_ITEMS; i++)
    {
        if(ul_earfcn >= liblte_interface_first_ul_earfcn[i] &&
           ul_earfcn <= liblte_interface_last_ul_earfcn[i])
        {
            break;
        }
    }
    if(LIBLTE_INTERFACE_BAND_N_ITEMS != i)
    {
        F_ul = F_ul_low[i] + 100000*(ul_earfcn - N_offs_ul[i]);
    }else{
        F_ul = 0;
    }

    return(F_ul);
}
uint16 liblte_interface_get_corresponding_ul_earfcn(uint16 dl_earfcn)
{
    uint16 ul_earfcn = LIBLTE_INTERFACE_UL_EARFCN_INVALID;

    if(0 != liblte_interface_dl_earfcn_to_frequency(dl_earfcn))
    {
        if(liblte_interface_first_dl_earfcn[0] <= dl_earfcn &&
           liblte_interface_last_dl_earfcn[25] >= dl_earfcn)
        {
            // FDD
            ul_earfcn = dl_earfcn + 18000;
        }else{
            // TDD
            ul_earfcn = dl_earfcn;
        }
    }

    return(ul_earfcn);
}

/*******************************************************************************
                              LOCAL FUNCTIONS
*******************************************************************************/
