#line 2 "LTE_fdd_enb_mme.cc" // Make __FILE__ omit the path
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

    File: LTE_fdd_enb_mme.cc

    Description: Contains all the implementations for the LTE FDD eNodeB
                 mobility management entity layer.

    Revision History
    ----------    -------------    --------------------------------------------
    11/10/2013    Ben Wojtowicz    Created file
    01/18/2014    Ben Wojtowicz    Added level to debug prints.
    06/15/2014    Ben Wojtowicz    Added RRC NAS message handler.
    08/03/2014    Ben Wojtowicz    Added message parsers, state machines, and
                                   message senders.
    09/03/2014    Ben Wojtowicz    Added authentication and security support.
    11/01/2014    Ben Wojtowicz    Added attach accept/complete, ESM info
                                   transfer, and default bearer setup support.
    11/29/2014    Ben Wojtowicz    Added service request, service reject, and
                                   activate dedicated EPS bearer context
                                   request support.
    12/16/2014    Ben Wojtowicz    Added ol extension to message queue and
                                   sending of EMM information message.
    12/24/2014    Ben Wojtowicz    Actually sending EMM information message.
    02/15/2015    Ben Wojtowicz    Moved to new message queue, added more debug
                                   log points, and using the fixed user switch.
    03/11/2015    Ben Wojtowicz    Added detach handling.
    07/25/2015    Ben Wojtowicz    Using the latest liblte and changed the
                                   dedicated bearer QoS to 9.
    12/06/2015    Ben Wojtowicz    Changed boost::mutex to pthread_mutex_t and
                                   sem_t and changed the user deletion and
                                   C-RNTI release procedures.
    02/13/2016    Ben Wojtowicz    Properly initialize present flags and change
                                   the packet filter evaluation precedence in
                                   activate dedicated EPS bearer context (thanks
                                   to Pedro Batista for reporting this).
    07/03/2016    Ben Wojtowicz    Fixed a bug when receiving a service request
                                   message for a non-existent user.  Thanks to
                                   Peter Nguyen for finding this.
    07/29/2017    Ben Wojtowicz    Moved away from singleton pattern.

*******************************************************************************/

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_fdd_enb_mme.h"
#include "LTE_fdd_enb_hss.h"
#include "LTE_fdd_enb_user_mgr.h"
#include "liblte_mme.h"
#include "liblte_security.h"
#include "libtools_scoped_lock.h"
#include <netinet/in.h>

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
                              CLASS IMPLEMENTATIONS
*******************************************************************************/

/********************************/
/*    Constructor/Destructor    */
/********************************/
LTE_fdd_enb_mme::LTE_fdd_enb_mme()
{
    sem_init(&start_sem, 0, 1);
    sem_init(&sys_info_sem, 0, 1);
    started = false;
}
LTE_fdd_enb_mme::~LTE_fdd_enb_mme()
{
    stop();
    sem_destroy(&sys_info_sem);
    sem_destroy(&start_sem);
}

/********************/
/*    Start/Stop    */
/********************/
void LTE_fdd_enb_mme::start(LTE_fdd_enb_msgq      *from_rrc,
                            LTE_fdd_enb_msgq      *to_rrc,
                            LTE_fdd_enb_interface *iface)
{
    libtools_scoped_lock  lock(start_sem);
    LTE_fdd_enb_cnfg_db  *cnfg_db = LTE_fdd_enb_cnfg_db::get_instance();
    LTE_fdd_enb_msgq_cb   rrc_cb(&LTE_fdd_enb_msgq_cb_wrapper<LTE_fdd_enb_mme, &LTE_fdd_enb_mme::handle_rrc_msg>, this);

    if(!started)
    {
        interface     = iface;
        started       = true;
        msgq_from_rrc = from_rrc;
        msgq_to_rrc   = to_rrc;
        msgq_from_rrc->attach_rx(rrc_cb);

        cnfg_db->get_param(LTE_FDD_ENB_PARAM_IP_ADDR_START, next_ip_addr);
        cnfg_db->get_param(LTE_FDD_ENB_PARAM_DNS_ADDR, dns_addr);
        next_ip_addr++;
    }
}
void LTE_fdd_enb_mme::stop(void)
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
void LTE_fdd_enb_mme::handle_rrc_msg(LTE_FDD_ENB_MESSAGE_STRUCT &msg)
{
    switch(msg.type)
    {
    case LTE_FDD_ENB_MESSAGE_TYPE_MME_NAS_MSG_READY:
        handle_nas_msg(&msg.msg.mme_nas_msg_ready);
        break;
    case LTE_FDD_ENB_MESSAGE_TYPE_MME_RRC_CMD_RESP:
        handle_rrc_cmd_resp(&msg.msg.mme_rrc_cmd_resp);
        break;
    default:
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                  __FILE__,
                                  __LINE__,
                                  "Received invalid RRC message %s",
                                  LTE_fdd_enb_message_type_text[msg.type]);
        break;
    }
}

/****************************/
/*    External Interface    */
/****************************/
void LTE_fdd_enb_mme::update_sys_info(void)
{
    libtools_scoped_lock  lock(sys_info_sem);
    LTE_fdd_enb_cnfg_db  *cnfg_db = LTE_fdd_enb_cnfg_db::get_instance();

    cnfg_db->get_sys_info(sys_info);
}

/******************************/
/*    RRC Message Handlers    */
/******************************/
void LTE_fdd_enb_mme::handle_nas_msg(LTE_FDD_ENB_MME_NAS_MSG_READY_MSG_STRUCT *nas_msg)
{
    LIBLTE_BYTE_MSG_STRUCT *msg;
    uint8                   pd;
    uint8                   msg_type;

    if(LTE_FDD_ENB_ERROR_NONE == nas_msg->rb->get_next_mme_nas_msg(&msg))
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                  __FILE__,
                                  __LINE__,
                                  msg,
                                  "Received NAS message for RNTI=%u and RB=%s",
                                  nas_msg->user->get_c_rnti(),
                                  LTE_fdd_enb_rb_text[nas_msg->rb->get_rb_id()]);

        // Parse the message
        liblte_mme_parse_msg_header(msg, &pd, &msg_type);
        switch(msg_type)
        {
        case LIBLTE_MME_MSG_TYPE_ATTACH_COMPLETE:
            parse_attach_complete(msg, nas_msg->user, nas_msg->rb);
            break;
        case LIBLTE_MME_MSG_TYPE_ATTACH_REQUEST:
            parse_attach_request(msg, &nas_msg->user, &nas_msg->rb);
            break;
        case LIBLTE_MME_MSG_TYPE_AUTHENTICATION_FAILURE:
            parse_authentication_failure(msg, nas_msg->user, nas_msg->rb);
            break;
        case LIBLTE_MME_MSG_TYPE_AUTHENTICATION_RESPONSE:
            parse_authentication_response(msg, nas_msg->user, nas_msg->rb);
            break;
        case LIBLTE_MME_MSG_TYPE_DETACH_REQUEST:
            parse_detach_request(msg, nas_msg->user, nas_msg->rb);
            break;
        case LIBLTE_MME_MSG_TYPE_EMM_STATUS:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                      __FILE__,
                                      __LINE__,
                                      "Not handling EMM Status");
            break;
        case LIBLTE_MME_MSG_TYPE_EXTENDED_SERVICE_REQUEST:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                      __FILE__,
                                      __LINE__,
                                      "Not handling Extended Service Request");
            break;
        case LIBLTE_MME_MSG_TYPE_GUTI_REALLOCATION_COMPLETE:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                      __FILE__,
                                      __LINE__,
                                      "Not handling GUTI Reallocation Complete");
            break;
        case LIBLTE_MME_MSG_TYPE_IDENTITY_RESPONSE:
            parse_identity_response(msg, nas_msg->user, nas_msg->rb);
            break;
        case LIBLTE_MME_MSG_TYPE_SECURITY_MODE_COMPLETE:
            parse_security_mode_complete(msg, nas_msg->user, nas_msg->rb);
            break;
        case LIBLTE_MME_MSG_TYPE_SECURITY_MODE_REJECT:
            parse_security_mode_reject(msg, nas_msg->user, nas_msg->rb);
            break;
        case LIBLTE_MME_SECURITY_HDR_TYPE_SERVICE_REQUEST:
            parse_service_request(msg, nas_msg->user, nas_msg->rb);
            break;
        case LIBLTE_MME_MSG_TYPE_TRACKING_AREA_UPDATE_COMPLETE:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                      __FILE__,
                                      __LINE__,
                                      "Not handling Tracking Area Update Complete");
            break;
        case LIBLTE_MME_MSG_TYPE_TRACKING_AREA_UPDATE_REQUEST:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                      __FILE__,
                                      __LINE__,
                                      "Not handling Tracking Area Update Request");
            break;
        case LIBLTE_MME_MSG_TYPE_UPLINK_NAS_TRANSPORT:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                      __FILE__,
                                      __LINE__,
                                      "Not handling Uplink NAS Transport");
            break;
        case LIBLTE_MME_MSG_TYPE_UPLINK_GENERIC_NAS_TRANSPORT:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                      __FILE__,
                                      __LINE__,
                                      "Not handling Uplink Generic NAS Transport");
            break;
        case LIBLTE_MME_MSG_TYPE_ACTIVATE_DEDICATED_EPS_BEARER_CONTEXT_ACCEPT:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                      __FILE__,
                                      __LINE__,
                                      "Not handling Activate Dedicated EPS Bearer Context Accept");
            break;
        case LIBLTE_MME_MSG_TYPE_ACTIVATE_DEDICATED_EPS_BEARER_CONTEXT_REJECT:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                      __FILE__,
                                      __LINE__,
                                      "Not handling Activate Dedicated EPS Bearer Context Reject");
            break;
        case LIBLTE_MME_MSG_TYPE_ACTIVATE_DEFAULT_EPS_BEARER_CONTEXT_ACCEPT:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                      __FILE__,
                                      __LINE__,
                                      "Not handling Activate Default EPS Bearer Context Accept");
            break;
        case LIBLTE_MME_MSG_TYPE_ACTIVATE_DEFAULT_EPS_BEARER_CONTEXT_REJECT:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                      __FILE__,
                                      __LINE__,
                                      "Not handling Activate Default EPS Bearer Context Reject");
            break;
        case LIBLTE_MME_MSG_TYPE_BEARER_RESOURCE_ALLOCATION_REQUEST:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                      __FILE__,
                                      __LINE__,
                                      "Not handling Bearer Resource Allocation Request");
            break;
        case LIBLTE_MME_MSG_TYPE_BEARER_RESOURCE_MODIFICATION_REQUEST:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                      __FILE__,
                                      __LINE__,
                                      "Not handling Bearer Resource Modification Request");
            break;
        case LIBLTE_MME_MSG_TYPE_DEACTIVATE_EPS_BEARER_CONTEXT_ACCEPT:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                      __FILE__,
                                      __LINE__,
                                      "Not handling Deactivate EPS Bearer Context Accept");
            break;
        case LIBLTE_MME_MSG_TYPE_ESM_INFORMATION_RESPONSE:
            parse_esm_information_response(msg, nas_msg->user, nas_msg->rb);
            break;
        case LIBLTE_MME_MSG_TYPE_MODIFY_EPS_BEARER_CONTEXT_ACCEPT:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                      __FILE__,
                                      __LINE__,
                                      "Not handling Modify EPS Bearer Context Accept");
            break;
        case LIBLTE_MME_MSG_TYPE_MODIFY_EPS_BEARER_CONTEXT_REJECT:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                      __FILE__,
                                      __LINE__,
                                      "Not handling Modify EPS Bearer Context Reject");
            break;
        case LIBLTE_MME_MSG_TYPE_PDN_CONNECTIVITY_REQUEST:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                      __FILE__,
                                      __LINE__,
                                      "Not handling PDN Connectivity Request");
            break;
        case LIBLTE_MME_MSG_TYPE_PDN_DISCONNECT_REQUEST:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                      __FILE__,
                                      __LINE__,
                                      "Not handling PDN Disconnect Request");
            break;
        default:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                      __FILE__,
                                      __LINE__,
                                      "Not handling NAS message with MSG_TYPE=%02X",
                                      msg_type);
            break;
        }

        // Increment the uplink NAS count
        nas_msg->user->increment_nas_count_ul();

        // Delete the NAS message
        nas_msg->rb->delete_next_mme_nas_msg();

        // Call the appropriate state machine
        switch(nas_msg->rb->get_mme_procedure())
        {
        case LTE_FDD_ENB_MME_PROC_ATTACH:
            attach_sm(nas_msg->user, nas_msg->rb);
            break;
        case LTE_FDD_ENB_MME_PROC_SERVICE_REQUEST:
            service_req_sm(nas_msg->user, nas_msg->rb);
            break;
        case LTE_FDD_ENB_MME_PROC_DETACH:
            detach_sm(nas_msg->user, nas_msg->rb);
            break;
        default:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                      __FILE__,
                                      __LINE__,
                                      "MME in invalid procedure %s",
                                      LTE_fdd_enb_mme_proc_text[nas_msg->rb->get_mme_procedure()]);
            break;
        }
    }else{
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                  __FILE__,
                                  __LINE__,
                                  "Received NAS message with no message queued");
    }
}
void LTE_fdd_enb_mme::handle_rrc_cmd_resp(LTE_FDD_ENB_MME_RRC_CMD_RESP_MSG_STRUCT *rrc_cmd_resp)
{
    switch(rrc_cmd_resp->cmd_resp)
    {
    case LTE_FDD_ENB_MME_RRC_CMD_RESP_SECURITY:
        switch(rrc_cmd_resp->rb->get_mme_procedure())
        {
        case LTE_FDD_ENB_MME_PROC_ATTACH:
            if(rrc_cmd_resp->user->get_esm_info_transfer())
            {
                rrc_cmd_resp->rb->set_mme_state(LTE_FDD_ENB_MME_STATE_ESM_INFO_TRANSFER);
                attach_sm(rrc_cmd_resp->user, rrc_cmd_resp->rb);
            }else{
                rrc_cmd_resp->rb->set_mme_state(LTE_FDD_ENB_MME_STATE_ATTACH_ACCEPT);
                attach_sm(rrc_cmd_resp->user, rrc_cmd_resp->rb);
            }
            break;
        case LTE_FDD_ENB_MME_PROC_SERVICE_REQUEST:
            rrc_cmd_resp->rb->set_mme_state(LTE_FDD_ENB_MME_STATE_SETUP_DRB);
            service_req_sm(rrc_cmd_resp->user, rrc_cmd_resp->rb);
            break;
        default:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                      __FILE__,
                                      __LINE__,
                                      "MME in invalid procedure %s",
                                      LTE_fdd_enb_mme_proc_text[rrc_cmd_resp->rb->get_mme_procedure()]);
            break;
        }
        break;
    default:
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                  __FILE__,
                                  __LINE__,
                                  "Received invalid RRC command response %s",
                                  LTE_fdd_enb_mme_rrc_cmd_resp_text[rrc_cmd_resp->cmd_resp]);
        break;
    }
}

