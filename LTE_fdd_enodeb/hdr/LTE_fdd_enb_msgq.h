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

    File: LTE_fdd_enb_msgq.h

    Description: Contains all the definitions for the LTE FDD eNodeB
                 message queues.

    Revision History
    ----------    -------------    --------------------------------------------
    11/09/2013    Ben Wojtowicz    Created file
    01/18/2014    Ben Wojtowicz    Added the ability to set priorities.
    03/26/2014    Ben Wojtowicz    Added RNTI to PUSCH decode message.
    05/04/2014    Ben Wojtowicz    Added messages for MAC, RLC, PDCP, and RRC
                                   communication.
    06/15/2014    Ben Wojtowicz    Added MME<->RRC messages.
    08/03/2014    Ben Wojtowicz    Added RRC command messages.
    11/01/2014    Ben Wojtowicz    Added RRC security command, RRC SRB2 setup
                                   command, and RRC command response messages.
    11/29/2014    Ben Wojtowicz    Added IP gateway communication messages.
    02/15/2015    Ben Wojtowicz    Moving to new message queue with semaphores
                                   and circular buffers and added a timer tick
                                   message.
    03/15/2015    Ben Wojtowicz    Added a mutex to the circular buffer.
    07/25/2015    Ben Wojtowicz    Combined the DL and UL schedule messages into
                                   a single PHY schedule message.
    12/06/2015    Ben Wojtowicz    Changed boost::mutex and
                                   boost::interprocess::interprocess_semaphore
                                   to sem_t.
    03/12/2016    Ben Wojtowicz    Added PUCCH support.
    07/29/2017    Ben Wojtowicz    Added SR Support.

*******************************************************************************/

#ifndef __LTE_FDD_ENB_MSGQ_H__
#define __LTE_FDD_ENB_MSGQ_H__

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_fdd_enb_user.h"
#include "liblte_rrc.h"
#include "liblte_phy.h"
#include <boost/circular_buffer.hpp>
#include <string>

/*******************************************************************************
                              DEFINES
*******************************************************************************/

#define LTE_FDD_ENB_N_SIB_ALLOCS      7
#define LTE_FDD_ENB_N_PUCCH_PER_SUBFR 12

/*******************************************************************************
                              FORWARD DECLARATIONS
*******************************************************************************/


/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/

