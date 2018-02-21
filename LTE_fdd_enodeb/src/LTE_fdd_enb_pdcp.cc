#line 2 "LTE_fdd_enb_pdcp.cc" // Make __FILE__ omit the path
/*******************************************************************************

    Copyright 2013-2015, 2017 Ben Wojtowicz

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

    File: LTE_fdd_enb_pdcp.cc

    Description: Contains all the implementations for the LTE FDD eNodeB
                 packet data convergence protocol layer.

    Revision History
    ----------    -------------    --------------------------------------------
    11/10/2013    Ben Wojtowicz    Created file
    01/18/2014    Ben Wojtowicz    Added level to debug prints.
    05/04/2014    Ben Wojtowicz    Added communication to RLC and RRC.
    06/15/2014    Ben Wojtowicz    Added simple header parsing.
    08/03/2014    Ben Wojtowicz    Added transmit functionality.
    11/01/2014    Ben Wojtowicz    Added SRB2 and security support.
    11/29/2014    Ben Wojtowicz    Added communication to IP gateway.
    12/16/2014    Ben Wojtowicz    Added ol extension to message queues.
    02/15/2015    Ben Wojtowicz    Moved to new message queue.
    12/06/2015    Ben Wojtowicz    Changed boost::mutex to pthread_mutex_t and
                                   sem_t.
    07/29/2017    Ben Wojtowicz    Moved away from singleton pattern.

*******************************************************************************/

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_fdd_enb_pdcp.h"
#include "LTE_fdd_enb_rlc.h"
#include "liblte_pdcp.h"
#include "liblte_security.h"
#include "libtools_scoped_lock.h"

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
LTE_fdd_enb_pdcp::LTE_fdd_enb_pdcp()
{
    sem_init(&start_sem, 0, 1);
    sem_init(&sys_info_sem, 0, 1);
    started = false;
}
LTE_fdd_enb_pdcp::~LTE_fdd_enb_pdcp()
{
    stop();
    sem_destroy(&sys_info_sem);
    sem_destroy(&start_sem);
}

/********************/
/*    Start/Stop    */
/********************/
void LTE_fdd_enb_pdcp::start(LTE_fdd_enb_msgq      *from_rlc,
                             LTE_fdd_enb_msgq      *from_rrc,
                             LTE_fdd_enb_msgq      *from_gw,
                             LTE_fdd_enb_msgq      *to_rlc,
                             LTE_fdd_enb_msgq      *to_rrc,
                             LTE_fdd_enb_msgq      *to_gw,
                             LTE_fdd_enb_interface *iface)
{
    libtools_scoped_lock lock(start_sem);
    LTE_fdd_enb_msgq_cb  rlc_cb(&LTE_fdd_enb_msgq_cb_wrapper<LTE_fdd_enb_pdcp, &LTE_fdd_enb_pdcp::handle_rlc_msg>, this);
    LTE_fdd_enb_msgq_cb  rrc_cb(&LTE_fdd_enb_msgq_cb_wrapper<LTE_fdd_enb_pdcp, &LTE_fdd_enb_pdcp::handle_rrc_msg>, this);
    LTE_fdd_enb_msgq_cb  gw_cb(&LTE_fdd_enb_msgq_cb_wrapper<LTE_fdd_enb_pdcp, &LTE_fdd_enb_pdcp::handle_gw_msg>, this);

    if(!started)
    {
        interface     = iface;
        started       = true;
        msgq_from_rlc = from_rlc;
        msgq_from_rrc = from_rrc;
        msgq_from_gw  = from_gw;
        msgq_to_rlc   = to_rlc;
        msgq_to_rrc   = to_rrc;
        msgq_to_gw    = to_gw;
        msgq_from_rlc->attach_rx(rlc_cb);
        msgq_from_rrc->attach_rx(rrc_cb);
        msgq_from_gw->attach_rx(gw_cb);
    }
}
void LTE_fdd_enb_pdcp::stop(void)
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
void LTE_fdd_enb_pdcp::handle_rlc_msg(LTE_FDD_ENB_MESSAGE_STRUCT &msg)
{
    if(LTE_FDD_ENB_DEST_LAYER_PDCP == msg.dest_layer ||
       LTE_FDD_ENB_DEST_LAYER_ANY  == msg.dest_layer)
    {
        switch(msg.type)
        {
        case LTE_FDD_ENB_MESSAGE_TYPE_PDCP_PDU_READY:
            handle_pdu_ready(&msg.msg.pdcp_pdu_ready);
            break;
        default:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_PDCP,
                                      __FILE__,
                                      __LINE__,
                                      "Received invalid RLC message %s",
                                      LTE_fdd_enb_message_type_text[msg.type]);
            break;
        }
    }else{
        // Forward message to RRC
        msgq_to_rrc->send(msg);
    }
}
void LTE_fdd_enb_pdcp::handle_rrc_msg(LTE_FDD_ENB_MESSAGE_STRUCT &msg)
{
    if(LTE_FDD_ENB_DEST_LAYER_PDCP == msg.dest_layer ||
       LTE_FDD_ENB_DEST_LAYER_ANY  == msg.dest_layer)
    {
        switch(msg.type)
        {
        case LTE_FDD_ENB_MESSAGE_TYPE_PDCP_SDU_READY:
            handle_sdu_ready(&msg.msg.pdcp_sdu_ready);
            break;
        default:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_PDCP,
                                      __FILE__,
                                      __LINE__,
                                      "Received invalid RRC message %s",
                                      LTE_fdd_enb_message_type_text[msg.type]);
            break;
        }
    }else{
        // Forward message to RLC
        msgq_to_rlc->send(msg);
    }
}
void LTE_fdd_enb_pdcp::handle_gw_msg(LTE_FDD_ENB_MESSAGE_STRUCT &msg)
{
    if(LTE_FDD_ENB_DEST_LAYER_PDCP == msg.dest_layer ||
       LTE_FDD_ENB_DEST_LAYER_ANY  == msg.dest_layer)
    {
        switch(msg.type)
        {
        case LTE_FDD_ENB_MESSAGE_TYPE_PDCP_DATA_SDU_READY:
            handle_data_sdu_ready(&msg.msg.pdcp_data_sdu_ready);
            break;
        default:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_PDCP,
                                      __FILE__,
                                      __LINE__,
                                      "Received invalid GW message %s",
                                      LTE_fdd_enb_message_type_text[msg.type]);
            break;
        }
    }else{
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_PDCP,
                                  __FILE__,
                                  __LINE__,
                                  "Received GW message for invalid layer %s",
                                  LTE_fdd_enb_dest_layer_text[msg.dest_layer]);
    }
}