/*************************/
/*    Message Parsers    */
/*************************/
void LTE_fdd_enb_mme::parse_attach_complete(LIBLTE_BYTE_MSG_STRUCT *msg,
                                            LTE_fdd_enb_user       *user,
                                            LTE_fdd_enb_rb         *rb)
{
    LIBLTE_MME_ATTACH_COMPLETE_MSG_STRUCT attach_comp;
    uint8                                 pd;
    uint8                                 msg_type;

    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_MME,
                              __FILE__,
                              __LINE__,
                              "Received Attach Complete for RNTI=%u and RB=%s",
                              user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[rb->get_rb_id()]);

    // Unpack the message
    liblte_mme_unpack_attach_complete_msg(msg, &attach_comp);

    interface->send_ctrl_info_msg("user fully attached imsi=%s imei=%s",
                                  user->get_imsi_str().c_str(),
                                  user->get_imei_str().c_str());

    rb->set_mme_state(LTE_FDD_ENB_MME_STATE_ATTACHED);

    // Parse the ESM message
    liblte_mme_parse_msg_header(&attach_comp.esm_msg, &pd, &msg_type);
    switch(msg_type)
    {
    case LIBLTE_MME_MSG_TYPE_ACTIVATE_DEFAULT_EPS_BEARER_CONTEXT_ACCEPT:
        parse_activate_default_eps_bearer_context_accept(&attach_comp.esm_msg, user, rb);
        break;
    default:
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                  __FILE__,
                                  __LINE__,
                                  "Not handling NAS message with MSG_TYPE=%02X",
                                  msg_type);
        break;
    }
}
void LTE_fdd_enb_mme::parse_attach_request(LIBLTE_BYTE_MSG_STRUCT  *msg,
                                           LTE_fdd_enb_user       **user,
                                           LTE_fdd_enb_rb         **rb)
{
    LTE_fdd_enb_hss                      *hss       = LTE_fdd_enb_hss::get_instance();
    LTE_fdd_enb_user_mgr                 *user_mgr  = LTE_fdd_enb_user_mgr::get_instance();
    LTE_fdd_enb_user                     *act_user;
    LIBLTE_MME_ATTACH_REQUEST_MSG_STRUCT  attach_req;
    uint64                                imsi_num = 0;
    uint64                                imei_num = 0;
    uint32                                i;
    uint8                                 pd;
    uint8                                 msg_type;

    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_MME,
                              __FILE__,
                              __LINE__,
                              "Received Attach Request for RNTI=%u and RB=%s",
                              (*user)->get_c_rnti(),
                              LTE_fdd_enb_rb_text[(*rb)->get_rb_id()]);

    // Unpack the message
    liblte_mme_unpack_attach_request_msg(msg, &attach_req);

    // Parse the ESM message
    liblte_mme_parse_msg_header(&attach_req.esm_msg, &pd, &msg_type);
    switch(msg_type)
    {
    case LIBLTE_MME_MSG_TYPE_PDN_CONNECTIVITY_REQUEST:
        parse_pdn_connectivity_request(&attach_req.esm_msg, (*user), (*rb));
        break;
    default:
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                  __FILE__,
                                  __LINE__,
                                  "Not handling NAS message with MSG_TYPE=%02X",
                                  msg_type);
        break;
    }

    // Set the procedure
    (*rb)->set_mme_procedure(LTE_FDD_ENB_MME_PROC_ATTACH);

    // Store the attach type
    (*user)->set_attach_type(attach_req.eps_attach_type);

    // Store UE capabilities
    for(i=0; i<8; i++)
    {
        (*user)->set_eea_support(i, attach_req.ue_network_cap.eea[i]);
        (*user)->set_eia_support(i, attach_req.ue_network_cap.eia[i]);
    }
    if(attach_req.ue_network_cap.uea_present)
    {
        for(i=0; i<8; i++)
        {
            (*user)->set_uea_support(i, attach_req.ue_network_cap.uea[i]);
        }
    }
    if(attach_req.ue_network_cap.uia_present)
    {
        for(i=1; i<8; i++)
        {
            (*user)->set_uia_support(i, attach_req.ue_network_cap.uia[i]);
        }
    }
    if(attach_req.ms_network_cap_present)
    {
        for(i=1; i<8; i++)
        {
            (*user)->set_gea_support(i, attach_req.ms_network_cap.gea[i]);
        }
    }

    // Send an info message
    if(LIBLTE_MME_EPS_MOBILE_ID_TYPE_GUTI == attach_req.eps_mobile_id.type_of_id)
    {
        if(LTE_FDD_ENB_ERROR_NONE == user_mgr->find_user(&attach_req.eps_mobile_id.guti, &act_user))
        {
            if(act_user != (*user))
            {
                act_user->copy_rbs((*user));
                (*user)->clear_rbs();
                user_mgr->transfer_c_rnti(*user, act_user);
                *user = act_user;
            }
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                      __FILE__,
                                      __LINE__,
                                      "IMSI=%015llu is associated with RNTI=%u, RB=%s",
                                      (*user)->get_id()->imsi,
                                      (*user)->get_c_rnti(),
                                      LTE_fdd_enb_rb_text[(*rb)->get_rb_id()]);
            (*rb)->set_mme_state(LTE_FDD_ENB_MME_STATE_AUTHENTICATE);
        }else{
            if((*user)->is_id_set())
            {
                if((*user)->get_eea_support(0) && (*user)->get_eia_support(2))
                {
                    (*rb)->set_mme_state(LTE_FDD_ENB_MME_STATE_AUTHENTICATE);
                }else{
                    (*user)->set_emm_cause(LIBLTE_MME_EMM_CAUSE_UE_SECURITY_CAPABILITIES_MISMATCH);
                    (*rb)->set_mme_state(LTE_FDD_ENB_MME_STATE_REJECT);
                }
            }else{
                (*rb)->set_mme_state(LTE_FDD_ENB_MME_STATE_ID_REQUEST_IMSI);
            }
        }
    }else if(LIBLTE_MME_EPS_MOBILE_ID_TYPE_IMSI == attach_req.eps_mobile_id.type_of_id){
        for(i=0; i<15; i++)
        {
            imsi_num *= 10;
            imsi_num += attach_req.eps_mobile_id.imsi[i];
        }
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                  __FILE__,
                                  __LINE__,
                                  "IMSI=%015llu is associated with RNTI=%u, RB=%s",
                                  imsi_num,
                                  (*user)->get_c_rnti(),
                                  LTE_fdd_enb_rb_text[(*rb)->get_rb_id()]);
        if(hss->is_imsi_allowed(imsi_num))
        {
            if((*user)->get_eea_support(0) && (*user)->get_eia_support(2))
            {
                (*rb)->set_mme_state(LTE_FDD_ENB_MME_STATE_AUTHENTICATE);
                (*user)->set_id(hss->get_user_id_from_imsi(imsi_num));
            }else{
                (*user)->set_emm_cause(LIBLTE_MME_EMM_CAUSE_UE_SECURITY_CAPABILITIES_MISMATCH);
                (*rb)->set_mme_state(LTE_FDD_ENB_MME_STATE_REJECT);
            }
        }else{
            (*user)->set_temp_id(imsi_num);
            (*rb)->set_mme_state(LTE_FDD_ENB_MME_STATE_REJECT);
        }
    }else{
        for(i=0; i<15; i++)
        {
            imei_num *= 10;
            imei_num += attach_req.eps_mobile_id.imei[i];
        }
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                  __FILE__,
                                  __LINE__,
                                  "IMEI=%015llu is associated with RNTI=%u, RB=%s",
                                  imei_num,
                                  (*user)->get_c_rnti(),
                                  LTE_fdd_enb_rb_text[(*rb)->get_rb_id()]);
        if(hss->is_imei_allowed(imei_num))
        {
            if((*user)->get_eea_support(0) && (*user)->get_eia_support(2))
            {
                (*rb)->set_mme_state(LTE_FDD_ENB_MME_STATE_AUTHENTICATE);
                (*user)->set_id(hss->get_user_id_from_imei(imei_num));
            }else{
                (*user)->set_emm_cause(LIBLTE_MME_EMM_CAUSE_UE_SECURITY_CAPABILITIES_MISMATCH);
                (*rb)->set_mme_state(LTE_FDD_ENB_MME_STATE_REJECT);
            }
        }else{
            (*user)->set_temp_id(imei_num);
            (*rb)->set_mme_state(LTE_FDD_ENB_MME_STATE_REJECT);
        }
    }
}
void LTE_fdd_enb_mme::parse_authentication_failure(LIBLTE_BYTE_MSG_STRUCT *msg,
                                                   LTE_fdd_enb_user       *user,
                                                   LTE_fdd_enb_rb         *rb)
{
    LTE_fdd_enb_hss                              *hss       = LTE_fdd_enb_hss::get_instance();
    LIBLTE_MME_AUTHENTICATION_FAILURE_MSG_STRUCT  auth_fail;

    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_MME,
                              __FILE__,
                              __LINE__,
                              "Received Authentication Failure for RNTI=%u and RB=%s",
                              user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[rb->get_rb_id()]);

    // Unpack the message
    liblte_mme_unpack_authentication_failure_msg(msg, &auth_fail);

    if(LIBLTE_MME_EMM_CAUSE_SYNCH_FAILURE == auth_fail.emm_cause &&
       auth_fail.auth_fail_param_present)
    {
        sem_wait(&sys_info_sem);
        hss->security_resynch(user->get_id(), sys_info.mcc, sys_info.mnc, auth_fail.auth_fail_param);
        sem_post(&sys_info_sem);
    }else{
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                  __FILE__,
                                  __LINE__,
                                  "Authentication failure cause=%02X, RNTI=%u, RB=%s",
                                  auth_fail.emm_cause,
                                  user->get_c_rnti(),
                                  LTE_fdd_enb_rb_text[rb->get_rb_id()]);
        rb->set_mme_state(LTE_FDD_ENB_MME_STATE_RELEASE);
    }
}
void LTE_fdd_enb_mme::parse_authentication_response(LIBLTE_BYTE_MSG_STRUCT *msg,
                                                    LTE_fdd_enb_user       *user,
                                                    LTE_fdd_enb_rb         *rb)
{
    LTE_fdd_enb_hss                               *hss       = LTE_fdd_enb_hss::get_instance();
    LTE_FDD_ENB_AUTHENTICATION_VECTOR_STRUCT      *auth_vec  = NULL;
    LIBLTE_MME_AUTHENTICATION_RESPONSE_MSG_STRUCT  auth_resp;
    uint32                                         i;
    bool                                           res_match = true;

    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_MME,
                              __FILE__,
                              __LINE__,
                              "Received Authentication Response for RNTI=%u and RB=%s",
                              user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[rb->get_rb_id()]);

    // Unpack the message
    liblte_mme_unpack_authentication_response_msg(msg, &auth_resp);

    // Check RES
    auth_vec = hss->get_auth_vec(user->get_id());
    if(NULL != auth_vec)
    {
        res_match = true;
        for(i=0; i<8; i++)
        {
            if(auth_vec->res[i] != auth_resp.res[i])
            {
                res_match = false;
                break;
            }
        }

        if(res_match)
        {
            interface->send_ctrl_info_msg("user authentication successful imsi=%s imei=%s",
                                          user->get_imsi_str().c_str(),
                                          user->get_imei_str().c_str());
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                      __FILE__,
                                      __LINE__,
                                      "Authentication successful for RNTI=%u and RB=%s",
                                      user->get_c_rnti(),
                                      LTE_fdd_enb_rb_text[rb->get_rb_id()]);
            user->set_auth_vec(auth_vec);
            rb->set_mme_state(LTE_FDD_ENB_MME_STATE_ENABLE_SECURITY);
        }else{
            interface->send_ctrl_info_msg("user authentication rejected (RES MISMATCH) imsi=%s imei=%s",
                                          user->get_imsi_str().c_str(),
                                          user->get_imei_str().c_str());
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                      __FILE__,
                                      __LINE__,
                                      "Authentication rejected (RES MISMATCH) for RNTI=%u and RB=%s",
                                      user->get_c_rnti(),
                                      LTE_fdd_enb_rb_text[rb->get_rb_id()]);
            rb->set_mme_state(LTE_FDD_ENB_MME_STATE_AUTH_REJECTED);
        }
    }else{
        interface->send_ctrl_info_msg("user authentication rejected (NO AUTH VEC) imsi=%s imei=%s",
                                      user->get_imsi_str().c_str(),
                                      user->get_imei_str().c_str());
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                  __FILE__,
                                  __LINE__,
                                  "Authentication rejected (NO AUTH VEC) for RNTI=%u and RB=%s",
                                  user->get_c_rnti(),
                                  LTE_fdd_enb_rb_text[rb->get_rb_id()]);
        rb->set_mme_state(LTE_FDD_ENB_MME_STATE_AUTH_REJECTED);
    }
}
void LTE_fdd_enb_mme::parse_detach_request(LIBLTE_BYTE_MSG_STRUCT *msg,
                                           LTE_fdd_enb_user       *user,
                                           LTE_fdd_enb_rb         *rb)
{
    LTE_fdd_enb_user_mgr                 *user_mgr = LTE_fdd_enb_user_mgr::get_instance();
    LIBLTE_MME_DETACH_REQUEST_MSG_STRUCT  detach_req;

    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_MME,
                              __FILE__,
                              __LINE__,
                              "Received Detach Request for RNTI=%u and RB=%s",
                              user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[rb->get_rb_id()]);

    // Unpack the message
    liblte_mme_unpack_detach_request_msg(msg, &detach_req);

    // Set the procedure
    rb->set_mme_procedure(LTE_FDD_ENB_MME_PROC_DETACH);
    rb->set_mme_state(LTE_FDD_ENB_MME_STATE_SEND_DETACH_ACCEPT);

    // Delete the user
    user->prepare_for_deletion();
}
void LTE_fdd_enb_mme::parse_identity_response(LIBLTE_BYTE_MSG_STRUCT *msg,
                                              LTE_fdd_enb_user       *user,
                                              LTE_fdd_enb_rb         *rb)
{
    LTE_fdd_enb_hss                   *hss = LTE_fdd_enb_hss::get_instance();
    LIBLTE_MME_ID_RESPONSE_MSG_STRUCT  id_resp;
    uint64                             imsi_num = 0;
    uint64                             imei_num = 0;
    uint32                             i;

    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_MME,
                              __FILE__,
                              __LINE__,
                              "Received Identity Response for RNTI=%u and RB=%s",
                              user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[rb->get_rb_id()]);

    // Unpack the message
    liblte_mme_unpack_identity_response_msg(msg, &id_resp);

    // Store the ID
    if(LIBLTE_MME_MOBILE_ID_TYPE_IMSI == id_resp.mobile_id.type_of_id)
    {
        for(i=0; i<15; i++)
        {
            imsi_num *= 10;
            imsi_num += id_resp.mobile_id.imsi[i];
        }
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                  __FILE__,
                                  __LINE__,
                                  "IMSI=%015llu is associated with RNTI=%u, RB=%s",
                                  imsi_num,
                                  user->get_c_rnti(),
                                  LTE_fdd_enb_rb_text[rb->get_rb_id()]);
        if(hss->is_imsi_allowed(imsi_num))
        {
            if(user->get_eea_support(0) && user->get_eia_support(2))
            {
                rb->set_mme_state(LTE_FDD_ENB_MME_STATE_AUTHENTICATE);
                user->set_id(hss->get_user_id_from_imsi(imsi_num));
            }else{
                user->set_emm_cause(LIBLTE_MME_EMM_CAUSE_UE_SECURITY_CAPABILITIES_MISMATCH);
                rb->set_mme_state(LTE_FDD_ENB_MME_STATE_REJECT);
            }
        }else{
            user->set_temp_id(imsi_num);
            rb->set_mme_state(LTE_FDD_ENB_MME_STATE_REJECT);
        }
    }else if(LIBLTE_MME_MOBILE_ID_TYPE_IMEI == id_resp.mobile_id.type_of_id){
        for(i=0; i<15; i++)
        {
            imei_num *= 10;
            imei_num += id_resp.mobile_id.imei[i];
        }
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                  __FILE__,
                                  __LINE__,
                                  "IMEI=%015llu is associated with RNTI=%u, RB=%s",
                                  imei_num,
                                  user->get_c_rnti(),
                                  LTE_fdd_enb_rb_text[rb->get_rb_id()]);
        if(hss->is_imei_allowed(imei_num))
        {
            if(user->get_eea_support(0) && user->get_eia_support(2))
            {
                rb->set_mme_state(LTE_FDD_ENB_MME_STATE_AUTHENTICATE);
                user->set_id(hss->get_user_id_from_imei(imei_num));
            }else{
                user->set_emm_cause(LIBLTE_MME_EMM_CAUSE_UE_SECURITY_CAPABILITIES_MISMATCH);
                rb->set_mme_state(LTE_FDD_ENB_MME_STATE_REJECT);
            }
        }else{
            user->set_temp_id(imei_num);
            rb->set_mme_state(LTE_FDD_ENB_MME_STATE_REJECT);
        }
    }else{
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                  __FILE__,
                                  __LINE__,
                                  "Invalid ID_TYPE=%u",
                                  id_resp.mobile_id.type_of_id);
    }
}
void LTE_fdd_enb_mme::parse_security_mode_complete(LIBLTE_BYTE_MSG_STRUCT *msg,
                                                   LTE_fdd_enb_user       *user,
                                                   LTE_fdd_enb_rb         *rb)
{
    LIBLTE_MME_SECURITY_MODE_COMPLETE_MSG_STRUCT sec_mode_comp;
    uint64                                       imei_num = 0;
    uint32                                       i;

    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_MME,
                              __FILE__,
                              __LINE__,
                              "Received Security Mode Complete for RNTI=%u and RB=%s",
                              user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[rb->get_rb_id()]);

    // Unpack the message
    liblte_mme_unpack_security_mode_complete_msg(msg, &sec_mode_comp);

    if(sec_mode_comp.imeisv_present)
    {
        if(LIBLTE_MME_MOBILE_ID_TYPE_IMEISV == sec_mode_comp.imeisv.type_of_id)
        {
            for(i=0; i<14; i++)
            {
                imei_num *= 10;
                imei_num += sec_mode_comp.imeisv.imeisv[i];
            }
            if((user->get_id()->imei/10) != imei_num)
            {
                interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                          LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                          __FILE__,
                                          __LINE__,
                                          "Received IMEI (%015llu) does not match stored IMEI (%015llu), RNTI=%u, RB=%s",
                                          imei_num*10,
                                          (user->get_id()->imei/10)*10,
                                          user->get_c_rnti(),
                                          LTE_fdd_enb_rb_text[rb->get_rb_id()]);
            }
        }else{
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                      __FILE__,
                                      __LINE__,
                                      "Security Mode Complete received with invalid ID type (%u), RNTI=%u, RB=%s",
                                      sec_mode_comp.imeisv.type_of_id,
                                      user->get_c_rnti(),
                                      LTE_fdd_enb_rb_text[rb->get_rb_id()]);
        }
    }

    rb->set_mme_state(LTE_FDD_ENB_MME_STATE_RRC_SECURITY);
}
void LTE_fdd_enb_mme::parse_security_mode_reject(LIBLTE_BYTE_MSG_STRUCT *msg,
                                                 LTE_fdd_enb_user       *user,
                                                 LTE_fdd_enb_rb         *rb)
{
    LIBLTE_MME_SECURITY_MODE_REJECT_MSG_STRUCT sec_mode_rej;

    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_MME,
                              __FILE__,
                              __LINE__,
                              "Received Security Mode Reject for RNTI=%u and RB=%s",
                              user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[rb->get_rb_id()]);

    // Unpack the message
    liblte_mme_unpack_security_mode_reject_msg(msg, &sec_mode_rej);

    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                              LTE_FDD_ENB_DEBUG_LEVEL_MME,
                              __FILE__,
                              __LINE__,
                              "Security Mode Rejected cause=%02X, RNTI=%u, RB=%s",
                              sec_mode_rej.emm_cause,
                              user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[rb->get_rb_id()]);
    rb->set_mme_state(LTE_FDD_ENB_MME_STATE_RELEASE);
}
void LTE_fdd_enb_mme::parse_service_request(LIBLTE_BYTE_MSG_STRUCT *msg,
                                            LTE_fdd_enb_user       *user,
                                            LTE_fdd_enb_rb         *rb)
{
    LTE_fdd_enb_hss                          *hss      = LTE_fdd_enb_hss::get_instance();
    LTE_FDD_ENB_AUTHENTICATION_VECTOR_STRUCT *auth_vec = user->get_auth_vec();
    LTE_FDD_ENB_AUTHENTICATION_VECTOR_STRUCT *hss_auth_vec;
    LTE_FDD_ENB_RRC_CMD_READY_MSG_STRUCT      cmd_ready;
    LIBLTE_MME_SERVICE_REQUEST_MSG_STRUCT     service_req;
    uint32                                    i;

    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_MME,
                              __FILE__,
                              __LINE__,
                              "Received Service Request for RNTI=%u, RB=%s",
                              user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[rb->get_rb_id()]);

    // Unpack the message
    liblte_mme_unpack_service_request_msg(msg, &service_req);

    // Set the procedure
    rb->set_mme_procedure(LTE_FDD_ENB_MME_PROC_SERVICE_REQUEST);

    // Verify KSI and sequence number
    if(0 != service_req.ksi_and_seq_num.ksi)
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                  __FILE__,
                                  __LINE__,
                                  "Invalid KSI (%u) for RNTI=%u, RB=%s",
                                  service_req.ksi_and_seq_num.ksi,
                                  user->get_c_rnti(),
                                  LTE_fdd_enb_rb_text[rb->get_rb_id()]);

        send_service_reject(user, rb, LIBLTE_MME_EMM_CAUSE_IMPLICITLY_DETACHED);

        // Set the state
        rb->set_mme_state(LTE_FDD_ENB_MME_STATE_RELEASE);
    }else{
        if(auth_vec->nas_count_ul != service_req.ksi_and_seq_num.seq_num)
        {
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                      __FILE__,
                                      __LINE__,
                                      "Sequence number mismatch (rx=%u, stored=%u) for RNTI=%u, RB=%s",
                                      service_req.ksi_and_seq_num.seq_num,
                                      user->get_auth_vec()->nas_count_ul,
                                      user->get_c_rnti(),
                                      LTE_fdd_enb_rb_text[rb->get_rb_id()]);

            // Resolve sequence number mismatch
            auth_vec->nas_count_ul = service_req.ksi_and_seq_num.seq_num;
            hss_auth_vec           = hss->regenerate_enb_security_data(user->get_id(), auth_vec->nas_count_ul);
            if(NULL != hss_auth_vec)
            {
                for(i=0; i<32; i++)
                {
                    auth_vec->k_rrc_enc[i] = hss_auth_vec->k_rrc_enc[i];
                    auth_vec->k_rrc_int[i] = hss_auth_vec->k_rrc_int[i];
                }
            }
        }

        // Set the state
        if(NULL != hss_auth_vec)
        {
            rb->set_mme_state(LTE_FDD_ENB_MME_STATE_RRC_SECURITY);
        }else{
            send_service_reject(user, rb, LIBLTE_MME_EMM_CAUSE_IMPLICITLY_DETACHED);
            rb->set_mme_state(LTE_FDD_ENB_MME_STATE_RELEASE);
        }
    }
}
void LTE_fdd_enb_mme::parse_activate_default_eps_bearer_context_accept(LIBLTE_BYTE_MSG_STRUCT *msg,
                                                                       LTE_fdd_enb_user       *user,
                                                                       LTE_fdd_enb_rb         *rb)
{
    LIBLTE_MME_ACTIVATE_DEFAULT_EPS_BEARER_CONTEXT_ACCEPT_MSG_STRUCT act_def_eps_bearer_context_accept;

    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_MME,
                              __FILE__,
                              __LINE__,
                              "Received Activate Default EPS Bearer Context Accept for RNTI=%u and RB=%s",
                              user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[rb->get_rb_id()]);

    // Unpack the message
    liblte_mme_unpack_activate_default_eps_bearer_context_accept_msg(msg, &act_def_eps_bearer_context_accept);

    interface->send_ctrl_info_msg("default bearer setup for imsi=%s imei=%s",
                                  user->get_imsi_str().c_str(),
                                  user->get_imei_str().c_str());
}
void LTE_fdd_enb_mme::parse_esm_information_response(LIBLTE_BYTE_MSG_STRUCT *msg,
                                                     LTE_fdd_enb_user       *user,
                                                     LTE_fdd_enb_rb         *rb)
{
    LIBLTE_MME_ESM_INFORMATION_RESPONSE_MSG_STRUCT esm_info_resp;

    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_MME,
                              __FILE__,
                              __LINE__,
                              "Received ESM Information Response for RNTI=%u and RB=%s",
                              user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[rb->get_rb_id()]);

    // Unpack the message
    liblte_mme_unpack_esm_information_response_msg(msg, &esm_info_resp);

    // FIXME

    rb->set_mme_state(LTE_FDD_ENB_MME_STATE_ATTACH_ACCEPT);
}
void LTE_fdd_enb_mme::parse_pdn_connectivity_request(LIBLTE_BYTE_MSG_STRUCT *msg,
                                                     LTE_fdd_enb_user       *user,
                                                     LTE_fdd_enb_rb         *rb)
{
    LIBLTE_MME_PDN_CONNECTIVITY_REQUEST_MSG_STRUCT pdn_con_req;
    LIBLTE_MME_PROTOCOL_CONFIG_OPTIONS_STRUCT      pco_resp;
    uint32                                         i;

    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_MME,
                              __FILE__,
                              __LINE__,
                              "Received PDN Connectivity Request for RNTI=%u and RB=%s",
                              user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[rb->get_rb_id()]);

    // Unpack the message
    liblte_mme_unpack_pdn_connectivity_request_msg(msg, &pdn_con_req);

    // Store the EPS Bearer ID
    user->set_eps_bearer_id(pdn_con_req.eps_bearer_id);

    // Store the Procedure Transaction ID
    user->set_proc_transaction_id(pdn_con_req.proc_transaction_id);

    // Store the PDN Type
    user->set_pdn_type(pdn_con_req.pdn_type);

    // Store the ESM Information Transfer Flag
