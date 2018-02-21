#line 2 "LTE_fdd_enb_mac.cc" // Make __FILE__ omit the path
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

    File: LTE_fdd_enb_mac.cc

    Description: Contains all the implementations for the LTE FDD eNodeB
                 medium access control layer.

    Revision History
    ----------    -------------    --------------------------------------------
    11/10/2013    Ben Wojtowicz    Created file
    01/18/2014    Ben Wojtowicz    Cached a copy of the interface class, added
                                   real-time priority to PHY->MAC message queue,
                                   added level to debug prints, and fixed
                                   subframe scheduling.
    03/26/2014    Ben Wojtowicz    Fixed the uplink decode subframe scheduling.
    04/12/2014    Ben Wojtowicz    Using the latest LTE library.
    05/04/2014    Ben Wojtowicz    Added RLC communication handling and UL and
                                   DL CCCH message processing.
    06/15/2014    Ben Wojtowicz    Added uplink scheduling and changed fn_combo
                                   to current_tti.
    08/03/2014    Ben Wojtowicz    Removed debug message.
    09/03/2014    Ben Wojtowicz    Combined the contention resolution ID and
                                   the first downlink RLC message.
    11/01/2014    Ben Wojtowicz    Added NDI toggling.
    11/29/2014    Ben Wojtowicz    Using the byte message struct for SDUs.
    12/16/2014    Ben Wojtowicz    Added ol extension to message queues.
    12/24/2014    Ben Wojtowicz    Dynamically determining MCS for downlink
                                   data.
    02/15/2015    Ben Wojtowicz    Moved to new message queue, added support
                                   for higher order modulation schemes, fixed
                                   C-RNTI release, changed RTS timing, and
                                   added DL QoS TTI frequency.
    03/15/2015    Ben Wojtowicz    Fixed RTS issues.
    07/25/2015    Ben Wojtowicz    Combined the DL and UL schedule messages into
                                   a single PHY schedule message and using a
                                   local copy of LIBLTE_MAC_PDU_STRUCT.
    12/06/2015    Ben Wojtowicz    Changed boost::mutex to pthread_mutex_t and
                                   sem_t, added and used some helper functions,
                                   fixed an issue with RTS response on late
                                   subframes, and properly initialize allocation
                                   structure (thanks to Markus Grab for finding
                                   this).
    02/13/2016    Ben Wojtowicz    Added a user inactivity timer.
    03/12/2016    Ben Wojtowicz    Added PUCCH and H-ARQ support.
    07/03/2016    Ben Wojtowicz    Fixed memcpy lengths.
    07/31/2016    Ben Wojtowicz    Reworked the RTS logic and limited the number
                                   of HARQ retransmissions.
    07/29/2017    Ben Wojtowicz    Added SR support, added IPC direct to a UE
                                   MAC, using the latest LTE library, and
                                   refactored the UL scheduler from fixed
                                   scheduling to using SR and BSR.

*******************************************************************************/

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_fdd_enb_user_mgr.h"
#include "LTE_fdd_enb_timer_mgr.h"
#include "LTE_fdd_enb_mac.h"
#include "LTE_fdd_enb_phy.h"
#include "libtools_scoped_lock.h"

/*******************************************************************************
                              DEFINES
*******************************************************************************/

#define BSR_GRANT_SIZE_BYTES 10

/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/


/*******************************************************************************
                              GLOBAL VARIABLES
*******************************************************************************/

LTE_fdd_enb_mac*       LTE_fdd_enb_mac::instance = NULL;
static pthread_mutex_t mac_instance_mutex        = PTHREAD_MUTEX_INITIALIZER;

/*******************************************************************************
                              CLASS IMPLEMENTATIONS
*******************************************************************************/

/*******************/
/*    Singleton    */
/*******************/
LTE_fdd_enb_mac* LTE_fdd_enb_mac::get_instance(void)
{
    libtools_scoped_lock lock(mac_instance_mutex);

    if(NULL == instance)
    {
        instance = new LTE_fdd_enb_mac();
    }

    return(instance);
}
void LTE_fdd_enb_mac::cleanup(void)
{
    libtools_scoped_lock lock(mac_instance_mutex);

    if(NULL != instance)
    {
        delete instance;
        instance = NULL;
    }
}

/********************************/
/*    Constructor/Destructor    */
/********************************/
LTE_fdd_enb_mac::LTE_fdd_enb_mac()
{
    sem_init(&start_sem, 0, 1);
    sem_init(&rar_sched_queue_sem, 0, 1);
    sem_init(&dl_sched_queue_sem, 0, 1);
    sem_init(&ul_sched_queue_sem, 0, 1);
    sem_init(&ul_sr_sched_queue_sem, 0, 1);
    sem_init(&sys_info_sem, 0, 1);
    interface = NULL;
    started   = false;
}
LTE_fdd_enb_mac::~LTE_fdd_enb_mac()
{
    stop();
    sem_destroy(&sys_info_sem);
    sem_destroy(&ul_sr_sched_queue_sem);
    sem_destroy(&ul_sched_queue_sem);
    sem_destroy(&dl_sched_queue_sem);
    sem_destroy(&rar_sched_queue_sem);
    sem_destroy(&start_sem);
}

/********************/
/*    Start/Stop    */
/********************/
void LTE_fdd_enb_mac::start(LTE_fdd_enb_msgq      *from_phy,
                            LTE_fdd_enb_msgq      *from_rlc,
                            LTE_fdd_enb_msgq      *to_phy,
                            LTE_fdd_enb_msgq      *to_rlc,
                            LTE_fdd_enb_msgq      *to_timer,
                            bool                   direct_to_ue,
                            LTE_fdd_enb_interface *iface)
{
    libtools_scoped_lock  lock(start_sem);
    LTE_fdd_enb_msgq_cb   phy_cb(&LTE_fdd_enb_msgq_cb_wrapper<LTE_fdd_enb_mac, &LTE_fdd_enb_mac::handle_phy_msg>, this);
    LTE_fdd_enb_msgq_cb   rlc_cb(&LTE_fdd_enb_msgq_cb_wrapper<LTE_fdd_enb_mac, &LTE_fdd_enb_mac::handle_rlc_msg>, this);
    libtools_ipc_msgq_cb  ue_cb(&libtools_ipc_msgq_cb_wrapper<LTE_fdd_enb_mac, &LTE_fdd_enb_mac::handle_ue_msg>, this);
    LTE_fdd_enb_cnfg_db  *cnfg_db = LTE_fdd_enb_cnfg_db::get_instance();
    uint32                i;

    if(!started)
    {
        interface     = iface;
        started       = true;
        msgq_from_phy = from_phy;
        msgq_from_rlc = from_rlc;
        msgq_to_phy   = to_phy;
        msgq_to_rlc   = to_rlc;
        msgq_to_timer = to_timer;
        msgq_from_phy->attach_rx(phy_cb, 90);
        msgq_from_rlc->attach_rx(rlc_cb);

        if(direct_to_ue)
        {
            msgq_to_ue = new libtools_ipc_msgq("enb_ue", ue_cb);
        }else{
            msgq_to_ue = NULL;
        }

        // Scheduler
        cnfg_db->get_sys_info(sys_info);
        for(i=0; i<10; i++)
        {
            sched_dl_subfr[i].dl_allocations.N_alloc = 0;
            sched_dl_subfr[i].ul_allocations.N_alloc = 0;
            sched_dl_subfr[i].N_avail_prbs           = sys_info.N_rb_dl - get_n_reserved_prbs(i);
            sched_dl_subfr[i].N_sched_prbs           = 0;
            sched_dl_subfr[i].current_tti            = i;

            sched_ul_subfr[i].decodes.N_alloc = 0;
            sched_ul_subfr[i].N_avail_prbs    = sys_info.N_rb_ul;
            sched_ul_subfr[i].N_sched_prbs    = 0;
            sched_ul_subfr[i].current_tti     = i;
            sched_ul_subfr[i].N_pucch         = 0;
            sched_ul_subfr[i].next_prb        = 0;
        }
        sched_dl_subfr[0].current_tti = 10;
        sched_dl_subfr[1].current_tti = 11;
        sched_dl_subfr[2].current_tti = 12;
        sched_cur_dl_subfn            = 3;
        sched_cur_ul_subfn            = 0;
    }
}
void LTE_fdd_enb_mac::stop(void)
{
    libtools_scoped_lock lock(start_sem);

    if(started)
    {
        started = false;

        if(NULL != msgq_to_ue)
        {
            delete msgq_to_ue;
        }
    }
}

