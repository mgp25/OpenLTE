#line 2 "LTE_fdd_enb_phy.cc" // Make __FILE__ omit the path
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

    File: LTE_fdd_enb_phy.cc

    Description: Contains all the implementations for the LTE FDD eNodeB
                 physical layer.

    Revision History
    ----------    -------------    --------------------------------------------
    11/10/2013    Ben Wojtowicz    Created file
    01/18/2014    Ben Wojtowicz    Cached a copy of the interface class, added
                                   level to debug prints, add the ability to
                                   handle late subframes, fixed a bug with
                                   transmitting SIB2 for 1.4MHz bandwidth, and
                                   added PRACH detection.
    03/26/2014    Ben Wojtowicz    Using the latest LTE library and added PUSCH
                                   decode support.
    04/12/2014    Ben Wojtowicz    Using the latest LTE library.
    05/04/2014    Ben Wojtowicz    Added PCAP support, PHICH support, and timer
                                   support.
    06/15/2014    Ben Wojtowicz    Changed fn_combo to current_tti.
    08/03/2014    Ben Wojtowicz    Added support for limiting PCAP output.
    12/16/2014    Ben Wojtowicz    Added ol extension to message queue.
    02/15/2015    Ben Wojtowicz    Moved to new message queue and changed the
                                   RTS timing.
    03/11/2015    Ben Wojtowicz    Changed the RTS timing.
    07/25/2015    Ben Wojtowicz    Combined the DL and UL schedule messages into
                                   a single PHY schedule message and using the
                                   new radio interface.
    12/06/2015    Ben Wojtowicz    Changed boost::mutex to pthread_mutex_t and
                                   sem_t.
    03/12/2016    Ben Wojtowicz    Added PUCCH support.
    07/03/2016    Ben Wojtowicz    Not including UL PDCCH allocations in a check
                                   of how many DL PRBs are being used.
    07/31/2016    Ben Wojtowicz    Added an external interface for getting the
                                   current TTIs.
    07/29/2017    Ben Wojtowicz    Added IPC direct to a UE PHY, added SR
                                   support, and using the latest LTE library.

*******************************************************************************/

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_fdd_enb_phy.h"
#include "LTE_fdd_enb_radio.h"
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

LTE_fdd_enb_phy*       LTE_fdd_enb_phy::instance = NULL;
static pthread_mutex_t phy_instance_mutex        = PTHREAD_MUTEX_INITIALIZER;

/*******************************************************************************
                              CLASS IMPLEMENTATIONS
*******************************************************************************/

/*******************/
/*    Singleton    */
/*******************/
LTE_fdd_enb_phy* LTE_fdd_enb_phy::get_instance(void)
{
    libtools_scoped_lock lock(phy_instance_mutex);

    if(NULL == instance)
    {
        instance = new LTE_fdd_enb_phy();
    }

    return(instance);
}
void LTE_fdd_enb_phy::cleanup(void)
{
    libtools_scoped_lock lock(phy_instance_mutex);

    if(NULL != instance)
    {
        delete instance;
        instance = NULL;
    }
}

/********************************/
/*    Constructor/Destructor    */
/********************************/
LTE_fdd_enb_phy::LTE_fdd_enb_phy()
{
    sem_init(&sys_info_sem, 0, 1);
    sem_init(&dl_sched_sem, 0, 1);
    sem_init(&ul_sched_sem, 0, 1);
    interface = NULL;
    started   = false;
}
LTE_fdd_enb_phy::~LTE_fdd_enb_phy()
{
    stop();
    sem_destroy(&ul_sched_sem);
    sem_destroy(&dl_sched_sem);
    sem_destroy(&sys_info_sem);
}

