#line 2 "LTE_fdd_enb_rrc.cc" // Make __FILE__ omit the path
/*******************************************************************************

    Copyright 2013-2017 Ben Wojtowicz
    Copyright 2016 Przemek Bereski (UE capability information and UE capability
                                    enquiry support)

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

    File: LTE_fdd_enb_rrc.cc

    Description: Contains all the implementations for the LTE FDD eNodeB
                 radio resource control layer.

    Revision History
    ----------    -------------    --------------------------------------------
    11/10/2013    Ben Wojtowicz    Created file
    01/18/2014    Ben Wojtowicz    Added level to debug prints.
    05/04/2014    Ben Wojtowicz    Added PDCP communication and UL CCCH state
                                   machine.
    06/15/2014    Ben Wojtowicz    Added UL DCCH message handling and MME NAS
                                   message handling.
    08/03/2014    Ben Wojtowicz    Added downlink NAS message handling and
                                   connection release.
    11/01/2014    Ben Wojtowicz    Added RRC connection reconfiguration and
                                   security mode command messages.
    11/29/2014    Ben Wojtowicz    Added DRB1 and DRB2 support and user
                                   recovery on RRC connection request.
    12/16/2014    Ben Wojtowicz    Added ol extension to message queues.
    12/24/2014    Ben Wojtowicz    Using default data QoS and copying SRB1
                                   PDCP config to SRB2.
    02/15/2015    Ben Wojtowicz    Moved to new message queue and using the
                                   fixed user switch.
    07/25/2015    Ben Wojtowicz    Using the new user QoS structure, moved DRBs
                                   to RLC AM, and changed the default time
                                   alignment timer to 10240 subframes.
    12/06/2015    Ben Wojtowicz    Changed boost::mutex to pthread_mutex_t and
                                   sem_t.
    02/13/2016    Ben Wojtowicz    Added RRC connection reestablishment state
                                   machine.
    07/03/2016    Przemek Bereski  Added UE capability information and UE
                                   capability enquiry support.
    07/29/2017    Ben Wojtowicz    Add SR support, populate physical layer
                                   dedicated configurations, remove QOS and
                                   fixed UL scheduling.

*******************************************************************************/

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_fdd_enb_rrc.h"
#include "LTE_fdd_enb_pdcp.h"
#include "LTE_fdd_enb_mac.h"
#include "LTE_fdd_enb_user_mgr.h"
#include "libtools_scoped_lock.h"

/*******************************************************************************
                              DEFINES
*******************************************************************************/

// SR configuration
#define I_SR_MIN       15
#define I_SR_MAX       34
#define N_1_P_PUCCH_SR 1

/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/


/*******************************************************************************
                              GLOBAL VARIABLES
*******************************************************************************/

LTE_fdd_enb_rrc*       LTE_fdd_enb_rrc::instance = NULL;
static pthread_mutex_t rrc_instance_mutex        = PTHREAD_MUTEX_INITIALIZER;

/*******************************************************************************
                              CLASS IMPLEMENTATIONS
*******************************************************************************/

/*******************/
/*    Singleton    */
/*******************/
LTE_fdd_enb_rrc* LTE_fdd_enb_rrc::get_instance(void)
{
    libtools_scoped_lock lock(rrc_instance_mutex);

    if(NULL == instance)
    {
        instance = new LTE_fdd_enb_rrc();
    }

    return(instance);
}
void LTE_fdd_enb_rrc::cleanup(void)
{
    libtools_scoped_lock lock(rrc_instance_mutex);

    if(NULL != instance)
    {
        delete instance;
        instance = NULL;
    }
}

/********************************/
/*    Constructor/Destructor    */
/********************************/
LTE_fdd_enb_rrc::LTE_fdd_enb_rrc()
{
    sem_init(&start_sem, 0, 1);
    sem_init(&sys_info_sem, 0, 1);
    started = false;
}
LTE_fdd_enb_rrc::~LTE_fdd_enb_rrc()
{
    stop();
    sem_destroy(&sys_info_sem);
    sem_destroy(&start_sem);
}

/********************/
/*    Start/Stop    */
/********************/
void LTE_fdd_enb_rrc::start(LTE_fdd_enb_msgq      *from_pdcp,
                            LTE_fdd_enb_msgq      *from_mme,
                            LTE_fdd_enb_msgq      *to_pdcp,
                            LTE_fdd_enb_msgq      *to_mme,
                            LTE_fdd_enb_interface *iface)
{
    libtools_scoped_lock lock(start_sem);
    LTE_fdd_enb_msgq_cb  pdcp_cb(&LTE_fdd_enb_msgq_cb_wrapper<LTE_fdd_enb_rrc, &LTE_fdd_enb_rrc::handle_pdcp_msg>, this);
    LTE_fdd_enb_msgq_cb  mme_cb(&LTE_fdd_enb_msgq_cb_wrapper<LTE_fdd_enb_rrc, &LTE_fdd_enb_rrc::handle_mme_msg>, this);

    if(!started)
    {
        interface      = iface;
        started        = true;
        i_sr           = I_SR_MIN;
        msgq_from_pdcp = from_pdcp;
        msgq_from_mme  = from_mme;
        msgq_to_pdcp   = to_pdcp;
        msgq_to_mme    = to_mme;
        msgq_from_pdcp->attach_rx(pdcp_cb);
        msgq_from_mme->attach_rx(mme_cb);
    }
}
void LTE_fdd_enb_rrc::stop(void)
{
    libtools_scoped_lock lock(start_sem);

    if(started)
    {
        started = false;
    }
}

/***********************/
/*    Communication    */
/***********************/
void LTE_fdd_enb_rrc::handle_pdcp_msg(LTE_FDD_ENB_MESSAGE_STRUCT &msg)
{
    if(LTE_FDD_ENB_DEST_LAYER_RRC == msg.dest_layer ||
       LTE_FDD_ENB_DEST_LAYER_ANY == msg.dest_layer)
    {
        switch(msg.type)
        {
        case LTE_FDD_ENB_MESSAGE_TYPE_RRC_PDU_READY:
            handle_pdu_ready(&msg.msg.rrc_pdu_ready);
            break;
        default:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                                      __FILE__,
                                      __LINE__,
                                      "Received invalid PDCP message %s",
                                      LTE_fdd_enb_message_type_text[msg.type]);
            break;
        }
    }else{
        // Forward message to MME
        msgq_to_mme->send(msg);
    }
}
void LTE_fdd_enb_rrc::handle_mme_msg(LTE_FDD_ENB_MESSAGE_STRUCT &msg)
{
    if(LTE_FDD_ENB_DEST_LAYER_RRC == msg.dest_layer ||
       LTE_FDD_ENB_DEST_LAYER_ANY == msg.dest_layer)
    {
        switch(msg.type)
        {
        case LTE_FDD_ENB_MESSAGE_TYPE_RRC_NAS_MSG_READY:
            handle_nas_msg(&msg.msg.rrc_nas_msg_ready);
            break;
        case LTE_FDD_ENB_MESSAGE_TYPE_RRC_CMD_READY:
            handle_cmd(&msg.msg.rrc_cmd_ready);
            break;
        default:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                                      __FILE__,
                                      __LINE__,
                                      "Received invalid MME message %s",
                                      LTE_fdd_enb_message_type_text[msg.type]);
            break;
        }
    }else{
        // Forward message to PDCP
        msgq_to_pdcp->send(msg);
    }
}