/****************************/
/*    External Interface    */
/****************************/
void LTE_fdd_enb_pdcp::update_sys_info(void)
{
    libtools_scoped_lock  lock(sys_info_sem);
    LTE_fdd_enb_cnfg_db  *cnfg_db = LTE_fdd_enb_cnfg_db::get_instance();

    cnfg_db->get_sys_info(sys_info);
}

/******************************/
/*    RLC Message Handlers    */
/******************************/
void LTE_fdd_enb_pdcp::handle_pdu_ready(LTE_FDD_ENB_PDCP_PDU_READY_MSG_STRUCT *pdu_ready)
{
    LTE_FDD_ENB_RRC_PDU_READY_MSG_STRUCT      rrc_pdu_ready;
    LTE_FDD_ENB_GW_DATA_READY_MSG_STRUCT      gw_data_ready;
    LIBLTE_PDCP_CONTROL_PDU_STRUCT            contents;
    LIBLTE_PDCP_DATA_PDU_WITH_LONG_SN_STRUCT  data_contents;
    LIBLTE_BYTE_MSG_STRUCT                   *pdu;
    LIBLTE_BIT_MSG_STRUCT                     rrc_pdu;
    uint8                                    *pdu_ptr;
    uint32                                    i;

    if(LTE_FDD_ENB_ERROR_NONE == pdu_ready->rb->get_next_pdcp_pdu(&pdu))
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                  LTE_FDD_ENB_DEBUG_LEVEL_PDCP,
                                  __FILE__,
                                  __LINE__,
                                  pdu,
                                  "Received PDU for RNTI=%u and RB=%s",
                                  pdu_ready->user->get_c_rnti(),
                                  LTE_fdd_enb_rb_text[pdu_ready->rb->get_rb_id()]);

        // FIXME: Add SN and integrity verification

        if(LTE_FDD_ENB_RB_SRB0 == pdu_ready->rb->get_rb_id())
        {
            // Convert to bit struct for RRC
            pdu_ptr = rrc_pdu.msg;
            for(i=0; i<pdu->N_bytes; i++)
            {
                liblte_value_2_bits(pdu->msg[i], &pdu_ptr, 8);
            }
            rrc_pdu.N_bits = pdu_ptr - rrc_pdu.msg;

            // Queue the SDU for RRC
            pdu_ready->rb->queue_rrc_pdu(&rrc_pdu);

            // Signal RRC
            rrc_pdu_ready.user = pdu_ready->user;
            rrc_pdu_ready.rb   = pdu_ready->rb;
            msgq_to_rrc->send(LTE_FDD_ENB_MESSAGE_TYPE_RRC_PDU_READY,
                              LTE_FDD_ENB_DEST_LAYER_RRC,
                              (LTE_FDD_ENB_MESSAGE_UNION *)&rrc_pdu_ready,
                              sizeof(LTE_FDD_ENB_RRC_PDU_READY_MSG_STRUCT));
        }else if(LTE_FDD_ENB_RB_SRB1 == pdu_ready->rb->get_rb_id()){
            liblte_pdcp_unpack_control_pdu(pdu, &contents);

            // Queue the SDU for RRC
            pdu_ready->rb->queue_rrc_pdu(&contents.data);

            // Signal RRC
            rrc_pdu_ready.user = pdu_ready->user;
            rrc_pdu_ready.rb   = pdu_ready->rb;
            msgq_to_rrc->send(LTE_FDD_ENB_MESSAGE_TYPE_RRC_PDU_READY,
                              LTE_FDD_ENB_DEST_LAYER_RRC,
                              (LTE_FDD_ENB_MESSAGE_UNION *)&rrc_pdu_ready,
                              sizeof(LTE_FDD_ENB_RRC_PDU_READY_MSG_STRUCT));
        }else if(LTE_FDD_ENB_RB_SRB2 == pdu_ready->rb->get_rb_id()){
            liblte_pdcp_unpack_control_pdu(pdu, &contents);

            // Queue the SDU for RRC
            pdu_ready->rb->queue_rrc_pdu(&contents.data);

            // Signal RRC
            rrc_pdu_ready.user = pdu_ready->user;
            rrc_pdu_ready.rb   = pdu_ready->rb;
            msgq_to_rrc->send(LTE_FDD_ENB_MESSAGE_TYPE_RRC_PDU_READY,
                              LTE_FDD_ENB_DEST_LAYER_RRC,
                              (LTE_FDD_ENB_MESSAGE_UNION *)&rrc_pdu_ready,
                              sizeof(LTE_FDD_ENB_RRC_PDU_READY_MSG_STRUCT));
        }else if(LTE_FDD_ENB_RB_DRB1 == pdu_ready->rb->get_rb_id()){
            liblte_pdcp_unpack_data_pdu_with_long_sn(pdu, &data_contents);

            // Queue the SDU for GW
            pdu_ready->rb->queue_gw_data_msg(&data_contents.data);

            // Signal GW
            gw_data_ready.user = pdu_ready->user;
            gw_data_ready.rb   = pdu_ready->rb;
            msgq_to_gw->send(LTE_FDD_ENB_MESSAGE_TYPE_GW_DATA_READY,
                             LTE_FDD_ENB_DEST_LAYER_GW,
                             (LTE_FDD_ENB_MESSAGE_UNION *)&gw_data_ready,
                             sizeof(LTE_FDD_ENB_GW_DATA_READY_MSG_STRUCT));
        }else{
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_PDCP,
                                      __FILE__,
                                      __LINE__,
                                      pdu,
                                      "Received PDU for RNTI=%u with invalid RB=%s",
                                      pdu_ready->user->get_c_rnti(),
                                      LTE_fdd_enb_rb_text[pdu_ready->rb->get_rb_id()]);
        }

        // Delete the PDU
        pdu_ready->rb->delete_next_pdcp_pdu();
    }
}

