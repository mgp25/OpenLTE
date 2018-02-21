#line 2 "LTE_fdd_enb_rb.cc" // Make __FILE__ omit the path
/*******************************************************************************

    Copyright 2014-2017 Ben Wojtowicz

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

    File: LTE_fdd_enb_rb.cc

    Description: Contains all the implementations for the LTE FDD eNodeB
                 radio bearer class.

    Revision History
    ----------    -------------    --------------------------------------------
    05/04/2014    Ben Wojtowicz    Created file
    06/15/2014    Ben Wojtowicz    Added more states and procedures, QoS, MME,
                                   RLC, and uplink scheduling functionality.
    08/03/2014    Ben Wojtowicz    Added MME procedures/states, RRC NAS support,
                                   RRC transaction id, PDCP sequence numbers,
                                   and RLC transmit variables.
    09/03/2014    Ben Wojtowicz    Added ability to store the contetion
                                   resolution identity and fixed an issue with
                                   t_poll_retransmit.
    11/01/2014    Ben Wojtowicz    Added SRB2 support and PDCP security.
    11/29/2014    Ben Wojtowicz    Added more DRB support, moved almost
                                   everything to byte messages structs, added
                                   IP gateway and RLC UMD support.
    12/16/2014    Ben Wojtowicz    Added QoS for default data services.
    12/24/2014    Ben Wojtowicz    Added asymmetric QoS support and fixed a
                                   UMD reassembly bug.
    02/15/2015    Ben Wojtowicz    Split UL/DL QoS TTI frequency, added reset
                                   user support, and added multiple UMD RLC data
                                   support.
    07/25/2015    Ben Wojtowicz    Moved QoS structure to the user class, fixed
                                   RLC AM TX and RX buffers, and moved DRBs to
                                   RLC AM.
    12/06/2015    Ben Wojtowicz    Changed boost::mutex to pthread_mutex_t and
                                   sem_t and fixed the updating of VT(A) in the
                                   retransmission buffer.
    12/18/2016    Ben Wojtowicz    Properly handling multiple RLC AMD PDUs.
    07/29/2017    Ben Wojtowicz    Remove last TTI storage and refactored AMD
                                   reception buffer handling to support more
                                   than one PDU with the same SN.

*******************************************************************************/

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_fdd_enb_rb.h"
#include "LTE_fdd_enb_timer_mgr.h"
#include "LTE_fdd_enb_user.h"
#include "LTE_fdd_enb_rlc.h"
#include "LTE_fdd_enb_mac.h"
#include "libtools_scoped_lock.h"
#include "libtools_helpers.h"

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
LTE_fdd_enb_rb::LTE_fdd_enb_rb(LTE_FDD_ENB_RB_ENUM  _rb,
                               LTE_fdd_enb_user    *_user)
{
    rb   = _rb;
    user = _user;

    t_poll_retransmit_timer_id = LTE_FDD_ENB_INVALID_TIMER_ID;

    if(LTE_FDD_ENB_RB_SRB0 == rb)
    {
        mme_procedure = LTE_FDD_ENB_MME_PROC_IDLE;
        mme_state     = LTE_FDD_ENB_MME_STATE_IDLE;
        rrc_procedure = LTE_FDD_ENB_RRC_PROC_IDLE;
        rrc_state     = LTE_FDD_ENB_RRC_STATE_IDLE;
        pdcp_config   = LTE_FDD_ENB_PDCP_CONFIG_N_A;
        rlc_config    = LTE_FDD_ENB_RLC_CONFIG_TM;
        mac_config    = LTE_FDD_ENB_MAC_CONFIG_TM;
    }else if(LTE_FDD_ENB_RB_SRB1 == rb){
        mme_procedure = LTE_FDD_ENB_MME_PROC_IDLE;
        mme_state     = LTE_FDD_ENB_MME_STATE_IDLE;
        rrc_procedure = LTE_FDD_ENB_RRC_PROC_IDLE;
        rrc_state     = LTE_FDD_ENB_RRC_STATE_IDLE;
        pdcp_config   = LTE_FDD_ENB_PDCP_CONFIG_N_A;
        rlc_config    = LTE_FDD_ENB_RLC_CONFIG_AM;
        mac_config    = LTE_FDD_ENB_MAC_CONFIG_TM;
    }else if(LTE_FDD_ENB_RB_SRB2 == rb){
        mme_procedure = LTE_FDD_ENB_MME_PROC_IDLE;
        mme_state     = LTE_FDD_ENB_MME_STATE_IDLE;
        rrc_procedure = LTE_FDD_ENB_RRC_PROC_IDLE;
        rrc_state     = LTE_FDD_ENB_RRC_STATE_IDLE;
        pdcp_config   = LTE_FDD_ENB_PDCP_CONFIG_N_A;
        rlc_config    = LTE_FDD_ENB_RLC_CONFIG_AM;
        mac_config    = LTE_FDD_ENB_MAC_CONFIG_TM;
    }else if(LTE_FDD_ENB_RB_DRB1 == rb){
        pdcp_config = LTE_FDD_ENB_PDCP_CONFIG_LONG_SN;
        rlc_config  = LTE_FDD_ENB_RLC_CONFIG_AM;
        mac_config  = LTE_FDD_ENB_MAC_CONFIG_TM;
    }else if(LTE_FDD_ENB_RB_DRB2 == rb){
        pdcp_config = LTE_FDD_ENB_PDCP_CONFIG_LONG_SN;
        rlc_config  = LTE_FDD_ENB_RLC_CONFIG_AM;
        mac_config  = LTE_FDD_ENB_MAC_CONFIG_TM;
    }

    // GW
    sem_init(&gw_data_msg_queue_sem, 0, 1);

    // MME
    sem_init(&mme_nas_msg_queue_sem, 0, 1);

    // RRC
    sem_init(&rrc_pdu_queue_sem, 0, 1);
    sem_init(&rrc_nas_msg_queue_sem, 0, 1);
    rrc_transaction_id = 0;

    // PDCP
    sem_init(&pdcp_pdu_queue_sem, 0, 1);
    sem_init(&pdcp_sdu_queue_sem, 0, 1);
    sem_init(&pdcp_data_sdu_queue_sem, 0, 1);
    pdcp_rx_count = 0;
    pdcp_tx_count = 0;

    // RLC
    sem_init(&rlc_pdu_queue_sem, 0, 1);
    sem_init(&rlc_sdu_queue_sem, 0, 1);
    rlc_am_reception_buffer.clear();
    rlc_am_transmission_buffer.clear();
    rlc_um_reception_buffer.clear();
    t_poll_retransmit_timer_id = LTE_FDD_ENB_INVALID_TIMER_ID;
    rlc_vrr                    = 0;
    rlc_vrmr                   = rlc_vrr + LIBLTE_RLC_AM_WINDOW_SIZE;
    rlc_vrh                    = 0;
    rlc_vta                    = 0;
    rlc_vtms                   = rlc_vta + LIBLTE_RLC_AM_WINDOW_SIZE;
    rlc_vts                    = 0;
    rlc_vruh                   = 0;
    rlc_vrur                   = 0;
    rlc_um_window_size         = 512;
    rlc_first_um_segment_sn    = 0xFFFF;
    rlc_last_um_segment_sn     = 0xFFFF;
    rlc_vtus                   = 0;

    // MAC
    sem_init(&mac_sdu_queue_sem, 0, 1);
    mac_con_res_id = 0;
}
LTE_fdd_enb_rb::~LTE_fdd_enb_rb()
{
    LTE_fdd_enb_timer_mgr                                          *timer_mgr = LTE_fdd_enb_timer_mgr::get_instance();
    std::list<LIBLTE_BYTE_MSG_STRUCT *>::iterator                   byte_iter;
    std::list<LIBLTE_BIT_MSG_STRUCT *>::iterator                    bit_iter;
    std::map<uint16, LIBLTE_RLC_SINGLE_AMD_PDU_STRUCT *>::iterator  rlc_am_iter;
    std::map<uint16, LIBLTE_BYTE_MSG_STRUCT *>::iterator            rlc_um_iter;

    // MAC
    sem_wait(&mac_sdu_queue_sem);
    for(byte_iter=mac_sdu_queue.begin(); byte_iter!=mac_sdu_queue.end(); byte_iter++)
    {
        delete (*byte_iter);
    }
    sem_destroy(&mac_sdu_queue_sem);

    // RLC
    sem_wait(&rlc_pdu_queue_sem);
    for(byte_iter=rlc_pdu_queue.begin(); byte_iter!=rlc_pdu_queue.end(); byte_iter++)
    {
        delete (*byte_iter);
    }
    sem_destroy(&rlc_pdu_queue_sem);
    sem_wait(&rlc_sdu_queue_sem);
    for(byte_iter=rlc_sdu_queue.begin(); byte_iter!=rlc_sdu_queue.end(); byte_iter++)
    {
        delete (*byte_iter);
    }
    sem_destroy(&rlc_sdu_queue_sem);
    for(rlc_am_iter=rlc_am_reception_buffer.begin(); rlc_am_iter!=rlc_am_reception_buffer.end(); rlc_am_iter++)
    {
        delete (*rlc_am_iter).second;
    }
    for(rlc_am_iter=rlc_am_transmission_buffer.begin(); rlc_am_iter!=rlc_am_transmission_buffer.end(); rlc_am_iter++)
    {
        delete (*rlc_am_iter).second;
    }
    for(rlc_um_iter=rlc_um_reception_buffer.begin(); rlc_um_iter!=rlc_um_reception_buffer.end(); rlc_um_iter++)
    {
        delete (*rlc_um_iter).second;
    }
    if(LTE_FDD_ENB_INVALID_TIMER_ID != t_poll_retransmit_timer_id)
    {
        timer_mgr->stop_timer(t_poll_retransmit_timer_id);
    }

    // PDCP
    sem_wait(&pdcp_pdu_queue_sem);
    for(byte_iter=pdcp_pdu_queue.begin(); byte_iter!=pdcp_pdu_queue.end(); byte_iter++)
    {
        delete (*byte_iter);
    }
    sem_destroy(&pdcp_pdu_queue_sem);
    sem_wait(&pdcp_sdu_queue_sem);
    for(bit_iter=pdcp_sdu_queue.begin(); bit_iter!=pdcp_sdu_queue.end(); bit_iter++)
    {
        delete (*bit_iter);
    }
    sem_destroy(&pdcp_sdu_queue_sem);
    sem_wait(&pdcp_data_sdu_queue_sem);
    for(byte_iter=pdcp_data_sdu_queue.begin(); byte_iter!=pdcp_data_sdu_queue.end(); byte_iter++)
    {
        delete (*byte_iter);
    }
    sem_destroy(&pdcp_data_sdu_queue_sem);

    // RRC
    sem_wait(&rrc_pdu_queue_sem);
    for(bit_iter=rrc_pdu_queue.begin(); bit_iter!=rrc_pdu_queue.end(); bit_iter++)
    {
        delete (*bit_iter);
    }
    sem_destroy(&rrc_pdu_queue_sem);
    sem_wait(&rrc_nas_msg_queue_sem);
    for(byte_iter=rrc_nas_msg_queue.begin(); byte_iter!=rrc_nas_msg_queue.end(); byte_iter++)
    {
        delete (*byte_iter);
    }
    sem_destroy(&rrc_nas_msg_queue_sem);

    // MME
    sem_wait(&mme_nas_msg_queue_sem);
    for(byte_iter=mme_nas_msg_queue.begin(); byte_iter!=mme_nas_msg_queue.end(); byte_iter++)
    {
        delete (*byte_iter);
    }
    sem_destroy(&mme_nas_msg_queue_sem);

    // GW
    sem_wait(&gw_data_msg_queue_sem);
    for(byte_iter=gw_data_msg_queue.begin(); byte_iter!=gw_data_msg_queue.end(); byte_iter++)
    {
        delete (*byte_iter);
    }
    sem_destroy(&gw_data_msg_queue_sem);
}