/***********************/
/*    Communication    */
/***********************/
void LTE_fdd_enb_mac::handle_phy_msg(LTE_FDD_ENB_MESSAGE_STRUCT &msg)
{
    if(LTE_FDD_ENB_DEST_LAYER_MAC == msg.dest_layer ||
       LTE_FDD_ENB_DEST_LAYER_ANY == msg.dest_layer)
    {
        switch(msg.type)
        {
        case LTE_FDD_ENB_MESSAGE_TYPE_READY_TO_SEND:
            handle_ready_to_send(&msg.msg.ready_to_send);
            break;
        case LTE_FDD_ENB_MESSAGE_TYPE_PRACH_DECODE:
            handle_prach_decode(&msg.msg.prach_decode);
            break;
        case LTE_FDD_ENB_MESSAGE_TYPE_PUCCH_DECODE:
            handle_pucch_decode(&msg.msg.pucch_decode);
            break;
        case LTE_FDD_ENB_MESSAGE_TYPE_PUSCH_DECODE:
            handle_pusch_decode(&msg.msg.pusch_decode);
            break;
        default:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_WARNING,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                      __FILE__,
                                      __LINE__,
                                      "Received invalid PHY message %s",
                                      LTE_fdd_enb_message_type_text[msg.type]);
            break;
        }
    }else{
        // Forward message to RLC
        msgq_to_rlc->send(msg);
    }
}
void LTE_fdd_enb_mac::handle_rlc_msg(LTE_FDD_ENB_MESSAGE_STRUCT &msg)
{
    if(LTE_FDD_ENB_DEST_LAYER_MAC == msg.dest_layer ||
       LTE_FDD_ENB_DEST_LAYER_ANY == msg.dest_layer)
    {
        switch(msg.type)
        {
        case LTE_FDD_ENB_MESSAGE_TYPE_MAC_SDU_READY:
            handle_sdu_ready(&msg.msg.mac_sdu_ready);
            break;
        default:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                      __FILE__,
                                      __LINE__,
                                      "Received invalid RLC message %s",
                                      LTE_fdd_enb_message_type_text[msg.type]);
            break;
        }
    }else{
        // Forward message to PHY
        msgq_to_phy->send(msg);
    }
}
void LTE_fdd_enb_mac::handle_ue_msg(LIBTOOLS_IPC_MSGQ_MESSAGE_STRUCT *msg)
{
    LTE_FDD_ENB_PRACH_DECODE_MSG_STRUCT prach_decode;
    LTE_FDD_ENB_PUSCH_DECODE_MSG_STRUCT pusch_decode;

    switch(msg->type)
    {
    case LIBTOOLS_IPC_MSGQ_MESSAGE_TYPE_KILL:
        msgq_to_ue = NULL;
        break;
    case LIBTOOLS_IPC_MSGQ_MESSAGE_TYPE_RACH:
        prach_decode.num_preambles = 1;
        prach_decode.current_tti   = sched_ul_subfr[sched_cur_ul_subfn].current_tti;
        prach_decode.timing_adv[0] = 0;
        prach_decode.preamble[0]   = msg->msg.rach.preamble;
        handle_prach_decode(&prach_decode);
        break;
    case LIBTOOLS_IPC_MSGQ_MESSAGE_TYPE_MAC_PDU:
        memcpy(&pusch_decode.msg, &msg->msg.mac_pdu_msg.msg, sizeof(pusch_decode.msg));
        pusch_decode.current_tti = sched_ul_subfr[sched_cur_ul_subfn].current_tti;
        pusch_decode.rnti        = msg->msg.mac_pdu_msg.rnti;
        handle_pusch_decode(&pusch_decode);
        break;
    default:
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                  __FILE__,
                                  __LINE__,
                                  "Received invalid UE message %s",
                                  libtools_ipc_msgq_message_type_text[msg->type]);
        break;
    }
}

/****************************/
/*    External Interface    */
/****************************/
void LTE_fdd_enb_mac::update_sys_info(void)
{
    libtools_scoped_lock  lock(sys_info_sem);
    LTE_fdd_enb_cnfg_db  *cnfg_db = LTE_fdd_enb_cnfg_db::get_instance();

    cnfg_db->get_sys_info(sys_info);
}
void LTE_fdd_enb_mac::add_periodic_sr_pucch(uint16 rnti,
                                            uint32 i_sr,
                                            uint32 n_1_p_pucch)
{
    LTE_FDD_ENB_UL_SR_SCHED_QUEUE_STRUCT *sr = NULL;

    sr = new LTE_FDD_ENB_UL_SR_SCHED_QUEUE_STRUCT;

    if(NULL != sr)
    {
        sr->i_sr        = i_sr;
        sr->n_1_p_pucch = n_1_p_pucch - 1;
        sr->rnti        = rnti;

        sem_wait(&ul_sr_sched_queue_sem);
        ul_sr_sched_queue.push_back(sr);
        sem_post(&ul_sr_sched_queue_sem);
    }
}
void LTE_fdd_enb_mac::remove_periodic_sr_pucch(uint16 rnti)
{
    std::list<LTE_FDD_ENB_UL_SR_SCHED_QUEUE_STRUCT *>::iterator  iter;
    LTE_FDD_ENB_UL_SR_SCHED_QUEUE_STRUCT                        *sr;

    sem_wait(&ul_sr_sched_queue_sem);
    for(iter=ul_sr_sched_queue.begin(); iter!=ul_sr_sched_queue.end(); iter++)
    {
        if(rnti == (*iter)->rnti)
        {
            sr = (*iter);
            ul_sr_sched_queue.erase(iter);
            delete sr;
            break;
        }
    }
    sem_post(&ul_sr_sched_queue_sem);
}

