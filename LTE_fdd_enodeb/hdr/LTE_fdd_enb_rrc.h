/*******************************************************************************

    Copyright 2013-2017 Ben Wojtowicz
    Copyright 2016 Przemek Bereski (send_ue_capability_enquiry)

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

    File: LTE_fdd_enb_rrc.h

    Description: Contains all the definitions for the LTE FDD eNodeB
                 radio resource control layer.

    Revision History
    ----------    -------------    --------------------------------------------
    11/09/2013    Ben Wojtowicz    Created file
    05/04/2014    Ben Wojtowicz    Added PDCP communication and UL CCCH state
                                   machine.
    06/15/2014    Ben Wojtowicz    Added UL DCCH message handling and MME NAS
                                   message handling.
    08/03/2014    Ben Wojtowicz    Added downlink NAS message handling and
                                   connection release.
    11/01/2014    Ben Wojtowicz    Added RRC connection reconfiguration and
                                   security mode command messages.
    11/29/2014    Ben Wojtowicz    Added user and rb update to
                                   parse_ul_ccch_message.
    12/16/2014    Ben Wojtowicz    Added ol extension to message queues.
    02/15/2015    Ben Wojtowicz    Moved to new message queue.
    12/06/2015    Ben Wojtowicz    Changed boost::mutex to sem_t.
    02/13/2016    Ben Wojtowicz    Removed boost message queue include and
                                   add support for connection reestablishment
                                   and connection reestablishment reject.
    07/03/2016    Przemek Bereski  Added send_ue_capability_enquiry.
    07/29/2017    Ben Wojtowicz    Added SR support.

*******************************************************************************/

#ifndef __LTE_FDD_ENB_RRC_H__
#define __LTE_FDD_ENB_RRC_H__

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_fdd_enb_cnfg_db.h"
#include "LTE_fdd_enb_user.h"
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

class LTE_fdd_enb_rrc
{
public:
    // Singleton
    static LTE_fdd_enb_rrc* get_instance(void);
    static void cleanup(void);

    // Start/Stop
    void start(LTE_fdd_enb_msgq *from_pdcp, LTE_fdd_enb_msgq *from_mme, LTE_fdd_enb_msgq *to_pdcp, LTE_fdd_enb_msgq *to_mme, LTE_fdd_enb_interface *iface);
    void stop(void);

    // External interface
    void update_sys_info(void);
    void handle_cmd(LTE_FDD_ENB_RRC_CMD_READY_MSG_STRUCT *cmd);

private:
    // Singleton
    static LTE_fdd_enb_rrc *instance;
    LTE_fdd_enb_rrc();
    ~LTE_fdd_enb_rrc();

    // Start/Stop
    LTE_fdd_enb_interface *interface;
    sem_t                  start_sem;
    bool                   started;

    // Communication
    void handle_pdcp_msg(LTE_FDD_ENB_MESSAGE_STRUCT &msg);
    void handle_mme_msg(LTE_FDD_ENB_MESSAGE_STRUCT &msg);
    LTE_fdd_enb_msgq *msgq_from_pdcp;
    LTE_fdd_enb_msgq *msgq_from_mme;
    LTE_fdd_enb_msgq *msgq_to_pdcp;
    LTE_fdd_enb_msgq *msgq_to_mme;

    // PDCP Message Handlers
    void handle_pdu_ready(LTE_FDD_ENB_RRC_PDU_READY_MSG_STRUCT *pdu_ready);

    // MME Message Handlers
    void handle_nas_msg(LTE_FDD_ENB_RRC_NAS_MSG_READY_MSG_STRUCT *nas_msg);

    // State Machines
    void ccch_sm(LIBLTE_BIT_MSG_STRUCT *msg, LTE_fdd_enb_user *user, LTE_fdd_enb_rb *rb);
    void dcch_sm(LIBLTE_BIT_MSG_STRUCT *msg, LTE_fdd_enb_user *user, LTE_fdd_enb_rb *rb);

    // Message Parsers
    void parse_ul_ccch_message(LIBLTE_BIT_MSG_STRUCT *msg, LTE_fdd_enb_user **user, LTE_fdd_enb_rb **rb);
    void parse_ul_dcch_message(LIBLTE_BIT_MSG_STRUCT *msg, LTE_fdd_enb_user *user, LTE_fdd_enb_rb *rb);

    // Message Senders
    void send_dl_info_transfer(LTE_fdd_enb_user *user, LTE_fdd_enb_rb *rb, LIBLTE_BYTE_MSG_STRUCT *msg);
    void send_rrc_con_reconfig(LTE_fdd_enb_user *user, LTE_fdd_enb_rb *rb, LIBLTE_BYTE_MSG_STRUCT *msg);
    void send_rrc_con_reest(LTE_fdd_enb_user *user, LTE_fdd_enb_rb *rb);
    void send_rrc_con_reest_reject(LTE_fdd_enb_user *user, LTE_fdd_enb_rb *rb);
    void send_rrc_con_release(LTE_fdd_enb_user *user, LTE_fdd_enb_rb *rb);
    void send_rrc_con_setup(LTE_fdd_enb_user *user, LTE_fdd_enb_rb *rb);
    void send_security_mode_command(LTE_fdd_enb_user *user, LTE_fdd_enb_rb *rb);
    void send_ue_capability_enquiry(LTE_fdd_enb_user *user, LTE_fdd_enb_rb *rb);

    // Helpers
    void increment_i_sr(void);

    // Parameters
    sem_t                       sys_info_sem;
    LTE_FDD_ENB_SYS_INFO_STRUCT sys_info;
    uint32                      i_sr;
};

#endif /* __LTE_FDD_ENB_RRC_H__ */