/******************/
/*    Identity    */
/******************/
LTE_FDD_ENB_RB_ENUM LTE_fdd_enb_rb::get_rb_id(void)
{
    return(rb);
}
void LTE_fdd_enb_rb::reset_user(LTE_fdd_enb_user *_user)
{
    user = _user;
}

/************/
/*    GW    */
/************/
void LTE_fdd_enb_rb::queue_gw_data_msg(LIBLTE_BYTE_MSG_STRUCT *gw_data)
{
    queue_msg(gw_data, &gw_data_msg_queue_sem, &gw_data_msg_queue);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_rb::get_next_gw_data_msg(LIBLTE_BYTE_MSG_STRUCT **gw_data)
{
    return(get_next_msg(&gw_data_msg_queue_sem, &gw_data_msg_queue, gw_data));
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_rb::delete_next_gw_data_msg(void)
{
    return(delete_next_msg(&gw_data_msg_queue_sem, &gw_data_msg_queue));
}

/*************/
/*    MME    */
/*************/
void LTE_fdd_enb_rb::queue_mme_nas_msg(LIBLTE_BYTE_MSG_STRUCT *nas_msg)
{
    queue_msg(nas_msg, &mme_nas_msg_queue_sem, &mme_nas_msg_queue);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_rb::get_next_mme_nas_msg(LIBLTE_BYTE_MSG_STRUCT **nas_msg)
{
    return(get_next_msg(&mme_nas_msg_queue_sem, &mme_nas_msg_queue, nas_msg));
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_rb::delete_next_mme_nas_msg(void)
{
    return(delete_next_msg(&mme_nas_msg_queue_sem, &mme_nas_msg_queue));
}
void LTE_fdd_enb_rb::set_mme_procedure(LTE_FDD_ENB_MME_PROC_ENUM procedure)
{
    LTE_fdd_enb_interface *interface = LTE_fdd_enb_interface::get_instance();

    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_RB,
                              __FILE__,
                              __LINE__,
                              "%s MME procedure moving from %s to %s for RNTI=%u",
                              LTE_fdd_enb_rb_text[rb],
                              LTE_fdd_enb_mme_proc_text[mme_procedure],
                              LTE_fdd_enb_mme_proc_text[procedure],
                              user->get_c_rnti());

    mme_procedure = procedure;
}
LTE_FDD_ENB_MME_PROC_ENUM LTE_fdd_enb_rb::get_mme_procedure(void)
{
    return(mme_procedure);
}
void LTE_fdd_enb_rb::set_mme_state(LTE_FDD_ENB_MME_STATE_ENUM state)
{
    LTE_fdd_enb_interface *interface = LTE_fdd_enb_interface::get_instance();

    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_RB,
                              __FILE__,
                              __LINE__,
                              "%s MME state moving from %s to %s for RNTI=%u",
                              LTE_fdd_enb_rb_text[rb],
                              LTE_fdd_enb_mme_state_text[mme_state],
                              LTE_fdd_enb_mme_state_text[state],
                              user->get_c_rnti());

    mme_state = state;
}
LTE_FDD_ENB_MME_STATE_ENUM LTE_fdd_enb_rb::get_mme_state(void)
{
    return(mme_state);
}

/*************/
/*    RRC    */
/*************/
void LTE_fdd_enb_rb::queue_rrc_pdu(LIBLTE_BIT_MSG_STRUCT *pdu)
{
    queue_msg(pdu, &rrc_pdu_queue_sem, &rrc_pdu_queue);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_rb::get_next_rrc_pdu(LIBLTE_BIT_MSG_STRUCT **pdu)
{
    return(get_next_msg(&rrc_pdu_queue_sem, &rrc_pdu_queue, pdu));
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_rb::delete_next_rrc_pdu(void)
{
    return(delete_next_msg(&rrc_pdu_queue_sem, &rrc_pdu_queue));
}
void LTE_fdd_enb_rb::queue_rrc_nas_msg(LIBLTE_BYTE_MSG_STRUCT *nas_msg)
{
    queue_msg(nas_msg, &rrc_nas_msg_queue_sem, &rrc_nas_msg_queue);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_rb::get_next_rrc_nas_msg(LIBLTE_BYTE_MSG_STRUCT **nas_msg)
{
    return(get_next_msg(&rrc_nas_msg_queue_sem, &rrc_nas_msg_queue, nas_msg));
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_rb::delete_next_rrc_nas_msg(void)
{
    return(delete_next_msg(&rrc_nas_msg_queue_sem, &rrc_nas_msg_queue));
}
void LTE_fdd_enb_rb::set_rrc_procedure(LTE_FDD_ENB_RRC_PROC_ENUM procedure)
{
    LTE_fdd_enb_interface *interface = LTE_fdd_enb_interface::get_instance();

    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_RB,
                              __FILE__,
                              __LINE__,
                              "%s RRC procedure moving from %s to %s for RNTI=%u",
                              LTE_fdd_enb_rb_text[rb],
                              LTE_fdd_enb_rrc_proc_text[rrc_procedure],
                              LTE_fdd_enb_rrc_proc_text[procedure],
                              user->get_c_rnti());
    rrc_procedure = procedure;
}
LTE_FDD_ENB_RRC_PROC_ENUM LTE_fdd_enb_rb::get_rrc_procedure(void)
{
    return(rrc_procedure);
}
void LTE_fdd_enb_rb::set_rrc_state(LTE_FDD_ENB_RRC_STATE_ENUM state)
{
    LTE_fdd_enb_interface *interface = LTE_fdd_enb_interface::get_instance();

    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_RB,
                              __FILE__,
                              __LINE__,
                              "%s RRC state moving from %s to %s for RNTI=%u",
                              LTE_fdd_enb_rb_text[rb],
                              LTE_fdd_enb_rrc_state_text[rrc_state],
                              LTE_fdd_enb_rrc_state_text[state],
                              user->get_c_rnti());
    rrc_state = state;
}
LTE_FDD_ENB_RRC_STATE_ENUM LTE_fdd_enb_rb::get_rrc_state(void)
{
    return(rrc_state);
}
uint8 LTE_fdd_enb_rb::get_rrc_transaction_id(void)
{
    return(rrc_transaction_id);
}
void LTE_fdd_enb_rb::set_rrc_transaction_id(uint8 transaction_id)
{
    rrc_transaction_id = transaction_id;
}

/**************/
/*    PDCP    */
/**************/
void LTE_fdd_enb_rb::queue_pdcp_pdu(LIBLTE_BYTE_MSG_STRUCT *pdu)
{
    queue_msg(pdu, &pdcp_pdu_queue_sem, &pdcp_pdu_queue);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_rb::get_next_pdcp_pdu(LIBLTE_BYTE_MSG_STRUCT **pdu)
{
    return(get_next_msg(&pdcp_pdu_queue_sem, &pdcp_pdu_queue, pdu));
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_rb::delete_next_pdcp_pdu(void)
{
    return(delete_next_msg(&pdcp_pdu_queue_sem, &pdcp_pdu_queue));
}
void LTE_fdd_enb_rb::queue_pdcp_sdu(LIBLTE_BIT_MSG_STRUCT *sdu)
{
    queue_msg(sdu, &pdcp_sdu_queue_sem, &pdcp_sdu_queue);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_rb::get_next_pdcp_sdu(LIBLTE_BIT_MSG_STRUCT **sdu)
{
    return(get_next_msg(&pdcp_sdu_queue_sem, &pdcp_sdu_queue, sdu));
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_rb::delete_next_pdcp_sdu(void)
{
    return(delete_next_msg(&pdcp_sdu_queue_sem, &pdcp_sdu_queue));
}
void LTE_fdd_enb_rb::queue_pdcp_data_sdu(LIBLTE_BYTE_MSG_STRUCT *sdu)
{
    queue_msg(sdu, &pdcp_data_sdu_queue_sem, &pdcp_data_sdu_queue);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_rb::get_next_pdcp_data_sdu(LIBLTE_BYTE_MSG_STRUCT **sdu)
{
    return(get_next_msg(&pdcp_data_sdu_queue_sem, &pdcp_data_sdu_queue, sdu));
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_rb::delete_next_pdcp_data_sdu(void)
{
    return(delete_next_msg(&pdcp_data_sdu_queue_sem, &pdcp_data_sdu_queue));
}
void LTE_fdd_enb_rb::set_pdcp_config(LTE_FDD_ENB_PDCP_CONFIG_ENUM config)
{
    pdcp_config = config;
}
LTE_FDD_ENB_PDCP_CONFIG_ENUM LTE_fdd_enb_rb::get_pdcp_config(void)
{
    return(pdcp_config);
}
uint32 LTE_fdd_enb_rb::get_pdcp_rx_count(void)
{
    return(pdcp_rx_count);
}
void LTE_fdd_enb_rb::set_pdcp_rx_count(uint32 rx_count)
{
    pdcp_rx_count = rx_count;
}
uint32 LTE_fdd_enb_rb::get_pdcp_tx_count(void)
{
    return(pdcp_tx_count);
}
void LTE_fdd_enb_rb::set_pdcp_tx_count(uint32 tx_count)
{
    pdcp_tx_count = tx_count;
}

/*************/
/*    RLC    */
/*************/
void LTE_fdd_enb_rb::queue_rlc_pdu(LIBLTE_BYTE_MSG_STRUCT *pdu)
{
    queue_msg(pdu, &rlc_pdu_queue_sem, &rlc_pdu_queue);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_rb::get_next_rlc_pdu(LIBLTE_BYTE_MSG_STRUCT **pdu)
{
    return(get_next_msg(&rlc_pdu_queue_sem, &rlc_pdu_queue, pdu));
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_rb::delete_next_rlc_pdu(void)
{
    return(delete_next_msg(&rlc_pdu_queue_sem, &rlc_pdu_queue));
}
void LTE_fdd_enb_rb::queue_rlc_sdu(LIBLTE_BYTE_MSG_STRUCT *sdu)
{
    queue_msg(sdu, &rlc_sdu_queue_sem, &rlc_sdu_queue);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_rb::get_next_rlc_sdu(LIBLTE_BYTE_MSG_STRUCT **sdu)
{
    return(get_next_msg(&rlc_sdu_queue_sem, &rlc_sdu_queue, sdu));
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_rb::delete_next_rlc_sdu(void)
{
    return(delete_next_msg(&rlc_sdu_queue_sem, &rlc_sdu_queue));
}
LTE_FDD_ENB_RLC_CONFIG_ENUM LTE_fdd_enb_rb::get_rlc_config(void)
{
    return(rlc_config);
}
uint16 LTE_fdd_enb_rb::get_rlc_vrr(void)
{
    return(rlc_vrr);
}
void LTE_fdd_enb_rb::set_rlc_vrr(uint16 vrr)
{
    rlc_vrr  = vrr;
    rlc_vrmr = rlc_vrr + LIBLTE_RLC_AM_WINDOW_SIZE;
}
void LTE_fdd_enb_rb::update_rlc_vrr(void)
{
    std::map<uint16, LIBLTE_RLC_SINGLE_AMD_PDU_STRUCT *>::iterator iter;
    uint32                                                         i;
    uint16                                                         vrr = rlc_vrr;

    for(i=vrr; i<rlc_vrh; i++)
    {
        iter = rlc_am_reception_buffer.find(i * 10);
        if(rlc_am_reception_buffer.end() != iter)
        {
            rlc_vrr = i+1;
        }else{
            iter = rlc_am_reception_buffer.find(i * 10 + 1);
            if(rlc_am_reception_buffer.end() != iter)
            {
                rlc_vrr = i+1;
            }else{
                break;
            }
        }
    }
}
uint16 LTE_fdd_enb_rb::get_rlc_vrmr(void)
{
    return(rlc_vrmr);
}
uint16 LTE_fdd_enb_rb::get_rlc_vrh(void)
{
    return(rlc_vrh);
}
void LTE_fdd_enb_rb::set_rlc_vrh(uint16 vrh)
{
    rlc_vrh = vrh;
}
void LTE_fdd_enb_rb::rlc_add_to_am_reception_buffer(LIBLTE_RLC_SINGLE_AMD_PDU_STRUCT *amd_pdu)
{
    std::map<uint16, LIBLTE_RLC_SINGLE_AMD_PDU_STRUCT *>::iterator  iter;
    LIBLTE_RLC_SINGLE_AMD_PDU_STRUCT                               *new_pdu = NULL;

    new_pdu = new LIBLTE_RLC_SINGLE_AMD_PDU_STRUCT;

    if(NULL != new_pdu)
    {
        iter = rlc_am_reception_buffer.find(amd_pdu->hdr.sn * 10);
        if(rlc_am_reception_buffer.end() == iter)
        {
            memcpy(new_pdu, amd_pdu, sizeof(LIBLTE_RLC_SINGLE_AMD_PDU_STRUCT));
            rlc_am_reception_buffer[amd_pdu->hdr.sn * 10] = new_pdu;
        }else{
            iter = rlc_am_reception_buffer.find(amd_pdu->hdr.sn * 10 + 1);
            if(rlc_am_reception_buffer.end() == iter)
            {
                memcpy(new_pdu, amd_pdu, sizeof(LIBLTE_RLC_SINGLE_AMD_PDU_STRUCT));
                rlc_am_reception_buffer[amd_pdu->hdr.sn * 10 + 1] = new_pdu;
            }else{
                delete new_pdu;
            }
        }
    }
}
void LTE_fdd_enb_rb::rlc_get_am_reception_buffer_status(LIBLTE_RLC_STATUS_PDU_STRUCT *status)
{
    std::map<uint16, LIBLTE_RLC_SINGLE_AMD_PDU_STRUCT *>::iterator iter;
    uint32                                                         i;

    // Fill in the ACK_SN
    status->ack_sn = rlc_vrh;

    // Determine if any NACK_SNs are needed
    status->N_nack = 0;
    if(rlc_vrh != rlc_vrr)
    {
        for(i=rlc_vrr; i<rlc_vrh; i++)
        {
            if(i > 0x3FFFF)
            {
                i -= 0x3FFFF;
            }

            iter = rlc_am_reception_buffer.find(i * 10);
            if(rlc_am_reception_buffer.end() == iter)
            {
                status->nack_sn[status->N_nack++] = i;
            }
        }

        // Update VR(R) if there are no missing frames
        if(0 == status->N_nack)
        {
            set_rlc_vrr(rlc_vrh);
        }
    }
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_rb::rlc_am_reassemble(LIBLTE_BYTE_MSG_STRUCT *sdu)
{
    std::map<uint16, LIBLTE_RLC_SINGLE_AMD_PDU_STRUCT *>::iterator iter;
    std::list<uint16>                                              first;
    std::list<uint16>                                              last;
    std::list<uint16>::iterator                                    first_iter;
    std::list<uint16>::iterator                                    last_iter;
    LTE_FDD_ENB_ERROR_ENUM                                         err = LTE_FDD_ENB_ERROR_CANT_REASSEMBLE_SDU;
    uint32                                                         i;
    bool                                                           reassemble;

    // Find all first and last SDUs
    for(iter=rlc_am_reception_buffer.begin(); iter!=rlc_am_reception_buffer.end(); iter++)
    {
        if(LIBLTE_RLC_FI_FIELD_FULL_SDU == (*iter).second->hdr.fi)
        {
            first.push_back((*iter).second->hdr.sn);
            last.push_back((*iter).second->hdr.sn);
        }else if(LIBLTE_RLC_FI_FIELD_FIRST_SDU_SEGMENT == (*iter).second->hdr.fi){
            first.push_back((*iter).second->hdr.sn);
        }else if(LIBLTE_RLC_FI_FIELD_LAST_SDU_SEGMENT == (*iter).second->hdr.fi){
            last.push_back((*iter).second->hdr.sn);
        }
    }

    // Check all first segments to see if the complete SDU is present
    for(first_iter=first.begin(); first_iter!=first.end(); first_iter++)
    {
        for(last_iter=last.begin(); last_iter!=last.end(); last_iter++)
        {
            if((*last_iter) >= (*first_iter))
            {
                break;
            }
        }

        if(last_iter != last.end())
        {
            reassemble = true;
            // Check that the whole SDU is present
            for(i=(*first_iter); i<=(*last_iter); i++)
            {
                iter = rlc_am_reception_buffer.find(i * 10);
                if(rlc_am_reception_buffer.end() == iter)
                {
                    iter = rlc_am_reception_buffer.find(i * 10 + 1);
                    if(rlc_am_reception_buffer.end() == iter)
                    {
                        reassemble = false;
                        break;
                    }
                }
            }

            if(reassemble)
            {
                // Reorder and reassemble the SDU
                sdu->N_bytes = 0;
                for(i=(*first_iter); i<=(*last_iter); i++)
                {
                    iter = rlc_am_reception_buffer.find(i * 10);
                    if(rlc_am_reception_buffer.end() == iter)
                    {
                        iter = rlc_am_reception_buffer.find(i * 10 + 1);
                    }
                    memcpy(&sdu->msg[sdu->N_bytes], (*iter).second->data.msg, (*iter).second->data.N_bytes);
                    sdu->N_bytes += (*iter).second->data.N_bytes;
                    delete (*iter).second;
                    rlc_am_reception_buffer.erase(iter);
                }
                err = LTE_FDD_ENB_ERROR_NONE;
                break;
            }
        }
    }

    return(err);
}
uint16 LTE_fdd_enb_rb::get_rlc_vta(void)
{
    return(rlc_vta);
}
void LTE_fdd_enb_rb::set_rlc_vta(uint16 vta)
{
    rlc_vta  = vta;
    rlc_vtms = rlc_vta + LIBLTE_RLC_AM_WINDOW_SIZE;
}
uint16 LTE_fdd_enb_rb::get_rlc_vtms(void)
{
    return(rlc_vtms);
}
uint16 LTE_fdd_enb_rb::get_rlc_vts(void)
{
    return(rlc_vts);
}
void LTE_fdd_enb_rb::set_rlc_vts(uint16 vts)
{
    rlc_vts = vts;
}
void LTE_fdd_enb_rb::rlc_add_to_transmission_buffer(LIBLTE_RLC_SINGLE_AMD_PDU_STRUCT *amd_pdu)
{
    LIBLTE_RLC_SINGLE_AMD_PDU_STRUCT *new_pdu = NULL;

    new_pdu = new LIBLTE_RLC_SINGLE_AMD_PDU_STRUCT;

    if(NULL != new_pdu)
    {
        memcpy(new_pdu, amd_pdu, sizeof(LIBLTE_RLC_SINGLE_AMD_PDU_STRUCT));
        rlc_am_transmission_buffer[amd_pdu->hdr.sn] = new_pdu;
    }
}
void LTE_fdd_enb_rb::rlc_update_transmission_buffer(LIBLTE_RLC_STATUS_PDU_STRUCT *status)
{
    std::map<uint16, LIBLTE_RLC_SINGLE_AMD_PDU_STRUCT *>::iterator iter;
    uint32                                                         i = rlc_vta;
    uint32                                                         j;
    bool                                                           update_vta = true;
    bool                                                           remove_sn;

    while(i != status->ack_sn)
    {
        remove_sn = true;
        for(j=0; j<status->N_nack; j++)
        {
            if(i == status->nack_sn[j])
            {
                remove_sn  = false;
                update_vta = false;
                break;
            }
        }
        if(remove_sn)
        {
            iter = rlc_am_transmission_buffer.find(i);
            if(rlc_am_transmission_buffer.end() != iter)
            {
                delete (*iter).second;
                rlc_am_transmission_buffer.erase(iter);
            }
            if(update_vta)
            {
                set_rlc_vta(i+1);
            }
        }
        i = (i+1) % 1024;
    }

    if(rlc_am_transmission_buffer.size() == 0)
    {
        rlc_stop_t_poll_retransmit();
    }
}
void LTE_fdd_enb_rb::rlc_start_t_poll_retransmit(void)
{
    LTE_fdd_enb_timer_mgr *timer_mgr = LTE_fdd_enb_timer_mgr::get_instance();
    LTE_fdd_enb_timer_cb   timer_expiry_cb(&LTE_fdd_enb_timer_cb_wrapper<LTE_fdd_enb_rb, &LTE_fdd_enb_rb::handle_t_poll_retransmit_timer_expiry>, this);

    if(LTE_FDD_ENB_INVALID_TIMER_ID == t_poll_retransmit_timer_id)
    {
        timer_mgr->start_timer(100, timer_expiry_cb, &t_poll_retransmit_timer_id);
    }
}
void LTE_fdd_enb_rb::rlc_stop_t_poll_retransmit(void)
{
    LTE_fdd_enb_timer_mgr *timer_mgr = LTE_fdd_enb_timer_mgr::get_instance();

    timer_mgr->stop_timer(t_poll_retransmit_timer_id);
    t_poll_retransmit_timer_id = LTE_FDD_ENB_INVALID_TIMER_ID;
}
void LTE_fdd_enb_rb::handle_t_poll_retransmit_timer_expiry(uint32 timer_id)
{
    LTE_fdd_enb_rlc                                                *rlc  = LTE_fdd_enb_rlc::get_instance();
    std::map<uint16, LIBLTE_RLC_SINGLE_AMD_PDU_STRUCT *>::iterator  iter = rlc_am_transmission_buffer.find(rlc_vta);

    t_poll_retransmit_timer_id = LTE_FDD_ENB_INVALID_TIMER_ID;

    if(rlc_am_transmission_buffer.end() != iter)
    {
        rlc->handle_retransmit((*iter).second, user, this);
    }
}
void LTE_fdd_enb_rb::set_rlc_vruh(uint16 vruh)
{
    rlc_vruh = vruh;
}
uint16 LTE_fdd_enb_rb::get_rlc_vruh(void)
{
    return(rlc_vruh);
}
void LTE_fdd_enb_rb::set_rlc_vrur(uint16 vrur)
{
    rlc_vrur = vrur;
}
uint16 LTE_fdd_enb_rb::get_rlc_vrur(void)
{
    return(rlc_vrur);
}
uint16 LTE_fdd_enb_rb::get_rlc_um_window_size(void)
{
    return(rlc_um_window_size);
}
void LTE_fdd_enb_rb::rlc_add_to_um_reception_buffer(LIBLTE_RLC_UMD_PDU_STRUCT *umd_pdu,
                                                    uint32                     idx)
{
    std::map<uint16, LIBLTE_BYTE_MSG_STRUCT *>::iterator  iter;
    LIBLTE_BYTE_MSG_STRUCT                               *new_pdu = NULL;

    new_pdu = new LIBLTE_BYTE_MSG_STRUCT;

    if(NULL != new_pdu)
    {
        iter = rlc_um_reception_buffer.find(umd_pdu->hdr.sn);
        if(rlc_um_reception_buffer.end() == iter)
        {
            memcpy(new_pdu, &umd_pdu->data[idx], sizeof(LIBLTE_BYTE_MSG_STRUCT));
            rlc_um_reception_buffer[umd_pdu->hdr.sn] = new_pdu;

            if(LIBLTE_RLC_FI_FIELD_FULL_SDU == umd_pdu->hdr.fi)
            {
                rlc_first_um_segment_sn = umd_pdu->hdr.sn;
                rlc_last_um_segment_sn  = umd_pdu->hdr.sn;
            }else if(LIBLTE_RLC_FI_FIELD_FIRST_SDU_SEGMENT == umd_pdu->hdr.fi){
                rlc_first_um_segment_sn = umd_pdu->hdr.sn;
            }else if(LIBLTE_RLC_FI_FIELD_LAST_SDU_SEGMENT == umd_pdu->hdr.fi){
                rlc_last_um_segment_sn = umd_pdu->hdr.sn;
            }
            if(rlc_last_um_segment_sn < rlc_first_um_segment_sn)
            {
                rlc_last_um_segment_sn = 0xFFFF;
            }
        }else{
            delete new_pdu;
        }
    }
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_rb::rlc_um_reassemble(LIBLTE_BYTE_MSG_STRUCT *sdu)
{
    std::map<uint16, LIBLTE_BYTE_MSG_STRUCT *>::iterator iter;
    LTE_FDD_ENB_ERROR_ENUM                               err = LTE_FDD_ENB_ERROR_CANT_REASSEMBLE_SDU;
    uint32                                               i;
    bool                                                 reassemble = true;

    if(0xFFFF != rlc_first_um_segment_sn &&
       0xFFFF != rlc_last_um_segment_sn)
    {
        // Make sure all segments are available
        for(i=rlc_first_um_segment_sn; i<=rlc_last_um_segment_sn; i++)
        {
            iter = rlc_um_reception_buffer.find(i);
            if(rlc_um_reception_buffer.end() == iter)
            {
                reassemble = false;
            }
        }

        if(reassemble)
        {
            // Reorder and reassemble the SDU
            sdu->N_bytes = 0;
            for(i=rlc_first_um_segment_sn; i<=rlc_last_um_segment_sn; i++)
            {
                iter = rlc_um_reception_buffer.find(i);
                memcpy(&sdu->msg[sdu->N_bytes], (*iter).second->msg, (*iter).second->N_bytes);
                sdu->N_bytes += (*iter).second->N_bytes;
                delete (*iter).second;
                rlc_um_reception_buffer.erase(iter);
            }

            // Clear the first/last segment SNs
            rlc_first_um_segment_sn = 0xFFFF;
            rlc_last_um_segment_sn  = 0xFFFF;

            err = LTE_FDD_ENB_ERROR_NONE;
        }
    }

    return(err);
}
void LTE_fdd_enb_rb::set_rlc_vtus(uint16 vtus)
{
    rlc_vtus = vtus;
}
uint16 LTE_fdd_enb_rb::get_rlc_vtus(void)
{
    return(rlc_vtus);
}

/*************/
/*    MAC    */
/*************/
void LTE_fdd_enb_rb::queue_mac_sdu(LIBLTE_BYTE_MSG_STRUCT *sdu)
{
    queue_msg(sdu, &mac_sdu_queue_sem, &mac_sdu_queue);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_rb::get_next_mac_sdu(LIBLTE_BYTE_MSG_STRUCT **sdu)
{
    return(get_next_msg(&mac_sdu_queue_sem, &mac_sdu_queue, sdu));
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_rb::delete_next_mac_sdu(void)
{
    return(delete_next_msg(&mac_sdu_queue_sem, &mac_sdu_queue));
}
LTE_FDD_ENB_MAC_CONFIG_ENUM LTE_fdd_enb_rb::get_mac_config(void)
{
    return(mac_config);
}
void LTE_fdd_enb_rb::set_con_res_id(uint64 con_res_id)
{
    mac_con_res_id = con_res_id;
}
uint64 LTE_fdd_enb_rb::get_con_res_id(void)
{
    return(mac_con_res_id);
}
void LTE_fdd_enb_rb::set_send_con_res_id(bool send_con_res_id)
{
    mac_send_con_res_id = send_con_res_id;
}
bool LTE_fdd_enb_rb::get_send_con_res_id(void)
{
    return(mac_send_con_res_id);
}

/*************/
/*    DRB    */
/*************/
void LTE_fdd_enb_rb::set_eps_bearer_id(uint32 ebi)
{
    eps_bearer_id = ebi;
}
uint32 LTE_fdd_enb_rb::get_eps_bearer_id(void)
{
    return(eps_bearer_id);
}
void LTE_fdd_enb_rb::set_lc_id(uint32 _lc_id)
{
    lc_id = _lc_id;
}
uint32 LTE_fdd_enb_rb::get_lc_id(void)
{
    return(lc_id);
}
void LTE_fdd_enb_rb::set_drb_id(uint8 _drb_id)
{
    drb_id = _drb_id;
}
uint8 LTE_fdd_enb_rb::get_drb_id(void)
{
    return(drb_id);
}
void LTE_fdd_enb_rb::set_log_chan_group(uint8 lcg)
{
    log_chan_group = lcg;
}
uint8 LTE_fdd_enb_rb::get_log_chan_group(void)
{
    return(log_chan_group);
}

/*****************/
/*    Generic    */
/*****************/
void LTE_fdd_enb_rb::queue_msg(LIBLTE_BIT_MSG_STRUCT              *msg,
                               sem_t                              *sem,
                               std::list<LIBLTE_BIT_MSG_STRUCT *> *queue)
{
    libtools_scoped_lock   lock(*sem);
    LIBLTE_BIT_MSG_STRUCT *loc_msg;

    loc_msg = new LIBLTE_BIT_MSG_STRUCT;
    memcpy(loc_msg, msg, sizeof(LIBLTE_BIT_MSG_STRUCT));

    queue->push_back(loc_msg);
}
void LTE_fdd_enb_rb::queue_msg(LIBLTE_BYTE_MSG_STRUCT              *msg,
                               sem_t                               *sem,
                               std::list<LIBLTE_BYTE_MSG_STRUCT *> *queue)
{
    libtools_scoped_lock    lock(*sem);
    LIBLTE_BYTE_MSG_STRUCT *loc_msg;

    loc_msg = new LIBLTE_BYTE_MSG_STRUCT;
    memcpy(loc_msg, msg, sizeof(LIBLTE_BYTE_MSG_STRUCT));

    queue->push_back(loc_msg);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_rb::get_next_msg(sem_t                               *sem,
                                                    std::list<LIBLTE_BIT_MSG_STRUCT *>  *queue,
                                                    LIBLTE_BIT_MSG_STRUCT              **msg)
{
    libtools_scoped_lock   lock(*sem);
    LTE_FDD_ENB_ERROR_ENUM err = LTE_FDD_ENB_ERROR_NO_MSG_IN_QUEUE;

    if(0 != queue->size())
    {
        *msg = queue->front();
        err  = LTE_FDD_ENB_ERROR_NONE;
    }

    return(err);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_rb::get_next_msg(sem_t                                *sem,
                                                    std::list<LIBLTE_BYTE_MSG_STRUCT *>  *queue,
                                                    LIBLTE_BYTE_MSG_STRUCT              **msg)
{
    libtools_scoped_lock   lock(*sem);
    LTE_FDD_ENB_ERROR_ENUM err = LTE_FDD_ENB_ERROR_NO_MSG_IN_QUEUE;

    if(0 != queue->size())
    {
        *msg = queue->front();
        err  = LTE_FDD_ENB_ERROR_NONE;
    }

    return(err);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_rb::delete_next_msg(sem_t                              *sem,
                                                       std::list<LIBLTE_BIT_MSG_STRUCT *> *queue)
{
    libtools_scoped_lock    lock(*sem);
    LTE_FDD_ENB_ERROR_ENUM  err = LTE_FDD_ENB_ERROR_NO_MSG_IN_QUEUE;
    LIBLTE_BIT_MSG_STRUCT  *msg;

    if(0 != queue->size())
    {
        msg = queue->front();
        queue->pop_front();
        delete msg;
        err = LTE_FDD_ENB_ERROR_NONE;
    }

    return(err);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_rb::delete_next_msg(sem_t                               *sem,
                                                       std::list<LIBLTE_BYTE_MSG_STRUCT *> *queue)
{
    libtools_scoped_lock    lock(*sem);
    LTE_FDD_ENB_ERROR_ENUM  err = LTE_FDD_ENB_ERROR_NO_MSG_IN_QUEUE;
    LIBLTE_BYTE_MSG_STRUCT *msg;

    if(0 != queue->size())
    {
        msg = queue->front();
        queue->pop_front();
        delete msg;
        err = LTE_FDD_ENB_ERROR_NONE;
    }

    return(err);
}