/**********************/
/*    PHY Handlers    */
/**********************/
void LTE_fdd_enb_mac::handle_ready_to_send(LTE_FDD_ENB_READY_TO_SEND_MSG_STRUCT *rts)
{
    LTE_fdd_enb_phy                   *phy = LTE_fdd_enb_phy::get_instance();
    LTE_FDD_ENB_TIMER_TICK_MSG_STRUCT  timer_tick;
    uint32                             i;
    uint32                             dl_tti;
    uint32                             ul_tti;
    int32                              num_subfrs_to_skip;

    // Send tick to timer manager
    msgq_to_timer->send(LTE_FDD_ENB_MESSAGE_TYPE_TIMER_TICK,
                        LTE_FDD_ENB_DEST_LAYER_TIMER_MGR,
                        (LTE_FDD_ENB_MESSAGE_UNION *)&timer_tick,
                        sizeof(LTE_FDD_ENB_TIMER_TICK_MSG_STRUCT));

    phy->get_current_ttis(&dl_tti, &ul_tti);
    if(2 != ((int32)(sched_dl_subfr[sched_cur_dl_subfn].current_tti) - (int32)(dl_tti)))
    {
        num_subfrs_to_skip = (int32)(sched_dl_subfr[sched_cur_dl_subfn].current_tti) - (int32)(dl_tti);
        if(1000 < fabs(num_subfrs_to_skip))
        {
            num_subfrs_to_skip = 0;
        }else if(0 > num_subfrs_to_skip){
            num_subfrs_to_skip  = -num_subfrs_to_skip;
            num_subfrs_to_skip += 3;
        }else{
            num_subfrs_to_skip = 0;
        }
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                  __FILE__,
                                  __LINE__,
                                  "MAC_dl_tti - PHY_dl_tti != 2 (%d), skipping %d subframes",
                                  (int32)(sched_dl_subfr[sched_cur_dl_subfn].current_tti) - (int32)(dl_tti),
                                  num_subfrs_to_skip);
        sem_wait(&sys_info_sem);
        for(i=0; i<num_subfrs_to_skip; i++)
        {
            // Advance the frame number combination
            sched_dl_subfr[sched_cur_dl_subfn].current_tti = add_to_tti(sched_dl_subfr[sched_cur_dl_subfn].current_tti, 10);
            sched_ul_subfr[sched_cur_ul_subfn].current_tti = add_to_tti(sched_ul_subfr[sched_cur_ul_subfn].current_tti, 10);

            // Clear the subframes
            sched_dl_subfr[sched_cur_dl_subfn].dl_allocations.N_alloc = 0;
            sched_dl_subfr[sched_cur_dl_subfn].ul_allocations.N_alloc = 0;
            sched_dl_subfr[sched_cur_dl_subfn].N_avail_prbs           = sys_info.N_rb_dl - get_n_reserved_prbs(sched_dl_subfr[sched_cur_dl_subfn].current_tti);
            sched_dl_subfr[sched_cur_dl_subfn].N_sched_prbs           = 0;
            sched_ul_subfr[sched_cur_ul_subfn].decodes.N_alloc        = 0;
            sched_ul_subfr[sched_cur_ul_subfn].N_sched_prbs           = 0;
            sched_ul_subfr[sched_cur_ul_subfn].N_pucch                = 0;
            sched_ul_subfr[sched_cur_ul_subfn].next_prb               = 0;

            // Advance the subframe numbers
            sched_cur_dl_subfn = (sched_cur_dl_subfn + 1) % 10;
            sched_cur_ul_subfn = (sched_cur_ul_subfn + 1) % 10;
        }
        sem_post(&sys_info_sem);
    }else if(rts->dl_current_tti == sched_dl_subfr[sched_cur_dl_subfn].current_tti &&
             rts->ul_current_tti == sched_ul_subfr[sched_cur_ul_subfn].current_tti){
        msgq_to_phy->send(LTE_FDD_ENB_MESSAGE_TYPE_PHY_SCHEDULE,
                          &sched_dl_subfr[sched_cur_dl_subfn],
                          &sched_ul_subfr[sched_cur_ul_subfn]);

        // Advance the frame number combination
        sched_dl_subfr[sched_cur_dl_subfn].current_tti = add_to_tti(sched_dl_subfr[sched_cur_dl_subfn].current_tti, 10);
        sched_ul_subfr[sched_cur_ul_subfn].current_tti = add_to_tti(sched_ul_subfr[sched_cur_ul_subfn].current_tti, 10);

        // Clear the subframes
        sem_wait(&sys_info_sem);
        sched_dl_subfr[sched_cur_dl_subfn].dl_allocations.N_alloc = 0;
        sched_dl_subfr[sched_cur_dl_subfn].ul_allocations.N_alloc = 0;
        sched_dl_subfr[sched_cur_dl_subfn].N_avail_prbs           = sys_info.N_rb_dl - get_n_reserved_prbs(sched_dl_subfr[sched_cur_dl_subfn].current_tti);
        sched_dl_subfr[sched_cur_dl_subfn].N_sched_prbs           = 0;
        sched_ul_subfr[sched_cur_ul_subfn].decodes.N_alloc        = 0;
        sched_ul_subfr[sched_cur_ul_subfn].N_sched_prbs           = 0;
        sched_ul_subfr[sched_cur_ul_subfn].N_pucch                = 0;
        sched_ul_subfr[sched_cur_ul_subfn].next_prb               = 0;
        sem_post(&sys_info_sem);

        // Advance the subframe numbers
        sched_cur_dl_subfn = (sched_cur_dl_subfn + 1) % 10;
        sched_cur_ul_subfn = (sched_cur_ul_subfn + 1) % 10;

        scheduler();
    }else{
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                  __FILE__,
                                  __LINE__,
                                  "RTS issue DL %u:%u, UL %u:%u",
                                  sched_dl_subfr[sched_cur_dl_subfn].current_tti,
                                  dl_tti,
                                  sched_ul_subfr[sched_cur_ul_subfn].current_tti,
                                  ul_tti);
    }
}
void LTE_fdd_enb_mac::handle_prach_decode(LTE_FDD_ENB_PRACH_DECODE_MSG_STRUCT *prach_decode)
{
    uint32 i;

    // Construct a random access response for each preamble
    for(i=0; i<prach_decode->num_preambles; i++)
    {
        construct_random_access_response(prach_decode->preamble[i],
                                         prach_decode->timing_adv[i],
                                         prach_decode->current_tti);
    }
}
void LTE_fdd_enb_mac::handle_pucch_decode(LTE_FDD_ENB_PUCCH_DECODE_MSG_STRUCT *pucch_decode)
{
    LTE_fdd_enb_user_mgr *user_mgr = LTE_fdd_enb_user_mgr::get_instance();
    LTE_fdd_enb_user     *user;

    if(LTE_FDD_ENB_ERROR_NONE == user_mgr->find_user(pucch_decode->rnti, &user))
    {
        if(LTE_FDD_ENB_PUCCH_TYPE_ACK_NACK == pucch_decode->type)
        {
            handle_pucch_ack_nack(user,
                                  pucch_decode->current_tti,
                                  &pucch_decode->msg);
        }else{
            handle_pucch_sr(user,
                            pucch_decode->current_tti,
                            &pucch_decode->msg);
        }
    }else{
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                  __FILE__,
                                  __LINE__,
                                  "Failed to find PUCCH user RNTI=%u TTI=%u",
                                  pucch_decode->rnti,
                                  pucch_decode->current_tti);
    }
}
void LTE_fdd_enb_mac::handle_pucch_ack_nack(LTE_fdd_enb_user      *user,
                                            uint32                 current_tti,
                                            LIBLTE_BIT_MSG_STRUCT *msg)
{
    LIBLTE_MAC_PDU_STRUCT        mac_pdu;
    LIBLTE_PHY_ALLOCATION_STRUCT alloc;

    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                              __FILE__,
                              __LINE__,
                              "PUCCH ACK/NACK received for %u RNTI=%u",
                              current_tti,
                              user->get_c_rnti());

    if(msg->msg[0])
    {
        user->clear_harq_info(current_tti);
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                  __FILE__,
                                  __LINE__,
                                  "Clearing HARQ info RNTI=%u TTI=%u",
                                  user->get_c_rnti(),
                                  current_tti);
    }else{
        if(LTE_FDD_ENB_ERROR_NONE == user->get_harq_info(current_tti,
                                                         &mac_pdu,
                                                         &alloc))
        {
            if(LTE_FDD_ENB_MAX_HARQ_RETX <= alloc.harq_retx_count)
            {
                interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                          LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                          __FILE__,
                                          __LINE__,
                                          "Not resending HARQ due to max retx RNTI=%u TTI=%u",
                                          user->get_c_rnti(),
                                          current_tti);
            }else{
                alloc.harq_retx_count++;
                alloc.ndi = user->get_dl_ndi();
                if(LTE_FDD_ENB_ERROR_NONE == add_to_dl_sched_queue(add_to_tti(sched_dl_subfr[sched_cur_dl_subfn].current_tti,
                                                                              4),
                                                                   &mac_pdu,
                                                                   &alloc))
                {
                    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                              LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                              __FILE__,
                                              __LINE__,
                                              "Resending HARQ info RNTI=%u TTI=%u (%u)",
                                              user->get_c_rnti(),
                                              current_tti,
                                              alloc.harq_retx_count);
                }else{
                    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                              LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                              __FILE__,
                                              __LINE__,
                                              "Failed to resend HARQ info RNTI=%u TTI=%u",
                                              user->get_c_rnti(),
                                              current_tti);
                }
            }
        }else{
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                      __FILE__,
                                      __LINE__,
                                      "Failed to find HARQ info RNTI=%u TTI=%u",
                                      user->get_c_rnti(),
                                      current_tti);
        }
    }
}
void LTE_fdd_enb_mac::handle_pucch_sr(LTE_fdd_enb_user      *user,
                                      uint32                 current_tti,
                                      LIBLTE_BIT_MSG_STRUCT *msg)
{
    if(msg->msg[0])
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                  __FILE__,
                                  __LINE__,
                                  "PUCCH SR received for %u RNTI=%u",
                                  current_tti,
                                  user->get_c_rnti());

        // Schedule a grant big enough to at least hold long BSR
        sched_ul(user, BSR_GRANT_SIZE_BYTES*8);
    }
}
void LTE_fdd_enb_mac::handle_pusch_decode(LTE_FDD_ENB_PUSCH_DECODE_MSG_STRUCT *pusch_decode)
{
    LTE_fdd_enb_user_mgr  *user_mgr = LTE_fdd_enb_user_mgr::get_instance();
    LTE_fdd_enb_user      *user     = NULL;
    LIBLTE_MAC_PDU_STRUCT  mac_pdu;
    uint32                 i;

    // Find the user
    if(LTE_FDD_ENB_ERROR_NONE == user_mgr->find_user(pusch_decode->rnti, &user))
    {
        // Reset the C-RNTI release timer
        user_mgr->reset_c_rnti_timer(pusch_decode->rnti);

        // Reset the inactivity timer
        user->reset_inactivity_timer(LTE_FDD_ENB_USER_INACTIVITY_TIMER_VALUE_MS);

        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                  __FILE__,
                                  __LINE__,
                                  &pusch_decode->msg,
                                  "PUSCH decode for RNTI=%u CURRENT_TTI=%u",
                                  pusch_decode->rnti,
                                  pusch_decode->current_tti);
        interface->send_lte_pcap_msg(LTE_FDD_ENB_PCAP_DIRECTION_UL,
                                     pusch_decode->rnti,
                                     pusch_decode->current_tti,
                                     pusch_decode->msg.msg,
                                     pusch_decode->msg.N_bits);

        // Set the correct channel type
        mac_pdu.chan_type = LIBLTE_MAC_CHAN_TYPE_ULSCH;

        // Unpack MAC PDU
        liblte_mac_unpack_mac_pdu(&pusch_decode->msg,
                                  false, // Simultaneous PUCCH/PUSCH
                                  &mac_pdu);

        for(i=0; i<mac_pdu.N_subheaders; i++)
        {
            if(LIBLTE_MAC_ULSCH_CCCH_LCID == mac_pdu.subheader[i].lcid)
            {
                handle_ulsch_ccch_sdu(user, mac_pdu.subheader[i].lcid, &mac_pdu.subheader[i].payload.sdu);
            }else if(LIBLTE_MAC_ULSCH_DCCH_LCID_BEGIN <= mac_pdu.subheader[i].lcid &&
                     LIBLTE_MAC_ULSCH_DCCH_LCID_END   >= mac_pdu.subheader[i].lcid){
                handle_ulsch_dcch_sdu(user, mac_pdu.subheader[i].lcid, &mac_pdu.subheader[i].payload.sdu);
            }else if(LIBLTE_MAC_ULSCH_EXT_POWER_HEADROOM_REPORT_LCID == mac_pdu.subheader[i].lcid){
                handle_ulsch_ext_power_headroom_report(user, &mac_pdu.subheader[i].payload.ext_power_headroom);
            }else if(LIBLTE_MAC_ULSCH_POWER_HEADROOM_REPORT_LCID == mac_pdu.subheader[i].lcid){
                handle_ulsch_power_headroom_report(user, &mac_pdu.subheader[i].payload.power_headroom);
            }else if(LIBLTE_MAC_ULSCH_C_RNTI_LCID == mac_pdu.subheader[i].lcid){
                handle_ulsch_c_rnti(&user, &mac_pdu.subheader[i].payload.c_rnti);
            }else if(LIBLTE_MAC_ULSCH_TRUNCATED_BSR_LCID == mac_pdu.subheader[i].lcid){
                handle_ulsch_truncated_bsr(user, &mac_pdu.subheader[i].payload.truncated_bsr);
            }else if(LIBLTE_MAC_ULSCH_SHORT_BSR_LCID == mac_pdu.subheader[i].lcid){
                handle_ulsch_short_bsr(user, &mac_pdu.subheader[i].payload.short_bsr);
            }else if(LIBLTE_MAC_ULSCH_LONG_BSR_LCID == mac_pdu.subheader[i].lcid){
                handle_ulsch_long_bsr(user, &mac_pdu.subheader[i].payload.long_bsr);
            }
        }
    }else{
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                  __FILE__,
                                  __LINE__,
                                  &pusch_decode->msg,
                                  "PUSCH decode for invalid RNTI (%u)",
                                  pusch_decode->rnti);
    }
}