/********************/
/*    Start/Stop    */
/********************/
void LTE_fdd_enb_phy::start(LTE_fdd_enb_msgq      *from_mac,
                            LTE_fdd_enb_msgq      *to_mac,
                            bool                   direct_to_ue,
                            LTE_fdd_enb_interface *iface)
{
    LTE_fdd_enb_radio    *radio = LTE_fdd_enb_radio::get_instance();
    LTE_fdd_enb_msgq_cb   mac_cb(&LTE_fdd_enb_msgq_cb_wrapper<LTE_fdd_enb_phy, &LTE_fdd_enb_phy::handle_mac_msg>, this);
    libtools_ipc_msgq_cb  ue_cb(&libtools_ipc_msgq_cb_wrapper<LTE_fdd_enb_phy, &LTE_fdd_enb_phy::handle_ue_msg>, this);
    LIBLTE_PHY_FS_ENUM    fs;
    uint32                i;
    uint32                j;
    uint32                k;
    uint32                samp_rate;
    uint8                 prach_cnfg_idx;

    if(!started)
    {
        // Get the latest sys info
        update_sys_info();

        // Initialize phy
        samp_rate = radio->get_phy_sample_rate();
        if(30720000 == samp_rate)
        {
            fs = LIBLTE_PHY_FS_30_72MHZ;
        }else if(15360000 == samp_rate){
            fs = LIBLTE_PHY_FS_15_36MHZ;
        }else if(7680000 == samp_rate){
            fs = LIBLTE_PHY_FS_7_68MHZ;
        }else if(3840000 == samp_rate){
            fs = LIBLTE_PHY_FS_3_84MHZ;
        }else if(1920000 == samp_rate){
            fs = LIBLTE_PHY_FS_1_92MHZ;
        }else{
            iface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_PHY,
                                  __FILE__,
                                  __LINE__,
                                  "Invalid sample rate %u",
                                  samp_rate);
        }
        liblte_phy_init(&phy_struct,
                        fs,
                        sys_info.N_id_cell,
                        sys_info.N_ant,
                        sys_info.N_rb_dl,
                        sys_info.N_sc_rb_dl,
                        liblte_rrc_phich_resource_num[sys_info.mib.phich_config.res]);
        liblte_phy_ul_init(phy_struct,
                           sys_info.N_id_cell,
                           sys_info.sib2.rr_config_common_sib.prach_cnfg.root_sequence_index,
                           sys_info.sib2.rr_config_common_sib.prach_cnfg.prach_cnfg_info.prach_config_index>>4,
                           sys_info.sib2.rr_config_common_sib.prach_cnfg.prach_cnfg_info.zero_correlation_zone_config,
                           sys_info.sib2.rr_config_common_sib.prach_cnfg.prach_cnfg_info.high_speed_flag,
                           sys_info.sib2.rr_config_common_sib.pusch_cnfg.ul_rs.group_assignment_pusch,
                           sys_info.sib2.rr_config_common_sib.pusch_cnfg.ul_rs.group_hopping_enabled,
                           sys_info.sib2.rr_config_common_sib.pusch_cnfg.ul_rs.sequence_hopping_enabled,
                           sys_info.sib2.rr_config_common_sib.pusch_cnfg.ul_rs.cyclic_shift,
                           0,
                           sys_info.sib2.rr_config_common_sib.pucch_cnfg.n_cs_an,
                           sys_info.sib2.rr_config_common_sib.pucch_cnfg.delta_pucch_shift);

        // Downlink
        for(i=0; i<10; i++)
        {
            dl_schedule[i].current_tti            = i;
            dl_schedule[i].dl_allocations.N_alloc = 0;
            dl_schedule[i].ul_allocations.N_alloc = 0;
            ul_schedule[i].current_tti            = i;
            ul_schedule[i].decodes.N_alloc        = 0;
            ul_schedule[i].N_pucch                = 0;
        }
        pcfich.cfi = 2; // FIXME: Make this dynamic every subfr
        for(i=0; i<10; i++)
        {
            for(j=0; j<25; j++)
            {
                for(k=0; k<8; k++)
                {
                    phich[i].present[j][k] = false;
                }
            }
        }
        pdcch.N_alloc        = 0;
        pdcch.N_symbs        = 2; // FIXME: Make this dynamic every subfr
        dl_subframe.num      = 0;
        dl_current_tti       = 0;
        last_rts_current_tti = 0;
        late_subfr           = false;

        // Uplink
        ul_current_tti = (LTE_FDD_ENB_CURRENT_TTI_MAX + 1) - 2;
        prach_cnfg_idx = sys_info.sib2.rr_config_common_sib.prach_cnfg.prach_cnfg_info.prach_config_index;
        if(prach_cnfg_idx ==  0 ||
           prach_cnfg_idx ==  1 ||
           prach_cnfg_idx ==  2 ||
           prach_cnfg_idx == 15 ||
           prach_cnfg_idx == 16 ||
           prach_cnfg_idx == 17 ||
           prach_cnfg_idx == 18 ||
           prach_cnfg_idx == 31 ||
           prach_cnfg_idx == 32 ||
           prach_cnfg_idx == 33 ||
           prach_cnfg_idx == 34 ||
           prach_cnfg_idx == 47 ||
           prach_cnfg_idx == 48 ||
           prach_cnfg_idx == 49 ||
           prach_cnfg_idx == 50 ||
           prach_cnfg_idx == 63)
        {
            prach_sfn_mod = 2;
        }else{
            prach_sfn_mod = 1;
        }
        prach_subfn_zero_allowed = true;
        switch(prach_cnfg_idx % 16)
        {
        case 0:
            // Intentional fall through
        case 3:
            prach_subfn_mod   = 10;
            prach_subfn_check = 1;
            break;
        case 1:
            // Intentional fall through
        case 4:
            prach_subfn_mod   = 10;
            prach_subfn_check = 4;
            break;
        case 2:
            // Intentional fall through
        case 5:
            prach_subfn_mod   = 10;
            prach_subfn_check = 7;
            break;
        case 6:
            prach_subfn_mod   = 5;
            prach_subfn_check = 1;
            break;
        case 7:
            prach_subfn_mod   = 5;
            prach_subfn_check = 2;
            break;
        case 8:
            prach_subfn_mod   = 5;
            prach_subfn_check = 3;
            break;
        case 9:
            prach_subfn_mod   = 3;
            prach_subfn_check = 1;
            break;
        case 10:
            prach_subfn_mod   = 3;
            prach_subfn_check = 2;
            break;
        case 11:
            prach_subfn_mod          = 3;
            prach_subfn_check        = 0;
            prach_subfn_zero_allowed = false;
            break;
        case 12:
            prach_subfn_mod   = 2;
            prach_subfn_check = 0;
            break;
        case 13:
            prach_subfn_mod   = 2;
            prach_subfn_check = 1;
            break;
        case 14:
            prach_subfn_mod   = 1;
            prach_subfn_check = 0;
            break;
        case 15:
            prach_subfn_mod   = 10;
            prach_subfn_check = 9;
            break;
        }

        // eNodeB communication
        msgq_from_mac = from_mac;
        msgq_to_mac   = to_mac;
        msgq_from_mac->attach_rx(mac_cb);

        // UE communication
        if(direct_to_ue)
        {
            msgq_to_ue = new libtools_ipc_msgq("enb_ue", ue_cb);
        }else{
            msgq_to_ue = NULL;
        }

        interface = iface;
        started   = true;
    }
}
void LTE_fdd_enb_phy::stop(void)
{
    if(started)
    {
        started = false;

        if(NULL != msgq_to_ue)
        {
            delete msgq_to_ue;
        }

        liblte_phy_ul_cleanup(phy_struct);
        liblte_phy_cleanup(phy_struct);
    }
}