/****************************/
/*    External Interface    */
/****************************/
void LTE_fdd_enb_rrc::update_sys_info(void)
{
    libtools_scoped_lock  lock(sys_info_sem);
    LTE_fdd_enb_cnfg_db  *cnfg_db = LTE_fdd_enb_cnfg_db::get_instance();

    cnfg_db->get_sys_info(sys_info);
}

/*******************************/
/*    PDCP Message Handlers    */
/*******************************/
void LTE_fdd_enb_rrc::handle_pdu_ready(LTE_FDD_ENB_RRC_PDU_READY_MSG_STRUCT *pdu_ready)
{
    LIBLTE_BIT_MSG_STRUCT *pdu;

    if(LTE_FDD_ENB_ERROR_NONE == pdu_ready->rb->get_next_rrc_pdu(&pdu))
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                  LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                                  __FILE__,
                                  __LINE__,
                                  pdu,
                                  "Received PDU for RNTI=%u and RB=%s",
                                  pdu_ready->user->get_c_rnti(),
                                  LTE_fdd_enb_rb_text[pdu_ready->rb->get_rb_id()]);

        // Call the appropriate state machine
        switch(pdu_ready->rb->get_rb_id())
        {
        case LTE_FDD_ENB_RB_SRB0:
            ccch_sm(pdu, pdu_ready->user, pdu_ready->rb);
            break;
        case LTE_FDD_ENB_RB_SRB1:
        case LTE_FDD_ENB_RB_SRB2:
            dcch_sm(pdu, pdu_ready->user, pdu_ready->rb);
            break;
        default:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                                      __FILE__,
                                      __LINE__,
                                      pdu,
                                      "Received PDU for RNTI=%u and invalid RB=%s",
                                      pdu_ready->user->get_c_rnti(),
                                      LTE_fdd_enb_rb_text[pdu_ready->rb->get_rb_id()]);
            break;
        }

        // Delete the PDU
        pdu_ready->rb->delete_next_rrc_pdu();
    }else{
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                                  __FILE__,
                                  __LINE__,
                                  "Received pdu_ready message with no PDU queued");
    }
}

/******************************/
/*    MME Message Handlers    */
/******************************/
void LTE_fdd_enb_rrc::handle_nas_msg(LTE_FDD_ENB_RRC_NAS_MSG_READY_MSG_STRUCT *nas_msg)
{
    LIBLTE_BYTE_MSG_STRUCT *msg;

    if(LTE_FDD_ENB_ERROR_NONE == nas_msg->rb->get_next_rrc_nas_msg(&msg))
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                  LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                                  __FILE__,
                                  __LINE__,
                                  msg,
                                  "Received NAS message for RNTI=%u and RB=%s",
                                  nas_msg->user->get_c_rnti(),
                                  LTE_fdd_enb_rb_text[nas_msg->rb->get_rb_id()]);

        // Send the NAS message
        send_dl_info_transfer(nas_msg->user, nas_msg->rb, msg);

        // Delete the NAS message
        nas_msg->rb->delete_next_rrc_nas_msg();
    }else{
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                                  __FILE__,
                                  __LINE__,
                                  "Received NAS message with no message queued");
    }
}
void LTE_fdd_enb_rrc::handle_cmd(LTE_FDD_ENB_RRC_CMD_READY_MSG_STRUCT *cmd)
{
    LIBLTE_BYTE_MSG_STRUCT *msg;
    LTE_fdd_enb_rb         *srb2 = NULL;
    LTE_fdd_enb_rb         *drb1 = NULL;
    LTE_fdd_enb_rb         *drb2 = NULL;

    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                              __FILE__,
                              __LINE__,
                              "Received MME command %s for RNTI=%u and RB=%s",
                              LTE_fdd_enb_rrc_cmd_text[cmd->cmd],
                              cmd->user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[cmd->rb->get_rb_id()]);

    switch(cmd->cmd)
    {
    case LTE_FDD_ENB_RRC_CMD_RELEASE:
        send_rrc_con_release(cmd->user, cmd->rb);
        break;
    case LTE_FDD_ENB_RRC_CMD_SECURITY:
        send_ue_capability_enquiry(cmd->user, cmd->rb);
        send_security_mode_command(cmd->user, cmd->rb);
        break;
    case LTE_FDD_ENB_RRC_CMD_SETUP_DEF_DRB:
        if(LTE_FDD_ENB_ERROR_NONE == cmd->user->setup_srb2(&srb2) &&
           LTE_FDD_ENB_ERROR_NONE == cmd->user->setup_drb(LTE_FDD_ENB_RB_DRB1, &drb1))
        {
            // Configure SRB2
            srb2->set_rrc_procedure(cmd->rb->get_rrc_procedure());
            srb2->set_rrc_state(cmd->rb->get_rrc_state());
            srb2->set_mme_procedure(cmd->rb->get_mme_procedure());
            srb2->set_mme_state(cmd->rb->get_mme_state());
            srb2->set_pdcp_config(cmd->rb->get_pdcp_config());

            // Configure DRB1
            drb1->set_eps_bearer_id(cmd->user->get_eps_bearer_id());
            drb1->set_drb_id(1);
            drb1->set_lc_id(3);
            drb1->set_log_chan_group(2);

            if(LTE_FDD_ENB_ERROR_NONE == cmd->rb->get_next_rrc_nas_msg(&msg))
            {
                send_rrc_con_reconfig(cmd->user, cmd->rb, msg);
                cmd->rb->delete_next_rrc_nas_msg();
            }else{
                send_rrc_con_reconfig(cmd->user, cmd->rb, NULL);
            }
        }else{
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                                      __FILE__,
                                      __LINE__,
                                      "Handle CMD can't setup default DRB");
        }
        break;
    case LTE_FDD_ENB_RRC_CMD_SETUP_DED_DRB:
        if(LTE_FDD_ENB_ERROR_NONE == cmd->user->setup_srb2(&srb2)                     &&
           LTE_FDD_ENB_ERROR_NONE == cmd->user->setup_drb(LTE_FDD_ENB_RB_DRB1, &drb1) &&
           LTE_FDD_ENB_ERROR_NONE == cmd->user->setup_drb(LTE_FDD_ENB_RB_DRB2, &drb2))
        {
            // Configure SRB2
            srb2->set_rrc_procedure(cmd->rb->get_rrc_procedure());
            srb2->set_rrc_state(cmd->rb->get_rrc_state());
            srb2->set_mme_procedure(cmd->rb->get_mme_procedure());
            srb2->set_mme_state(cmd->rb->get_mme_state());
            srb2->set_pdcp_config(cmd->rb->get_pdcp_config());

            // Configure DRB1
            drb1->set_eps_bearer_id(cmd->user->get_eps_bearer_id());
            drb1->set_drb_id(1);
            drb1->set_lc_id(3);
            drb1->set_log_chan_group(2);

            // Configure DRB2
            drb2->set_eps_bearer_id(cmd->user->get_eps_bearer_id()+1);
            drb2->set_drb_id(2);
            drb2->set_lc_id(4);
            drb2->set_log_chan_group(3);

            if(LTE_FDD_ENB_ERROR_NONE == cmd->rb->get_next_rrc_nas_msg(&msg))
            {
                send_rrc_con_reconfig(cmd->user, cmd->rb, msg);
                cmd->rb->delete_next_rrc_nas_msg();
            }else{
                send_rrc_con_reconfig(cmd->user, cmd->rb, NULL);
            }
        }else{
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                                      __FILE__,
                                      __LINE__,
                                      "Handle CMD can't setup dedicated DRB");
        }
        break;
    default:
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                                  __FILE__,
                                  __LINE__,
                                  "Received MME command %s for RNTI=%u and RB=%s",
                                  LTE_fdd_enb_rrc_cmd_text[cmd->cmd],
                                  cmd->user->get_c_rnti(),
                                  LTE_fdd_enb_rb_text[cmd->rb->get_rb_id()]);
        break;
    }
}