//    if(pdn_con_req.esm_info_transfer_flag_present &&
//       LIBLTE_MME_ESM_INFO_TRANSFER_FLAG_REQUIRED == pdn_con_req.esm_info_transfer_flag)
//    {
//        user->set_esm_info_transfer(true);
//    }else{
        user->set_esm_info_transfer(false);
//    }

    if(pdn_con_req.protocol_cnfg_opts_present)
    {
        pco_resp.N_opts = 0;
        for(i=0; i<pdn_con_req.protocol_cnfg_opts.N_opts; i++)
        {
            if(LIBLTE_MME_CONFIGURATION_PROTOCOL_OPTIONS_IPCP == pdn_con_req.protocol_cnfg_opts.opt[i].id)
            {
                if(0x01 == pdn_con_req.protocol_cnfg_opts.opt[i].contents[0] &&
                   0x81 == pdn_con_req.protocol_cnfg_opts.opt[i].contents[4] &&
                   0x83 == pdn_con_req.protocol_cnfg_opts.opt[i].contents[10])
                {
                    pco_resp.opt[pco_resp.N_opts].id           = LIBLTE_MME_CONFIGURATION_PROTOCOL_OPTIONS_IPCP;
                    pco_resp.opt[pco_resp.N_opts].len          = 16;
                    pco_resp.opt[pco_resp.N_opts].contents[0]  = 0x03;
                    pco_resp.opt[pco_resp.N_opts].contents[1]  = pdn_con_req.protocol_cnfg_opts.opt[i].contents[1];
                    pco_resp.opt[pco_resp.N_opts].contents[2]  = 0x00;
                    pco_resp.opt[pco_resp.N_opts].contents[3]  = 0x10;
                    pco_resp.opt[pco_resp.N_opts].contents[4]  = 0x81;
                    pco_resp.opt[pco_resp.N_opts].contents[5]  = 0x06;
                    pco_resp.opt[pco_resp.N_opts].contents[6]  = (dns_addr >> 24) & 0xFF;
                    pco_resp.opt[pco_resp.N_opts].contents[7]  = (dns_addr >> 16) & 0xFF;
                    pco_resp.opt[pco_resp.N_opts].contents[8]  = (dns_addr >> 8) & 0xFF;
                    pco_resp.opt[pco_resp.N_opts].contents[9]  = dns_addr & 0xFF;
                    pco_resp.opt[pco_resp.N_opts].contents[10] = 0x83;
                    pco_resp.opt[pco_resp.N_opts].contents[11] = 0x06;
                    pco_resp.opt[pco_resp.N_opts].contents[12] = (dns_addr >> 24) & 0xFF;
                    pco_resp.opt[pco_resp.N_opts].contents[13] = (dns_addr >> 16) & 0xFF;
                    pco_resp.opt[pco_resp.N_opts].contents[14] = (dns_addr >> 8) & 0xFF;
                    pco_resp.opt[pco_resp.N_opts].contents[15] = dns_addr & 0xFF;
                    pco_resp.N_opts++;
                }else{
                    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                              LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                              __FILE__,
                                              __LINE__,
                                              "Unknown PCO");
                }
            }else if(LIBLTE_MME_ADDITIONAL_PARAMETERS_UL_DNS_SERVER_IPV4_ADDRESS_REQUEST == pdn_con_req.protocol_cnfg_opts.opt[i].id){
                pco_resp.opt[pco_resp.N_opts].id          = LIBLTE_MME_ADDITIONAL_PARAMETERS_DL_DNS_SERVER_IPV4_ADDRESS;
                pco_resp.opt[pco_resp.N_opts].len         = 4;
                pco_resp.opt[pco_resp.N_opts].contents[0] = (dns_addr >> 24) & 0xFF;
                pco_resp.opt[pco_resp.N_opts].contents[1] = (dns_addr >> 16) & 0xFF;
                pco_resp.opt[pco_resp.N_opts].contents[2] = (dns_addr >> 8) & 0xFF;
                pco_resp.opt[pco_resp.N_opts].contents[3] = dns_addr & 0xFF;
                pco_resp.N_opts++;
            }else if(LIBLTE_MME_ADDITIONAL_PARAMETERS_UL_IP_ADDRESS_ALLOCATION_VIA_NAS_SIGNALLING == pdn_con_req.protocol_cnfg_opts.opt[i].id){
                // Nothing to do
            }else{
                interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                          LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                          __FILE__,
                                          __LINE__,
                                          "Invalid PCO ID (%04X)",
                                          pdn_con_req.protocol_cnfg_opts.opt[i].id);
            }
        }
        user->set_protocol_cnfg_opts(&pco_resp);
    }
}