/****************************/
/*    External Interface    */
/****************************/
void LTE_fdd_enb_phy::update_sys_info(void)
{
    libtools_scoped_lock  lock(sys_info_sem);
    LTE_fdd_enb_cnfg_db  *cnfg_db = LTE_fdd_enb_cnfg_db::get_instance();

    cnfg_db->get_sys_info(sys_info);
}
uint32 LTE_fdd_enb_phy::get_n_cce(void)
{
    libtools_scoped_lock lock(sys_info_sem);
    uint32               N_cce;

    liblte_phy_get_n_cce(phy_struct,
                         liblte_rrc_phich_resource_num[sys_info.mib.phich_config.res],
                         pdcch.N_symbs,
                         sys_info.N_ant,
                         &N_cce);

    return(N_cce);
}
void LTE_fdd_enb_phy::get_current_ttis(uint32 *dl_tti,
                                       uint32 *ul_tti)
{
    *dl_tti = dl_current_tti;
    *ul_tti = ul_current_tti;
}

/***********************/
/*    Communication    */
/***********************/
void LTE_fdd_enb_phy::handle_mac_msg(LTE_FDD_ENB_MESSAGE_STRUCT &msg)
{
    if(LTE_FDD_ENB_DEST_LAYER_PHY == msg.dest_layer ||
       LTE_FDD_ENB_DEST_LAYER_ANY == msg.dest_layer)
    {
        switch(msg.type)
        {
        case LTE_FDD_ENB_MESSAGE_TYPE_PHY_SCHEDULE:
            handle_phy_schedule(&msg.msg.phy_schedule);
            break;
        default:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_WARNING,
                                      LTE_FDD_ENB_DEBUG_LEVEL_PHY,
                                      __FILE__,
                                      __LINE__,
                                      "Received invalid message %s",
                                      LTE_fdd_enb_message_type_text[msg.type]);
            break;
        }
    }else{
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_PHY,
                                  __FILE__,
                                  __LINE__,
                                  "Received message for invalid layer %s",
                                  LTE_fdd_enb_dest_layer_text[msg.dest_layer]);
    }
}
void LTE_fdd_enb_phy::handle_ue_msg(LIBTOOLS_IPC_MSGQ_MESSAGE_STRUCT *msg)
{
    switch(msg->type)
    {
    case LIBTOOLS_IPC_MSGQ_MESSAGE_TYPE_KILL:
        msgq_to_ue = NULL;
        break;
    default:
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_PHY,
                                  __FILE__,
                                  __LINE__,
                                  "Received invalid UE message %s",
                                  libtools_ipc_msgq_message_type_text[msg->type]);
        break;
    }
}