/************************/
/*    State Machines    */
/************************/
void LTE_fdd_enb_rrc::ccch_sm(LIBLTE_BIT_MSG_STRUCT *msg,
                              LTE_fdd_enb_user      *user,
                              LTE_fdd_enb_rb        *rb)
{
    LTE_fdd_enb_user *loc_user = user;
    LTE_fdd_enb_rb   *loc_rb   = rb;
    LTE_fdd_enb_rb   *srb1     = NULL;

    // Parse the message
    parse_ul_ccch_message(msg, &loc_user, &loc_rb);

    switch(loc_rb->get_rrc_procedure())
    {
    case LTE_FDD_ENB_RRC_PROC_RRC_CON_REQ:
        switch(loc_rb->get_rrc_state())
        {
        case LTE_FDD_ENB_RRC_STATE_IDLE:
            loc_user->setup_srb1(&srb1);
            if(NULL != srb1)
            {
                loc_rb->set_rrc_state(LTE_FDD_ENB_RRC_STATE_SRB1_SETUP);
                send_rrc_con_setup(loc_user, loc_rb);
                srb1->set_rrc_procedure(LTE_FDD_ENB_RRC_PROC_RRC_CON_REQ);
                srb1->set_rrc_state(LTE_FDD_ENB_RRC_STATE_WAIT_FOR_CON_SETUP_COMPLETE);
            }else{
                interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                          LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                                          __FILE__,
                                          __LINE__,
                                          "UL-CCCH-Message can't setup srb1");
            }
            break;
        default:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                                      __FILE__,
                                      __LINE__,
                                      "UL-CCCH-Message RRC CON REQ state machine invalid state %s",
                                      LTE_fdd_enb_rrc_state_text[loc_rb->get_rrc_state()]);
            break;
        }
        break;
    case LTE_FDD_ENB_RRC_PROC_RRC_CON_REEST_REQ:
        switch(loc_rb->get_rrc_state())
        {
        case LTE_FDD_ENB_RRC_STATE_IDLE:
            loc_user->setup_srb1(&srb1);
            if(NULL != srb1)
            {
                loc_rb->set_rrc_state(LTE_FDD_ENB_RRC_STATE_SRB1_SETUP);
                send_rrc_con_reest(loc_user, loc_rb);
                srb1->set_rrc_procedure(LTE_FDD_ENB_RRC_PROC_RRC_CON_REEST_REQ);
                srb1->set_rrc_state(LTE_FDD_ENB_RRC_STATE_WAIT_FOR_CON_REEST_COMPLETE);
            }else{
                send_rrc_con_reest_reject(loc_user, loc_rb);
            }
            break;
        default:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                                      __FILE__,
                                      __LINE__,
                                      "UL-CCCH-Message RRC CON REEST REQ state machine invalid state %s",
                                      LTE_fdd_enb_rrc_state_text[loc_rb->get_rrc_state()]);
            break;
        }
        break;
    default:
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                                  __FILE__,
                                  __LINE__,
                                  "CCCH state machine invalid procedure %s",
                                  LTE_fdd_enb_rrc_proc_text[loc_rb->get_rrc_procedure()]);
        break;
    }
}
void LTE_fdd_enb_rrc::dcch_sm(LIBLTE_BIT_MSG_STRUCT *msg,
                              LTE_fdd_enb_user      *user,
                              LTE_fdd_enb_rb        *rb)
{
    switch(rb->get_rrc_procedure())
    {
    case LTE_FDD_ENB_RRC_PROC_RRC_CON_REEST_REQ:
    case LTE_FDD_ENB_RRC_PROC_RRC_CON_REQ:
        switch(rb->get_rrc_state())
        {
        case LTE_FDD_ENB_RRC_STATE_WAIT_FOR_CON_SETUP_COMPLETE:
        case LTE_FDD_ENB_RRC_STATE_WAIT_FOR_CON_REEST_COMPLETE:
        case LTE_FDD_ENB_RRC_STATE_RRC_CONNECTED:
            // Parse the message
            parse_ul_dcch_message(msg, user, rb);
            break;
        default:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                                      __FILE__,
                                      __LINE__,
                                      "UL-DCCH-Message RRC CON REQ state machine invalid state %s",
                                      LTE_fdd_enb_rrc_state_text[rb->get_rrc_state()]);
            break;
        }
        break;
    default:
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                                  __FILE__,
                                  __LINE__,
                                  "DCCH state machine invalid procedure %s",
                                  LTE_fdd_enb_rrc_proc_text[rb->get_rrc_procedure()]);
        break;
    }
}