/******************************/
/*    RRC Message Handlers    */
/******************************/
void LTE_fdd_enb_pdcp::handle_sdu_ready(LTE_FDD_ENB_PDCP_SDU_READY_MSG_STRUCT *sdu_ready)
{
    LTE_FDD_ENB_RLC_SDU_READY_MSG_STRUCT  rlc_sdu_ready;
    LIBLTE_PDCP_CONTROL_PDU_STRUCT        contents;
    LIBLTE_BYTE_MSG_STRUCT                pdu;
    LIBLTE_BIT_MSG_STRUCT                *sdu;
    uint8                                *sdu_ptr;
    uint32                                i;

    if(LTE_FDD_ENB_ERROR_NONE == sdu_ready->rb->get_next_pdcp_sdu(&sdu))
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                  LTE_FDD_ENB_DEBUG_LEVEL_PDCP,
                                  __FILE__,
                                  __LINE__,
                                  sdu,
                                  "Received SDU for RNTI=%u and RB=%s",
                                  sdu_ready->user->get_c_rnti(),
                                  LTE_fdd_enb_rb_text[sdu_ready->rb->get_rb_id()]);

        if(LTE_FDD_ENB_RB_SRB0 == sdu_ready->rb->get_rb_id())
        {
            // Make sure the SDU is byte aligned
            if((sdu->N_bits % 8) != 0)
            {
                for(i=0; i<8-(sdu->N_bits % 8); i++)
                {
                    sdu->msg[sdu->N_bits + i] = 0;
                }
                sdu->N_bits += 8 - (sdu->N_bits % 8);
            }

            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                      LTE_FDD_ENB_DEBUG_LEVEL_PDCP,
                                      __FILE__,
                                      __LINE__,
                                      sdu,
                                      "Sending PDU for RNTI=%u and RB=%s",
                                      sdu_ready->user->get_c_rnti(),
                                      LTE_fdd_enb_rb_text[sdu_ready->rb->get_rb_id()]);

            // Convert from bit to byte struct
            sdu_ptr = sdu->msg;
            for(i=0; i<sdu->N_bits/8; i++)
            {
                pdu.msg[i] = liblte_bits_2_value(&sdu_ptr, 8);
            }
            pdu.N_bytes = sdu->N_bits/8;

            // Queue the PDU for RLC
            sdu_ready->rb->queue_rlc_sdu(&pdu);

            // Signal RLC
            rlc_sdu_ready.user = sdu_ready->user;
            rlc_sdu_ready.rb   = sdu_ready->rb;
            msgq_to_rlc->send(LTE_FDD_ENB_MESSAGE_TYPE_RLC_SDU_READY,
                              LTE_FDD_ENB_DEST_LAYER_RLC,
                              (LTE_FDD_ENB_MESSAGE_UNION *)&rlc_sdu_ready,
                              sizeof(LTE_FDD_ENB_RLC_SDU_READY_MSG_STRUCT));

            // Delete the SDU
            sdu_ready->rb->delete_next_pdcp_sdu();
        }else if(LTE_FDD_ENB_RB_SRB1 == sdu_ready->rb->get_rb_id() ||
                 LTE_FDD_ENB_RB_SRB2 == sdu_ready->rb->get_rb_id()){
            // Pack the control PDU
            contents.count = sdu_ready->rb->get_pdcp_tx_count();
            if(LTE_FDD_ENB_PDCP_CONFIG_SECURITY == sdu_ready->rb->get_pdcp_config())
            {
                liblte_pdcp_pack_control_pdu(&contents,
                                             sdu,
                                             sdu_ready->user->get_auth_vec()->k_rrc_int,
                                             LIBLTE_SECURITY_DIRECTION_DOWNLINK,
                                             sdu_ready->rb->get_rb_id()-1,
                                             &pdu);
            }else{
                liblte_pdcp_pack_control_pdu(&contents,
                                             sdu,
                                             &pdu);
            }

            // Increment the SN
            sdu_ready->rb->set_pdcp_tx_count(contents.count + 1);

            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                      LTE_FDD_ENB_DEBUG_LEVEL_PDCP,
                                      __FILE__,
                                      __LINE__,
                                      &pdu,
                                      "Sending PDU for RNTI=%u and RB=%s",
                                      sdu_ready->user->get_c_rnti(),
                                      LTE_fdd_enb_rb_text[sdu_ready->rb->get_rb_id()]);

            // Queue the PDU for RLC
            sdu_ready->rb->queue_rlc_sdu(&pdu);

            // Signal RLC
            rlc_sdu_ready.user = sdu_ready->user;
            rlc_sdu_ready.rb   = sdu_ready->rb;
            msgq_to_rlc->send(LTE_FDD_ENB_MESSAGE_TYPE_RLC_SDU_READY,
                              LTE_FDD_ENB_DEST_LAYER_RLC,
                              (LTE_FDD_ENB_MESSAGE_UNION *)&rlc_sdu_ready,
                              sizeof(LTE_FDD_ENB_RLC_SDU_READY_MSG_STRUCT));

            // Delete the SDU
            sdu_ready->rb->delete_next_pdcp_sdu();
        }else{
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_PDCP,
                                      __FILE__,
                                      __LINE__,
                                      sdu,
                                      "Received SDU for RNTI=%u with invalid RB=%s",
                                      sdu_ready->user->get_c_rnti(),
                                      LTE_fdd_enb_rb_text[sdu_ready->rb->get_rb_id()]);
        }
    }else{
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_PDCP,
                                  __FILE__,
                                  __LINE__,
                                  "Received SDU ready with no data, RNTI=%u, RB=%s",
                                  sdu_ready->user->get_c_rnti(),
                                  LTE_fdd_enb_rb_text[sdu_ready->rb->get_rb_id()]);
    }
}