/*************************/
/*    Radio Interface    */
/*************************/
void LTE_fdd_enb_phy::radio_interface(LTE_FDD_ENB_RADIO_TX_BUF_STRUCT *tx_buf,
                                      LTE_FDD_ENB_RADIO_RX_BUF_STRUCT *rx_buf)
{
    if(started)
    {
        // Once started, this routine gets called every millisecond to:
        //     1) process the new uplink subframe
        //     2) generate the next downlink subframe
        process_ul(rx_buf);
        process_dl(tx_buf);

        if(NULL != msgq_to_ue)
        {
            msgq_to_ue->send(LIBTOOLS_IPC_MSGQ_MESSAGE_TYPE_PHY_SAMPS,
                             (LIBTOOLS_IPC_MSGQ_MESSAGE_UNION *)tx_buf,
                             sizeof(LIBTOOLS_IPC_MSGQ_PHY_SAMPS_MSG_STRUCT));
        }
    }
}
void LTE_fdd_enb_phy::radio_interface(LTE_FDD_ENB_RADIO_TX_BUF_STRUCT *tx_buf)
{
    // This routine gets called once to generate the first downlink subframe
    process_dl(tx_buf);
}

/******************/
/*    Downlink    */
/******************/
void LTE_fdd_enb_phy::handle_phy_schedule(LTE_FDD_ENB_PHY_SCHEDULE_MSG_STRUCT *phy_sched)
{
    sem_wait(&dl_sched_sem);
    sem_wait(&ul_sched_sem);

    if(phy_sched->dl_sched.current_tti                    < dl_current_tti &&
       (dl_current_tti - phy_sched->dl_sched.current_tti) < (LTE_FDD_ENB_CURRENT_TTI_MAX/2))
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_PHY,
                                  __FILE__,
                                  __LINE__,
                                  "Late DL subframe from MAC:%u, PHY is currently on %u",
                                  phy_sched->dl_sched.current_tti,
                                  dl_current_tti);

        late_subfr = true;
        if(phy_sched->dl_sched.current_tti == last_rts_current_tti)
        {
            late_subfr = false;
        }
    }else{
        if(phy_sched->dl_sched.dl_allocations.N_alloc ||
           phy_sched->dl_sched.ul_allocations.N_alloc)
        {
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                      LTE_FDD_ENB_DEBUG_LEVEL_PHY,
                                      __FILE__,
                                      __LINE__,
                                      "Received PDSCH schedule from MAC CURRENT_TTI:MAC=%u,PHY=%u N_dl_allocs=%u N_ul_allocs=%u",
                                      phy_sched->dl_sched.current_tti,
                                      dl_current_tti,
                                      phy_sched->dl_sched.dl_allocations.N_alloc,
                                      phy_sched->dl_sched.ul_allocations.N_alloc);
        }

        memcpy(&dl_schedule[phy_sched->dl_sched.current_tti%10], &phy_sched->dl_sched, sizeof(LTE_FDD_ENB_DL_SCHEDULE_MSG_STRUCT));

        late_subfr = false;
    }
    if(phy_sched->ul_sched.current_tti                    < ul_current_tti &&
       (ul_current_tti - phy_sched->ul_sched.current_tti) < (LTE_FDD_ENB_CURRENT_TTI_MAX/2))
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_PHY,
                                  __FILE__,
                                  __LINE__,
                                  "Late UL subframe from MAC:%u, PHY is currently on %u",
                                  phy_sched->ul_sched.current_tti,
                                  ul_current_tti);
    }else{
        if(phy_sched->ul_sched.decodes.N_alloc)
        {
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                      LTE_FDD_ENB_DEBUG_LEVEL_PHY,
                                      __FILE__,
                                      __LINE__,
                                      "Received PUSCH schedule from MAC CURRENT_TTI:MAC=%u,PHY=%u N_ul_decodes=%u",
                                      phy_sched->ul_sched.current_tti,
                                      ul_current_tti,
                                      phy_sched->ul_sched.decodes.N_alloc);
        }

        memcpy(&ul_schedule[phy_sched->ul_sched.current_tti%10], &phy_sched->ul_sched, sizeof(LTE_FDD_ENB_UL_SCHEDULE_MSG_STRUCT));
    }

    sem_post(&ul_sched_sem);
    sem_post(&dl_sched_sem);
}
void LTE_fdd_enb_phy::process_dl(LTE_FDD_ENB_RADIO_TX_BUF_STRUCT *tx_buf)
{
    LTE_fdd_enb_radio                    *radio = LTE_fdd_enb_radio::get_instance();
    libtools_scoped_lock                  lock(sys_info_sem);
    LTE_FDD_ENB_READY_TO_SEND_MSG_STRUCT  rts;
    uint32                                p;
    uint32                                i;
    uint32                                j;
    uint32                                last_prb = 0;
    uint32                                act_noutput_items;
    uint32                                sfn   = dl_current_tti/10;
    uint32                                subfn = dl_current_tti%10;

    // Initialize the output to all zeros
    for(p=0; p<sys_info.N_ant; p++)
    {
        for(i=0; i<14; i++)
        {
            for(j=0; j<LIBLTE_PHY_N_RB_DL_20MHZ*LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP; j++)
            {
                dl_subframe.tx_symb_re[p][i][j] = 0;
                dl_subframe.tx_symb_im[p][i][j] = 0;
            }
        }
    }
    dl_subframe.num = subfn;

    // Handle PSS and SSS
    if(0 == dl_subframe.num ||
       5 == dl_subframe.num)
    {
        liblte_phy_map_pss(phy_struct,
                           &dl_subframe,
                           sys_info.N_id_2,
                           sys_info.N_ant);
        liblte_phy_map_sss(phy_struct,
                           &dl_subframe,
                           sys_info.N_id_1,
                           sys_info.N_id_2,
                           sys_info.N_ant);
    }

    // Handle CRS
    liblte_phy_map_crs(phy_struct,
                       &dl_subframe,
                       sys_info.N_id_cell,
                       sys_info.N_ant);

    // Handle PBCH
    if(0 == dl_subframe.num)
    {
        sys_info.mib.sfn_div_4 = sfn/4;
        liblte_rrc_pack_bcch_bch_msg(&sys_info.mib,
                                     &dl_rrc_msg);
        if(!sys_info.mib_pcap_sent)
        {
            interface->send_lte_pcap_msg(LTE_FDD_ENB_PCAP_DIRECTION_DL,
                                         0xFFFFFFFF,
                                         dl_current_tti,
                                         dl_rrc_msg.msg,
                                         dl_rrc_msg.N_bits);
            if(!sys_info.continuous_sib_pcap)
            {
                sys_info.mib_pcap_sent = true;
            }
        }
        liblte_phy_bch_channel_encode(phy_struct,
                                      dl_rrc_msg.msg,
                                      dl_rrc_msg.N_bits,
                                      sys_info.N_id_cell,
                                      sys_info.N_ant,
                                      &dl_subframe,
                                      sfn);
    }

    // Handle SIB data
    pdcch.N_alloc = 0;
    if(5 == dl_subframe.num &&
       0 == (sfn % 2))
    {
        // SIB1
        if(!sys_info.sib1_pcap_sent)
        {
            interface->send_lte_pcap_msg(LTE_FDD_ENB_PCAP_DIRECTION_DL,
                                         LIBLTE_MAC_SI_RNTI,
                                         dl_current_tti,
                                         sys_info.sib1_alloc.msg[0].msg,
                                         sys_info.sib1_alloc.msg[0].N_bits);
            if(!sys_info.continuous_sib_pcap)
            {
                sys_info.sib1_pcap_sent = true;
            }
        }
        memcpy(&pdcch.alloc[pdcch.N_alloc], &sys_info.sib1_alloc, sizeof(LIBLTE_PHY_ALLOCATION_STRUCT));
        liblte_phy_get_tbs_mcs_and_n_prb_for_dl(pdcch.alloc[pdcch.N_alloc].msg[0].N_bits,
                                                dl_subframe.num,
                                                sys_info.N_rb_dl,
                                                pdcch.alloc[pdcch.N_alloc].rnti,
                                                &pdcch.alloc[pdcch.N_alloc].tbs,
                                                &pdcch.alloc[pdcch.N_alloc].mcs,
                                                &pdcch.alloc[pdcch.N_alloc].N_prb);
        pdcch.alloc[pdcch.N_alloc].rv_idx     = (uint32)ceilf(1.5 * ((sfn / 2) % 4)) % 4; //36.321 section 5.3.1
        pdcch.alloc[pdcch.N_alloc++].dl_alloc = true;
    }
    if((0 * sys_info.si_win_len)%10   <= dl_subframe.num &&
       (1 * sys_info.si_win_len)%10   >  dl_subframe.num &&
       ((0 * sys_info.si_win_len)/10) == (sfn % sys_info.si_periodicity_T))
    {
        // SIs in 1st scheduling info list entry
        if(!sys_info.sib_pcap_sent[0])
        {
            interface->send_lte_pcap_msg(LTE_FDD_ENB_PCAP_DIRECTION_DL,
                                         LIBLTE_MAC_SI_RNTI,
                                         dl_current_tti,
                                         sys_info.sib_alloc[0].msg[0].msg,
                                         sys_info.sib_alloc[0].msg[0].N_bits);
            if(!sys_info.continuous_sib_pcap)
            {
                sys_info.sib_pcap_sent[0] = true;
            }
        }
        memcpy(&pdcch.alloc[pdcch.N_alloc], &sys_info.sib_alloc[0], sizeof(LIBLTE_PHY_ALLOCATION_STRUCT));
        // FIXME: This was a hack to allow SIB2 decoding with 1.4MHz BW due to overlap with MIB
        if(LIBLTE_SUCCESS == liblte_phy_get_tbs_mcs_and_n_prb_for_dl(pdcch.alloc[pdcch.N_alloc].msg[0].N_bits,
                                                                     dl_subframe.num,
                                                                     sys_info.N_rb_dl,
                                                                     pdcch.alloc[pdcch.N_alloc].rnti,
                                                                     &pdcch.alloc[pdcch.N_alloc].tbs,
                                                                     &pdcch.alloc[pdcch.N_alloc].mcs,
                                                                     &pdcch.alloc[pdcch.N_alloc].N_prb))
        {
            pdcch.alloc[pdcch.N_alloc++].dl_alloc = true;
        }
    }
    for(i=1; i<sys_info.sib1.N_sched_info; i++)
    {
        if(0                              != sys_info.sib_alloc[i].msg[0].N_bits &&
           (i * sys_info.si_win_len)%10   == dl_subframe.num                     &&
           ((i * sys_info.si_win_len)/10) == (sfn % sys_info.si_periodicity_T))
        {
            if(!sys_info.sib_pcap_sent[i])
            {
                interface->send_lte_pcap_msg(LTE_FDD_ENB_PCAP_DIRECTION_DL,
                                             LIBLTE_MAC_SI_RNTI,
                                             dl_current_tti,
                                             sys_info.sib_alloc[i].msg[0].msg,
                                             sys_info.sib_alloc[i].msg[0].N_bits);
                if(!sys_info.continuous_sib_pcap)
                {
                    sys_info.sib_pcap_sent[i] = true;
                }
            }
            memcpy(&pdcch.alloc[pdcch.N_alloc], &sys_info.sib_alloc[i], sizeof(LIBLTE_PHY_ALLOCATION_STRUCT));
            liblte_phy_get_tbs_mcs_and_n_prb_for_dl(pdcch.alloc[pdcch.N_alloc].msg[0].N_bits,
                                                    dl_subframe.num,
                                                    sys_info.N_rb_dl,
                                                    pdcch.alloc[pdcch.N_alloc].rnti,
                                                    &pdcch.alloc[pdcch.N_alloc].tbs,
                                                    &pdcch.alloc[pdcch.N_alloc].mcs,
                                                    &pdcch.alloc[pdcch.N_alloc].N_prb);
            pdcch.alloc[pdcch.N_alloc++].dl_alloc = true;
        }
    }

    // Handle user data
    sem_wait(&dl_sched_sem);
    if(dl_schedule[dl_current_tti%10].current_tti == dl_current_tti)
    {
        for(i=0; i<dl_schedule[subfn].dl_allocations.N_alloc; i++)
        {
            memcpy(&pdcch.alloc[pdcch.N_alloc], &dl_schedule[subfn].dl_allocations.alloc[i], sizeof(LIBLTE_PHY_ALLOCATION_STRUCT));
            pdcch.alloc[pdcch.N_alloc++].dl_alloc = true;
        }
        for(i=0; i<dl_schedule[subfn].ul_allocations.N_alloc; i++)
        {
            memcpy(&pdcch.alloc[pdcch.N_alloc], &dl_schedule[subfn].ul_allocations.alloc[i], sizeof(LIBLTE_PHY_ALLOCATION_STRUCT));
            pdcch.alloc[pdcch.N_alloc++].dl_alloc = false;
        }
    }else{
        late_subfr = true;
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_PHY,
                                  __FILE__,
                                  __LINE__,
                                  "PDSCH current_tti from MAC (%u) does not match PHY (%u)",
                                  dl_schedule[subfn].current_tti,
                                  dl_current_tti);
    }
    sem_post(&dl_sched_sem);

    // Handle PDCCH and PDSCH
    for(i=0; i<pdcch.N_alloc; i++)
    {
        for(j=0; j<pdcch.alloc[i].N_prb; j++)
        {
            if(pdcch.alloc[i].dl_alloc)
            {
                pdcch.alloc[i].prb[0][j] = last_prb;
                pdcch.alloc[i].prb[1][j] = last_prb++;
            }
        }
    }
    if(last_prb > phy_struct->N_rb_dl)
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_PHY,
                                  __FILE__,
                                  __LINE__,
                                  "More PRBs allocated than are available");
    }else{
        liblte_phy_pdcch_channel_encode(phy_struct,
                                        &pcfich,
                                        &phich[subfn],
                                        &pdcch,
                                        sys_info.N_id_cell,
                                        sys_info.N_ant,
                                        liblte_rrc_phich_resource_num[sys_info.mib.phich_config.res],
                                        sys_info.mib.phich_config.dur,
                                        &dl_subframe);
        if(0 != pdcch.N_alloc)
        {
            liblte_phy_pdsch_channel_encode(phy_struct,
                                            &pdcch,
                                            sys_info.N_id_cell,
                                            sys_info.N_ant,
                                            &dl_subframe);
        }
        // Clear PHICH
        for(i=0; i<25; i++)
        {
            for(j=0; j<8; j++)
            {
                phich[subfn].present[i][j] = false;
            }
        }
    }

    for(p=0; p<sys_info.N_ant; p++)
    {
        liblte_phy_create_dl_subframe(phy_struct,
                                      &dl_subframe,
                                      p,
                                      &tx_buf->i_buf[p][0],
                                      &tx_buf->q_buf[p][0]);
    }
    tx_buf->N_samps_per_ant = phy_struct->N_samps_per_subfr;
    tx_buf->current_tti     = dl_current_tti;
    tx_buf->N_ant           = sys_info.N_ant;

    // Update current TTI
    dl_current_tti = (dl_current_tti + 1) % (LTE_FDD_ENB_CURRENT_TTI_MAX + 1);

    // Send READY TO SEND message to MAC
    rts.dl_current_tti   = (dl_current_tti + 2) % (LTE_FDD_ENB_CURRENT_TTI_MAX + 1);
    rts.ul_current_tti   = (ul_current_tti + 2) % (LTE_FDD_ENB_CURRENT_TTI_MAX + 1);
    rts.late             = late_subfr;
    last_rts_current_tti = rts.dl_current_tti;
    msgq_to_mac->send(LTE_FDD_ENB_MESSAGE_TYPE_READY_TO_SEND,
                      LTE_FDD_ENB_DEST_LAYER_MAC,
                      (LTE_FDD_ENB_MESSAGE_UNION *)&rts,
                      sizeof(rts));

    // Send samples to radio
    radio->send(tx_buf);
}

