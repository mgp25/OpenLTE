/*******************************************************************************

    Copyright 2013-2017 Ben Wojtowicz

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

    File: LTE_fdd_enb_pdcp.h

    Description: Contains all the definitions for the LTE FDD eNodeB
                 packet data convergence protocol layer.

    Revision History
    ----------    -------------    --------------------------------------------
    11/09/2013    Ben Wojtowicz    Created file
    05/04/2014    Ben Wojtowicz    Added communication to RLC and RRC.
    11/29/2014    Ben Wojtowicz    Added communication to IP gateway.
    12/16/2014    Ben Wojtowicz    Added ol extension to message queues.
    02/15/2015    Ben Wojtowicz    Moved to new message queue.
    12/06/2015    Ben Wojtowicz    Changed boost::mutex to sem_t.
    02/13/2016    Ben Wojtowicz    Removed boost message queue include.
    07/29/2017    Ben Wojtowicz    Moved away from singleton pattern.

*******************************************************************************/

#ifndef __LTE_FDD_ENB_PDCP_H__
#define __LTE_FDD_ENB_PDCP_H__

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_fdd_enb_cnfg_db.h"
#include "LTE_fdd_enb_msgq.h"

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
                              CLASS DECLARATIONS
*******************************************************************************/

class LTE_fdd_enb_pdcp
{
public:
    // Constructor/Destructor
    LTE_fdd_enb_pdcp();
    ~LTE_fdd_enb_pdcp();

    // Start/Stop
    void start(LTE_fdd_enb_msgq *from_rlc, LTE_fdd_enb_msgq *from_rrc, LTE_fdd_enb_msgq *from_gw, LTE_fdd_enb_msgq *to_rlc, LTE_fdd_enb_msgq *to_rrc, LTE_fdd_enb_msgq *to_gw, LTE_fdd_enb_interface *iface);
    void stop(void);

    // External interface
    void update_sys_info(void);

private:
    // Start/Stop
    LTE_fdd_enb_interface *interface;
    sem_t                  start_sem;
    bool                   started;

    // Communication
    void handle_rlc_msg(LTE_FDD_ENB_MESSAGE_STRUCT &msg);
    void handle_rrc_msg(LTE_FDD_ENB_MESSAGE_STRUCT &msg);
    void handle_gw_msg(LTE_FDD_ENB_MESSAGE_STRUCT &msg);
    LTE_fdd_enb_msgq *msgq_from_rlc;
    LTE_fdd_enb_msgq *msgq_from_rrc;
    LTE_fdd_enb_msgq *msgq_from_gw;
    LTE_fdd_enb_msgq *msgq_to_rlc;
    LTE_fdd_enb_msgq *msgq_to_rrc;
    LTE_fdd_enb_msgq *msgq_to_gw;

    // RLC Message Handlers
    void handle_pdu_ready(LTE_FDD_ENB_PDCP_PDU_READY_MSG_STRUCT *pdu_ready);

    // RRC Message Handlers
    void handle_sdu_ready(LTE_FDD_ENB_PDCP_SDU_READY_MSG_STRUCT *sdu_ready);

    // GW Message Handlers
    void handle_data_sdu_ready(LTE_FDD_ENB_PDCP_DATA_SDU_READY_MSG_STRUCT *data_sdu_ready);

    // Parameters
    sem_t                       sys_info_sem;
    LTE_FDD_ENB_SYS_INFO_STRUCT sys_info;
};

#endif /* __LTE_FDD_ENB_PDCP_H__ */