/*****************************/
/*    GW Message Handlers    */
/*****************************/
void LTE_fdd_enb_pdcp::handle_data_sdu_ready(LTE_FDD_ENB_PDCP_DATA_SDU_READY_MSG_STRUCT *data_sdu_ready)
{
    LTE_FDD_ENB_RLC_SDU_READY_MSG_STRUCT      rlc_sdu_ready;
    LIBLTE_PDCP_DATA_PDU_WITH_LONG_SN_STRUCT  contents;
    LIBLTE_BYTE_MSG_STRUCT                    pdu;
    LIBLTE_BYTE_MSG_STRUCT                   *sdu;

    if(LTE_FDD_ENB_ERROR_NONE == data_sdu_ready->rb->get_next_pdcp_data_sdu(&sdu))
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                  LTE_FDD_ENB_DEBUG_LEVEL_PDCP,
                                  __FILE__,
                                  __LINE__,
                                  sdu,
                                  "Received data SDU from GW for RNTI=%u and RB=%s",
                                  data_sdu_ready->user->get_c_rnti(),
                                  LTE_fdd_enb_rb_text[data_sdu_ready->rb->get_rb_id()]);

        if(data_sdu_ready->rb->get_rb_id()       >= LTE_FDD_ENB_RB_DRB1 &&
           data_sdu_ready->rb->get_pdcp_config() == LTE_FDD_ENB_PDCP_CONFIG_LONG_SN)
        {
            // Pack the data PDU
            contents.count = data_sdu_ready->rb->get_pdcp_tx_count();
            liblte_pdcp_pack_data_pdu_with_long_sn(&contents,
                                                   sdu,
                                                   &pdu);

            // Increment the SN
            data_sdu_ready->rb->set_pdcp_tx_count(contents.count + 1);

            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                      LTE_FDD_ENB_DEBUG_LEVEL_PDCP,
                                      __FILE__,
                                      __LINE__,
                                      &pdu,
                                      "Sending PDU for RNTI=%u and RB=%s",
                                      data_sdu_ready->user->get_c_rnti(),
                                      LTE_fdd_enb_rb_text[data_sdu_ready->rb->get_rb_id()]);

            // Queue the PDU for RLC
            data_sdu_ready->rb->queue_rlc_sdu(&pdu);

            // Signal RLC
            rlc_sdu_ready.user = data_sdu_ready->user;
            rlc_sdu_ready.rb   = data_sdu_ready->rb;
            msgq_to_rlc->send(LTE_FDD_ENB_MESSAGE_TYPE_RLC_SDU_READY,
                              LTE_FDD_ENB_DEST_LAYER_RLC,
                              (LTE_FDD_ENB_MESSAGE_UNION *)&rlc_sdu_ready,
                              sizeof(LTE_FDD_ENB_RLC_SDU_READY_MSG_STRUCT));

            // Delete the SDU
            data_sdu_ready->rb->delete_next_pdcp_data_sdu();
        }else{
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_PDCP,
                                      __FILE__,
                                      __LINE__,
                                      "Received data SDU from GW for invalid RB=%s, RNTI=%u",
                                      LTE_fdd_enb_rb_text[data_sdu_ready->rb->get_rb_id()],
                                      data_sdu_ready->user->get_c_rnti());
        }
    }else{
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_PDCP,
                                  __FILE__,
                                  __LINE__,
                                  "Received data SDU ready from GW with no data, RNTI=%u, RB=%s",
                                  data_sdu_ready->user->get_c_rnti(),
                                  LTE_fdd_enb_rb_text[data_sdu_ready->rb->get_rb_id()]);
    }
}