typedef enum{
    // Generic Messages
    LTE_FDD_ENB_MESSAGE_TYPE_KILL = 0,

    // MAC -> PHY Messages
    LTE_FDD_ENB_MESSAGE_TYPE_PHY_SCHEDULE,

    // PHY -> MAC Messages
    LTE_FDD_ENB_MESSAGE_TYPE_READY_TO_SEND,
    LTE_FDD_ENB_MESSAGE_TYPE_PRACH_DECODE,
    LTE_FDD_ENB_MESSAGE_TYPE_PUCCH_DECODE,
    LTE_FDD_ENB_MESSAGE_TYPE_PUSCH_DECODE,

    // RLC -> MAC Messages
    LTE_FDD_ENB_MESSAGE_TYPE_MAC_SDU_READY,

    // MAC -> RLC Messages
    LTE_FDD_ENB_MESSAGE_TYPE_RLC_PDU_READY,

    // MAC -> TIMER Messages
    LTE_FDD_ENB_MESSAGE_TYPE_TIMER_TICK,

    // PDCP -> RLC Messages
    LTE_FDD_ENB_MESSAGE_TYPE_RLC_SDU_READY,

    // RLC -> PDCP Messages
    LTE_FDD_ENB_MESSAGE_TYPE_PDCP_PDU_READY,

    // RRC -> PDCP Messages
    LTE_FDD_ENB_MESSAGE_TYPE_PDCP_SDU_READY,

    // PDCP -> RRC Messages
    LTE_FDD_ENB_MESSAGE_TYPE_RRC_PDU_READY,

    // MME -> RRC Messages
    LTE_FDD_ENB_MESSAGE_TYPE_RRC_NAS_MSG_READY,
    LTE_FDD_ENB_MESSAGE_TYPE_RRC_CMD_READY,

    // RRC -> MME Messages
    LTE_FDD_ENB_MESSAGE_TYPE_MME_NAS_MSG_READY,
    LTE_FDD_ENB_MESSAGE_TYPE_MME_RRC_CMD_RESP,

    // GW -> PDCP Messages
    LTE_FDD_ENB_MESSAGE_TYPE_PDCP_DATA_SDU_READY,

    // PDCP -> GW Messages
    LTE_FDD_ENB_MESSAGE_TYPE_GW_DATA_READY,

    LTE_FDD_ENB_MESSAGE_TYPE_N_ITEMS,
}LTE_FDD_ENB_MESSAGE_TYPE_ENUM;
static const char LTE_fdd_enb_message_type_text[LTE_FDD_ENB_MESSAGE_TYPE_N_ITEMS][100] = {"Kill",
                                                                                          "PHY schedule",
                                                                                          "Ready to send",
                                                                                          "PRACH decode",
                                                                                          "PUCCH decode",
                                                                                          "PUSCH decode",
                                                                                          "MAC sdu ready",
                                                                                          "RLC pdu ready",
                                                                                          "Timer tick",
                                                                                          "RLC sdu ready",
                                                                                          "PDCP pdu ready",
                                                                                          "PDCP sdu ready",
                                                                                          "RRC pdu ready",
                                                                                          "RRC NAS message ready",
                                                                                          "RRC command ready",
                                                                                          "MME NAS message ready",
                                                                                          "MME RRC command response",
                                                                                          "PDCP data sdu ready",
                                                                                          "GW data ready"};

typedef enum{
    LTE_FDD_ENB_DEST_LAYER_PHY = 0,
    LTE_FDD_ENB_DEST_LAYER_MAC,
    LTE_FDD_ENB_DEST_LAYER_RLC,
    LTE_FDD_ENB_DEST_LAYER_PDCP,
    LTE_FDD_ENB_DEST_LAYER_RRC,
    LTE_FDD_ENB_DEST_LAYER_MME,
    LTE_FDD_ENB_DEST_LAYER_GW,
    LTE_FDD_ENB_DEST_LAYER_TIMER_MGR,
    LTE_FDD_ENB_DEST_LAYER_ANY,
    LTE_FDD_ENB_DEST_LAYER_N_ITEMS,
}LTE_FDD_ENB_DEST_LAYER_ENUM;
static const char LTE_fdd_enb_dest_layer_text[LTE_FDD_ENB_DEST_LAYER_N_ITEMS][100] = {"PHY",
                                                                                      "MAC",
                                                                                      "RLC",
                                                                                      "PDCP",
                                                                                      "RRC",
                                                                                      "MME",
                                                                                      "GW",
                                                                                      "TIMER_MGR",
                                                                                      "ANY"};

// Generic Messages

// MAC -> PHY Messages
typedef struct{
    LIBLTE_PHY_PDCCH_STRUCT dl_allocations;
    LIBLTE_PHY_PDCCH_STRUCT ul_allocations;
    uint32                  N_avail_prbs;
    uint32                  N_sched_prbs;
    uint32                  current_tti;
}LTE_FDD_ENB_DL_SCHEDULE_MSG_STRUCT;
typedef enum{
    LTE_FDD_ENB_PUCCH_TYPE_ACK_NACK = 0,
    LTE_FDD_ENB_PUCCH_TYPE_SR,
    LTE_FDD_ENB_PUCCH_TYPE_N_ITEMS,
}LTE_FDD_ENB_PUCCH_TYPE_ENUM;
static const char LTE_fdd_enb_pucch_type_text[LTE_FDD_ENB_PUCCH_TYPE_N_ITEMS][100] = {"ACK_NACK",
                                                                                      "SR"};
