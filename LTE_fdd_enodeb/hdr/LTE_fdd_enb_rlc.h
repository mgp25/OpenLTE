/*******************************************************************************

    Copyright 2013-2016 Ben Wojtowicz

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

    File: LTE_fdd_enb_rlc.h

    Description: Contains all the definitions for the LTE FDD eNodeB
                 radio link control layer.

    Revision History
    ----------    -------------    --------------------------------------------
    11/09/2013    Ben Wojtowicz    Created file
    05/04/2014    Ben Wojtowicz    Added communication to MAC and PDCP.
    06/15/2014    Ben Wojtowicz    Using the latest LTE library.
    08/03/2014    Ben Wojtowicz    Added transmit functionality.
    11/29/2014    Ben Wojtowicz    Using the byte message structure.
    12/16/2014    Ben Wojtowicz    Added ol extension to message queues.
    02/15/2015    Ben Wojtowicz    Moved to new message queue.
    12/06/2015    Ben Wojtowicz    Changed boost::mutex to sem_t.
    02/13/2016    Ben Wojtowicz    Removed boost message queue include.
    12/18/2016    Ben Wojtowicz    Properly handling multiple AMD PDUs.

*******************************************************************************/

#ifndef __LTE_FDD_ENB_RLC_H__
#define __LTE_FDD_ENB_RLC_H__

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

class LTE_fdd_enb_rlc
{
public:
    // Singleton
    static LTE_fdd_enb_rlc* get_instance(void);
    static void cleanup(void);

    // Start/Stop
    void start(LTE_fdd_enb_msgq *from_mac, LTE_fdd_enb_msgq *from_pdcp, LTE_fdd_enb_msgq *to_mac, LTE_fdd_enb_msgq *to_pdcp, LTE_fdd_enb_interface *iface);
    void stop(void);

    // External interface
    void update_sys_info(void);
    void handle_retransmit(LIBLTE_RLC_SINGLE_AMD_PDU_STRUCT *amd, LTE_fdd_enb_user *user, LTE_fdd_enb_rb *rb);

private:
    // Singleton
    static LTE_fdd_enb_rlc *instance;
    LTE_fdd_enb_rlc();
    ~LTE_fdd_enb_rlc();

    // Start/Stop
    LTE_fdd_enb_interface *interface;
    sem_t                  start_sem;
    bool                   started;

    // Communication
    void handle_mac_msg(LTE_FDD_ENB_MESSAGE_STRUCT &msg);
    void handle_pdcp_msg(LTE_FDD_ENB_MESSAGE_STRUCT &msg);
    LTE_fdd_enb_msgq *msgq_from_mac;
    LTE_fdd_enb_msgq *msgq_from_pdcp;
    LTE_fdd_enb_msgq *msgq_to_mac;
    LTE_fdd_enb_msgq *msgq_to_pdcp;

    // MAC Message Handlers
    void handle_pdu_ready(LTE_FDD_ENB_RLC_PDU_READY_MSG_STRUCT *pdu_ready);
    void handle_tm_pdu(LIBLTE_BYTE_MSG_STRUCT *pdu, LTE_fdd_enb_user *user, LTE_fdd_enb_rb *rb);
    void handle_um_pdu(LIBLTE_BYTE_MSG_STRUCT *pdu, LTE_fdd_enb_user *user, LTE_fdd_enb_rb *rb);
    void handle_am_pdu(LIBLTE_BYTE_MSG_STRUCT *pdu, LTE_fdd_enb_user *user, LTE_fdd_enb_rb *rb);
    void handle_status_pdu(LIBLTE_BYTE_MSG_STRUCT *pdu, LTE_fdd_enb_user *user, LTE_fdd_enb_rb *rb);

    // PDCP Message Handlers
    void handle_sdu_ready(LTE_FDD_ENB_RLC_SDU_READY_MSG_STRUCT *sdu_ready);
    void handle_tm_sdu(LIBLTE_BYTE_MSG_STRUCT *sdu, LTE_fdd_enb_user *user, LTE_fdd_enb_rb *rb);
    void handle_um_sdu(LIBLTE_BYTE_MSG_STRUCT *sdu, LTE_fdd_enb_user *user, LTE_fdd_enb_rb *rb);
    void handle_am_sdu(LIBLTE_BYTE_MSG_STRUCT *sdu, LTE_fdd_enb_user *user, LTE_fdd_enb_rb *rb);

    // Message Constructors
    void send_status_pdu(LIBLTE_RLC_STATUS_PDU_STRUCT *status_pdu, LTE_fdd_enb_user *user, LTE_fdd_enb_rb *rb);
    void send_amd_pdu(LIBLTE_RLC_SINGLE_AMD_PDU_STRUCT *amd, LTE_fdd_enb_user *user, LTE_fdd_enb_rb *rb);

    // Parameters
    sem_t                       sys_info_sem;
    LTE_FDD_ENB_SYS_INFO_STRUCT sys_info;
};

#endif /* __LTE_FDD_ENB_RLC_H__ */