/****************/
/*    Uplink    */
/****************/
void LTE_fdd_enb_phy::process_ul(LTE_FDD_ENB_RADIO_RX_BUF_STRUCT *rx_buf)
{
    LIBLTE_ERROR_ENUM subfr_err;
    LIBLTE_ERROR_ENUM pucch_err;
    uint32            N_skipped_subfrs = 0;
    uint32            sfn;
    uint32            i;
    uint32            I_prb_ra;
    uint32            n_group_phich;
    uint32            n_seq_phich;

    // Check the received current_tti
    if(rx_buf->current_tti != ul_current_tti)
    {
        if(rx_buf->current_tti > ul_current_tti)
        {
            N_skipped_subfrs = rx_buf->current_tti - ul_current_tti;
        }else{
            N_skipped_subfrs = (rx_buf->current_tti + LTE_FDD_ENB_CURRENT_TTI_MAX + 1) - ul_current_tti;
        }

        // Jump the DL and UL current_tti
        dl_current_tti = (dl_current_tti + N_skipped_subfrs) % (LTE_FDD_ENB_CURRENT_TTI_MAX + 1);
        ul_current_tti = (ul_current_tti + N_skipped_subfrs) % (LTE_FDD_ENB_CURRENT_TTI_MAX + 1);
    }
    sfn             = ul_current_tti/10;
    ul_subframe.num = ul_current_tti%10;

    // Handle PRACH
    if((sfn % prach_sfn_mod) == 0)
    {
        if((ul_subframe.num % prach_subfn_mod) == prach_subfn_check)
        {
            if(ul_subframe.num != 0 ||
               true            == prach_subfn_zero_allowed)
            {
                prach_decode.current_tti = ul_current_tti;
                liblte_phy_detect_prach(phy_struct,
                                        rx_buf->i_buf[0],
                                        rx_buf->q_buf[0],
                                        sys_info.sib2.rr_config_common_sib.prach_cnfg.prach_cnfg_info.prach_freq_offset,
                                        &prach_decode.num_preambles,
                                        prach_decode.preamble,
                                        prach_decode.timing_adv);

                msgq_to_mac->send(LTE_FDD_ENB_MESSAGE_TYPE_PRACH_DECODE,
                                  LTE_FDD_ENB_DEST_LAYER_MAC,
                                  (LTE_FDD_ENB_MESSAGE_UNION *)&prach_decode,
                                  sizeof(LTE_FDD_ENB_PRACH_DECODE_MSG_STRUCT));
            }
        }
    }

    sem_wait(&ul_sched_sem);
    if(0 != ul_schedule[ul_subframe.num].N_pucch ||
       0 != ul_schedule[ul_subframe.num].decodes.N_alloc)
    {
        subfr_err = liblte_phy_get_ul_subframe(phy_struct,
                                               rx_buf->i_buf[0],
                                               rx_buf->q_buf[0],
                                               &ul_subframe);
    }

    // Handle PUCCH
    if(LIBLTE_SUCCESS == subfr_err &&
       0              != ul_schedule[ul_subframe.num].N_pucch)
    {
        pucch_decode.current_tti = ul_current_tti;
        for(i=0; i<ul_schedule[ul_subframe.num].N_pucch; i++)
        {
            pucch_decode.rnti = ul_schedule[ul_subframe.num].pucch[i].rnti;
            pucch_decode.type = ul_schedule[ul_subframe.num].pucch[i].type;
            pucch_err = liblte_phy_pucch_format_1_1a_1b_channel_decode(phy_struct,
                                                                       &ul_subframe,
                                                                       LIBLTE_PHY_PUCCH_FORMAT_1B,
                                                                       sys_info.N_id_cell,
                                                                       sys_info.N_ant,
                                                                       ul_schedule[ul_subframe.num].pucch[i].n_1_p_pucch,
                                                                       pucch_decode.msg.msg,
                                                                       &pucch_decode.msg.N_bits);
            if(LTE_FDD_ENB_PUCCH_TYPE_ACK_NACK == pucch_decode.type)
            {
                if(LIBLTE_SUCCESS == pucch_err)
                {
                    pucch_decode.msg.msg[0] = 1;
                    pucch_decode.msg.N_bits = 1;
                }else{
                    pucch_decode.msg.msg[0] = 0;
                    pucch_decode.msg.N_bits = 1;
                }
                msgq_to_mac->send(LTE_FDD_ENB_MESSAGE_TYPE_PUCCH_DECODE,
                                  LTE_FDD_ENB_DEST_LAYER_MAC,
                                  (LTE_FDD_ENB_MESSAGE_UNION *)&pucch_decode,
                                  sizeof(LTE_FDD_ENB_PUCCH_DECODE_MSG_STRUCT));
            }else if(LIBLTE_SUCCESS == pucch_err){
                msgq_to_mac->send(LTE_FDD_ENB_MESSAGE_TYPE_PUCCH_DECODE,
                                  LTE_FDD_ENB_DEST_LAYER_MAC,
                                  (LTE_FDD_ENB_MESSAGE_UNION *)&pucch_decode,
                                  sizeof(LTE_FDD_ENB_PUCCH_DECODE_MSG_STRUCT));
            }
        }
    }
    ul_schedule[ul_subframe.num].N_pucch = 0;

    // Handle PUSCH
    if(LIBLTE_SUCCESS == subfr_err &&
       0              != ul_schedule[ul_subframe.num].decodes.N_alloc)
    {
        for(i=0; i<ul_schedule[ul_subframe.num].decodes.N_alloc; i++)
        {
            // Determine PHICH indecies
            I_prb_ra      = ul_schedule[ul_subframe.num].decodes.alloc[i].prb[0][0];
            n_group_phich = I_prb_ra % phy_struct->N_group_phich;
            n_seq_phich   = (I_prb_ra/phy_struct->N_group_phich) % (2*phy_struct->N_sf_phich);

            // Attempt decode
            if(LIBLTE_SUCCESS == liblte_phy_pusch_channel_decode(phy_struct,
                                                                 &ul_subframe,
                                                                 &ul_schedule[ul_subframe.num].decodes.alloc[i],
                                                                 sys_info.N_id_cell,
                                                                 1,
                                                                 pusch_decode.msg.msg,
                                                                 &pusch_decode.msg.N_bits))
            {
                pusch_decode.current_tti = ul_current_tti;
                pusch_decode.rnti        = ul_schedule[ul_subframe.num].decodes.alloc[i].rnti;

                msgq_to_mac->send(LTE_FDD_ENB_MESSAGE_TYPE_PUSCH_DECODE,
                                  LTE_FDD_ENB_DEST_LAYER_MAC,
                                  (LTE_FDD_ENB_MESSAGE_UNION *)&pusch_decode,
                                  sizeof(LTE_FDD_ENB_PUSCH_DECODE_MSG_STRUCT));

                // Add ACK to PHICH
                phich[(ul_subframe.num + 4) % 10].present[n_group_phich][n_seq_phich] = true;
                phich[(ul_subframe.num + 4) % 10].b[n_group_phich][n_seq_phich]       = 1;
            }else{
                // Add NACK to PHICH
                phich[(ul_subframe.num + 4) % 10].present[n_group_phich][n_seq_phich] = true;
                phich[(ul_subframe.num + 4) % 10].b[n_group_phich][n_seq_phich]       = 0;
            }
        }
    }
    ul_schedule[ul_subframe.num].decodes.N_alloc = 0;
    sem_post(&ul_sched_sem);

    // Update counters
    ul_current_tti = (ul_current_tti + 1) % (LTE_FDD_ENB_CURRENT_TTI_MAX + 1);
}