/******************************/
/*    RLC Message Handlers    */
/******************************/
void LTE_fdd_enb_mac::handle_sdu_ready(LTE_FDD_ENB_MAC_SDU_READY_MSG_STRUCT *sdu_ready)
{
    LTE_fdd_enb_user             *user;
    LIBLTE_MAC_PDU_STRUCT         mac_pdu;
    LIBLTE_PHY_ALLOCATION_STRUCT  alloc = {0};
    LIBLTE_BYTE_MSG_STRUCT       *sdu;

    if(LTE_FDD_ENB_ERROR_NONE == sdu_ready->rb->get_next_mac_sdu(&sdu))
    {
        user = sdu_ready->user;

        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                  __FILE__,
                                  __LINE__,
                                  sdu,
                                  "Received SDU for RNTI=%u and RB=%s",
                                  user->get_c_rnti(),
                                  LTE_fdd_enb_rb_text[sdu_ready->rb->get_rb_id()]);

        // Fill in the allocation
        alloc.pre_coder_type = LIBLTE_PHY_PRE_CODER_TYPE_TX_DIVERSITY;
        alloc.mod_type       = LIBLTE_PHY_MODULATION_TYPE_QPSK;
        alloc.chan_type      = LIBLTE_PHY_CHAN_TYPE_DLSCH;
        alloc.rv_idx         = 0;
        alloc.N_codewords    = 1;
        sem_wait(&sys_info_sem);
        if(1 == sys_info.N_ant)
        {
            alloc.tx_mode = 1;
        }else{
            alloc.tx_mode = 2;
        }
        sem_post(&sys_info_sem);
        alloc.rnti = user->get_c_rnti();
        alloc.tpc  = LIBLTE_PHY_TPC_COMMAND_DCI_1_1A_1B_1D_2_3_DB_ZERO;
        alloc.ndi  = user->get_dl_ndi();
        user->flip_dl_ndi();
        alloc.harq_retx_count = 0;

        // Pack the PDU
        mac_pdu.chan_type = LIBLTE_MAC_CHAN_TYPE_DLSCH;
        if(sdu_ready->rb->get_send_con_res_id())
        {
            mac_pdu.N_subheaders                          = 2;
            mac_pdu.subheader[0].lcid                     = LIBLTE_MAC_DLSCH_UE_CONTENTION_RESOLUTION_ID_LCID;
            mac_pdu.subheader[0].payload.ue_con_res_id.id = sdu_ready->rb->get_con_res_id();
            mac_pdu.subheader[1].lcid                     = sdu_ready->rb->get_rb_id();
            memcpy(&mac_pdu.subheader[1].payload.sdu, sdu, sizeof(LIBLTE_BYTE_MSG_STRUCT));
            sdu_ready->rb->set_send_con_res_id(false);
        }else{
            mac_pdu.N_subheaders      = 1;
            mac_pdu.subheader[0].lcid = sdu_ready->rb->get_rb_id();
            memcpy(&mac_pdu.subheader[0].payload.sdu, sdu, sizeof(LIBLTE_BYTE_MSG_STRUCT));
        }

        // Add the PDU to the scheduling queue
        if(LTE_FDD_ENB_ERROR_NONE != add_to_dl_sched_queue(add_to_tti(sched_dl_subfr[sched_cur_dl_subfn].current_tti,
                                                                      4),
                                                           &mac_pdu,
                                                           &alloc))
        {
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                      __FILE__,
                                      __LINE__,
                                      "Can't schedule PDU");
        }else{
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                      __FILE__,
                                      __LINE__,
                                      &alloc.msg[0],
                                      "PDU scheduled for RNTI=%u, DL_QUEUE_SIZE=%u",
                                      alloc.rnti,
                                      dl_sched_queue.size());
        }

        // Delete the SDU
        sdu_ready->rb->delete_next_mac_sdu();
    }else{
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                  __FILE__,
                                  __LINE__,
                                  "Received sdu_ready message with no SDU queued");
    }
}

/**************************/
/*    MAC PDU Handlers    */
/**************************/
void LTE_fdd_enb_mac::handle_ulsch_ccch_sdu(LTE_fdd_enb_user       *user,
                                            uint32                  lcid,
                                            LIBLTE_BYTE_MSG_STRUCT *sdu)
{
    LTE_fdd_enb_rb                       *rb = NULL;
    LTE_FDD_ENB_RLC_PDU_READY_MSG_STRUCT  rlc_pdu_ready;
    uint64                                con_res_id;
    uint32                                i;

    if(LIBLTE_MAC_ULSCH_CCCH_LCID == lcid)
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                  __FILE__,
                                  __LINE__,
                                  sdu,
                                  "Handling ULSCH SDU for RNTI=%u, LCID=%u",
                                  user->get_c_rnti(),
                                  lcid);

        // Get SRB0
        user->get_srb0(&rb);

        // Save the contention resolution ID
        con_res_id = 0;
        for(i=0; i<sdu->N_bytes; i++)
        {
            con_res_id <<= 8;
            con_res_id  |= sdu->msg[i];
        }
        rb->set_con_res_id(con_res_id);
        rb->set_send_con_res_id(true);

        // Queue the PDU for RLC
        rb->queue_rlc_pdu(sdu);

        // Signal RLC
        rlc_pdu_ready.user = user;
        rlc_pdu_ready.rb   = rb;
        msgq_to_rlc->send(LTE_FDD_ENB_MESSAGE_TYPE_RLC_PDU_READY,
                          LTE_FDD_ENB_DEST_LAYER_RLC,
                          (LTE_FDD_ENB_MESSAGE_UNION *)&rlc_pdu_ready,
                          sizeof(LTE_FDD_ENB_RLC_PDU_READY_MSG_STRUCT));

        // Update the uplink buffer size
        user->update_ul_buffer_size(sdu->N_bytes);

        // Schedule uplink
        sched_ul(user, user->get_ul_buffer_size()*8);
    }else{
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                  __FILE__,
                                  __LINE__,
                                  sdu,
                                  "Not handling ULSCH SDU for RNTI=%u, LCID=%u",
                                  user->get_c_rnti(),
                                  lcid);
    }
}
void LTE_fdd_enb_mac::handle_ulsch_dcch_sdu(LTE_fdd_enb_user       *user,
                                            uint32                  lcid,
                                            LIBLTE_BYTE_MSG_STRUCT *sdu)
{
    LTE_fdd_enb_rb                       *rb = NULL;
    LTE_FDD_ENB_RLC_PDU_READY_MSG_STRUCT  rlc_pdu_ready;

    if(LIBLTE_MAC_ULSCH_DCCH_LCID_BEGIN <= lcid &&
       LIBLTE_MAC_ULSCH_DCCH_LCID_END   >= lcid)
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                  __FILE__,
                                  __LINE__,
                                  sdu,
                                  "Handling ULSCH DCCH_SDU for RNTI=%u, LCID=%u",
                                  user->get_c_rnti(),
                                  lcid);

        // Get RB
        if(LTE_FDD_ENB_RB_SRB1 == lcid)
        {
            user->get_srb1(&rb);
        }else if(LTE_FDD_ENB_RB_SRB2 == lcid){
            user->get_srb2(&rb);
        }else if(LTE_FDD_ENB_ERROR_NONE != user->get_drb((LTE_FDD_ENB_RB_ENUM)lcid, &rb)){
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                      __FILE__,
                                      __LINE__,
                                      "Not handling ULSCH DCCH_SDU for RNTI=%u, LCID=%u",
                                      user->get_c_rnti(),
                                      lcid);
        }

        // Queue the SDU for RLC
        if(NULL != rb)
        {
            rb->queue_rlc_pdu(sdu);

            // Signal RLC
            rlc_pdu_ready.user = user;
            rlc_pdu_ready.rb   = rb;
            msgq_to_rlc->send(LTE_FDD_ENB_MESSAGE_TYPE_RLC_PDU_READY,
                              LTE_FDD_ENB_DEST_LAYER_RLC,
                              (LTE_FDD_ENB_MESSAGE_UNION *)&rlc_pdu_ready,
                              sizeof(LTE_FDD_ENB_RLC_PDU_READY_MSG_STRUCT));
        }

        // Update the uplink buffer size
        user->update_ul_buffer_size(sdu->N_bytes);

        // Schedule uplink
        sched_ul(user, user->get_ul_buffer_size()*8);
    }else{
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                  __FILE__,
                                  __LINE__,
                                  sdu,
                                  "Not handling ULSCH SDU for RNTI=%u, LCID=%u",
                                  user->get_c_rnti(),
                                  lcid);
    }
}
void LTE_fdd_enb_mac::handle_ulsch_ext_power_headroom_report(LTE_fdd_enb_user                        *user,
                                                             LIBLTE_MAC_EXT_POWER_HEADROOM_CE_STRUCT *ext_power_headroom)
{
    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                              LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                              __FILE__,
                              __LINE__,
                              "Not handling ULSCH EXTENDED_POWER_HEADROOM_REPORT");
}
void LTE_fdd_enb_mac::handle_ulsch_power_headroom_report(LTE_fdd_enb_user                    *user,
                                                         LIBLTE_MAC_POWER_HEADROOM_CE_STRUCT *power_headroom)
{
    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                              LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                              __FILE__,
                              __LINE__,
                              "Not handling ULSCH POWER_HEADROOM_REPORT");
}
void LTE_fdd_enb_mac::handle_ulsch_c_rnti(LTE_fdd_enb_user            **user,
                                          LIBLTE_MAC_C_RNTI_CE_STRUCT  *c_rnti)
{
    LTE_fdd_enb_user_mgr *user_mgr   = LTE_fdd_enb_user_mgr::get_instance();
    uint16                old_c_rnti = (*user)->get_c_rnti();

    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                              LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                              __FILE__,
                              __LINE__,
                              "Received C_RNTI=%u for C_RNTI=%u",
                              c_rnti->c_rnti,
                              (*user)->get_c_rnti());

    if(c_rnti->c_rnti         != (*user)->get_c_rnti() &&
       LTE_FDD_ENB_ERROR_NONE == user_mgr->find_user(c_rnti->c_rnti, user))
    {
        user_mgr->release_c_rnti(old_c_rnti);
    }
}
void LTE_fdd_enb_mac::handle_ulsch_truncated_bsr(LTE_fdd_enb_user                   *user,
                                                 LIBLTE_MAC_TRUNCATED_BSR_CE_STRUCT *truncated_bsr)
{
    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                              __FILE__,
                              __LINE__,
                              "Received truncated BSR for LCG_ID=%u (%u < Buffer Size <= %u), C_RNTI=%u",
                              truncated_bsr->lcg_id,
                              truncated_bsr->min_buffer_size,
                              truncated_bsr->max_buffer_size,
                              user->get_c_rnti());

    user->set_ul_buffer_size(truncated_bsr->max_buffer_size);

    sched_ul(user, truncated_bsr->max_buffer_size*8);
}
void LTE_fdd_enb_mac::handle_ulsch_short_bsr(LTE_fdd_enb_user               *user,
                                             LIBLTE_MAC_SHORT_BSR_CE_STRUCT *short_bsr)
{
    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                              __FILE__,
                              __LINE__,
                              "Received short BSR for LCG_ID=%u (%u < Buffer Size <= %u), C_RNTI=%u",
                              short_bsr->lcg_id,
                              short_bsr->min_buffer_size,
                              short_bsr->max_buffer_size,
                              user->get_c_rnti());

    user->set_ul_buffer_size(short_bsr->max_buffer_size);

    sched_ul(user, short_bsr->max_buffer_size*8);
}
void LTE_fdd_enb_mac::handle_ulsch_long_bsr(LTE_fdd_enb_user              *user,
                                            LIBLTE_MAC_LONG_BSR_CE_STRUCT *long_bsr)
{
    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                              __FILE__,
                              __LINE__,
                              "Received long BSR (%u < Buffer Size 0 <= %u), (%u < Buffer Size 1 <= %u), (%u < Buffer Size 2 <= %u), (%u < Buffer Size 3 <= %u), C_RNTI=%u",
                              long_bsr->min_buffer_size_0,
                              long_bsr->max_buffer_size_0,
                              long_bsr->min_buffer_size_1,
                              long_bsr->max_buffer_size_1,
                              long_bsr->min_buffer_size_2,
                              long_bsr->max_buffer_size_2,
                              long_bsr->min_buffer_size_3,
                              long_bsr->max_buffer_size_3,
                              user->get_c_rnti());

    user->set_ul_buffer_size(long_bsr->max_buffer_size_0 +
                             long_bsr->max_buffer_size_1 +
                             long_bsr->max_buffer_size_2 +
                             long_bsr->max_buffer_size_3);

    sched_ul(user, (long_bsr->max_buffer_size_0 +
                    long_bsr->max_buffer_size_1 +
                    long_bsr->max_buffer_size_2 +
                    long_bsr->max_buffer_size_3)*8);
}