typedef struct{
    LTE_FDD_ENB_PUCCH_TYPE_ENUM type;
    uint32                      n_1_p_pucch;
    uint16                      rnti;
    bool                        decode;
}LTE_FDD_ENB_PUCCH_STRUCT;
typedef struct{
    LIBLTE_PHY_PDCCH_STRUCT  decodes;
    LTE_FDD_ENB_PUCCH_STRUCT pucch[LTE_FDD_ENB_N_PUCCH_PER_SUBFR];
    uint32                   N_avail_prbs;
    uint32                   N_sched_prbs;
    uint32                   current_tti;
    uint32                   N_pucch;
    uint8                    next_prb;
}LTE_FDD_ENB_UL_SCHEDULE_MSG_STRUCT;
typedef struct{
    LTE_FDD_ENB_DL_SCHEDULE_MSG_STRUCT dl_sched;
    LTE_FDD_ENB_UL_SCHEDULE_MSG_STRUCT ul_sched;
}LTE_FDD_ENB_PHY_SCHEDULE_MSG_STRUCT;

// PHY -> MAC Messages
typedef struct{
    uint32 dl_current_tti;
    uint32 ul_current_tti;
    bool   late;
}LTE_FDD_ENB_READY_TO_SEND_MSG_STRUCT;
typedef struct{
    uint32 current_tti;
    uint32 timing_adv[64];
    uint32 preamble[64];
    uint32 num_preambles;
}LTE_FDD_ENB_PRACH_DECODE_MSG_STRUCT;
typedef struct{
    LIBLTE_BIT_MSG_STRUCT       msg;
    LTE_FDD_ENB_PUCCH_TYPE_ENUM type;
    uint32                      current_tti;
    uint16                      rnti;
}LTE_FDD_ENB_PUCCH_DECODE_MSG_STRUCT;
typedef struct{
    LIBLTE_BIT_MSG_STRUCT msg;
    uint32                current_tti;
    uint16                rnti;
}LTE_FDD_ENB_PUSCH_DECODE_MSG_STRUCT;

// RLC -> MAC Messages
typedef struct{
    LTE_fdd_enb_user *user;
    LTE_fdd_enb_rb   *rb;
}LTE_FDD_ENB_MAC_SDU_READY_MSG_STRUCT;

// MAC -> RLC Messages
typedef struct{
    LTE_fdd_enb_user *user;
    LTE_fdd_enb_rb   *rb;
}LTE_FDD_ENB_RLC_PDU_READY_MSG_STRUCT;

// MAC -> TIMER Messages
typedef struct{
    uint32 tick;
}LTE_FDD_ENB_TIMER_TICK_MSG_STRUCT;

// PDCP -> RLC Messages
typedef struct{
    LTE_fdd_enb_user *user;
    LTE_fdd_enb_rb   *rb;
}LTE_FDD_ENB_RLC_SDU_READY_MSG_STRUCT;

// RLC -> PDCP Messages
typedef struct{
    LTE_fdd_enb_user *user;
    LTE_fdd_enb_rb   *rb;
}LTE_FDD_ENB_PDCP_PDU_READY_MSG_STRUCT;

// RRC -> PDCP Messages
typedef struct{
    LTE_fdd_enb_user *user;
    LTE_fdd_enb_rb   *rb;
}LTE_FDD_ENB_PDCP_SDU_READY_MSG_STRUCT;

// PDCP -> RRC Messages
typedef struct{
    LTE_fdd_enb_user *user;
    LTE_fdd_enb_rb   *rb;
}LTE_FDD_ENB_RRC_PDU_READY_MSG_STRUCT;