/************************/
/*    State Machines    */
/************************/
void LTE_fdd_enb_mme::attach_sm(LTE_fdd_enb_user *user,
                                LTE_fdd_enb_rb   *rb)
{
    LTE_fdd_enb_hss      *hss      = LTE_fdd_enb_hss::get_instance();
    LTE_fdd_enb_user_mgr *user_mgr = LTE_fdd_enb_user_mgr::get_instance();

    switch(rb->get_mme_state())
    {
    case LTE_FDD_ENB_MME_STATE_ID_REQUEST_IMSI:
        send_identity_request(user, rb, LIBLTE_MME_ID_TYPE_2_IMSI);
        break;
    case LTE_FDD_ENB_MME_STATE_REJECT:
        user->prepare_for_deletion();
        send_attach_reject(user, rb);
        break;
    case LTE_FDD_ENB_MME_STATE_AUTHENTICATE:
        send_authentication_request(user, rb);
        break;
    case LTE_FDD_ENB_MME_STATE_AUTH_REJECTED:
        send_authentication_reject(user, rb);
        break;
    case LTE_FDD_ENB_MME_STATE_ENABLE_SECURITY:
        send_security_mode_command(user, rb);
        break;
    case LTE_FDD_ENB_MME_STATE_RELEASE:
        send_rrc_command(user, rb, LTE_FDD_ENB_RRC_CMD_RELEASE);
        break;
    case LTE_FDD_ENB_MME_STATE_RRC_SECURITY:
        send_rrc_command(user, rb, LTE_FDD_ENB_RRC_CMD_SECURITY);
        break;
    case LTE_FDD_ENB_MME_STATE_ESM_INFO_TRANSFER:
        send_esm_information_request(user, rb);
        break;
    case LTE_FDD_ENB_MME_STATE_ATTACH_ACCEPT:
        send_attach_accept(user, rb);
        break;
    case LTE_FDD_ENB_MME_STATE_ATTACHED:
        send_emm_information(user, rb);
        break;
    default:
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                  __FILE__,
                                  __LINE__,
                                  "ATTACH state machine invalid state %s, RNTI=%u and RB=%s",
                                  LTE_fdd_enb_mme_state_text[rb->get_mme_state()],
                                  user->get_c_rnti(),
                                  LTE_fdd_enb_rb_text[rb->get_rb_id()]);
        break;
    }
}
void LTE_fdd_enb_mme::service_req_sm(LTE_fdd_enb_user *user,
                                     LTE_fdd_enb_rb   *rb)
{
    switch(rb->get_mme_state())
    {
    case LTE_FDD_ENB_MME_STATE_RELEASE:
        send_rrc_command(user, rb, LTE_FDD_ENB_RRC_CMD_RELEASE);
        break;
    case LTE_FDD_ENB_MME_STATE_RRC_SECURITY:
        send_rrc_command(user, rb, LTE_FDD_ENB_RRC_CMD_SECURITY);
        break;
    case LTE_FDD_ENB_MME_STATE_SETUP_DRB:
        send_activate_dedicated_eps_bearer_context_request(user, rb);
        break;
    default:
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                  __FILE__,
                                  __LINE__,
                                  "SERVICE REQUEST state machine invalid state %s, RNTI=%u and RB=%s",
                                  LTE_fdd_enb_mme_state_text[rb->get_mme_state()],
                                  user->get_c_rnti(),
                                  LTE_fdd_enb_rb_text[rb->get_rb_id()]);
        break;
    }
}
void LTE_fdd_enb_mme::detach_sm(LTE_fdd_enb_user *user,
                                LTE_fdd_enb_rb   *rb)
{
    switch(rb->get_mme_state())
    {
    case LTE_FDD_ENB_MME_STATE_SEND_DETACH_ACCEPT:
        send_detach_accept(user, rb);
        break;
    default:
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                  __FILE__,
                                  __LINE__,
                                  "DETACH state machine invalid state %s, RNTI=%u and RB=%s",
                                  LTE_fdd_enb_mme_state_text[rb->get_mme_state()],
                                  user->get_c_rnti(),
                                  LTE_fdd_enb_rb_text[rb->get_rb_id()]);
        break;
    }
}