/***************************/
/*    Data Constructors    */
/***************************/
void LTE_fdd_enb_mac::construct_random_access_response(uint8  preamble,
                                                       uint16 timing_adv,
                                                       uint32 current_tti)
{
    LTE_fdd_enb_user_mgr         *user_mgr = LTE_fdd_enb_user_mgr::get_instance();
    LTE_fdd_enb_user             *user     = NULL;
    LIBLTE_MAC_RAR_STRUCT         rar;
    LIBLTE_PHY_ALLOCATION_STRUCT  dl_alloc;
    LIBLTE_PHY_ALLOCATION_STRUCT  ul_alloc;

    // Allocate a user
    if(LTE_FDD_ENB_ERROR_NONE == user_mgr->add_user(&user))
    {
        // Save C-RNTI
        rar.temp_c_rnti = user->get_c_rnti();

        // Fill in the DL allocation
        dl_alloc.pre_coder_type = LIBLTE_PHY_PRE_CODER_TYPE_TX_DIVERSITY;
        dl_alloc.mod_type       = LIBLTE_PHY_MODULATION_TYPE_QPSK;
        dl_alloc.chan_type      = LIBLTE_PHY_CHAN_TYPE_DLSCH;
        dl_alloc.rv_idx         = 0;
        dl_alloc.N_codewords    = 1;
        dl_alloc.tx_mode        = 1; // From 36.213 v10.3.0 section 7.1
        dl_alloc.rnti           = 1 + current_tti%10 + 0*10; // FIXME: What is f_id

        // Fill in the UL allocation
        ul_alloc.pre_coder_type = LIBLTE_PHY_PRE_CODER_TYPE_TX_DIVERSITY;
        ul_alloc.mod_type       = LIBLTE_PHY_MODULATION_TYPE_QPSK;
        ul_alloc.chan_type      = LIBLTE_PHY_CHAN_TYPE_ULSCH;
        ul_alloc.rv_idx         = 0; // From 36.213 v10.3.0 section 8.6.1
        ul_alloc.N_codewords    = 1;
        ul_alloc.N_layers       = 1;
        ul_alloc.tx_mode        = 1; // From 36.213 v10.3.0 section 7.1
        ul_alloc.rnti           = rar.temp_c_rnti;
        sem_wait(&sys_info_sem);
        liblte_phy_get_tbs_mcs_and_n_prb_for_ul(56,
                                                sys_info.N_rb_ul,
                                                &ul_alloc.tbs,
                                                &ul_alloc.mcs,
                                                &ul_alloc.N_prb);
        sem_post(&sys_info_sem);

        // Fill in the RAR
        rar.hdr_type       = LIBLTE_MAC_RAR_HEADER_TYPE_RAPID;
        rar.hopping_flag   = LIBLTE_MAC_RAR_HOPPING_DISABLED;
        rar.tpc_command    = LIBLTE_MAC_RAR_TPC_COMMAND_0dB;
        rar.ul_delay       = LIBLTE_MAC_RAR_UL_DELAY_DISABLED;
        rar.csi_req        = LIBLTE_MAC_RAR_CSI_REQ_DISABLED;
        rar.timing_adv_cmd = timing_adv;
        rar.mcs            = ul_alloc.mcs;
        rar.RAPID          = preamble;

        // Temporarily pack the RAR to determine the number of bits needed
        liblte_mac_pack_random_access_response_pdu(&rar,
                                                   &dl_alloc.msg[0]);

        // Add the RAR to the scheduling queue
        if(LTE_FDD_ENB_ERROR_NONE != add_to_rar_sched_queue(current_tti,
                                                            &dl_alloc,
                                                            &ul_alloc,
                                                            &rar))
        {
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                      __FILE__,
                                      __LINE__,
                                      "Can't schedule RAR");
        }else{
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                      __FILE__,
                                      __LINE__,
                                      "RAR scheduled %u",
                                      rar_sched_queue.size());
        }
    }else{
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                  __FILE__,
                                  __LINE__,
                                  "No free C-RNTI or add_user fail");
    }
}

