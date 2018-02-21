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

    File: liblte_common_tests.cc

    Description: Contains all the tests for the LTE common library.

    Revision History
    ----------    -------------    --------------------------------------------
    07/29/2017    Ben Wojtowicz    Created file.

*******************************************************************************/

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "liblte_common.h"

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
                              FUNCTIONS
*******************************************************************************/

int main(int argc, char *argv[])
{
    uint32  i;
    uint32  j;
    uint32  value;
    uint8   bits[32];
    uint8  *bits_ptr;

    // Check liblte_value_2_bits with single bit values
    for(i=0; i<32; i++)
    {
        bits_ptr = &bits[0];
        liblte_value_2_bits(1<<i, &bits_ptr, 32);
        for(j=0; j<32; j++)
        {
            if(!((j == i &&
                  bits[32-1-j] == 1) ||
                 bits[32-1-j] == 0))
            {
                // Test failed
                printf("Single bit tests for liblte_value_2_bits failed!\n");
                exit(-1);
            }
        }
    }

    // Check random values with liblte_value_2_bits and liblte_bits_2_value
    for(i=0; i<32; i++)
    {
        value    = rand();
        bits_ptr = &bits[0];
        liblte_value_2_bits(value, &bits_ptr, 32);
        bits_ptr = &bits[0];
        if(value != liblte_bits_2_value(&bits_ptr, 32))
        {
            // Test failed
            printf("Random tests for liblte_value_2_bits and liblte_bits_2_value failed!\n");
            exit(-1);
        }
    }

    // All tests passed
    printf("Tests passed!\n");
    exit(0);
}