// MME -> RRC Messages
typedef struct{
    LTE_fdd_enb_user *user;
    LTE_fdd_enb_rb   *rb;
}LTE_FDD_ENB_RRC_NAS_MSG_READY_MSG_STRUCT;
typedef enum{
    LTE_FDD_ENB_RRC_CMD_RELEASE = 0,
    LTE_FDD_ENB_RRC_CMD_SECURITY,
    LTE_FDD_ENB_RRC_CMD_SETUP_DEF_DRB,
    LTE_FDD_ENB_RRC_CMD_SETUP_DED_DRB,
    LTE_FDD_ENB_RRC_CMD_N_ITEMS,
}LTE_FDD_ENB_RRC_CMD_ENUM;
static const char LTE_fdd_enb_rrc_cmd_text[LTE_FDD_ENB_RRC_CMD_N_ITEMS][20] = {"Release",
                                                                               "Security",
                                                                               "Setup Default DRB",
                                                                               "Setup Dedicated DRB"};
typedef struct{
    LTE_fdd_enb_user         *user;
    LTE_fdd_enb_rb           *rb;
    LTE_FDD_ENB_RRC_CMD_ENUM  cmd;
}LTE_FDD_ENB_RRC_CMD_READY_MSG_STRUCT;

// RRC -> MME Messages
typedef struct{
    LTE_fdd_enb_user *user;
    LTE_fdd_enb_rb   *rb;
}LTE_FDD_ENB_MME_NAS_MSG_READY_MSG_STRUCT;
typedef enum{
    LTE_FDD_ENB_MME_RRC_CMD_RESP_SECURITY = 0,
    LTE_FDD_ENB_MME_RRC_CMD_RESP_N_ITEMS,
}LTE_FDD_ENB_MME_RRC_CMD_RESP_ENUM;
static const char LTE_fdd_enb_mme_rrc_cmd_resp_text[LTE_FDD_ENB_MME_RRC_CMD_RESP_N_ITEMS][20] = {"Security"};
typedef struct{
    LTE_fdd_enb_user                  *user;
    LTE_fdd_enb_rb                    *rb;
    LTE_FDD_ENB_MME_RRC_CMD_RESP_ENUM  cmd_resp;
}LTE_FDD_ENB_MME_RRC_CMD_RESP_MSG_STRUCT;

// GW -> PDCP Messages
typedef struct{
    LTE_fdd_enb_user *user;
    LTE_fdd_enb_rb   *rb;
}LTE_FDD_ENB_PDCP_DATA_SDU_READY_MSG_STRUCT;

// PDCP -> GW Messages
typedef struct{
    LTE_fdd_enb_user *user;
    LTE_fdd_enb_rb   *rb;
}LTE_FDD_ENB_GW_DATA_READY_MSG_STRUCT;

typedef union{
    // Generic Messages

    // MAC -> PHY Messages
    LTE_FDD_ENB_PHY_SCHEDULE_MSG_STRUCT phy_schedule;

    // PHY -> MAC Messages
    LTE_FDD_ENB_READY_TO_SEND_MSG_STRUCT ready_to_send;
    LTE_FDD_ENB_PRACH_DECODE_MSG_STRUCT  prach_decode;
    LTE_FDD_ENB_PUCCH_DECODE_MSG_STRUCT  pucch_decode;
    LTE_FDD_ENB_PUSCH_DECODE_MSG_STRUCT  pusch_decode;

    // RLC -> MAC Messages
    LTE_FDD_ENB_MAC_SDU_READY_MSG_STRUCT mac_sdu_ready;

    // MAC -> RLC Messages
    LTE_FDD_ENB_RLC_PDU_READY_MSG_STRUCT rlc_pdu_ready;

    // MAC -> TIMER Messages
    LTE_FDD_ENB_TIMER_TICK_MSG_STRUCT timer_tick;

    // PDCP -> RLC Messages
    LTE_FDD_ENB_RLC_SDU_READY_MSG_STRUCT rlc_sdu_ready;

    // RLC -> PDCP Messages
    LTE_FDD_ENB_PDCP_PDU_READY_MSG_STRUCT pdcp_pdu_ready;

    // RRC -> PDCP Messages
    LTE_FDD_ENB_PDCP_SDU_READY_MSG_STRUCT pdcp_sdu_ready;

    // PDCP -> RRC Messages
    LTE_FDD_ENB_RRC_PDU_READY_MSG_STRUCT rrc_pdu_ready;

    // MME -> RRC Messages
    LTE_FDD_ENB_RRC_NAS_MSG_READY_MSG_STRUCT rrc_nas_msg_ready;
    LTE_FDD_ENB_RRC_CMD_READY_MSG_STRUCT     rrc_cmd_ready;

    // RRC -> MME Messages
    LTE_FDD_ENB_MME_NAS_MSG_READY_MSG_STRUCT mme_nas_msg_ready;
    LTE_FDD_ENB_MME_RRC_CMD_RESP_MSG_STRUCT  mme_rrc_cmd_resp;

    // GW -> PDCP Messages
    LTE_FDD_ENB_PDCP_DATA_SDU_READY_MSG_STRUCT pdcp_data_sdu_ready;

    // PDCP -> GW Messages
    LTE_FDD_ENB_GW_DATA_READY_MSG_STRUCT gw_data_ready;
}LTE_FDD_ENB_MESSAGE_UNION;