/*************************/
/*    Message Senders    */
/*************************/
void LTE_fdd_enb_mme::send_attach_accept(LTE_fdd_enb_user *user,
                                         LTE_fdd_enb_rb   *rb)
{
    LTE_fdd_enb_user_mgr                                              *user_mgr = LTE_fdd_enb_user_mgr::get_instance();
    LTE_FDD_ENB_RRC_CMD_READY_MSG_STRUCT                               cmd_ready;
    LIBLTE_MME_ATTACH_ACCEPT_MSG_STRUCT                                attach_accept;
    LIBLTE_MME_ACTIVATE_DEFAULT_EPS_BEARER_CONTEXT_REQUEST_MSG_STRUCT  act_def_eps_bearer_context_req;
    LIBLTE_MME_PROTOCOL_CONFIG_OPTIONS_STRUCT                         *pco = user->get_protocol_cnfg_opts();
    LIBLTE_BYTE_MSG_STRUCT                                             msg;
    uint32                                                             ip_addr;

    // Assign IP address to user
    user->set_ip_addr(get_next_ip_addr());
    ip_addr = user->get_ip_addr();

    if(0 == user->get_eps_bearer_id())
    {
        act_def_eps_bearer_context_req.eps_bearer_id = 5;
        user->set_eps_bearer_id(5);
    }else{
        act_def_eps_bearer_context_req.eps_bearer_id = user->get_eps_bearer_id();
    }
    if(0 == user->get_proc_transaction_id())
    {
        act_def_eps_bearer_context_req.proc_transaction_id = 1;
        user->set_proc_transaction_id(1);
    }else{
        act_def_eps_bearer_context_req.proc_transaction_id = user->get_proc_transaction_id();
    }
    act_def_eps_bearer_context_req.eps_qos.qci            = 9;
    act_def_eps_bearer_context_req.eps_qos.br_present     = false;
    act_def_eps_bearer_context_req.eps_qos.br_ext_present = false;
    act_def_eps_bearer_context_req.apn.apn                = "www.openLTE.com";
    act_def_eps_bearer_context_req.pdn_addr.pdn_type      = LIBLTE_MME_PDN_TYPE_IPV4;
    act_def_eps_bearer_context_req.pdn_addr.addr[0]       = (ip_addr >> 24) & 0xFF;
    act_def_eps_bearer_context_req.pdn_addr.addr[1]       = (ip_addr >> 16) & 0xFF;
    act_def_eps_bearer_context_req.pdn_addr.addr[2]       = (ip_addr >> 8) & 0xFF;
    act_def_eps_bearer_context_req.pdn_addr.addr[3]       = ip_addr & 0xFF;
    act_def_eps_bearer_context_req.transaction_id_present = false;
    act_def_eps_bearer_context_req.negotiated_qos_present = false;
    act_def_eps_bearer_context_req.llc_sapi_present       = false;
    act_def_eps_bearer_context_req.radio_prio_present     = false;
    act_def_eps_bearer_context_req.packet_flow_id_present = false;
    act_def_eps_bearer_context_req.apn_ambr_present       = false;
    if(LIBLTE_MME_PDN_TYPE_IPV4 == user->get_pdn_type())
    {
        act_def_eps_bearer_context_req.esm_cause_present = false;
    }else{
        act_def_eps_bearer_context_req.esm_cause_present = true;
        act_def_eps_bearer_context_req.esm_cause         = LIBLTE_MME_ESM_CAUSE_PDN_TYPE_IPV4_ONLY_ALLOWED;
    }
    if(0 != pco->N_opts)
    {
        act_def_eps_bearer_context_req.protocol_cnfg_opts_present = true;
        memcpy(&act_def_eps_bearer_context_req.protocol_cnfg_opts, pco, sizeof(LIBLTE_MME_PROTOCOL_CONFIG_OPTIONS_STRUCT));
    }else{
        act_def_eps_bearer_context_req.protocol_cnfg_opts_present = false;
    }
    act_def_eps_bearer_context_req.connectivity_type_present = false;
    liblte_mme_pack_activate_default_eps_bearer_context_request_msg(&act_def_eps_bearer_context_req,
                                                                    &attach_accept.esm_msg);

    sem_wait(&sys_info_sem);
    attach_accept.eps_attach_result                   = user->get_attach_type();
    attach_accept.t3412.unit                          = LIBLTE_MME_GPRS_TIMER_DEACTIVATED;
    attach_accept.tai_list.N_tais                     = 1;
    attach_accept.tai_list.tai[0].mcc                 = sys_info.mcc;
    attach_accept.tai_list.tai[0].mnc                 = sys_info.mnc;
    attach_accept.tai_list.tai[0].tac                 = sys_info.sib1.tracking_area_code;
    attach_accept.guti_present                        = true;
    attach_accept.guti.type_of_id                     = LIBLTE_MME_EPS_MOBILE_ID_TYPE_GUTI;
    attach_accept.guti.guti.mcc                       = sys_info.mcc;
    attach_accept.guti.guti.mnc                       = sys_info.mnc;
    attach_accept.guti.guti.mme_group_id              = 0;
    attach_accept.guti.guti.mme_code                  = 0;
    attach_accept.guti.guti.m_tmsi                    = user_mgr->get_next_m_tmsi();
    attach_accept.lai_present                         = false;
    attach_accept.ms_id_present                       = false;
    attach_accept.emm_cause_present                   = false;
    attach_accept.t3402_present                       = false;
    attach_accept.t3423_present                       = false;
    attach_accept.equivalent_plmns_present            = false;
    attach_accept.emerg_num_list_present              = false;
    attach_accept.eps_network_feature_support_present = false;
    attach_accept.additional_update_result_present    = false;
    attach_accept.t3412_ext_present                   = false;
    sem_post(&sys_info_sem);
    user->set_guti(&attach_accept.guti.guti);
    liblte_mme_pack_attach_accept_msg(&attach_accept,
                                      LIBLTE_MME_SECURITY_HDR_TYPE_INTEGRITY_AND_CIPHERED,
                                      user->get_auth_vec()->k_nas_int,
                                      user->get_auth_vec()->nas_count_dl,
                                      LIBLTE_SECURITY_DIRECTION_DOWNLINK,
                                      &msg);
    user->increment_nas_count_dl();
    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_MME,
                              __FILE__,
                              __LINE__,
                              &msg,
                              "Sending Attach Accept for RNTI=%u, RB=%s",
                              user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[rb->get_rb_id()]);

    // Queue the NAS message for RRC
    rb->queue_rrc_nas_msg(&msg);

    // Signal RRC for NAS message
    cmd_ready.user = user;
    cmd_ready.rb   = rb;
    cmd_ready.cmd  = LTE_FDD_ENB_RRC_CMD_SETUP_DEF_DRB;
    msgq_to_rrc->send(LTE_FDD_ENB_MESSAGE_TYPE_RRC_CMD_READY,
                      LTE_FDD_ENB_DEST_LAYER_RRC,
                      (LTE_FDD_ENB_MESSAGE_UNION *)&cmd_ready,
                      sizeof(LTE_FDD_ENB_RRC_CMD_READY_MSG_STRUCT));
}
void LTE_fdd_enb_mme::send_attach_reject(LTE_fdd_enb_user *user,
                                         LTE_fdd_enb_rb   *rb)
{
    LTE_FDD_ENB_RRC_NAS_MSG_READY_MSG_STRUCT nas_msg_ready;
    LIBLTE_MME_ATTACH_REJECT_MSG_STRUCT      attach_rej;
    LIBLTE_BYTE_MSG_STRUCT                   msg;
    uint64                                   imsi_num;

    if(user->is_id_set())
    {
        imsi_num = user->get_id()->imsi;
    }else{
        imsi_num = user->get_temp_id();
    }

    attach_rej.emm_cause           = user->get_emm_cause();
    attach_rej.esm_msg_present     = false;
    attach_rej.t3446_value_present = false;
    liblte_mme_pack_attach_reject_msg(&attach_rej, &msg);
    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_MME,
                              __FILE__,
                              __LINE__,
                              &msg,
                              "Sending Attach Reject for IMSI=%015llu, RNTI=%u, RB=%s",
                              imsi_num,
                              user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[rb->get_rb_id()]);

    // Queue the NAS message for RRC
    rb->queue_rrc_nas_msg(&msg);

    // Signal RRC for NAS message
    nas_msg_ready.user = user;
    nas_msg_ready.rb   = rb;
    msgq_to_rrc->send(LTE_FDD_ENB_MESSAGE_TYPE_RRC_NAS_MSG_READY,
                      LTE_FDD_ENB_DEST_LAYER_RRC,
                      (LTE_FDD_ENB_MESSAGE_UNION *)&nas_msg_ready,
                      sizeof(LTE_FDD_ENB_RRC_NAS_MSG_READY_MSG_STRUCT));

    send_rrc_command(user, rb, LTE_FDD_ENB_RRC_CMD_RELEASE);
}
void LTE_fdd_enb_mme::send_authentication_reject(LTE_fdd_enb_user *user,
                                                 LTE_fdd_enb_rb   *rb)
{
    LTE_FDD_ENB_RRC_NAS_MSG_READY_MSG_STRUCT    nas_msg_ready;
    LIBLTE_MME_AUTHENTICATION_REJECT_MSG_STRUCT auth_rej;
    LIBLTE_BYTE_MSG_STRUCT                      msg;

    liblte_mme_pack_authentication_reject_msg(&auth_rej, &msg);
    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_MME,
                              __FILE__,
                              __LINE__,
                              &msg,
                              "Sending Authentication Reject for RNTI=%u, RB=%s",
                              user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[rb->get_rb_id()]);

    // Queue the NAS message for RRC
    rb->queue_rrc_nas_msg(&msg);

    // Signal RRC for NAS message
    nas_msg_ready.user = user;
    nas_msg_ready.rb   = rb;
    msgq_to_rrc->send(LTE_FDD_ENB_MESSAGE_TYPE_RRC_NAS_MSG_READY,
                      LTE_FDD_ENB_DEST_LAYER_RRC,
                      (LTE_FDD_ENB_MESSAGE_UNION *)&nas_msg_ready,
                      sizeof(LTE_FDD_ENB_RRC_NAS_MSG_READY_MSG_STRUCT));

    send_rrc_command(user, rb, LTE_FDD_ENB_RRC_CMD_RELEASE);
}
void LTE_fdd_enb_mme::send_authentication_request(LTE_fdd_enb_user *user,
                                                  LTE_fdd_enb_rb   *rb)
{
    LTE_fdd_enb_hss                              *hss      = LTE_fdd_enb_hss::get_instance();
    LTE_FDD_ENB_AUTHENTICATION_VECTOR_STRUCT     *auth_vec = NULL;
    LTE_FDD_ENB_RRC_NAS_MSG_READY_MSG_STRUCT      nas_msg_ready;
    LIBLTE_MME_AUTHENTICATION_REQUEST_MSG_STRUCT  auth_req;
    LIBLTE_BYTE_MSG_STRUCT                        msg;
    uint32                                        i;

    sem_wait(&sys_info_sem);
    hss->generate_security_data(user->get_id(), sys_info.mcc, sys_info.mnc);
    sem_post(&sys_info_sem);
    auth_vec = hss->get_auth_vec(user->get_id());
    if(NULL != auth_vec)
    {
        for(i=0; i<16; i++)
        {
            auth_req.autn[i] = auth_vec->autn[i];
            auth_req.rand[i] = auth_vec->rand[i];
        }
        auth_req.nas_ksi.tsc_flag = LIBLTE_MME_TYPE_OF_SECURITY_CONTEXT_FLAG_NATIVE;
        auth_req.nas_ksi.nas_ksi  = 0;
        liblte_mme_pack_authentication_request_msg(&auth_req, &msg);
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MME,
                                  __FILE__,
                                  __LINE__,
                                  &msg,
                                  "Sending Authentication Request for RNTI=%u, RB=%s",
                                  user->get_c_rnti(),
                                  LTE_fdd_enb_rb_text[rb->get_rb_id()]);

        // Queue the NAS message for RRC
        rb->queue_rrc_nas_msg(&msg);

        // Signal RRC
        nas_msg_ready.user = user;
        nas_msg_ready.rb   = rb;
        msgq_to_rrc->send(LTE_FDD_ENB_MESSAGE_TYPE_RRC_NAS_MSG_READY,
                          LTE_FDD_ENB_DEST_LAYER_RRC,
                          (LTE_FDD_ENB_MESSAGE_UNION *)&nas_msg_ready,
                          sizeof(LTE_FDD_ENB_RRC_NAS_MSG_READY_MSG_STRUCT));
    }
}
void LTE_fdd_enb_mme::send_detach_accept(LTE_fdd_enb_user *user,
                                         LTE_fdd_enb_rb   *rb)
{
    LTE_FDD_ENB_RRC_NAS_MSG_READY_MSG_STRUCT nas_msg_ready;
    LIBLTE_MME_DETACH_ACCEPT_MSG_STRUCT      detach_accept;
    LIBLTE_BYTE_MSG_STRUCT                   msg;

    if(user->is_auth_vec_set())
    {
        liblte_mme_pack_detach_accept_msg(&detach_accept,
                                          LIBLTE_MME_SECURITY_HDR_TYPE_INTEGRITY_AND_CIPHERED,
                                          user->get_auth_vec()->k_nas_int,
                                          user->get_auth_vec()->nas_count_dl,
                                          LIBLTE_SECURITY_DIRECTION_DOWNLINK,
                                          &msg);
        user->increment_nas_count_dl();
    }else{
        liblte_mme_pack_detach_accept_msg(&detach_accept,
                                          LIBLTE_MME_SECURITY_HDR_TYPE_PLAIN_NAS,
                                          NULL,
                                          0,
                                          0,
                                          &msg);
    }
    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_MME,
                              __FILE__,
                              __LINE__,
                              &msg,
                              "Sending Detach Accept for RNTI=%u, RB=%s",
                              user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[rb->get_rb_id()]);

    // Queue the NAS message for RRC
    rb->queue_rrc_nas_msg(&msg);

    // Signal RRC
    nas_msg_ready.user = user;
    nas_msg_ready.rb   = rb;
    msgq_to_rrc->send(LTE_FDD_ENB_MESSAGE_TYPE_RRC_NAS_MSG_READY,
                      LTE_FDD_ENB_DEST_LAYER_RRC,
                      (LTE_FDD_ENB_MESSAGE_UNION *)&nas_msg_ready,
                      sizeof(LTE_FDD_ENB_RRC_NAS_MSG_READY_MSG_STRUCT));
}
void LTE_fdd_enb_mme::send_emm_information(LTE_fdd_enb_user *user,
                                           LTE_fdd_enb_rb   *rb)
{
    LTE_FDD_ENB_RRC_NAS_MSG_READY_MSG_STRUCT  nas_msg_ready;
    LIBLTE_MME_EMM_INFORMATION_MSG_STRUCT     emm_info;
    LIBLTE_BYTE_MSG_STRUCT                    msg;
    struct tm                                *local_time;
    time_t                                    tmp_time;

    tmp_time   = time(NULL);
    local_time = localtime(&tmp_time);

    emm_info.full_net_name_present           = true;
    emm_info.full_net_name.name              = "openLTE";
    emm_info.full_net_name.add_ci            = LIBLTE_MME_ADD_CI_DONT_ADD;
    emm_info.short_net_name_present          = true;
    emm_info.short_net_name.name             = "oLTE";
    emm_info.short_net_name.add_ci           = LIBLTE_MME_ADD_CI_DONT_ADD;
    emm_info.local_time_zone_present         = false;
    emm_info.utc_and_local_time_zone_present = true;
    emm_info.utc_and_local_time_zone.year    = local_time->tm_year;
    emm_info.utc_and_local_time_zone.month   = local_time->tm_mon + 1;
    emm_info.utc_and_local_time_zone.day     = local_time->tm_mday;
    emm_info.utc_and_local_time_zone.hour    = local_time->tm_hour;
    emm_info.utc_and_local_time_zone.minute  = local_time->tm_min;
    emm_info.utc_and_local_time_zone.second  = local_time->tm_sec;
    emm_info.utc_and_local_time_zone.tz      = 0; // FIXME
    emm_info.net_dst_present                 = false;
    liblte_mme_pack_emm_information_msg(&emm_info,
                                        LIBLTE_MME_SECURITY_HDR_TYPE_INTEGRITY_AND_CIPHERED,
                                        user->get_auth_vec()->k_nas_int,
                                        user->get_auth_vec()->nas_count_dl,
                                        LIBLTE_SECURITY_DIRECTION_DOWNLINK,
                                        &msg);
    user->increment_nas_count_dl();
    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_MME,
                              __FILE__,
                              __LINE__,
                              &msg,
                              "Sending EMM Information for RNTI=%u, RB=%s",
                              user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[rb->get_rb_id()]);

    // Queue the NAS message for RRC
    rb->queue_rrc_nas_msg(&msg);

    // Signal RRC
    nas_msg_ready.user = user;
    nas_msg_ready.rb   = rb;
    msgq_to_rrc->send(LTE_FDD_ENB_MESSAGE_TYPE_RRC_NAS_MSG_READY,
                      LTE_FDD_ENB_DEST_LAYER_RRC,
                      (LTE_FDD_ENB_MESSAGE_UNION *)&nas_msg_ready,
                      sizeof(LTE_FDD_ENB_RRC_NAS_MSG_READY_MSG_STRUCT));
}
void LTE_fdd_enb_mme::send_identity_request(LTE_fdd_enb_user *user,
                                            LTE_fdd_enb_rb   *rb,
                                            uint8             id_type)
{
    LTE_FDD_ENB_RRC_NAS_MSG_READY_MSG_STRUCT nas_msg_ready;
    LIBLTE_MME_ID_REQUEST_MSG_STRUCT         id_req;
    LIBLTE_BYTE_MSG_STRUCT                   msg;

    id_req.id_type = id_type;
    liblte_mme_pack_identity_request_msg(&id_req, &msg);
    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_MME,
                              __FILE__,
                              __LINE__,
                              &msg,
                              "Sending ID Request for RNTI=%u, RB=%s",
                              user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[rb->get_rb_id()]);

    // Queue the NAS message for RRC
    rb->queue_rrc_nas_msg(&msg);

    // Signal RRC
    nas_msg_ready.user = user;
    nas_msg_ready.rb   = rb;
    msgq_to_rrc->send(LTE_FDD_ENB_MESSAGE_TYPE_RRC_NAS_MSG_READY,
                      LTE_FDD_ENB_DEST_LAYER_RRC,
                      (LTE_FDD_ENB_MESSAGE_UNION *)&nas_msg_ready,
                      sizeof(LTE_FDD_ENB_RRC_NAS_MSG_READY_MSG_STRUCT));
}
void LTE_fdd_enb_mme::send_security_mode_command(LTE_fdd_enb_user *user,
                                                 LTE_fdd_enb_rb   *rb)
{
    LTE_FDD_ENB_RRC_NAS_MSG_READY_MSG_STRUCT    nas_msg_ready;
    LIBLTE_MME_SECURITY_MODE_COMMAND_MSG_STRUCT sec_mode_cmd;
    LIBLTE_BYTE_MSG_STRUCT                      msg;
    uint32                                      i;

    sec_mode_cmd.selected_nas_sec_algs.type_of_eea = LIBLTE_MME_TYPE_OF_CIPHERING_ALGORITHM_EEA0;
    sec_mode_cmd.selected_nas_sec_algs.type_of_eia = LIBLTE_MME_TYPE_OF_INTEGRITY_ALGORITHM_128_EIA2;
    sec_mode_cmd.nas_ksi.tsc_flag                  = LIBLTE_MME_TYPE_OF_SECURITY_CONTEXT_FLAG_NATIVE;
    sec_mode_cmd.nas_ksi.nas_ksi                   = 0;
    for(i=0; i<8; i++)
    {
        sec_mode_cmd.ue_security_cap.eea[i] = user->get_eea_support(i);
        sec_mode_cmd.ue_security_cap.eia[i] = user->get_eia_support(i);
        sec_mode_cmd.ue_security_cap.uea[i] = user->get_uea_support(i);
        sec_mode_cmd.ue_security_cap.uia[i] = user->get_uia_support(i);
        sec_mode_cmd.ue_security_cap.gea[i] = user->get_gea_support(i);
    }
    if(user->is_uea_set())
    {
        sec_mode_cmd.ue_security_cap.uea_present = true;
    }else{
        sec_mode_cmd.ue_security_cap.uea_present = false;
    }
    if(user->is_uia_set())
    {
        sec_mode_cmd.ue_security_cap.uia_present = true;
    }else{
        sec_mode_cmd.ue_security_cap.uia_present = false;
    }
    if(user->is_gea_set())
    {
        sec_mode_cmd.ue_security_cap.gea_present = true;
    }else{
        sec_mode_cmd.ue_security_cap.gea_present = false;
    }
    sec_mode_cmd.imeisv_req         = LIBLTE_MME_IMEISV_REQUESTED;
    sec_mode_cmd.imeisv_req_present = true;
    sec_mode_cmd.nonce_ue_present   = false;
    sec_mode_cmd.nonce_mme_present  = false;
    liblte_mme_pack_security_mode_command_msg(&sec_mode_cmd,
                                              LIBLTE_MME_SECURITY_HDR_TYPE_INTEGRITY_WITH_NEW_EPS_SECURITY_CONTEXT,
                                              user->get_auth_vec()->k_nas_int,
                                              user->get_auth_vec()->nas_count_dl,
                                              LIBLTE_SECURITY_DIRECTION_DOWNLINK,
                                              &msg);
    user->increment_nas_count_dl();
    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_MME,
                              __FILE__,
                              __LINE__,
                              &msg,
                              "Sending Security Mode Command for RNTI=%u, RB=%s",
                              user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[rb->get_rb_id()]);

    // Queue the message for RRC
    rb->queue_rrc_nas_msg(&msg);

    // Signal RRC
    nas_msg_ready.user = user;
    nas_msg_ready.rb   = rb;
    msgq_to_rrc->send(LTE_FDD_ENB_MESSAGE_TYPE_RRC_NAS_MSG_READY,
                      LTE_FDD_ENB_DEST_LAYER_RRC,
                      (LTE_FDD_ENB_MESSAGE_UNION *)&nas_msg_ready,
                      sizeof(LTE_FDD_ENB_RRC_NAS_MSG_READY_MSG_STRUCT));
}
void LTE_fdd_enb_mme::send_service_reject(LTE_fdd_enb_user *user,
                                          LTE_fdd_enb_rb   *rb,
                                          uint8             cause)
{
    LTE_FDD_ENB_RRC_NAS_MSG_READY_MSG_STRUCT nas_msg_ready;
    LIBLTE_MME_SERVICE_REJECT_MSG_STRUCT     service_rej;
    LIBLTE_BYTE_MSG_STRUCT                   msg;

    service_rej.emm_cause     = cause;
    service_rej.t3442_present = false;
    service_rej.t3446_present = false;
    liblte_mme_pack_service_reject_msg(&service_rej,
                                       LIBLTE_MME_SECURITY_HDR_TYPE_PLAIN_NAS,
                                       NULL,
                                       0,
                                       0,
                                       &msg);
    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_MME,
                              __FILE__,
                              __LINE__,
                              &msg,
                              "Sending Service Reject for RNTI=%u, RB=%s",
                              user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[rb->get_rb_id()]);

    // Queue the NAS message for RRC
    rb->queue_rrc_nas_msg(&msg);

    nas_msg_ready.user = user;
    nas_msg_ready.rb   = rb;
    msgq_to_rrc->send(LTE_FDD_ENB_MESSAGE_TYPE_RRC_NAS_MSG_READY,
                      LTE_FDD_ENB_DEST_LAYER_RRC,
                      (LTE_FDD_ENB_MESSAGE_UNION *)&nas_msg_ready,
                      sizeof(LTE_FDD_ENB_RRC_NAS_MSG_READY_MSG_STRUCT));
}
void LTE_fdd_enb_mme::send_activate_dedicated_eps_bearer_context_request(LTE_fdd_enb_user *user,
                                                                         LTE_fdd_enb_rb   *rb)
{
    LTE_FDD_ENB_RRC_CMD_READY_MSG_STRUCT                                cmd_ready;
    LIBLTE_MME_ACTIVATE_DEDICATED_EPS_BEARER_CONTEXT_REQUEST_MSG_STRUCT act_ded_eps_bearer_context_req;
    LIBLTE_BYTE_MSG_STRUCT                                              msg;
    LIBLTE_BYTE_MSG_STRUCT                                              sec_msg;

    if(0 == user->get_eps_bearer_id())
    {
        act_ded_eps_bearer_context_req.eps_bearer_id = 6;
        user->set_eps_bearer_id(5);
    }else{
        act_ded_eps_bearer_context_req.eps_bearer_id = user->get_eps_bearer_id()+1;
    }
    act_ded_eps_bearer_context_req.proc_transaction_id                       = 0;
    act_ded_eps_bearer_context_req.linked_eps_bearer_id                      = user->get_eps_bearer_id();
    act_ded_eps_bearer_context_req.eps_qos.qci                               = 9;
    act_ded_eps_bearer_context_req.eps_qos.br_present                        = false;
    act_ded_eps_bearer_context_req.eps_qos.br_ext_present                    = false;
    act_ded_eps_bearer_context_req.tft.tft_op_code                           = LIBLTE_MME_TFT_OPERATION_CODE_CREATE_NEW_TFT;
    act_ded_eps_bearer_context_req.tft.parameter_list_size                   = 0;
    act_ded_eps_bearer_context_req.tft.packet_filter_list_size               = 3;
    act_ded_eps_bearer_context_req.tft.packet_filter_list[0].id              = 1;
    act_ded_eps_bearer_context_req.tft.packet_filter_list[0].dir             = LIBLTE_MME_TFT_PACKET_FILTER_DIRECTION_BIDIRECTIONAL;
    act_ded_eps_bearer_context_req.tft.packet_filter_list[0].eval_precedence = 1;
    act_ded_eps_bearer_context_req.tft.packet_filter_list[0].filter_size     = 2;
    act_ded_eps_bearer_context_req.tft.packet_filter_list[0].filter[0]       = LIBLTE_MME_TFT_PACKET_FILTER_COMPONENT_TYPE_ID_PROTOCOL_ID_NEXT_HEADER_TYPE;
    act_ded_eps_bearer_context_req.tft.packet_filter_list[0].filter[1]       = IPPROTO_UDP;
    act_ded_eps_bearer_context_req.tft.packet_filter_list[1].id              = 2;
    act_ded_eps_bearer_context_req.tft.packet_filter_list[1].dir             = LIBLTE_MME_TFT_PACKET_FILTER_DIRECTION_BIDIRECTIONAL;
    act_ded_eps_bearer_context_req.tft.packet_filter_list[1].eval_precedence = 2;
    act_ded_eps_bearer_context_req.tft.packet_filter_list[1].filter_size     = 2;
    act_ded_eps_bearer_context_req.tft.packet_filter_list[1].filter[0]       = LIBLTE_MME_TFT_PACKET_FILTER_COMPONENT_TYPE_ID_PROTOCOL_ID_NEXT_HEADER_TYPE;
    act_ded_eps_bearer_context_req.tft.packet_filter_list[1].filter[1]       = IPPROTO_TCP;
    act_ded_eps_bearer_context_req.tft.packet_filter_list[2].id              = 3;
    act_ded_eps_bearer_context_req.tft.packet_filter_list[2].dir             = LIBLTE_MME_TFT_PACKET_FILTER_DIRECTION_BIDIRECTIONAL;
    act_ded_eps_bearer_context_req.tft.packet_filter_list[2].eval_precedence = 3;
    act_ded_eps_bearer_context_req.tft.packet_filter_list[2].filter_size     = 2;
    act_ded_eps_bearer_context_req.tft.packet_filter_list[2].filter[0]       = LIBLTE_MME_TFT_PACKET_FILTER_COMPONENT_TYPE_ID_PROTOCOL_ID_NEXT_HEADER_TYPE;
    act_ded_eps_bearer_context_req.tft.packet_filter_list[2].filter[1]       = IPPROTO_ICMP;
    act_ded_eps_bearer_context_req.transaction_id_present                    = false;
    act_ded_eps_bearer_context_req.negotiated_qos_present                    = false;
    act_ded_eps_bearer_context_req.llc_sapi_present                          = false;
    act_ded_eps_bearer_context_req.radio_prio_present                        = false;
    act_ded_eps_bearer_context_req.packet_flow_id_present                    = false;
    act_ded_eps_bearer_context_req.protocol_cnfg_opts_present                = false;
    liblte_mme_pack_activate_dedicated_eps_bearer_context_request_msg(&act_ded_eps_bearer_context_req,
                                                                      &msg);
    liblte_mme_pack_security_protected_nas_msg(&msg,
                                               LIBLTE_MME_SECURITY_HDR_TYPE_INTEGRITY_AND_CIPHERED,
                                               user->get_auth_vec()->k_nas_int,
                                               user->get_auth_vec()->nas_count_dl,
                                               LIBLTE_SECURITY_DIRECTION_DOWNLINK,
                                               &sec_msg);
    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_MME,
                              __FILE__,
                              __LINE__,
                              &sec_msg,
                              "Sending Activate Dedicated EPS Bearer Context Request for RNTI=%u, RB=%s",
                              user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[rb->get_rb_id()]);

    // Queue the NAS message for RRC
    rb->queue_rrc_nas_msg(&sec_msg);

    // Signal RRC for NAS message
    cmd_ready.user = user;
    cmd_ready.rb   = rb;
    cmd_ready.cmd  = LTE_FDD_ENB_RRC_CMD_SETUP_DED_DRB;
    msgq_to_rrc->send(LTE_FDD_ENB_MESSAGE_TYPE_RRC_CMD_READY,
                      LTE_FDD_ENB_DEST_LAYER_RRC,
                      (LTE_FDD_ENB_MESSAGE_UNION *)&cmd_ready,
                      sizeof(LTE_FDD_ENB_RRC_CMD_READY_MSG_STRUCT));
}
void LTE_fdd_enb_mme::send_esm_information_request(LTE_fdd_enb_user *user,
                                                   LTE_fdd_enb_rb   *rb)
{
    LTE_FDD_ENB_RRC_NAS_MSG_READY_MSG_STRUCT      nas_msg_ready;
    LIBLTE_MME_ESM_INFORMATION_REQUEST_MSG_STRUCT esm_info_req;
    LIBLTE_BYTE_MSG_STRUCT                        msg;
    LIBLTE_BYTE_MSG_STRUCT                        sec_msg;

    esm_info_req.eps_bearer_id = 0;
    if(0 == user->get_proc_transaction_id())
    {
        esm_info_req.proc_transaction_id = 1;
        user->set_proc_transaction_id(1);
    }else{
        esm_info_req.proc_transaction_id = user->get_proc_transaction_id();
    }
    liblte_mme_pack_esm_information_request_msg(&esm_info_req, &msg);
    liblte_mme_pack_security_protected_nas_msg(&msg,
                                               LIBLTE_MME_SECURITY_HDR_TYPE_INTEGRITY_AND_CIPHERED,
                                               user->get_auth_vec()->k_nas_int,
                                               user->get_auth_vec()->nas_count_dl,
                                               LIBLTE_SECURITY_DIRECTION_DOWNLINK,
                                               &sec_msg);
    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_MME,
                              __FILE__,
                              __LINE__,
                              &sec_msg,
                              "Sending ESM Info Request for RNTI=%u, RB=%s",
                              user->get_c_rnti(),
                              LTE_fdd_enb_rb_text[rb->get_rb_id()]);

    // Queue the NAS message for RRC
    rb->queue_rrc_nas_msg(&sec_msg);

    // Signal RRC
    nas_msg_ready.user = user;
    nas_msg_ready.rb   = rb;
    msgq_to_rrc->send(LTE_FDD_ENB_MESSAGE_TYPE_RRC_NAS_MSG_READY,
                      LTE_FDD_ENB_DEST_LAYER_RRC,
                      (LTE_FDD_ENB_MESSAGE_UNION *)&nas_msg_ready,
                      sizeof(LTE_FDD_ENB_RRC_NAS_MSG_READY_MSG_STRUCT));
}
void LTE_fdd_enb_mme::send_rrc_command(LTE_fdd_enb_user         *user,
                                       LTE_fdd_enb_rb           *rb,
                                       LTE_FDD_ENB_RRC_CMD_ENUM  cmd)
{
    LTE_FDD_ENB_RRC_CMD_READY_MSG_STRUCT cmd_ready;

    // Signal RRC for command
    cmd_ready.user = user;
    cmd_ready.rb   = rb;
    cmd_ready.cmd  = cmd;
    msgq_to_rrc->send(LTE_FDD_ENB_MESSAGE_TYPE_RRC_CMD_READY,
                      LTE_FDD_ENB_DEST_LAYER_RRC,
                      (LTE_FDD_ENB_MESSAGE_UNION *)&cmd_ready,
                      sizeof(LTE_FDD_ENB_RRC_CMD_READY_MSG_STRUCT));
}

/*****************/
/*    Helpers    */
/*****************/
uint32 LTE_fdd_enb_mme::get_next_ip_addr(void)
{
    uint32 ip_addr = next_ip_addr;

    next_ip_addr++;
    if((next_ip_addr & 0xFF) == 0xFF)
    {
        next_ip_addr++;
    }

    return(ip_addr);
}