/*************************/
/*    Message Parsers    */
/*************************/
void LTE_fdd_enb_rrc::parse_ul_ccch_message(LIBLTE_BIT_MSG_STRUCT  *msg,
                                            LTE_fdd_enb_user      **user,
                                            LTE_fdd_enb_rb        **rb)
{
    LTE_fdd_enb_user_mgr *user_mgr = LTE_fdd_enb_user_mgr::get_instance();
    LTE_fdd_enb_user     *act_user = NULL;
    LTE_fdd_enb_rb       *tmp_rb;

    // Parse the message
    liblte_rrc_unpack_ul_ccch_msg(msg,
                                  &(*rb)->ul_ccch_msg);

    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                              __FILE__,
                              __LINE__,
                              "Received %s for RNTI=%u, RB=%s",
                              liblte_rrc_ul_ccch_msg_type_text[(*rb)->ul_ccch_msg.msg_type],
                              (*user)->get_c_rnti(),
                              LTE_fdd_enb_rb_text[(*rb)->get_rb_id()]);

    switch((*rb)->ul_ccch_msg.msg_type)
    {
    case LIBLTE_RRC_UL_CCCH_MSG_TYPE_RRC_CON_REQ:
        if(LIBLTE_RRC_CON_REQ_UE_ID_TYPE_S_TMSI == (*rb)->ul_ccch_msg.msg.rrc_con_req.ue_id_type)
        {
            if(LTE_FDD_ENB_ERROR_NONE == user_mgr->find_user(&(*rb)->ul_ccch_msg.msg.rrc_con_req.ue_id.s_tmsi, &act_user))
            {
                if(act_user != (*user))
                {
                    act_user->copy_rbs((*user));
                    (*user)->clear_rbs();
                    user_mgr->transfer_c_rnti(*user, act_user);
                    *user = act_user;
                }
                interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                          LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                                          __FILE__,
                                          __LINE__,
                                          "IMSI=%s is associated with RNTI=%u, RB=%s",
                                          (*user)->get_imsi_str().c_str(),
                                          (*user)->get_c_rnti(),
                                          LTE_fdd_enb_rb_text[(*rb)->get_rb_id()]);
            }
        }
        (*rb)->set_rrc_procedure(LTE_FDD_ENB_RRC_PROC_RRC_CON_REQ);
        break;
    case LIBLTE_RRC_UL_CCCH_MSG_TYPE_RRC_CON_REEST_REQ:
        if(LTE_FDD_ENB_ERROR_NONE == user_mgr->find_user((*rb)->ul_ccch_msg.msg.rrc_con_reest_req.ue_id.c_rnti, &act_user))
        {
            if(act_user != (*user))
            {
                act_user->copy_rbs((*user));
                (*user)->clear_rbs();
                user_mgr->transfer_c_rnti(*user, act_user);
                *user = act_user;
            }
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                      LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                                      __FILE__,
                                      __LINE__,
                                      "IMSI=%s is associated with RNTI=%u, RB=%s",
                                      (*user)->get_imsi_str().c_str(),
                                      (*user)->get_c_rnti(),
                                      LTE_fdd_enb_rb_text[(*rb)->get_rb_id()]);
        }
        (*rb)->set_rrc_procedure(LTE_FDD_ENB_RRC_PROC_RRC_CON_REEST_REQ);
        (*rb)->set_rrc_state(LTE_FDD_ENB_RRC_STATE_IDLE);
        break;
    default:
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                                  __FILE__,
                                  __LINE__,
                                  msg,
                                  "UL-CCCH-Message received with invalid msg_type=%s",
                                  liblte_rrc_ul_ccch_msg_type_text[(*rb)->ul_ccch_msg.msg_type]);
        break;
    }
}
void LTE_fdd_enb_rrc::parse_ul_dcch_message(LIBLTE_BIT_MSG_STRUCT *msg,
                                            LTE_fdd_enb_user      *user,
                                            LTE_fdd_enb_rb        *rb)
{
    LTE_FDD_ENB_MME_NAS_MSG_READY_MSG_STRUCT nas_msg_ready;
    LTE_FDD_ENB_MME_RRC_CMD_RESP_MSG_STRUCT  cmd_resp;

    // Parse the message
    liblte_rrc_unpack_ul_dcch_msg(msg,
                                  &rb->ul_dcch_msg);

    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                              __FILE__,
                              __LINE__,
                              "Received %s for RNTI=%u, RB=%s",
                              liblte_rrc_ul_dcch_msg_type_text[rb->ul_dcch_msg.msg_type],
                              user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[rb->get_rb_id()]);

    switch(rb->ul_dcch_msg.msg_type)
    {
    case LIBLTE_RRC_UL_DCCH_MSG_TYPE_RRC_CON_REEST_COMPLETE:
        rb->set_rrc_state(LTE_FDD_ENB_RRC_STATE_RRC_CONNECTED);
        break;
    case LIBLTE_RRC_UL_DCCH_MSG_TYPE_RRC_CON_SETUP_COMPLETE:
        rb->set_rrc_state(LTE_FDD_ENB_RRC_STATE_RRC_CONNECTED);

        // Queue the NAS message for MME
        rb->queue_mme_nas_msg(&rb->ul_dcch_msg.msg.rrc_con_setup_complete.dedicated_info_nas);

        // Signal MME
        nas_msg_ready.user = user;
        nas_msg_ready.rb   = rb;
        msgq_to_mme->send(LTE_FDD_ENB_MESSAGE_TYPE_MME_NAS_MSG_READY,
                          LTE_FDD_ENB_DEST_LAYER_MME,
                          (LTE_FDD_ENB_MESSAGE_UNION *)&nas_msg_ready,
                          sizeof(LTE_FDD_ENB_MME_NAS_MSG_READY_MSG_STRUCT));
        break;
    case LIBLTE_RRC_UL_DCCH_MSG_TYPE_UL_INFO_TRANSFER:
        if(LIBLTE_RRC_UL_INFORMATION_TRANSFER_TYPE_NAS == rb->ul_dcch_msg.msg.ul_info_transfer.dedicated_info_type)
        {
            // Queue the NAS message for MME
            rb->queue_mme_nas_msg(&rb->ul_dcch_msg.msg.ul_info_transfer.dedicated_info);

            // Signal MME
            nas_msg_ready.user = user;
            nas_msg_ready.rb   = rb;
            msgq_to_mme->send(LTE_FDD_ENB_MESSAGE_TYPE_MME_NAS_MSG_READY,
                              LTE_FDD_ENB_DEST_LAYER_MME,
                              (LTE_FDD_ENB_MESSAGE_UNION *)&nas_msg_ready,
                              sizeof(LTE_FDD_ENB_MME_NAS_MSG_READY_MSG_STRUCT));
        }else{
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                                      __FILE__,
                                      __LINE__,
                                      msg,
                                      "UL-DCCH-Message UL Information Transfer received with invalid dedicated info %s",
                                      liblte_rrc_ul_information_transfer_type_text[rb->ul_dcch_msg.msg.ul_info_transfer.dedicated_info_type]);
        }
        break;
    case LIBLTE_RRC_UL_DCCH_MSG_TYPE_SECURITY_MODE_COMPLETE:
        // Signal MME
        cmd_resp.user     = user;
        cmd_resp.rb       = rb;
        cmd_resp.cmd_resp = LTE_FDD_ENB_MME_RRC_CMD_RESP_SECURITY;
        msgq_to_mme->send(LTE_FDD_ENB_MESSAGE_TYPE_MME_RRC_CMD_RESP,
                          LTE_FDD_ENB_DEST_LAYER_MME,
                          (LTE_FDD_ENB_MESSAGE_UNION *)&cmd_resp,
                          sizeof(LTE_FDD_ENB_MME_RRC_CMD_RESP_MSG_STRUCT));
        break;
    case LIBLTE_RRC_UL_DCCH_MSG_TYPE_RRC_CON_RECONFIG_COMPLETE:
        break;
    case LIBLTE_RRC_UL_DCCH_MSG_TYPE_UE_CAPABILITY_INFO:
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                  LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                                  __FILE__,
                                  __LINE__,
                                  msg,
                                  "UE Capability Information for RNTI=%u",
                                  user->get_c_rnti());
        break;
    default:
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                                  __FILE__,
                                  __LINE__,
                                  msg,
                                  "UL-DCCH-Message received with invalid msg_type=%s",
                                  liblte_rrc_ul_dcch_msg_type_text[rb->ul_dcch_msg.msg_type]);
        break;
    }
}