typedef struct{
    LTE_FDD_ENB_MESSAGE_TYPE_ENUM type;
    LTE_FDD_ENB_DEST_LAYER_ENUM   dest_layer;
    LTE_FDD_ENB_MESSAGE_UNION     msg;
}LTE_FDD_ENB_MESSAGE_STRUCT;

/*******************************************************************************
                              CLASS DECLARATIONS
*******************************************************************************/

// Message queue callback
class LTE_fdd_enb_msgq_cb
{
public:
    typedef void (*FuncType)(void*, LTE_FDD_ENB_MESSAGE_STRUCT&);
    LTE_fdd_enb_msgq_cb();
    LTE_fdd_enb_msgq_cb(FuncType f, void* o);
    void operator()(LTE_FDD_ENB_MESSAGE_STRUCT &msg);
private:
    FuncType  func;
    void     *obj;
};
template<class class_type, void (class_type::*Func)(LTE_FDD_ENB_MESSAGE_STRUCT&)>
    void LTE_fdd_enb_msgq_cb_wrapper(void *o, LTE_FDD_ENB_MESSAGE_STRUCT &msg)
{
    return (static_cast<class_type*>(o)->*Func)(msg);
}

class LTE_fdd_enb_msgq
{
public:
    LTE_fdd_enb_msgq(std::string _msgq_name);
    ~LTE_fdd_enb_msgq();

    // Setup
    void attach_rx(LTE_fdd_enb_msgq_cb cb);
    void attach_rx(LTE_fdd_enb_msgq_cb cb, uint32 _prio);

    // Send/Receive
    void send(LTE_FDD_ENB_MESSAGE_TYPE_ENUM  type,
              LTE_FDD_ENB_DEST_LAYER_ENUM    dest_layer,
              LTE_FDD_ENB_MESSAGE_UNION     *msg_content,
              uint32                         msg_content_size);
    void send(LTE_FDD_ENB_MESSAGE_TYPE_ENUM       type,
              LTE_FDD_ENB_DL_SCHEDULE_MSG_STRUCT *dl_sched,
              LTE_FDD_ENB_UL_SCHEDULE_MSG_STRUCT *ul_sched);
    void send(LTE_FDD_ENB_MESSAGE_STRUCT &msg);

private:
    // Send/Receive
    static void* receive_thread(void *inputs);

    // Variables
    LTE_fdd_enb_msgq_cb                                 callback;
    sem_t                                               sync_sem;
    sem_t                                               msg_sem;
    boost::circular_buffer<LTE_FDD_ENB_MESSAGE_STRUCT> *circ_buf;
    std::string                                         msgq_name;
    pthread_t                                           rx_thread;
    uint32                                              prio;
    bool                                                rx_setup;
};

#endif /* __LTE_FDD_ENB_MSGQ_H__ */
