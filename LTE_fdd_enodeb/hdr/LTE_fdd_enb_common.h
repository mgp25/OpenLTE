/*******************************************************************************

    Copyright 2015-2016 Ben Wojtowicz

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

    File: LTE_fdd_enb_common.h

    Description: Contains all the common definitions for the LTE FDD eNodeB.

    Revision History
    ----------    -------------    --------------------------------------------
    02/15/2015    Ben Wojtowicz    Created file
    03/12/2016    Ben Wojtowicz    Added error for H-ARQ info not found.

*******************************************************************************/

#ifndef __LTE_FDD_ENB_COMMON_H__
#define __LTE_FDD_ENB_COMMON_H__

/*******************************************************************************
                              INCLUDES
*******************************************************************************/


/*******************************************************************************
                              DEFINES
*******************************************************************************/


/*******************************************************************************
                              FORWARD DECLARATIONS
*******************************************************************************/


/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/

typedef enum{
    LTE_FDD_ENB_ERROR_NONE = 0,
    LTE_FDD_ENB_ERROR_INVALID_COMMAND,
    LTE_FDD_ENB_ERROR_INVALID_PARAM,
    LTE_FDD_ENB_ERROR_OUT_OF_BOUNDS,
    LTE_FDD_ENB_ERROR_EXCEPTION,
    LTE_FDD_ENB_ERROR_ALREADY_STARTED,
    LTE_FDD_ENB_ERROR_ALREADY_STOPPED,
    LTE_FDD_ENB_ERROR_CANT_START,
    LTE_FDD_ENB_ERROR_CANT_STOP,
    LTE_FDD_ENB_ERROR_BAD_ALLOC,
    LTE_FDD_ENB_ERROR_USER_NOT_FOUND,
    LTE_FDD_ENB_ERROR_NO_FREE_C_RNTI,
    LTE_FDD_ENB_ERROR_C_RNTI_NOT_FOUND,
    LTE_FDD_ENB_ERROR_CANT_SCHEDULE,
    LTE_FDD_ENB_ERROR_VARIABLE_NOT_DYNAMIC,
    LTE_FDD_ENB_ERROR_MASTER_CLOCK_FAIL,
    LTE_FDD_ENB_ERROR_NO_MSG_IN_QUEUE,
    LTE_FDD_ENB_ERROR_RB_NOT_SETUP,
    LTE_FDD_ENB_ERROR_RB_ALREADY_SETUP,
    LTE_FDD_ENB_ERROR_TIMER_NOT_FOUND,
    LTE_FDD_ENB_ERROR_CANT_REASSEMBLE_SDU,
    LTE_FDD_ENB_ERROR_DUPLICATE_ENTRY,
    LTE_FDD_ENB_ERROR_READ_ONLY,
    LTE_FDD_ENB_ERROR_HARQ_INFO_NOT_FOUND,
    LTE_FDD_ENB_ERROR_N_ITEMS,
}LTE_FDD_ENB_ERROR_ENUM;
static const char LTE_fdd_enb_error_text[LTE_FDD_ENB_ERROR_N_ITEMS][100] = {"none",
                                                                            "invalid command",
                                                                            "invalid parameter",
                                                                            "out of bounds",
                                                                            "exception",
                                                                            "already started",
                                                                            "already stopped",
                                                                            "cant start",
                                                                            "cant stop",
                                                                            "bad alloc",
                                                                            "user not found",
                                                                            "no free C-RNTI",
                                                                            "C-RNTI not found",
                                                                            "cant schedule",
                                                                            "variable not dynamic",
                                                                            "unable to set master clock rate",
                                                                            "no message in queue",
                                                                            "RB not setup",
                                                                            "RB already setup",
                                                                            "timer not found",
                                                                            "cant reassemble SDU",
                                                                            "duplicate entry",
                                                                            "read only",
                                                                            "HARQ info not found"};

/*******************************************************************************
                              CLASS DECLARATIONS
*******************************************************************************/

#endif /* __LTE_FDD_ENB_COMMON_H__ */