/*******************/
/*    Scheduler    */
/*******************/
void LTE_fdd_enb_mac::sched_ul(LTE_fdd_enb_user *user,
                               uint32            requested_tbs)
{
    LIBLTE_PHY_ALLOCATION_STRUCT alloc;
    uint32                       bits_req = requested_tbs;

    if(requested_tbs != 0)
    {
        alloc.pre_coder_type = LIBLTE_PHY_PRE_CODER_TYPE_TX_DIVERSITY;
        alloc.mod_type       = LIBLTE_PHY_MODULATION_TYPE_QPSK;
        alloc.chan_type      = LIBLTE_PHY_CHAN_TYPE_ULSCH;
        alloc.rv_idx         = 0;
        alloc.N_codewords    = 1;
        alloc.N_layers       = 1;
        alloc.tx_mode        = 1;
        alloc.rnti           = user->get_c_rnti();
        alloc.tpc            = LIBLTE_PHY_TPC_COMMAND_DCI_0_3_4_DB_NEG_1;
        alloc.ndi            = user->get_ul_ndi();
        user->flip_ul_ndi();
        if(bits_req > (user->get_max_ul_bytes_per_subfn()*8))
        {
            bits_req = user->get_max_ul_bytes_per_subfn()*8;
        }
        sem_wait(&sys_info_sem);
        liblte_phy_get_tbs_mcs_and_n_prb_for_ul(bits_req,
                                                sys_info.N_rb_ul,
                                                &alloc.tbs,
                                                &alloc.mcs,
                                                &alloc.N_prb);
        sem_post(&sys_info_sem);
        if(11 > alloc.mcs)
        {
            alloc.mod_type = LIBLTE_PHY_MODULATION_TYPE_QPSK;
        }else if(21 > alloc.mcs){
            alloc.mod_type = LIBLTE_PHY_MODULATION_TYPE_16QAM;
        }else{
            alloc.mod_type = LIBLTE_PHY_MODULATION_TYPE_64QAM;
        }

        // Add the allocation to the scheduling queue
        if(LTE_FDD_ENB_ERROR_NONE != add_to_ul_sched_queue(add_to_tti(sched_ul_subfr[sched_cur_ul_subfn].current_tti,
                                                                      4),
                                                           &alloc))
        {
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                      __FILE__,
                                      __LINE__,
                                      "Can't schedule UL");
        }else{
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                      __FILE__,
                                      __LINE__,
                                      "UL scheduled (mcs=%u, tbs=%u, N_prb=%u) for RNTI=%u, UL_QUEUE_SIZE=%u",
                                      alloc.mcs,
                                      alloc.tbs,
                                      alloc.N_prb,
                                      alloc.rnti,
                                      ul_sched_queue.size());
        }
    }
}
void LTE_fdd_enb_mac::scheduler(void)
{
    libtools_scoped_lock                                         lock(sys_info_sem);
    LTE_fdd_enb_phy                                             *phy      = LTE_fdd_enb_phy::get_instance();
    LTE_fdd_enb_user_mgr                                        *user_mgr = LTE_fdd_enb_user_mgr::get_instance();
    LTE_fdd_enb_user                                            *user;
    std::list<LTE_FDD_ENB_UL_SR_SCHED_QUEUE_STRUCT *>::iterator  iter;
    LTE_FDD_ENB_RAR_SCHED_QUEUE_STRUCT                          *rar_sched;
    LTE_FDD_ENB_DL_SCHED_QUEUE_STRUCT                           *dl_sched;
    LTE_FDD_ENB_UL_SCHED_QUEUE_STRUCT                           *ul_sched;
    LTE_FDD_ENB_DL_SCHEDULE_MSG_STRUCT                          *dl_subfr;
    LTE_FDD_ENB_UL_SCHEDULE_MSG_STRUCT                          *ul_subfr;
    LIBTOOLS_IPC_MSGQ_RAR_PDU_MSG_STRUCT                         rar_pdu_msg;
    LIBTOOLS_IPC_MSGQ_UL_ALLOC_MSG_STRUCT                        ul_alloc_msg;
    uint32                                                       N_cce;
    uint32                                                       N_pad;
    uint32                                                       resp_win_start;
    uint32                                                       resp_win_stop;
    uint32                                                       i;
    uint32                                                       rb_start;
    uint32                                                       riv;
    uint32                                                       mask;
    uint32                                                       sr_periodicity;
    uint32                                                       N_offset_sr;
    int32                                                        N_avail_dl_prbs;
    int32                                                        N_avail_ul_prbs;
    int32                                                        N_avail_dcis;
    bool                                                         sched_out_of_headroom;

    // Get the number of CCEs for the next subframe
    N_cce = phy->get_n_cce();

    // Schedule RAR for the next subframe
    sem_wait(&rar_sched_queue_sem);
    sched_out_of_headroom = false;
    while(0     != rar_sched_queue.size() &&
          false == sched_out_of_headroom)
    {
        rar_sched = rar_sched_queue.front();

        // Determine when the response window starts
        resp_win_start = add_to_tti(rar_sched->current_tti, 3);

        // Determine when the response window stops
        resp_win_stop = add_to_tti(resp_win_start,
                                   liblte_rrc_ra_response_window_size_num[sys_info.sib2.rr_config_common_sib.rach_cnfg.ra_resp_win_size]);

        // Take into account the SFN wrap
        // FIXME: Test this
        if(resp_win_start                                 <  LIBLTE_PHY_SFN_MAX*10 &&
           sched_dl_subfr[sched_cur_dl_subfn].current_tti >= LIBLTE_PHY_SFN_MAX*10)
        {
            resp_win_start += LIBLTE_PHY_SFN_MAX*10;
        }
        if(resp_win_stop                                  <  LIBLTE_PHY_SFN_MAX*10 &&
           sched_dl_subfr[sched_cur_dl_subfn].current_tti >= LIBLTE_PHY_SFN_MAX*10)
        {
            resp_win_stop += LIBLTE_PHY_SFN_MAX*10;
        }

        // Check to see if this current_tti falls in the response window
        if(resp_win_start <= sched_dl_subfr[sched_cur_dl_subfn].current_tti &&
           resp_win_stop  >= sched_dl_subfr[sched_cur_dl_subfn].current_tti)
        {
            dl_subfr = &sched_dl_subfr[sched_cur_dl_subfn];
            ul_subfr = &sched_ul_subfr[(sched_cur_dl_subfn+6)%10];

            // Determine how many PRBs are needed for the DL allocation, if using this subframe
            interface->send_lte_pcap_msg(LTE_FDD_ENB_PCAP_DIRECTION_DL,
                                         rar_sched->dl_alloc.rnti,
                                         dl_subfr->current_tti,
                                         rar_sched->dl_alloc.msg[0].msg,
                                         rar_sched->dl_alloc.msg[0].N_bits);
            liblte_phy_get_tbs_mcs_and_n_prb_for_dl(rar_sched->dl_alloc.msg[0].N_bits,
                                                    sched_cur_dl_subfn,
                                                    sys_info.N_rb_dl,
                                                    rar_sched->dl_alloc.rnti,
                                                    &rar_sched->dl_alloc.tbs,
                                                    &rar_sched->dl_alloc.mcs,
                                                    &rar_sched->dl_alloc.N_prb);

            // Determine how many PRBs and DCIs are available in this subframe
            N_avail_dl_prbs = dl_subfr->N_avail_prbs - dl_subfr->N_sched_prbs;
            N_avail_ul_prbs = ul_subfr->N_avail_prbs - ul_subfr->N_sched_prbs;
            N_avail_dcis    = N_cce - (dl_subfr->dl_allocations.N_alloc + dl_subfr->ul_allocations.N_alloc);

            if(rar_sched->dl_alloc.N_prb <= N_avail_dl_prbs &&
               rar_sched->ul_alloc.N_prb <= N_avail_ul_prbs &&
               1                         <= N_avail_dcis)
            {
                // Determine the RB start for the UL allocation
                rb_start            = ul_subfr->next_prb;
                ul_subfr->next_prb += rar_sched->ul_alloc.N_prb;

                // Fill in the PRBs for the UL allocation
                for(i=0; i<rar_sched->ul_alloc.N_prb; i++)
                {
                    rar_sched->ul_alloc.prb[0][i] = rb_start+i;
                    rar_sched->ul_alloc.prb[1][i] = rb_start+i;
                }

                // Determine the RIV for the UL and re-pack the RAR
                if((rar_sched->ul_alloc.N_prb-1) <= (sys_info.N_rb_ul/2))
                {
                    riv = sys_info.N_rb_ul*(rar_sched->ul_alloc.N_prb - 1) + rb_start;
                }else{
                    riv = sys_info.N_rb_ul*(sys_info.N_rb_ul - rar_sched->ul_alloc.N_prb + 1) + (sys_info.N_rb_ul - 1 - rb_start);
                }

                // Fill in the RBA for the UL allocation
                mask = 0;
                for(i=0; i<(uint32)ceilf(logf(sys_info.N_rb_ul*(sys_info.N_rb_ul+1)/2)/logf(2)); i++)
                {
                    mask = (mask << 1) | 1;
                }
                rar_sched->rar.rba = riv & mask;

                // Re-pack the RAR
                liblte_mac_pack_random_access_response_pdu(&rar_sched->rar,
                                                           &rar_sched->dl_alloc.msg[0]);

                interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                          LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                          __FILE__,
                                          __LINE__,
                                          &rar_sched->dl_alloc.msg[0],
                                          "RAR sent %u %u %u %u",
                                          resp_win_start,
                                          resp_win_stop,
                                          dl_subfr->current_tti,
                                          ul_subfr->current_tti);

                if(NULL == msgq_to_ue)
                {
                    // Schedule DL
                    memcpy(&dl_subfr->dl_allocations.alloc[dl_subfr->dl_allocations.N_alloc],
                           &rar_sched->dl_alloc,
                           sizeof(LIBLTE_PHY_ALLOCATION_STRUCT));
                    dl_subfr->dl_allocations.N_alloc++;
                    // Schedule UL decode 6 subframes from now
                    memcpy(&ul_subfr->decodes.alloc[ul_subfr->decodes.N_alloc],
                           &rar_sched->ul_alloc,
                           sizeof(LIBLTE_PHY_ALLOCATION_STRUCT));
                    ul_subfr->decodes.N_alloc++;
                }else{
                    memcpy(&rar_pdu_msg.msg, &rar_sched->dl_alloc.msg, sizeof(rar_pdu_msg.msg));
                    rar_pdu_msg.tti = ul_subfr->current_tti;
                    msgq_to_ue->send(LIBTOOLS_IPC_MSGQ_MESSAGE_TYPE_RAR_PDU,
                                     (LIBTOOLS_IPC_MSGQ_MESSAGE_UNION *)&rar_pdu_msg,
                                     sizeof(rar_pdu_msg));
                }

                // Remove RAR from queue
                rar_sched_queue.pop_front();
                delete rar_sched;
            }else{
                sched_out_of_headroom = true;
            }
        }else if(resp_win_stop < sched_dl_subfr[sched_cur_dl_subfn].current_tti){ // Check to see if the response window has passed
            // Response window has passed, remove from queue
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                      __FILE__,
                                      __LINE__,
                                      "RAR outside of resp win %u %u",
                                      resp_win_stop,
                                      sched_dl_subfr[sched_cur_dl_subfn].current_tti);
            rar_sched_queue.pop_front();
            delete rar_sched;
        }else{
            break;
        }
    }
    sem_post(&rar_sched_queue_sem);

    // Schedule DL for the next subframe
    sem_wait(&dl_sched_queue_sem);
    sched_out_of_headroom = false;
    while(0     != dl_sched_queue.size() &&
          false == sched_out_of_headroom)
    {
        dl_sched = dl_sched_queue.front();

        if(dl_sched->current_tti >= sched_dl_subfr[sched_cur_dl_subfn].current_tti)
        {
            dl_subfr = &sched_dl_subfr[sched_cur_dl_subfn];

            // Determine how many PRBs and DCIs are available in this subframe
            N_avail_dl_prbs = dl_subfr->N_avail_prbs - dl_subfr->N_sched_prbs;
            N_avail_dcis    = N_cce - (dl_subfr->dl_allocations.N_alloc + dl_subfr->ul_allocations.N_alloc);

            // Pack the message and determine TBS
            liblte_mac_pack_mac_pdu(&dl_sched->mac_pdu,
                                    &dl_sched->alloc.msg[0]);
            liblte_phy_get_tbs_mcs_and_n_prb_for_dl(dl_sched->alloc.msg[0].N_bits,
                                                    dl_subfr->current_tti % 10,
                                                    N_avail_dl_prbs,
                                                    dl_sched->alloc.rnti,
                                                    &dl_sched->alloc.tbs,
                                                    &dl_sched->alloc.mcs,
                                                    &dl_sched->alloc.N_prb);
            if(10 > dl_sched->alloc.mcs)
            {
                dl_sched->alloc.mod_type = LIBLTE_PHY_MODULATION_TYPE_QPSK;
            }else if(17 > dl_sched->alloc.mcs){
                dl_sched->alloc.mod_type = LIBLTE_PHY_MODULATION_TYPE_16QAM;
            }else{
                dl_sched->alloc.mod_type = LIBLTE_PHY_MODULATION_TYPE_64QAM;
            }

            // Pad and repack if needed
            if(dl_sched->alloc.tbs > dl_sched->alloc.msg[0].N_bits)
            {
                N_pad = (dl_sched->alloc.tbs - dl_sched->alloc.msg[0].N_bits)/8;

                if(1 == N_pad)
                {
                    for(i=0; i<dl_sched->mac_pdu.N_subheaders; i++)
                    {
                        memcpy(&dl_sched->mac_pdu.subheader[dl_sched->mac_pdu.N_subheaders-i], &dl_sched->mac_pdu.subheader[dl_sched->mac_pdu.N_subheaders-i-1], sizeof(LIBLTE_MAC_PDU_SUBHEADER_STRUCT));
                    }
                    dl_sched->mac_pdu.subheader[0].lcid = LIBLTE_MAC_DLSCH_PADDING_LCID;
                    dl_sched->mac_pdu.N_subheaders++;
                }else if(2 == N_pad){
                    for(i=0; i<dl_sched->mac_pdu.N_subheaders; i++)
                    {
                        memcpy(&dl_sched->mac_pdu.subheader[dl_sched->mac_pdu.N_subheaders-i+1], &dl_sched->mac_pdu.subheader[dl_sched->mac_pdu.N_subheaders-i-1], sizeof(LIBLTE_MAC_PDU_SUBHEADER_STRUCT));
                    }
                    dl_sched->mac_pdu.subheader[0].lcid  = LIBLTE_MAC_DLSCH_PADDING_LCID;
                    dl_sched->mac_pdu.subheader[1].lcid  = LIBLTE_MAC_DLSCH_PADDING_LCID;
                    dl_sched->mac_pdu.N_subheaders      += 2;
                }else{
                    dl_sched->mac_pdu.subheader[dl_sched->mac_pdu.N_subheaders].lcid = LIBLTE_MAC_DLSCH_PADDING_LCID;
                    dl_sched->mac_pdu.N_subheaders++;
                }

                liblte_mac_pack_mac_pdu(&dl_sched->mac_pdu,
                                        &dl_sched->alloc.msg[0]);
            }

            // Send a PCAP message
            interface->send_lte_pcap_msg(LTE_FDD_ENB_PCAP_DIRECTION_DL,
                                         dl_sched->alloc.rnti,
                                         dl_subfr->current_tti,
                                         dl_sched->alloc.msg[0].msg,
                                         dl_sched->alloc.tbs);

            // Determine how many PRBs and DCIs are available in this subframe
            N_avail_dl_prbs = dl_subfr->N_avail_prbs - dl_subfr->N_sched_prbs;
            N_avail_dcis    = N_cce - (dl_subfr->dl_allocations.N_alloc + dl_subfr->ul_allocations.N_alloc);

            if(dl_sched->alloc.N_prb <= N_avail_dl_prbs &&
               1                     <= N_avail_dcis)
            {
                interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                          LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                          __FILE__,
                                          __LINE__,
                                          &dl_sched->alloc.msg[0],
                                          "DL allocation (mcs=%u, tbs=%u, N_prb=%u) sent for RNTI=%u CURRENT_TTI=%u",
                                          dl_sched->alloc.mcs,
                                          dl_sched->alloc.tbs,
                                          dl_sched->alloc.N_prb,
                                          dl_sched->alloc.rnti,
                                          dl_subfr->current_tti);

                if(NULL == msgq_to_ue)
                {
                    // Schedule DL
                    memcpy(&dl_subfr->dl_allocations.alloc[dl_subfr->dl_allocations.N_alloc],
                           &dl_sched->alloc,
                           sizeof(LIBLTE_PHY_ALLOCATION_STRUCT));
                    dl_subfr->dl_allocations.N_alloc++;

                    // Schedule ACK/NACK PUCCH 4 subframes from now and store the DL allocation for potential H-ARQ retransmission
                    ul_subfr = &sched_ul_subfr[(sched_cur_dl_subfn+4)%10];
                    ul_subfr->pucch[ul_subfr->N_pucch].type        = LTE_FDD_ENB_PUCCH_TYPE_ACK_NACK;
                    ul_subfr->pucch[ul_subfr->N_pucch].rnti        = dl_sched->alloc.rnti;
                    ul_subfr->pucch[ul_subfr->N_pucch].n_1_p_pucch = sys_info.sib2.rr_config_common_sib.pucch_cnfg.n1_pucch_an;
                    ul_subfr->pucch[ul_subfr->N_pucch].decode      = true;
                    ul_subfr->N_pucch++;
                    if(LTE_FDD_ENB_ERROR_NONE == user_mgr->find_user(dl_sched->alloc.rnti, &user))
                    {
                        user->store_harq_info(ul_subfr->current_tti, &dl_sched->mac_pdu, &dl_sched->alloc);
                    }
                }else{
                    msgq_to_ue->send(LIBTOOLS_IPC_MSGQ_MESSAGE_TYPE_MAC_PDU,
                                     (LIBTOOLS_IPC_MSGQ_MESSAGE_UNION *)&dl_sched->alloc.msg,
                                     sizeof(dl_sched->alloc.msg));
                }

                // Remove DL schedule from queue
                dl_sched_queue.pop_front();
                delete dl_sched;
            }else{
                interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                          LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                          __FILE__,
                                          __LINE__,
                                          "DISCARDING DL MESSAGE %u %u %u %u",
                                          dl_sched->alloc.N_prb,
                                          N_avail_dl_prbs,
                                          1,
                                          N_avail_dcis);

                // Remove DL schedule from queue
                dl_sched_queue.pop_front();
                delete dl_sched;
            }
        }else if(dl_sched->current_tti < sched_dl_subfr[sched_cur_dl_subfn].current_tti){
            // Remove DL schedule from queue
            dl_sched_queue.pop_front();
            delete dl_sched;
        }else{
            sched_out_of_headroom = true;
        }
    }
    sem_post(&dl_sched_queue_sem);

    // Schedule UL for the next subframe
    sem_wait(&ul_sched_queue_sem);
    sched_out_of_headroom = false;
    while(0     != ul_sched_queue.size() &&
          false == sched_out_of_headroom)
    {
        ul_sched = ul_sched_queue.front();
        dl_subfr = &sched_dl_subfr[sched_cur_dl_subfn];
        ul_subfr = &sched_ul_subfr[(sched_cur_dl_subfn+4)%10];

        // Determine how many PRBs and DCIs are available in this subframe
        N_avail_ul_prbs = ul_subfr->N_avail_prbs - ul_subfr->N_sched_prbs;
        N_avail_dcis    = N_cce - (dl_subfr->dl_allocations.N_alloc + dl_subfr->ul_allocations.N_alloc);

        if(ul_sched->alloc.N_prb <= N_avail_ul_prbs &&
           1                     <= N_avail_dcis)
        {
            // Determine the RB start
            rb_start            = ul_subfr->next_prb;
            ul_subfr->next_prb += ul_sched->alloc.N_prb;

            // Fill in the PRBs
            for(i=0; i<ul_sched->alloc.N_prb; i++)
            {
                ul_sched->alloc.prb[0][i] = rb_start+i;
                ul_sched->alloc.prb[1][i] = rb_start+i;
            }

            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MAC,
                                      __FILE__,
                                      __LINE__,
                                      "UL allocation sent for RNTI=%u CURRENT_TTI=%u",
                                      ul_sched->alloc.rnti,
                                      ul_subfr->current_tti);

            if(NULL == msgq_to_ue)
            {
                // Schedule UL decode 4 subframes from now
                memcpy(&ul_subfr->decodes.alloc[ul_subfr->decodes.N_alloc],
                       &ul_sched->alloc,
                       sizeof(LIBLTE_PHY_ALLOCATION_STRUCT));
                ul_subfr->decodes.N_alloc++;
                // Schedule UL allocation
                memcpy(&dl_subfr->ul_allocations.alloc[dl_subfr->ul_allocations.N_alloc],
                       &ul_sched->alloc,
                       sizeof(LIBLTE_PHY_ALLOCATION_STRUCT));
                dl_subfr->ul_allocations.N_alloc++;
            }else{
                ul_alloc_msg.size = ul_sched->alloc.tbs;
                ul_alloc_msg.tti  = ul_subfr->current_tti;
                msgq_to_ue->send(LIBTOOLS_IPC_MSGQ_MESSAGE_TYPE_UL_ALLOC,
                                 (LIBTOOLS_IPC_MSGQ_MESSAGE_UNION *)&ul_alloc_msg,
                                 sizeof(ul_alloc_msg));
            }

            // Remove UL schedule from queue
            ul_sched_queue.pop_front();
            delete ul_sched;
        }else{
            sched_out_of_headroom = true;
        }
    }
    sem_post(&ul_sched_queue_sem);

    // Schedule UL SR for the next subframe
    sem_wait(&ul_sr_sched_queue_sem);
    ul_subfr = &sched_ul_subfr[sched_cur_ul_subfn];
    for(iter=ul_sr_sched_queue.begin(); iter!=ul_sr_sched_queue.end(); iter++)
    {
        liblte_phy_pucch_map_sr_config_idx((*iter)->i_sr, &sr_periodicity, &N_offset_sr);
        if(((ul_subfr->current_tti - N_offset_sr) % sr_periodicity) == 0)
        {
            ul_subfr->pucch[ul_subfr->N_pucch].type        = LTE_FDD_ENB_PUCCH_TYPE_SR;
            ul_subfr->pucch[ul_subfr->N_pucch].rnti        = (*iter)->rnti;
            ul_subfr->pucch[ul_subfr->N_pucch].n_1_p_pucch = (*iter)->n_1_p_pucch;
            ul_subfr->pucch[ul_subfr->N_pucch].decode      = true;
            ul_subfr->N_pucch++;
        }
    }
    sem_post(&ul_sr_sched_queue_sem);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_mac::add_to_rar_sched_queue(uint32                        current_tti,
                                                               LIBLTE_PHY_ALLOCATION_STRUCT *dl_alloc,
                                                               LIBLTE_PHY_ALLOCATION_STRUCT *ul_alloc,
                                                               LIBLTE_MAC_RAR_STRUCT        *rar)
{
    LTE_FDD_ENB_RAR_SCHED_QUEUE_STRUCT *rar_sched = NULL;
    LTE_FDD_ENB_ERROR_ENUM              err       = LTE_FDD_ENB_ERROR_CANT_SCHEDULE;

    rar_sched = new LTE_FDD_ENB_RAR_SCHED_QUEUE_STRUCT;

    if(NULL != rar_sched)
    {
        rar_sched->current_tti = current_tti;
        memcpy(&rar_sched->dl_alloc, dl_alloc, sizeof(LIBLTE_PHY_ALLOCATION_STRUCT));
        memcpy(&rar_sched->ul_alloc, ul_alloc, sizeof(LIBLTE_PHY_ALLOCATION_STRUCT));
        memcpy(&rar_sched->rar, rar, sizeof(LIBLTE_MAC_RAR_STRUCT));

        sem_wait(&rar_sched_queue_sem);
        rar_sched_queue.push_back(rar_sched);
        sem_post(&rar_sched_queue_sem);

        err = LTE_FDD_ENB_ERROR_NONE;
    }

    return(err);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_mac::add_to_dl_sched_queue(uint32                        current_tti,
                                                              LIBLTE_MAC_PDU_STRUCT        *mac_pdu,
                                                              LIBLTE_PHY_ALLOCATION_STRUCT *alloc)
{
    std::list<LTE_FDD_ENB_DL_SCHED_QUEUE_STRUCT *>::iterator  iter;
    LTE_FDD_ENB_DL_SCHED_QUEUE_STRUCT                        *dl_sched = NULL;
    LTE_FDD_ENB_ERROR_ENUM                                    err      = LTE_FDD_ENB_ERROR_CANT_SCHEDULE;

    dl_sched = new LTE_FDD_ENB_DL_SCHED_QUEUE_STRUCT;

    if(NULL != dl_sched)
    {
        dl_sched->current_tti = current_tti;
        memcpy(&dl_sched->mac_pdu, mac_pdu, sizeof(LIBLTE_MAC_PDU_STRUCT));
        memcpy(&dl_sched->alloc, alloc, sizeof(LIBLTE_PHY_ALLOCATION_STRUCT));

        sem_wait(&dl_sched_queue_sem);
        for(iter=dl_sched_queue.begin(); iter!=dl_sched_queue.end(); iter++)
        {
            if((*iter)->alloc.rnti  == alloc->rnti &&
               (*iter)->current_tti == dl_sched->current_tti)
            {
                dl_sched->current_tti = add_to_tti(dl_sched->current_tti, 1);
            }
        }
        dl_sched_queue.push_back(dl_sched);
        sem_post(&dl_sched_queue_sem);

        err = LTE_FDD_ENB_ERROR_NONE;
    }

    return(err);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_mac::add_to_ul_sched_queue(uint32                        current_tti,
                                                              LIBLTE_PHY_ALLOCATION_STRUCT *alloc)
{
    std::list<LTE_FDD_ENB_UL_SCHED_QUEUE_STRUCT *>::iterator  iter;
    LTE_FDD_ENB_UL_SCHED_QUEUE_STRUCT                        *ul_sched = NULL;
    LTE_FDD_ENB_ERROR_ENUM                                    err      = LTE_FDD_ENB_ERROR_CANT_SCHEDULE;
    bool                                                      sched    = true;

    ul_sched = new LTE_FDD_ENB_UL_SCHED_QUEUE_STRUCT;

    if(NULL != ul_sched)
    {
        ul_sched->current_tti = current_tti;
        memcpy(&ul_sched->alloc, alloc, sizeof(LIBLTE_PHY_ALLOCATION_STRUCT));

        sem_wait(&ul_sched_queue_sem);
        for(iter=ul_sched_queue.begin(); iter!=ul_sched_queue.end(); iter++)
        {
            if((*iter)->alloc.rnti  == alloc->rnti &&
               (*iter)->current_tti == ul_sched->current_tti)
            {
                sched = false;
            }
        }
        if(sched)
        {
            ul_sched_queue.push_back(ul_sched);
            err = LTE_FDD_ENB_ERROR_NONE;
        }
        sem_post(&ul_sched_queue_sem);
    }

    return(err);
}

/*****************/
/*    Helpers    */
/*****************/
uint32 LTE_fdd_enb_mac::get_n_reserved_prbs(uint32 current_tti)
{
    uint32 N_reserved_prbs = 0;
    uint32 i;

    // Reserve PRBs for the MIB
    if(0 == (current_tti % 10))
    {
        N_reserved_prbs += 6;
    }

    // Reserve PRBs for SIB1
    if(5 == (current_tti % 10) &&
       0 == ((current_tti / 10) % 2))
    {
        N_reserved_prbs += sys_info.sib1_alloc.N_prb;
    }

    // Reserve PRBs for all other SIBs
    for(i=0; i<sys_info.sib1.N_sched_info; i++)
    {
        if(0                             != sys_info.sib_alloc[i].msg[0].N_bits &&
           (i * sys_info.si_win_len)%10  == (current_tti % 10)                  &&
           ((i * sys_info.si_win_len)/10 == ((current_tti / 10) % sys_info.si_periodicity_T)))
        {
            N_reserved_prbs += sys_info.sib_alloc[i].N_prb;
        }
    }

    return(N_reserved_prbs);
}
uint32 LTE_fdd_enb_mac::add_to_tti(uint32 tti,
                                   uint32 addition)
{
    return((tti + addition) % (LTE_FDD_ENB_CURRENT_TTI_MAX + 1));
}
bool LTE_fdd_enb_mac::is_tti_in_future(uint32 tti_to_check,
                                       uint32 current_tti)
{
    uint32 max_future_tti = add_to_tti(current_tti, 100);
    bool   tti_in_future  = false;

    if(tti_to_check > current_tti &&
       tti_to_check < max_future_tti)
    {
        tti_in_future = true;
    }

    return(tti_in_future);
}