/*************************/
/*    Message Senders    */
/*************************/
void LTE_fdd_enb_rrc::send_dl_info_transfer(LTE_fdd_enb_user       *user,
                                            LTE_fdd_enb_rb         *rb,
                                            LIBLTE_BYTE_MSG_STRUCT *msg)
{
    LTE_FDD_ENB_PDCP_SDU_READY_MSG_STRUCT pdcp_sdu_ready;
    LIBLTE_BIT_MSG_STRUCT                 pdcp_sdu;

    rb->dl_dcch_msg.msg_type                                 = LIBLTE_RRC_DL_DCCH_MSG_TYPE_DL_INFO_TRANSFER;
    rb->dl_dcch_msg.msg.dl_info_transfer.rrc_transaction_id  = rb->get_rrc_transaction_id();
    rb->dl_dcch_msg.msg.dl_info_transfer.dedicated_info_type = LIBLTE_RRC_DL_INFORMATION_TRANSFER_TYPE_NAS;
    memcpy(&rb->dl_dcch_msg.msg.dl_info_transfer.dedicated_info, msg, sizeof(LIBLTE_BYTE_MSG_STRUCT));
    liblte_rrc_pack_dl_dcch_msg(&rb->dl_dcch_msg, &pdcp_sdu);
    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                              __FILE__,
                              __LINE__,
                              &pdcp_sdu,
                              "Sending DL Info Transfer for RNTI=%u, RB=%s",
                              user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[rb->get_rb_id()]);

    // Queue the PDU for PDCP
    rb->queue_pdcp_sdu(&pdcp_sdu);

    // Signal PDCP
    pdcp_sdu_ready.user = user;
    pdcp_sdu_ready.rb   = rb;
    msgq_to_pdcp->send(LTE_FDD_ENB_MESSAGE_TYPE_PDCP_SDU_READY,
                       LTE_FDD_ENB_DEST_LAYER_PDCP,
                       (LTE_FDD_ENB_MESSAGE_UNION *)&pdcp_sdu_ready,
                       sizeof(LTE_FDD_ENB_PDCP_SDU_READY_MSG_STRUCT));
}
void LTE_fdd_enb_rrc::send_rrc_con_reconfig(LTE_fdd_enb_user       *user,
                                            LTE_fdd_enb_rb         *rb,
                                            LIBLTE_BYTE_MSG_STRUCT *msg)
{
    LTE_fdd_enb_rb                               *drb1    = NULL;
    LTE_fdd_enb_rb                               *drb2    = NULL;
    LTE_fdd_enb_cnfg_db                          *cnfg_db = LTE_fdd_enb_cnfg_db::get_instance();
    LTE_fdd_enb_mac                              *mac     = LTE_fdd_enb_mac::get_instance();
    LTE_FDD_ENB_PDCP_SDU_READY_MSG_STRUCT         pdcp_sdu_ready;
    LIBLTE_RRC_CONNECTION_RECONFIGURATION_STRUCT *rrc_con_recnfg;
    LIBLTE_BIT_MSG_STRUCT                         pdcp_sdu;
    uint32                                        idx;

    user->get_drb(LTE_FDD_ENB_RB_DRB1, &drb1);
    user->get_drb(LTE_FDD_ENB_RB_DRB2, &drb2);

    rb->dl_dcch_msg.msg_type              = LIBLTE_RRC_DL_DCCH_MSG_TYPE_RRC_CON_RECONFIG;
    rrc_con_recnfg                        = (LIBLTE_RRC_CONNECTION_RECONFIGURATION_STRUCT *)&rb->dl_dcch_msg.msg.rrc_con_reconfig;
    rrc_con_recnfg->meas_cnfg_present     = false;
    rrc_con_recnfg->mob_ctrl_info_present = false;
    if(NULL == msg)
    {
        rrc_con_recnfg->N_ded_info_nas = 0;
    }else{
        rrc_con_recnfg->N_ded_info_nas = 1;
        memcpy(&rrc_con_recnfg->ded_info_nas_list[0], msg, sizeof(LIBLTE_BYTE_MSG_STRUCT));
    }
    rrc_con_recnfg->rr_cnfg_ded_present                                         = true;
    rrc_con_recnfg->rr_cnfg_ded.srb_to_add_mod_list_size                        = 1;
    rrc_con_recnfg->rr_cnfg_ded.srb_to_add_mod_list[0].srb_id                   = 2;
    rrc_con_recnfg->rr_cnfg_ded.srb_to_add_mod_list[0].rlc_cnfg_present         = true;
    rrc_con_recnfg->rr_cnfg_ded.srb_to_add_mod_list[0].rlc_default_cnfg_present = true;
    rrc_con_recnfg->rr_cnfg_ded.srb_to_add_mod_list[0].lc_cnfg_present          = true;
    rrc_con_recnfg->rr_cnfg_ded.srb_to_add_mod_list[0].lc_default_cnfg_present  = true;
    rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list_size                        = 0;
    if(NULL != drb1)
    {
        idx                                                                                                    = rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list_size;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].eps_bearer_id_present                             = true;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].eps_bearer_id                                     = drb1->get_eps_bearer_id();
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].drb_id                                            = drb1->get_drb_id();
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].pdcp_cnfg_present                                 = true;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].pdcp_cnfg.discard_timer_present                   = true;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].pdcp_cnfg.discard_timer                           = LIBLTE_RRC_DISCARD_TIMER_INFINITY;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].pdcp_cnfg.rlc_am_status_report_required_present   = true;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].pdcp_cnfg.rlc_am_status_report_required           = true;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].pdcp_cnfg.rlc_um_pdcp_sn_size_present             = false;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].pdcp_cnfg.hdr_compression_rohc                    = false;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].rlc_cnfg_present                                  = true;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].rlc_cnfg.rlc_mode                                 = LIBLTE_RRC_RLC_MODE_AM;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].rlc_cnfg.ul_am_rlc.t_poll_retx                    = LIBLTE_RRC_T_POLL_RETRANSMIT_MS45;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].rlc_cnfg.ul_am_rlc.poll_pdu                       = LIBLTE_RRC_POLL_PDU_INFINITY;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].rlc_cnfg.ul_am_rlc.poll_byte                      = LIBLTE_RRC_POLL_BYTE_INFINITY;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].rlc_cnfg.ul_am_rlc.max_retx_thresh                = LIBLTE_RRC_MAX_RETX_THRESHOLD_T4;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].rlc_cnfg.dl_am_rlc.t_reordering                   = LIBLTE_RRC_T_REORDERING_MS35;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].rlc_cnfg.dl_am_rlc.t_status_prohibit              = LIBLTE_RRC_T_STATUS_PROHIBIT_MS0;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].lc_id_present                                     = true;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].lc_id                                             = drb1->get_lc_id();
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].lc_cnfg_present                                   = true;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].lc_cnfg.ul_specific_params_present                = true;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].lc_cnfg.ul_specific_params.priority               = 13;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].lc_cnfg.ul_specific_params.prioritized_bit_rate   = LIBLTE_RRC_PRIORITIZED_BIT_RATE_INFINITY;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].lc_cnfg.ul_specific_params.bucket_size_duration   = LIBLTE_RRC_BUCKET_SIZE_DURATION_MS100;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].lc_cnfg.ul_specific_params.log_chan_group_present = true;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].lc_cnfg.ul_specific_params.log_chan_group         = drb1->get_log_chan_group();
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].lc_cnfg.log_chan_sr_mask_present                  = false;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list_size++;
    }
    if(NULL != drb2)
    {
        idx                                                                                                    = rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list_size;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].eps_bearer_id_present                             = true;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].eps_bearer_id                                     = drb2->get_eps_bearer_id();
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].drb_id                                            = drb2->get_drb_id();
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].pdcp_cnfg_present                                 = true;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].pdcp_cnfg.discard_timer_present                   = true;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].pdcp_cnfg.discard_timer                           = LIBLTE_RRC_DISCARD_TIMER_INFINITY;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].pdcp_cnfg.rlc_am_status_report_required_present   = true;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].pdcp_cnfg.rlc_am_status_report_required           = true;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].pdcp_cnfg.rlc_um_pdcp_sn_size_present             = false;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].pdcp_cnfg.hdr_compression_rohc                    = false;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].rlc_cnfg_present                                  = true;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].rlc_cnfg.rlc_mode                                 = LIBLTE_RRC_RLC_MODE_AM;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].rlc_cnfg.ul_am_rlc.t_poll_retx                    = LIBLTE_RRC_T_POLL_RETRANSMIT_MS45;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].rlc_cnfg.ul_am_rlc.poll_pdu                       = LIBLTE_RRC_POLL_PDU_INFINITY;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].rlc_cnfg.ul_am_rlc.poll_byte                      = LIBLTE_RRC_POLL_BYTE_INFINITY;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].rlc_cnfg.ul_am_rlc.max_retx_thresh                = LIBLTE_RRC_MAX_RETX_THRESHOLD_T4;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].rlc_cnfg.dl_am_rlc.t_reordering                   = LIBLTE_RRC_T_REORDERING_MS35;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].rlc_cnfg.dl_am_rlc.t_status_prohibit              = LIBLTE_RRC_T_STATUS_PROHIBIT_MS0;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].lc_id_present                                     = true;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].lc_id                                             = drb2->get_lc_id();
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].lc_cnfg_present                                   = true;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].lc_cnfg.ul_specific_params_present                = true;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].lc_cnfg.ul_specific_params.priority               = 13;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].lc_cnfg.ul_specific_params.prioritized_bit_rate   = LIBLTE_RRC_PRIORITIZED_BIT_RATE_INFINITY;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].lc_cnfg.ul_specific_params.bucket_size_duration   = LIBLTE_RRC_BUCKET_SIZE_DURATION_MS100;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].lc_cnfg.ul_specific_params.log_chan_group_present = true;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].lc_cnfg.ul_specific_params.log_chan_group         = drb2->get_log_chan_group();
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list[idx].lc_cnfg.log_chan_sr_mask_present                  = false;
        rrc_con_recnfg->rr_cnfg_ded.drb_to_add_mod_list_size++;
    }
    rrc_con_recnfg->rr_cnfg_ded.drb_to_release_list_size = 0;
    rrc_con_recnfg->rr_cnfg_ded.mac_main_cnfg_present    = false;
    rrc_con_recnfg->rr_cnfg_ded.sps_cnfg_present         = false;
    rrc_con_recnfg->rr_cnfg_ded.phy_cnfg_ded_present     = true;
    cnfg_db->populate_rrc_phy_config_dedicated(&rrc_con_recnfg->rr_cnfg_ded.phy_cnfg_ded, 0, 0, i_sr, N_1_P_PUCCH_SR);
    mac->add_periodic_sr_pucch(user->get_c_rnti(), i_sr, N_1_P_PUCCH_SR);
    increment_i_sr();
    rrc_con_recnfg->rr_cnfg_ded.rlf_timers_and_constants_present = false;
    rrc_con_recnfg->sec_cnfg_ho_present                          = false;
    liblte_rrc_pack_dl_dcch_msg(&rb->dl_dcch_msg, &pdcp_sdu);
    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                              __FILE__,
                              __LINE__,
                              &pdcp_sdu,
                              "Sending RRC Connection Reconfiguration for RNTI=%u, RB=%s",
                              user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[rb->get_rb_id()]);

    // Queue the PDU for PDCP
    rb->queue_pdcp_sdu(&pdcp_sdu);

    // Signal PDCP
    pdcp_sdu_ready.user = user;
    pdcp_sdu_ready.rb   = rb;
    msgq_to_pdcp->send(LTE_FDD_ENB_MESSAGE_TYPE_PDCP_SDU_READY,
                       LTE_FDD_ENB_DEST_LAYER_PDCP,
                       (LTE_FDD_ENB_MESSAGE_UNION *)&pdcp_sdu_ready,
                       sizeof(LTE_FDD_ENB_PDCP_SDU_READY_MSG_STRUCT));
}
void LTE_fdd_enb_rrc::send_rrc_con_reest(LTE_fdd_enb_user *user,
                                         LTE_fdd_enb_rb   *rb)
{
    LTE_fdd_enb_cnfg_db                          *cnfg_db = LTE_fdd_enb_cnfg_db::get_instance();
    LTE_fdd_enb_mac                              *mac     = LTE_fdd_enb_mac::get_instance();
    LTE_FDD_ENB_PDCP_SDU_READY_MSG_STRUCT         pdcp_sdu_ready;
    LIBLTE_RRC_CONNECTION_REESTABLISHMENT_STRUCT *rrc_con_reest;
    LIBLTE_BIT_MSG_STRUCT                         pdcp_sdu;

    rb->dl_ccch_msg.msg_type                                                                  = LIBLTE_RRC_DL_CCCH_MSG_TYPE_RRC_CON_REEST;
    rrc_con_reest                                                                             = (LIBLTE_RRC_CONNECTION_REESTABLISHMENT_STRUCT *)&rb->dl_ccch_msg.msg.rrc_con_reest;
    rrc_con_reest->rrc_transaction_id                                                         = rb->get_rrc_transaction_id();
    rrc_con_reest->rr_cnfg.srb_to_add_mod_list_size                                           = 1;
    rrc_con_reest->rr_cnfg.srb_to_add_mod_list[0].srb_id                                      = 1;
    rrc_con_reest->rr_cnfg.srb_to_add_mod_list[0].rlc_cnfg_present                            = true;
    rrc_con_reest->rr_cnfg.srb_to_add_mod_list[0].rlc_default_cnfg_present                    = true;
    rrc_con_reest->rr_cnfg.srb_to_add_mod_list[0].lc_cnfg_present                             = true;
    rrc_con_reest->rr_cnfg.srb_to_add_mod_list[0].lc_default_cnfg_present                     = true;
    rrc_con_reest->rr_cnfg.drb_to_add_mod_list_size                                           = 0;
    rrc_con_reest->rr_cnfg.drb_to_release_list_size                                           = 0;
    rrc_con_reest->rr_cnfg.mac_main_cnfg_present                                              = true;
    rrc_con_reest->rr_cnfg.mac_main_cnfg.default_value                                        = false;
    rrc_con_reest->rr_cnfg.mac_main_cnfg.explicit_value.ulsch_cnfg_present                    = true;
    rrc_con_reest->rr_cnfg.mac_main_cnfg.explicit_value.ulsch_cnfg.max_harq_tx_present        = true;
    rrc_con_reest->rr_cnfg.mac_main_cnfg.explicit_value.ulsch_cnfg.max_harq_tx                = LIBLTE_RRC_MAX_HARQ_TX_N1;
    rrc_con_reest->rr_cnfg.mac_main_cnfg.explicit_value.ulsch_cnfg.periodic_bsr_timer_present = false;
    rrc_con_reest->rr_cnfg.mac_main_cnfg.explicit_value.ulsch_cnfg.retx_bsr_timer             = LIBLTE_RRC_RETRANSMISSION_BSR_TIMER_SF1280;
    rrc_con_reest->rr_cnfg.mac_main_cnfg.explicit_value.ulsch_cnfg.tti_bundling               = false;
    rrc_con_reest->rr_cnfg.mac_main_cnfg.explicit_value.drx_cnfg_present                      = false;
    rrc_con_reest->rr_cnfg.mac_main_cnfg.explicit_value.phr_cnfg_present                      = false;
    rrc_con_reest->rr_cnfg.mac_main_cnfg.explicit_value.time_alignment_timer                  = LIBLTE_RRC_TIME_ALIGNMENT_TIMER_SF10240;
    rrc_con_reest->rr_cnfg.sps_cnfg_present                                                   = false;
    rrc_con_reest->rr_cnfg.phy_cnfg_ded_present                                               = true;
    cnfg_db->populate_rrc_phy_config_dedicated(&rrc_con_reest->rr_cnfg.phy_cnfg_ded, 0, 0, i_sr, N_1_P_PUCCH_SR);
    mac->add_periodic_sr_pucch(user->get_c_rnti(), i_sr, N_1_P_PUCCH_SR);
    increment_i_sr();
    rrc_con_reest->rr_cnfg.rlf_timers_and_constants_present = false;
    liblte_rrc_pack_dl_ccch_msg(&rb->dl_ccch_msg, &pdcp_sdu);
    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                              __FILE__,
                              __LINE__,
                              "Sending RRC Connection Reestablishment for RNTI=%u, RB=%s",
                              user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[rb->get_rb_id()]);

    // Queue the PDU for PDCP
    rb->queue_pdcp_sdu(&pdcp_sdu);

    // Signal PDCP
    pdcp_sdu_ready.user = user;
    pdcp_sdu_ready.rb   = rb;
    msgq_to_pdcp->send(LTE_FDD_ENB_MESSAGE_TYPE_PDCP_SDU_READY,
                       LTE_FDD_ENB_DEST_LAYER_PDCP,
                       (LTE_FDD_ENB_MESSAGE_UNION *)&pdcp_sdu_ready,
                       sizeof(LTE_FDD_ENB_PDCP_SDU_READY_MSG_STRUCT));
}
void LTE_fdd_enb_rrc::send_rrc_con_reest_reject(LTE_fdd_enb_user *user,
                                                LTE_fdd_enb_rb   *rb)
{
    LTE_FDD_ENB_PDCP_SDU_READY_MSG_STRUCT pdcp_sdu_ready;
    LIBLTE_BIT_MSG_STRUCT                 pdcp_sdu;

    rb->dl_ccch_msg.msg_type = LIBLTE_RRC_DL_CCCH_MSG_TYPE_RRC_CON_REEST_REJ;
    liblte_rrc_pack_dl_ccch_msg(&rb->dl_ccch_msg, &pdcp_sdu);
    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                              __FILE__,
                              __LINE__,
                              &pdcp_sdu,
                              "Sending RRC Connection Reestablishment Reject for RNTI=%u, RB=%s",
                              user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[rb->get_rb_id()]);

    // Queue the PDU for PDCP
    rb->queue_pdcp_sdu(&pdcp_sdu);

    // Signal PDCP
    pdcp_sdu_ready.user = user;
    pdcp_sdu_ready.rb   = rb;
    msgq_to_pdcp->send(LTE_FDD_ENB_MESSAGE_TYPE_PDCP_SDU_READY,
                       LTE_FDD_ENB_DEST_LAYER_PDCP,
                       (LTE_FDD_ENB_MESSAGE_UNION *)&pdcp_sdu_ready,
                       sizeof(LTE_FDD_ENB_PDCP_SDU_READY_MSG_STRUCT));
}
void LTE_fdd_enb_rrc::send_rrc_con_release(LTE_fdd_enb_user *user,
                                           LTE_fdd_enb_rb   *rb)
{
    LTE_fdd_enb_mac                       *mac = LTE_fdd_enb_mac::get_instance();
    LTE_FDD_ENB_PDCP_SDU_READY_MSG_STRUCT  pdcp_sdu_ready;
    LIBLTE_BIT_MSG_STRUCT                  pdcp_sdu;

    rb->dl_dcch_msg.msg_type                               = LIBLTE_RRC_DL_DCCH_MSG_TYPE_RRC_CON_RELEASE;
    rb->dl_dcch_msg.msg.rrc_con_release.rrc_transaction_id = rb->get_rrc_transaction_id();
    rb->dl_dcch_msg.msg.rrc_con_release.release_cause      = LIBLTE_RRC_RELEASE_CAUSE_OTHER;
    liblte_rrc_pack_dl_dcch_msg(&rb->dl_dcch_msg, &pdcp_sdu);
    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                              __FILE__,
                              __LINE__,
                              &pdcp_sdu,
                              "Sending RRC Connection Release for RNTI=%u, RB=%s",
                              user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[rb->get_rb_id()]);

    mac->remove_periodic_sr_pucch(user->get_c_rnti());

    // Queue the PDU for PDCP
    rb->queue_pdcp_sdu(&pdcp_sdu);

    // Signal PDCP
    pdcp_sdu_ready.user = user;
    pdcp_sdu_ready.rb   = rb;
    msgq_to_pdcp->send(LTE_FDD_ENB_MESSAGE_TYPE_PDCP_SDU_READY,
                       LTE_FDD_ENB_DEST_LAYER_PDCP,
                       (LTE_FDD_ENB_MESSAGE_UNION *)&pdcp_sdu_ready,
                       sizeof(LTE_FDD_ENB_PDCP_SDU_READY_MSG_STRUCT));
}
void LTE_fdd_enb_rrc::send_rrc_con_setup(LTE_fdd_enb_user *user,
                                         LTE_fdd_enb_rb   *rb)
{
    LTE_fdd_enb_cnfg_db                   *cnfg_db = LTE_fdd_enb_cnfg_db::get_instance();
    LTE_fdd_enb_mac                       *mac     = LTE_fdd_enb_mac::get_instance();
    LTE_FDD_ENB_PDCP_SDU_READY_MSG_STRUCT  pdcp_sdu_ready;
    LIBLTE_RRC_CONNECTION_SETUP_STRUCT    *rrc_con_setup;
    LIBLTE_BIT_MSG_STRUCT                  pdcp_sdu;

    rb->dl_ccch_msg.msg_type                                                                  = LIBLTE_RRC_DL_CCCH_MSG_TYPE_RRC_CON_SETUP;
    rrc_con_setup                                                                             = (LIBLTE_RRC_CONNECTION_SETUP_STRUCT *)&rb->dl_ccch_msg.msg.rrc_con_setup;
    rrc_con_setup->rrc_transaction_id                                                         = rb->get_rrc_transaction_id();
    rrc_con_setup->rr_cnfg.srb_to_add_mod_list_size                                           = 1;
    rrc_con_setup->rr_cnfg.srb_to_add_mod_list[0].srb_id                                      = 1;
    rrc_con_setup->rr_cnfg.srb_to_add_mod_list[0].rlc_cnfg_present                            = true;
    rrc_con_setup->rr_cnfg.srb_to_add_mod_list[0].rlc_default_cnfg_present                    = true;
    rrc_con_setup->rr_cnfg.srb_to_add_mod_list[0].lc_cnfg_present                             = true;
    rrc_con_setup->rr_cnfg.srb_to_add_mod_list[0].lc_default_cnfg_present                     = true;
    rrc_con_setup->rr_cnfg.drb_to_add_mod_list_size                                           = 0;
    rrc_con_setup->rr_cnfg.drb_to_release_list_size                                           = 0;
    rrc_con_setup->rr_cnfg.mac_main_cnfg_present                                              = true;
    rrc_con_setup->rr_cnfg.mac_main_cnfg.default_value                                        = false;
    rrc_con_setup->rr_cnfg.mac_main_cnfg.explicit_value.ulsch_cnfg_present                    = true;
    rrc_con_setup->rr_cnfg.mac_main_cnfg.explicit_value.ulsch_cnfg.max_harq_tx_present        = true;
    rrc_con_setup->rr_cnfg.mac_main_cnfg.explicit_value.ulsch_cnfg.max_harq_tx                = LIBLTE_RRC_MAX_HARQ_TX_N1;
    rrc_con_setup->rr_cnfg.mac_main_cnfg.explicit_value.ulsch_cnfg.periodic_bsr_timer_present = false;
    rrc_con_setup->rr_cnfg.mac_main_cnfg.explicit_value.ulsch_cnfg.retx_bsr_timer             = LIBLTE_RRC_RETRANSMISSION_BSR_TIMER_SF1280;
    rrc_con_setup->rr_cnfg.mac_main_cnfg.explicit_value.ulsch_cnfg.tti_bundling               = false;
    rrc_con_setup->rr_cnfg.mac_main_cnfg.explicit_value.drx_cnfg_present                      = false;
    rrc_con_setup->rr_cnfg.mac_main_cnfg.explicit_value.phr_cnfg_present                      = false;
    rrc_con_setup->rr_cnfg.mac_main_cnfg.explicit_value.time_alignment_timer                  = LIBLTE_RRC_TIME_ALIGNMENT_TIMER_SF10240;
    rrc_con_setup->rr_cnfg.sps_cnfg_present                                                   = false;
    rrc_con_setup->rr_cnfg.phy_cnfg_ded_present                                               = true;
    cnfg_db->populate_rrc_phy_config_dedicated(&rrc_con_setup->rr_cnfg.phy_cnfg_ded, 0, 0, i_sr, N_1_P_PUCCH_SR);
    mac->add_periodic_sr_pucch(user->get_c_rnti(), i_sr, N_1_P_PUCCH_SR);
    increment_i_sr();
    rrc_con_setup->rr_cnfg.rlf_timers_and_constants_present = false;
    liblte_rrc_pack_dl_ccch_msg(&rb->dl_ccch_msg, &pdcp_sdu);
    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                              __FILE__,
                              __LINE__,
                              &pdcp_sdu,
                              "Sending RRC Connection Setup for RNTI=%u, RB=%s",
                              user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[rb->get_rb_id()]);

    // Queue the PDU for PDCP
    rb->queue_pdcp_sdu(&pdcp_sdu);

    // Signal PDCP
    pdcp_sdu_ready.user = user;
    pdcp_sdu_ready.rb   = rb;
    msgq_to_pdcp->send(LTE_FDD_ENB_MESSAGE_TYPE_PDCP_SDU_READY,
                       LTE_FDD_ENB_DEST_LAYER_PDCP,
                       (LTE_FDD_ENB_MESSAGE_UNION *)&pdcp_sdu_ready,
                       sizeof(LTE_FDD_ENB_PDCP_SDU_READY_MSG_STRUCT));
}
void LTE_fdd_enb_rrc::send_security_mode_command(LTE_fdd_enb_user *user,
                                                 LTE_fdd_enb_rb   *rb)
{
    LTE_FDD_ENB_PDCP_SDU_READY_MSG_STRUCT pdcp_sdu_ready;
    LIBLTE_BIT_MSG_STRUCT                 pdcp_sdu;

    rb->dl_dcch_msg.msg_type                                  = LIBLTE_RRC_DL_DCCH_MSG_TYPE_SECURITY_MODE_COMMAND;
    rb->dl_dcch_msg.msg.security_mode_cmd.rrc_transaction_id  = rb->get_rrc_transaction_id();
    rb->dl_dcch_msg.msg.security_mode_cmd.sec_algs.cipher_alg = LIBLTE_RRC_CIPHERING_ALGORITHM_EEA0;
    rb->dl_dcch_msg.msg.security_mode_cmd.sec_algs.int_alg    = LIBLTE_RRC_INTEGRITY_PROT_ALGORITHM_EIA2;
    liblte_rrc_pack_dl_dcch_msg(&rb->dl_dcch_msg, &pdcp_sdu);
    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                              __FILE__,
                              __LINE__,
                              &pdcp_sdu,
                              "Sending Security Mode Command for RNTI=%u, RB=%s",
                              user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[rb->get_rb_id()]);

    // Configure PDCP for security
    rb->set_pdcp_config(LTE_FDD_ENB_PDCP_CONFIG_SECURITY);

    // Queue the PDU for PDCP
    rb->queue_pdcp_sdu(&pdcp_sdu);

    // Signal PDCP
    pdcp_sdu_ready.user = user;
    pdcp_sdu_ready.rb   = rb;
    msgq_to_pdcp->send(LTE_FDD_ENB_MESSAGE_TYPE_PDCP_SDU_READY,
                       LTE_FDD_ENB_DEST_LAYER_PDCP,
                       (LTE_FDD_ENB_MESSAGE_UNION *)&pdcp_sdu_ready,
                       sizeof(LTE_FDD_ENB_PDCP_SDU_READY_MSG_STRUCT));
}
void LTE_fdd_enb_rrc::send_ue_capability_enquiry(LTE_fdd_enb_user *user,
                                                 LTE_fdd_enb_rb   *rb)
{
    LTE_FDD_ENB_PDCP_SDU_READY_MSG_STRUCT    pdcp_sdu_ready;
    LIBLTE_BIT_MSG_STRUCT                    pdcp_sdu;
    LIBLTE_RRC_UE_CAPABILITY_ENQUIRY_STRUCT *ue_cap_enquiry = &rb->dl_dcch_msg.msg.ue_cap_enquiry;

    rb->dl_dcch_msg.msg_type                                            = LIBLTE_RRC_DL_DCCH_MSG_TYPE_UE_CAPABILITY_ENQUIRY;
    ue_cap_enquiry->rrc_transaction_id                                  = (rb->get_rrc_transaction_id() + 1) % 4;
    ue_cap_enquiry->rat_type_list_size                                  = 0;
    ue_cap_enquiry->rat_type_list[ue_cap_enquiry->rat_type_list_size++] = LIBLTE_RRC_RAT_TYPE_EUTRA;
    ue_cap_enquiry->rat_type_list[ue_cap_enquiry->rat_type_list_size++] = LIBLTE_RRC_RAT_TYPE_UTRA;
    ue_cap_enquiry->rat_type_list[ue_cap_enquiry->rat_type_list_size++] = LIBLTE_RRC_RAT_TYPE_GERAN_CS;
    ue_cap_enquiry->rat_type_list[ue_cap_enquiry->rat_type_list_size++] = LIBLTE_RRC_RAT_TYPE_GERAN_PS;
    ue_cap_enquiry->rat_type_list[ue_cap_enquiry->rat_type_list_size++] = LIBLTE_RRC_RAT_TYPE_CDMA2000_1XRTT;
    liblte_rrc_pack_dl_dcch_msg(&rb->dl_dcch_msg, &pdcp_sdu);
    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_RRC,
                              __FILE__,
                              __LINE__,
                              &pdcp_sdu,
                              "Sending UE Capability Enquiry for RNTI=%u, RB=%s",
                              user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[rb->get_rb_id()]);

    // Queue the PDU for PDCP
    rb->queue_pdcp_sdu(&pdcp_sdu);

    // Signal PDCP
    pdcp_sdu_ready.user = user;
    pdcp_sdu_ready.rb   = rb;
    msgq_to_pdcp->send(LTE_FDD_ENB_MESSAGE_TYPE_PDCP_SDU_READY,
                       LTE_FDD_ENB_DEST_LAYER_PDCP,
                       (LTE_FDD_ENB_MESSAGE_UNION *)&pdcp_sdu_ready,
                       sizeof(LTE_FDD_ENB_PDCP_SDU_READY_MSG_STRUCT));
}

/*****************/
/*    Helpers    */
/*****************/
void LTE_fdd_enb_rrc::increment_i_sr(void)
{
    i_sr++;
    if(i_sr > I_SR_MAX)
    {
        i_sr = I_SR_MIN;
    }
}
