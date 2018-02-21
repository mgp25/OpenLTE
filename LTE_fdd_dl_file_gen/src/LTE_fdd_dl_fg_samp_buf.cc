/*******************************************************************************

    Copyright 2012-2014, 2017 Ben Wojtowicz

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

    File: LTE_fdd_dl_fg_samp_buf.cc

    Description: Contains all the implementations for the LTE FDD DL File
                 Generator sample buffer block.

    Revision History
    ----------    -------------    --------------------------------------------
    06/16/2012    Ben Wojtowicz    Created file.
    08/19/2012    Ben Wojtowicz    Using the latest liblte library.
    11/10/2012    Ben Wojtowicz    Added SIB2 support and changed the parameter
                                   input method to be "interactive"
    12/01/2012    Ben Wojtowicz    Using the latest liblte library and added
                                   4 antenna support
    12/26/2012    Ben Wojtowicz    Added SIB3, SIB4, and SIB8 support, fixed a
                                   file size bug, and pulled in a 64 bit bug fix
                                   from Thomas Bertani.
    01/07/2013    Ben Wojtowicz    Moved from automake to cmake
    03/03/2013    Ben Wojtowicz    Added support for a test load and using the
                                   latest libraries.
    07/21/2013    Ben Wojtowicz    Using the latest LTE library.
    08/26/2013    Ben Wojtowicz    Updates to support GnuRadio 3.7 and the
                                   latest LTE library.
    09/16/2013    Ben Wojtowicz    Added support for changing the sample rate.
    09/28/2013    Ben Wojtowicz    Added support for setting the sample rate
                                   and output data type.
    01/18/2014    Ben Wojtowicz    Fixed a bug with transmitting SIB2 for 1.4MHz
                                   bandwidth.
    03/26/2014    Ben Wojtowicz    Using the latest LTE library.
    04/12/2014    Ben Wojtowicz    Using the latest LTE library.
    05/04/2014    Ben Wojtowicz    Added PHICH support.
    11/01/2014    Ben Wojtowicz    Using the latest LTE library.
    07/29/2017    Ben Wojtowicz    Using the latest LTE library.

*******************************************************************************/

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_fdd_dl_fg_samp_buf.h"
#include "liblte_mac.h"
#include <gnuradio/io_signature.h>
#include <errno.h>

/*******************************************************************************
                              DEFINES
*******************************************************************************/


/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/


/*******************************************************************************
                              GLOBAL VARIABLES
*******************************************************************************/

// minimum and maximum number of input and output streams
static const int32 MIN_IN  = 0;
static const int32 MAX_IN  = 0;
static const int32 MIN_OUT = 1;
static const int32 MAX_OUT = 1;

/*******************************************************************************
                              CLASS IMPLEMENTATIONS
*******************************************************************************/

LTE_fdd_dl_fg_samp_buf_sptr LTE_fdd_dl_fg_make_samp_buf(size_t out_size_val)
{
    return LTE_fdd_dl_fg_samp_buf_sptr(new LTE_fdd_dl_fg_samp_buf(out_size_val));
}

LTE_fdd_dl_fg_samp_buf::LTE_fdd_dl_fg_samp_buf(size_t out_size_val)
    : gr::sync_block ("samp_buf",
                      gr::io_signature::make(MIN_IN,  MAX_IN,  sizeof(int8)),
                      gr::io_signature::make(MIN_OUT, MAX_OUT, out_size_val))
{
    uint32 i;
    uint32 j;

    // Parse the inputs
    if(out_size_val == sizeof(gr_complex))
    {
        out_size = LTE_FDD_DL_FG_OUT_SIZE_GR_COMPLEX;
    }else if(out_size_val == sizeof(int8)){
        out_size = LTE_FDD_DL_FG_OUT_SIZE_INT8;
    }else{
        out_size = LTE_FDD_DL_FG_OUT_SIZE_INT8;
    }

    // Initialize the LTE parameters
    // General
    bandwidth    = 20;
    N_rb_dl      = LIBLTE_PHY_N_RB_DL_20MHZ;
    fs           = LIBLTE_PHY_FS_30_72MHZ;
    sfn          = 0;
    N_frames     = 30;
    N_ant        = 1;
    N_id_cell    = 0;
    N_id_2       = (N_id_cell % 3);
    N_id_1       = (N_id_cell - N_id_2)/3;
    sib_tx_mode  = 1;
    percent_load = 0;
    // MIB
    mib.dl_bw            = LIBLTE_RRC_DL_BANDWIDTH_100;
    mib.phich_config.dur = LIBLTE_RRC_PHICH_DURATION_NORMAL;
    mib.phich_config.res = LIBLTE_RRC_PHICH_RESOURCE_1;
    phich_res            = 1;
    mib.sfn_div_4        = sfn/4;
    // SIB1
    sib1.N_plmn_ids                       = 1;
    sib1.plmn_id[0].id.mcc                = 0xF001;
    sib1.plmn_id[0].id.mnc                = 0xFF01;
    sib1.plmn_id[0].resv_for_oper         = LIBLTE_RRC_NOT_RESV_FOR_OPER;
    sib1.N_sched_info                     = 1;
    sib1.sched_info[0].N_sib_mapping_info = 0;
    sib1.sched_info[0].si_periodicity     = LIBLTE_RRC_SI_PERIODICITY_RF8;
    si_periodicity_T                      = 8;
    sib1.cell_barred                      = LIBLTE_RRC_CELL_NOT_BARRED;
    sib1.intra_freq_reselection           = LIBLTE_RRC_INTRA_FREQ_RESELECTION_ALLOWED;
    sib1.si_window_length                 = LIBLTE_RRC_SI_WINDOW_LENGTH_MS2;
    si_win_len                            = 2;
    sib1.tdd_cnfg.sf_assignment           = LIBLTE_RRC_SUBFRAME_ASSIGNMENT_0;
    sib1.tdd_cnfg.special_sf_patterns     = LIBLTE_RRC_SPECIAL_SUBFRAME_PATTERNS_0;
    sib1.cell_id                          = 0;
    sib1.csg_id                           = 0;
    sib1.tracking_area_code               = 0;
    sib1.q_rx_lev_min                     = -140;
    sib1.csg_indication                   = 0;
    sib1.q_rx_lev_min_offset              = 1;
    sib1.freq_band_indicator              = 1;
    sib1.system_info_value_tag            = 0;
    sib1.p_max_present                    = true;
    sib1.p_max                            = -30;
    sib1.tdd                              = false;
    // SIB2
    sib2.ac_barring_info_present                                                      = false;
    sib2.rr_config_common_sib.rach_cnfg.num_ra_preambles                              = LIBLTE_RRC_NUMBER_OF_RA_PREAMBLES_N64;
    sib2.rr_config_common_sib.rach_cnfg.preambles_group_a_cnfg.present                = false;
    sib2.rr_config_common_sib.rach_cnfg.pwr_ramping_step                              = LIBLTE_RRC_POWER_RAMPING_STEP_DB6;
    sib2.rr_config_common_sib.rach_cnfg.preamble_init_rx_target_pwr                   = LIBLTE_RRC_PREAMBLE_INITIAL_RECEIVED_TARGET_POWER_DBM_N100;
    sib2.rr_config_common_sib.rach_cnfg.preamble_trans_max                            = LIBLTE_RRC_PREAMBLE_TRANS_MAX_N200;
    sib2.rr_config_common_sib.rach_cnfg.ra_resp_win_size                              = LIBLTE_RRC_RA_RESPONSE_WINDOW_SIZE_SF10;
    sib2.rr_config_common_sib.rach_cnfg.mac_con_res_timer                             = LIBLTE_RRC_MAC_CONTENTION_RESOLUTION_TIMER_SF64;
    sib2.rr_config_common_sib.rach_cnfg.max_harq_msg3_tx                              = 1;
    sib2.rr_config_common_sib.bcch_cnfg.modification_period_coeff                     = LIBLTE_RRC_MODIFICATION_PERIOD_COEFF_N2;
    sib2.rr_config_common_sib.pcch_cnfg.default_paging_cycle                          = LIBLTE_RRC_DEFAULT_PAGING_CYCLE_RF256;
    sib2.rr_config_common_sib.pcch_cnfg.nB                                            = LIBLTE_RRC_NB_ONE_T;
    sib2.rr_config_common_sib.prach_cnfg.root_sequence_index                          = 0;
    sib2.rr_config_common_sib.prach_cnfg.prach_cnfg_info.prach_config_index           = 0;
    sib2.rr_config_common_sib.prach_cnfg.prach_cnfg_info.high_speed_flag              = false;
    sib2.rr_config_common_sib.prach_cnfg.prach_cnfg_info.zero_correlation_zone_config = 0;
    sib2.rr_config_common_sib.prach_cnfg.prach_cnfg_info.prach_freq_offset            = 0;
    sib2.rr_config_common_sib.pdsch_cnfg.rs_power                                     = -60;
    sib2.rr_config_common_sib.pdsch_cnfg.p_b                                          = 0;
    sib2.rr_config_common_sib.pusch_cnfg.n_sb                                         = 1;
    sib2.rr_config_common_sib.pusch_cnfg.hopping_mode                                 = LIBLTE_RRC_HOPPING_MODE_INTER_SUBFRAME;
    sib2.rr_config_common_sib.pusch_cnfg.pusch_hopping_offset                         = 0;
    sib2.rr_config_common_sib.pusch_cnfg.enable_64_qam                                = true;
    sib2.rr_config_common_sib.pusch_cnfg.ul_rs.group_hopping_enabled                  = false;
    sib2.rr_config_common_sib.pusch_cnfg.ul_rs.group_assignment_pusch                 = 0;
    sib2.rr_config_common_sib.pusch_cnfg.ul_rs.sequence_hopping_enabled               = false;
    sib2.rr_config_common_sib.pusch_cnfg.ul_rs.cyclic_shift                           = 0;
    sib2.rr_config_common_sib.pucch_cnfg.delta_pucch_shift                            = LIBLTE_RRC_DELTA_PUCCH_SHIFT_DS1;
    sib2.rr_config_common_sib.pucch_cnfg.n_rb_cqi                                     = 0;
    sib2.rr_config_common_sib.pucch_cnfg.n_cs_an                                      = 0;
    sib2.rr_config_common_sib.pucch_cnfg.n1_pucch_an                                  = 0;
    sib2.rr_config_common_sib.srs_ul_cnfg.present                                     = false;
    sib2.rr_config_common_sib.ul_pwr_ctrl.p0_nominal_pusch                            = -70;
    sib2.rr_config_common_sib.ul_pwr_ctrl.alpha                                       = LIBLTE_RRC_UL_POWER_CONTROL_ALPHA_1;
    sib2.rr_config_common_sib.ul_pwr_ctrl.p0_nominal_pucch                            = -96;
    sib2.rr_config_common_sib.ul_pwr_ctrl.delta_flist_pucch.format_1                  = LIBLTE_RRC_DELTA_F_PUCCH_FORMAT_1_0;
    sib2.rr_config_common_sib.ul_pwr_ctrl.delta_flist_pucch.format_1b                 = LIBLTE_RRC_DELTA_F_PUCCH_FORMAT_1B_1;
    sib2.rr_config_common_sib.ul_pwr_ctrl.delta_flist_pucch.format_2                  = LIBLTE_RRC_DELTA_F_PUCCH_FORMAT_2_0;
    sib2.rr_config_common_sib.ul_pwr_ctrl.delta_flist_pucch.format_2a                 = LIBLTE_RRC_DELTA_F_PUCCH_FORMAT_2A_0;
    sib2.rr_config_common_sib.ul_pwr_ctrl.delta_flist_pucch.format_2b                 = LIBLTE_RRC_DELTA_F_PUCCH_FORMAT_2B_0;
    sib2.rr_config_common_sib.ul_pwr_ctrl.delta_preamble_msg3                         = -2;
    sib2.rr_config_common_sib.ul_cp_length                                            = LIBLTE_RRC_UL_CP_LENGTH_1;
    sib2.ue_timers_and_constants.t300                                                 = LIBLTE_RRC_T300_MS1000;
    sib2.ue_timers_and_constants.t301                                                 = LIBLTE_RRC_T301_MS1000;
    sib2.ue_timers_and_constants.t310                                                 = LIBLTE_RRC_T310_MS1000;
    sib2.ue_timers_and_constants.n310                                                 = LIBLTE_RRC_N310_N20;
    sib2.ue_timers_and_constants.t311                                                 = LIBLTE_RRC_T311_MS1000;
    sib2.ue_timers_and_constants.n311                                                 = LIBLTE_RRC_N311_N10;
    sib2.arfcn_value_eutra.present                                                    = false;
    sib2.ul_bw.present                                                                = false;
    sib2.additional_spectrum_emission                                                 = 1;
    sib2.mbsfn_subfr_cnfg_list_size                                                   = 0;
    sib2.time_alignment_timer                                                         = LIBLTE_RRC_TIME_ALIGNMENT_TIMER_SF500;
    // SIB3
    sib3_present                          = 0;
    sib3.q_hyst                           = LIBLTE_RRC_Q_HYST_DB_0;
    sib3.speed_state_resel_params.present = false;
    sib3.s_non_intra_search_present       = false;
    sib3.thresh_serving_low               = 0;
    sib3.cell_resel_prio                  = 0;
    sib3.q_rx_lev_min                     = sib1.q_rx_lev_min;
    sib3.p_max_present                    = true;
    sib3.p_max                            = sib1.p_max;
    sib3.s_intra_search_present           = false;
    sib3.allowed_meas_bw_present          = false;
    sib3.presence_ant_port_1              = false;
    sib3.neigh_cell_cnfg                  = 0;
    sib3.t_resel_eutra                    = 0;
    sib3.t_resel_eutra_sf_present         = false;
    // SIB4
    sib4_present                         = 0;
    sib4.intra_freq_neigh_cell_list_size = 0;
    sib4.intra_freq_black_cell_list_size = 0;
    sib4.csg_phys_cell_id_range_present  = false;
    // SIB8
    sib8_present                 = 0;
    sib8.sys_time_info_present   = false;
    sib8.search_win_size_present = true;
    sib8.search_win_size         = 0;
    sib8.params_hrpd_present     = false;
    sib8.params_1xrtt_present    = false;
    // PCFICH
    pcfich.cfi = 2;
    // PHICH
    for(i=0; i<25; i++)
    {
        for(j=0; j<8; j++)
        {
            phich.present[i][j] = false;
        }
    }

    // Initialize the configuration
    need_config = true;

    // Allocate the sample buffer
    i_buf           = (float *)malloc(4*LTE_FDD_DL_FG_SAMP_BUF_SIZE*sizeof(float));
    q_buf           = (float *)malloc(4*LTE_FDD_DL_FG_SAMP_BUF_SIZE*sizeof(float));
    samp_buf_idx    = 0;
    samples_ready   = false;
    last_samp_was_i = false;
}
LTE_fdd_dl_fg_samp_buf::~LTE_fdd_dl_fg_samp_buf()
{
    // Cleanup the LTE library
    liblte_phy_cleanup(phy_struct);

    // Free the sample buffer
    free(i_buf);
    free(q_buf);
}

int32 LTE_fdd_dl_fg_samp_buf::work(int32                      noutput_items,
                                   gr_vector_const_void_star &input_items,
                                   gr_vector_void_star       &output_items)
{
    gr_complex *gr_complex_out = (gr_complex *)output_items[0];
    float       i_samp;
    float       q_samp;
    int32       act_noutput_items;
    uint32      out_idx;
    uint32      loop_cnt;
    uint32      i;
    uint32      j;
    uint32      k;
    uint32      p;
    uint32      N_sfr;
    uint32      last_prb;
    uint32      max_N_prb;
    size_t      line_size = LINE_MAX;
    ssize_t     N_line_chars;
    int8       *int8_out = (int8 *)output_items[0];
    char       *line;
    bool        done = false;

    line = (char *)malloc(line_size);
    if(need_config)
    {
        print_config();
    }
    while(need_config)
    {
        N_line_chars         = getline(&line, &line_size, stdin);
        line[strlen(line)-1] = '\0';
        change_config(line);
        if(!need_config)
        {
            // Initialize the LTE library
            liblte_phy_init(&phy_struct,
                            fs,
                            N_id_cell,
                            N_ant,
                            N_rb_dl,
                            LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP,
                            phich_res);
        }
    }
    free(line);

    if(false == samples_ready)
    {
        // Generate frame
        if(sfn < N_frames)
        {
            for(N_sfr=0; N_sfr<10; N_sfr++)
            {
                // Initialize the output to all zeros
                for(p=0; p<N_ant; p++)
                {
                    for(j=0; j<16; j++)
                    {
                        for(k=0; k<LIBLTE_PHY_N_RB_DL_20MHZ*LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP; k++)
                        {
                            subframe.tx_symb_re[p][j][k] = 0;
                            subframe.tx_symb_im[p][j][k] = 0;
                        }
                    }
                }
                subframe.num = N_sfr;

                // PSS and SSS
                if(subframe.num == 0 ||
                   subframe.num == 5)
                {
                    liblte_phy_map_pss(phy_struct,
                                       &subframe,
                                       N_id_2,
                                       N_ant);
                    liblte_phy_map_sss(phy_struct,
                                       &subframe,
                                       N_id_1,
                                       N_id_2,
                                       N_ant);
                }

                // CRS
                liblte_phy_map_crs(phy_struct,
                                   &subframe,
                                   N_id_cell,
                                   N_ant);

                // PBCH
                if(subframe.num == 0)
                {
                    mib.sfn_div_4 = sfn/4;
                    liblte_rrc_pack_bcch_bch_msg(&mib,
                                                 &rrc_msg);
                    liblte_phy_bch_channel_encode(phy_struct,
                                                  rrc_msg.msg,
                                                  rrc_msg.N_bits,
                                                  N_id_cell,
                                                  N_ant,
                                                  &subframe,
                                                  sfn);
                }

                // PDCCH & PDSCH
                pdcch.N_alloc = 0;
                if(subframe.num == 5 &&
                   (sfn % 2)    == 0)
                {
                    // SIB1
                    bcch_dlsch_msg.N_sibs           = 0;
                    bcch_dlsch_msg.sibs[0].sib_type = LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_1;
                    memcpy(&bcch_dlsch_msg.sibs[0].sib, &sib1, sizeof(LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_1_STRUCT));
                    liblte_rrc_pack_bcch_dlsch_msg(&bcch_dlsch_msg,
                                                   &pdcch.alloc[pdcch.N_alloc].msg[0]);
                    liblte_phy_get_tbs_mcs_and_n_prb_for_dl(pdcch.alloc[pdcch.N_alloc].msg[0].N_bits,
                                                            subframe.num,
                                                            N_rb_dl,
                                                            LIBLTE_MAC_SI_RNTI,
                                                            &pdcch.alloc[pdcch.N_alloc].tbs,
                                                            &pdcch.alloc[pdcch.N_alloc].mcs,
                                                            &pdcch.alloc[pdcch.N_alloc].N_prb);
                    pdcch.alloc[pdcch.N_alloc].pre_coder_type = LIBLTE_PHY_PRE_CODER_TYPE_TX_DIVERSITY;
                    pdcch.alloc[pdcch.N_alloc].mod_type       = LIBLTE_PHY_MODULATION_TYPE_QPSK;
                    pdcch.alloc[pdcch.N_alloc].rv_idx         = (uint32)ceilf(1.5 * ((sfn / 2) % 4)) % 4; //36.321 section 5.3.1
                    pdcch.alloc[pdcch.N_alloc].N_codewords    = 1;
                    pdcch.alloc[pdcch.N_alloc].rnti           = LIBLTE_MAC_SI_RNTI;
                    pdcch.alloc[pdcch.N_alloc].tx_mode        = sib_tx_mode;
                    pdcch.N_alloc++;
                }
                if(subframe.num             >=  (0 * si_win_len)%10 &&
                   subframe.num             <   (1 * si_win_len)%10 &&
                   (sfn % si_periodicity_T) == ((0 * si_win_len)/10))
                {
                    // SIs in 1st scheduling info list entry
                    bcch_dlsch_msg.N_sibs           = 1;
                    bcch_dlsch_msg.sibs[0].sib_type = LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_2;
                    memcpy(&bcch_dlsch_msg.sibs[0].sib, &sib2, sizeof(LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_2_STRUCT));
                    if(sib1.sched_info[0].N_sib_mapping_info != 0)
                    {
                        switch(sib1.sched_info[0].sib_mapping_info[0].sib_type)
                        {
                        case LIBLTE_RRC_SIB_TYPE_3:
                            bcch_dlsch_msg.N_sibs++;
                            bcch_dlsch_msg.sibs[1].sib_type = LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_3;
                            memcpy(&bcch_dlsch_msg.sibs[1].sib, &sib3, sizeof(LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_3_STRUCT));
                            break;
                        case LIBLTE_RRC_SIB_TYPE_4:
                            bcch_dlsch_msg.N_sibs++;
                            bcch_dlsch_msg.sibs[1].sib_type = LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_4;
                            memcpy(&bcch_dlsch_msg.sibs[1].sib, &sib4, sizeof(LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_4_STRUCT));
                            break;
                        case LIBLTE_RRC_SIB_TYPE_8:
                            bcch_dlsch_msg.N_sibs++;
                            bcch_dlsch_msg.sibs[1].sib_type = LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_8;
                            memcpy(&bcch_dlsch_msg.sibs[1].sib, &sib8, sizeof(LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_8_STRUCT));
                            break;
                        default:
                            break;
                        }
                    }
                    liblte_rrc_pack_bcch_dlsch_msg(&bcch_dlsch_msg,
                                                   &pdcch.alloc[pdcch.N_alloc].msg[0]);

                    // FIXME: This was a hack to allow SIB2 decoding with 1.4MHz BW due to overlap with MIB
                    if(LIBLTE_SUCCESS == liblte_phy_get_tbs_mcs_and_n_prb_for_dl(pdcch.alloc[pdcch.N_alloc].msg[0].N_bits,
                                                                                 subframe.num,
                                                                                 N_rb_dl,
                                                                                 LIBLTE_MAC_SI_RNTI,
                                                                                 &pdcch.alloc[pdcch.N_alloc].tbs,
                                                                                 &pdcch.alloc[pdcch.N_alloc].mcs,
                                                                                 &pdcch.alloc[pdcch.N_alloc].N_prb))
                    {
                        pdcch.alloc[pdcch.N_alloc].pre_coder_type = LIBLTE_PHY_PRE_CODER_TYPE_TX_DIVERSITY;
                        pdcch.alloc[pdcch.N_alloc].mod_type       = LIBLTE_PHY_MODULATION_TYPE_QPSK;
                        pdcch.alloc[pdcch.N_alloc].rv_idx         = 0; //36.321 section 5.3.1
                        pdcch.alloc[pdcch.N_alloc].N_codewords    = 1;
                        pdcch.alloc[pdcch.N_alloc].rnti           = LIBLTE_MAC_SI_RNTI;
                        pdcch.alloc[pdcch.N_alloc].tx_mode        = sib_tx_mode;
                        pdcch.N_alloc++;
                    }
                }
                for(j=1; j<sib1.N_sched_info; j++)
                {
                    if(subframe.num             ==  (j * si_win_len)%10 &&
                       (sfn % si_periodicity_T) == ((j * si_win_len)/10))
                    {
                        // SIs in the jth scheduling info list entry
                        bcch_dlsch_msg.N_sibs = sib1.sched_info[j].N_sib_mapping_info;
                        for(i=0; i<bcch_dlsch_msg.N_sibs; i++)
                        {
                            switch(sib1.sched_info[j].sib_mapping_info[i].sib_type)
                            {
                            case LIBLTE_RRC_SIB_TYPE_3:
                                bcch_dlsch_msg.sibs[i].sib_type = LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_3;
                                memcpy(&bcch_dlsch_msg.sibs[i].sib, &sib3, sizeof(LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_3_STRUCT));
                                break;
                            case LIBLTE_RRC_SIB_TYPE_4:
                                bcch_dlsch_msg.sibs[i].sib_type = LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_4;
                                memcpy(&bcch_dlsch_msg.sibs[i].sib, &sib4, sizeof(LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_4_STRUCT));
                                break;
                            case LIBLTE_RRC_SIB_TYPE_8:
                                bcch_dlsch_msg.sibs[i].sib_type = LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_8;
                                memcpy(&bcch_dlsch_msg.sibs[i].sib, &sib8, sizeof(LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_8_STRUCT));
                                break;
                            default:
                                break;
                            }
                        }
                        if(0 != bcch_dlsch_msg.N_sibs)
                        {
                            liblte_rrc_pack_bcch_dlsch_msg(&bcch_dlsch_msg,
                                                           &pdcch.alloc[pdcch.N_alloc].msg[0]);
                            liblte_phy_get_tbs_mcs_and_n_prb_for_dl(pdcch.alloc[pdcch.N_alloc].msg[0].N_bits,
                                                                    subframe.num,
                                                                    N_rb_dl,
                                                                    LIBLTE_MAC_SI_RNTI,
                                                                    &pdcch.alloc[pdcch.N_alloc].tbs,
                                                                    &pdcch.alloc[pdcch.N_alloc].mcs,
                                                                    &pdcch.alloc[pdcch.N_alloc].N_prb);
                            pdcch.alloc[pdcch.N_alloc].pre_coder_type = LIBLTE_PHY_PRE_CODER_TYPE_TX_DIVERSITY;
                            pdcch.alloc[pdcch.N_alloc].mod_type       = LIBLTE_PHY_MODULATION_TYPE_QPSK;
                            pdcch.alloc[pdcch.N_alloc].rv_idx         = 0; //36.321 section 5.3.1
                            pdcch.alloc[pdcch.N_alloc].N_codewords    = 1;
                            pdcch.alloc[pdcch.N_alloc].rnti           = LIBLTE_MAC_SI_RNTI;
                            pdcch.alloc[pdcch.N_alloc].tx_mode        = sib_tx_mode;
                            pdcch.N_alloc++;
                        }
                    }
                }
                // Add test load
                if(0 == pdcch.N_alloc)
                {
                    pdcch.alloc[0].msg[0].N_bits = 0;
                    pdcch.alloc[0].N_prb         = 0;
                    liblte_phy_get_tbs_mcs_and_n_prb_for_dl(1480,
                                                            subframe.num,
                                                            N_rb_dl,
                                                            LIBLTE_MAC_P_RNTI,
                                                            &pdcch.alloc[0].tbs,
                                                            &pdcch.alloc[0].mcs,
                                                            &max_N_prb);
                    while(pdcch.alloc[0].N_prb < (uint32)((float)(max_N_prb*percent_load)/100.0))
                    {
                        pdcch.alloc[0].msg[0].N_bits += 8;
                        liblte_phy_get_tbs_mcs_and_n_prb_for_dl(pdcch.alloc[0].msg[0].N_bits,
                                                                subframe.num,
                                                                N_rb_dl,
                                                                LIBLTE_MAC_P_RNTI,
                                                                &pdcch.alloc[0].tbs,
                                                                &pdcch.alloc[0].mcs,
                                                                &pdcch.alloc[0].N_prb);
                    }
                    for(i=0; i<pdcch.alloc[0].msg[0].N_bits; i++)
                    {
                        pdcch.alloc[0].msg[0].msg[i] = liblte_rrc_test_fill[i%8];
                    }
                    if(0 != pdcch.alloc[0].N_prb)
                    {
                        pdcch.alloc[0].pre_coder_type = LIBLTE_PHY_PRE_CODER_TYPE_TX_DIVERSITY;
                        pdcch.alloc[0].mod_type       = LIBLTE_PHY_MODULATION_TYPE_QPSK;
                        pdcch.alloc[0].N_codewords    = 1;
                        pdcch.alloc[0].rnti           = LIBLTE_MAC_P_RNTI;
                        pdcch.alloc[0].tx_mode        = sib_tx_mode;
                        pdcch.N_alloc++;
                    }
                }

                // Schedule all allocations
                // FIXME: Scheduler
                last_prb = 0;
                for(i=0; i<pdcch.N_alloc; i++)
                {
                    for(j=0; j<pdcch.alloc[i].N_prb; j++)
                    {
                        pdcch.alloc[i].prb[0][j] = last_prb;
                        pdcch.alloc[i].prb[1][j] = last_prb++;
                    }
                }
                if(0 != pdcch.N_alloc)
                {
                    liblte_phy_pdcch_channel_encode(phy_struct,
                                                    &pcfich,
                                                    &phich,
                                                    &pdcch,
                                                    N_id_cell,
                                                    N_ant,
                                                    phich_res,
                                                    mib.phich_config.dur,
                                                    &subframe);
                    liblte_phy_pdsch_channel_encode(phy_struct,
                                                    &pdcch,
                                                    N_id_cell,
                                                    N_ant,
                                                    &subframe);
                }

                // Construct the output
                for(p=0; p<N_ant; p++)
                {
                    liblte_phy_create_dl_subframe(phy_struct,
                                                  &subframe,
                                                  p,
                                                  &i_buf[(p*phy_struct->N_samps_per_frame) + (subframe.num*phy_struct->N_samps_per_subfr)],
                                                  &q_buf[(p*phy_struct->N_samps_per_frame) + (subframe.num*phy_struct->N_samps_per_subfr)]);
                }
            }
        }else{
            done = true;
        }
        sfn++;
        samples_ready = true;
    }

    if(false == done &&
       true  == samples_ready)
    {
        act_noutput_items = 0;
        out_idx           = 0;
        if(noutput_items > 0)
        {
            if(LTE_FDD_DL_FG_OUT_SIZE_INT8 == out_size)
            {
                // Write out the first half sample if needed
                if(true == last_samp_was_i)
                {
                    q_samp = 0;
                    for(p=0; p<N_ant; p++)
                    {
                        q_samp += q_buf[(p*phy_struct->N_samps_per_frame) + samp_buf_idx];
                    }
                    int8_out[out_idx++] = (int8)(q_samp);
                    samp_buf_idx++;
                    act_noutput_items++;
                }

                // Determine how many full samples to write
                if((phy_struct->N_samps_per_frame - samp_buf_idx) < ((noutput_items - act_noutput_items) / 2))
                {
                    loop_cnt = (phy_struct->N_samps_per_frame - samp_buf_idx)*2;
                }else{
                    loop_cnt = noutput_items - act_noutput_items;
                }

                // Write out the full samples
                for(i=0; i<loop_cnt/2; i++)
                {
                    i_samp = 0;
                    q_samp = 0;
                    for(p=0; p<N_ant; p++)
                    {
                        i_samp += i_buf[(p*phy_struct->N_samps_per_frame) + samp_buf_idx];
                        q_samp += q_buf[(p*phy_struct->N_samps_per_frame) + samp_buf_idx];
                    }

                    int8_out[out_idx++] = (int8)(i_samp);
                    int8_out[out_idx++] = (int8)(q_samp);
                    samp_buf_idx++;
                    act_noutput_items += 2;
                }

                // Write out the last half sample if needed
                if((noutput_items - act_noutput_items) == 1)
                {
                    i_samp = 0;
                    for(p=0; p<N_ant; p++)
                    {
                        i_samp += i_buf[(p*phy_struct->N_samps_per_frame) + samp_buf_idx];
                    }
                    int8_out[out_idx++] = (int8)(i_samp);
                    act_noutput_items++;
                    last_samp_was_i = true;
                }else{
                    last_samp_was_i = false;
                }
            }else{ // LTE_FDD_DL_FG_OUT_SIZE_GR_COMPLEX == out_size
                // Determine how many samples to write
                if((phy_struct->N_samps_per_frame - samp_buf_idx) < noutput_items)
                {
                    loop_cnt = phy_struct->N_samps_per_frame - samp_buf_idx;
                }else{
                    loop_cnt = noutput_items;
                }

                // Write out samples
                for(i=0; i<loop_cnt; i++)
                {
                    i_samp = 0;
                    q_samp = 0;
                    for(p=0; p<N_ant; p++)
                    {
                        i_samp += i_buf[(p*phy_struct->N_samps_per_frame) + samp_buf_idx];
                        q_samp += q_buf[(p*phy_struct->N_samps_per_frame) + samp_buf_idx];
                    }
                    gr_complex_out[out_idx++] = gr_complex(i_samp, q_samp);
                    samp_buf_idx++;
                    act_noutput_items++;
                }
            }
        }

        // Check to see if we need more samples
        if(samp_buf_idx >= phy_struct->N_samps_per_frame)
        {
            samples_ready = false;
            samp_buf_idx  = 0;
        }
    }else if(true == done){
        // No more samples to write to file, so mark as done
        act_noutput_items = -1;
    }else{
        // Should never get here
        act_noutput_items = 0;
        samples_ready     = false;
    }

    // Tell runtime system how many input items we consumed
    consume_each(0);

    // Tell runtime system how many output items we produced
    return(act_noutput_items);
}

void LTE_fdd_dl_fg_samp_buf::recreate_sched_info(void)
{
    LIBLTE_RRC_SIB_TYPE_ENUM sib_array[20];
    uint32                   num_sibs      = 0;
    uint32                   sib_idx       = 0;
    uint32                   N_sibs_to_map = 0;
    uint32                   i;

    // Determine which SIBs need to be mapped
    if(1 == sib3_present)
    {
        sib_array[num_sibs++] = LIBLTE_RRC_SIB_TYPE_3;
    }
    if(1 == sib4_present)
    {
        sib_array[num_sibs++] = LIBLTE_RRC_SIB_TYPE_4;
    }
    if(1 == sib8_present)
    {
        sib_array[num_sibs++] = LIBLTE_RRC_SIB_TYPE_8;
    }

    // Initialize the scheduling info
    sib1.N_sched_info                     = 1;
    sib1.sched_info[0].N_sib_mapping_info = 0;

    // Map the SIBs
    while(num_sibs > 0)
    {
        // Determine how many SIBs can be mapped to this scheduling info
        if(1 == sib1.N_sched_info)
        {
            if(0                         == sib1.sched_info[0].N_sib_mapping_info &&
               LIBLTE_PHY_N_RB_DL_1_4MHZ != N_rb_dl)
            {
                N_sibs_to_map = 1;
            }else{
                N_sibs_to_map                                         = 2;
                sib1.sched_info[sib1.N_sched_info].N_sib_mapping_info = 0;
                sib1.sched_info[sib1.N_sched_info].si_periodicity     = LIBLTE_RRC_SI_PERIODICITY_RF8;
                sib1.N_sched_info++;
            }
        }else{
            if(2 > sib1.sched_info[sib1.N_sched_info-1].N_sib_mapping_info)
            {
                N_sibs_to_map = 2 - sib1.sched_info[sib1.N_sched_info-1].N_sib_mapping_info;
            }else{
                N_sibs_to_map                                         = 2;
                sib1.sched_info[sib1.N_sched_info].N_sib_mapping_info = 0;
                sib1.sched_info[sib1.N_sched_info].si_periodicity     = LIBLTE_RRC_SI_PERIODICITY_RF8;
                sib1.N_sched_info++;
            }
        }

        // Map the SIBs for this scheduling info
        for(i=0; i<N_sibs_to_map; i++)
        {
            sib1.sched_info[sib1.N_sched_info-1].sib_mapping_info[sib1.sched_info[sib1.N_sched_info-1].N_sib_mapping_info].sib_type = sib_array[sib_idx++];
            sib1.sched_info[sib1.N_sched_info-1].N_sib_mapping_info++;
            num_sibs--;

            if(0 == num_sibs)
            {
                break;
            }
        }
    }
}

void LTE_fdd_dl_fg_samp_buf::print_config(void)
{
    uint32 i;

    printf("***System Configuration Parameters***\n");
    printf("\tType 'help' to reprint this menu\n");
    printf("\tHit enter to finish config and generate file\n");
    printf("\tSet parameters using <param>=<value> format\n");

    // BANDWIDTH
    printf("\t%-30s = ",
           BANDWIDTH_PARAM);
    switch(N_rb_dl)
    {
    case LIBLTE_PHY_N_RB_DL_1_4MHZ:
        printf("%10s", "1.4");
        break;
    case LIBLTE_PHY_N_RB_DL_3MHZ:
        printf("%10s", "3");
        break;
    case LIBLTE_PHY_N_RB_DL_5MHZ:
        printf("%10s", "5");
        break;
    case LIBLTE_PHY_N_RB_DL_10MHZ:
        printf("%10s", "10");
        break;
    case LIBLTE_PHY_N_RB_DL_15MHZ:
        printf("%10s", "15");
        break;
    case LIBLTE_PHY_N_RB_DL_20MHZ:
        printf("%10s", "20");
        break;
    }
    printf(", values = [1.4, 3, 5, 10, 15, 20]\n");

    // FS
    printf("\t%-30s = %10s, values = [",
           FS_PARAM,
           liblte_phy_fs_text[fs]);
    for(i=0; i<LIBLTE_PHY_FS_N_ITEMS; i++)
    {
        if(0 != i)
        {
            printf(", ");
        }
        printf("%s", liblte_phy_fs_text[i]);
    }
    printf("]\n");

    // FREQ_BAND
    printf("\t%-30s = %10u, bounds = [1, 25]\n",
           FREQ_BAND_PARAM,
           sib1.freq_band_indicator);

    // N_frames
    printf("\t%-30s = %10u, bounds = [1, 1000]\n",
           N_FRAMES_PARAM,
           N_frames);

    // N_ant
    printf("\t%-30s = %10u, values = [1, 2, 4]\n",
           N_ANT_PARAM,
           N_ant);

    // N_id_cell
    printf("\t%-30s = %10u, bounds = [0, 503]\n",
           N_ID_CELL_PARAM,
           N_id_cell);

    // MCC
    printf("\t%-30s = %7s%03X, bounds = [000, 999]\n",
           MCC_PARAM, "",
           (sib1.plmn_id[0].id.mcc & 0x0FFF));

    // MNC
    if((sib1.plmn_id[0].id.mnc & 0xFF00) == 0xFF00)
    {
        printf("\t%-30s = %8s%02X, bounds = [00, 999]\n",
               MNC_PARAM, "",
               (sib1.plmn_id[0].id.mnc & 0x00FF));
    }else{
        printf("\t%-30s = %7s%03X, bounds = [00, 999]\n",
               MNC_PARAM, "",
               (sib1.plmn_id[0].id.mnc & 0x0FFF));
    }

    // CELL_ID
    printf("\t%-30s = %10u, bounds = [0, 268435455]\n",
           CELL_ID_PARAM,
           sib1.cell_id);

    // TRACKING_AREA_CODE
    printf("\t%-30s = %10u, bounds = [0, 65535]\n",
           TRACKING_AREA_CODE_PARAM,
           sib1.tracking_area_code);

    // Q_RX_LEV_MIN
    printf("\t%-30s = %10d, bounds = [-140, -44]\n",
           Q_RX_LEV_MIN_PARAM,
           sib1.q_rx_lev_min);

    // P0_NOMINAL_PUSCH
    printf("\t%-30s = %10d, bounds = [-126, 24]\n",
           P0_NOMINAL_PUSCH_PARAM,
           sib2.rr_config_common_sib.ul_pwr_ctrl.p0_nominal_pusch);

    // P0_NOMINAL_PUCCH
    printf("\t%-30s = %10d, bounds = [-127, -96]\n",
           P0_NOMINAL_PUCCH_PARAM,
           sib2.rr_config_common_sib.ul_pwr_ctrl.p0_nominal_pucch);

    // SIB3_PRESENT
    printf("\t%-30s = %10d, bounds = [0, 1]\n",
           SIB3_PRESENT_PARAM,
           sib3_present);
    if(sib3_present)
    {
        // Q_HYST
        printf("\t%-30s = %10s, values = [0, 1, 2, 3, 4, 5, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24]\n",
               Q_HYST_PARAM,
               liblte_rrc_q_hyst_text[sib3.q_hyst]);
    }

    // SIB4_PRESENT
    printf("\t%-30s = %10d, bounds = [0, 1]\n",
           SIB4_PRESENT_PARAM,
           sib4_present);
    if(sib4_present)
    {
        // NEIGH_CELL_LIST
        printf("\t%-30s = %10d:",
               NEIGH_CELL_LIST_PARAM,
               sib4.intra_freq_neigh_cell_list_size);
        for(i=0; i<sib4.intra_freq_neigh_cell_list_size; i++)
        {
            printf("%u,%s;",
                   sib4.intra_freq_neigh_cell_list[i].phys_cell_id,
                   liblte_rrc_q_offset_range_text[sib4.intra_freq_neigh_cell_list[i].q_offset_range]);
        }
        printf(" format=<list_size>:<phys_cell_id_0>,<q_offset_range_0>;...;<phys_cell_id_n>,<q_offset_range_n>\n");
    }

    // SIB8_PRESENT
    printf("\t%-30s = %10d, bounds = [0, 1]\n",
           SIB8_PRESENT_PARAM,
           sib8_present);
    if(sib8_present)
    {
        // SEARCH_WIN_SIZE
        printf("\t%-30s = %10d, bounds = [0, 15]\n",
               SEARCH_WIN_SIZE_PARAM,
               sib8.search_win_size);
    }

    // PERCENT_LOAD
    printf("\t%-30s = %10u, bounds = [0, 66]\n",
           PERCENT_LOAD_PARAM,
           percent_load);
}

void LTE_fdd_dl_fg_samp_buf::change_config(char *line)
{
    char *param;
    char *value;
    bool  err = false;

    param = strtok(line, "=");
    value = strtok(NULL, "=");

    if(param == NULL)
    {
        need_config = false;
    }else{
        if(!strcasecmp(param, "help"))
        {
            print_config();
        }else if(value != NULL){
            if(!strcasecmp(param, BANDWIDTH_PARAM))
            {
                err = set_bandwidth(value);
            }else if(!strcasecmp(param, FS_PARAM)){
                err = set_fs(value);
            }else if(!strcasecmp(param, FREQ_BAND_PARAM)){
                err = set_param(&sib1.freq_band_indicator, value, 1, 25);
            }else if(!strcasecmp(param, N_FRAMES_PARAM)){
                err = set_param(&N_frames, value, 1, 1000);
            }else if(!strcasecmp(param, N_ANT_PARAM)){
                err = set_n_ant(value);
            }else if(!strcasecmp(param, N_ID_CELL_PARAM)){
                err = set_n_id_cell(value);
            }else if(!strcasecmp(param, MCC_PARAM)){
                err = set_mcc(value);
            }else if(!strcasecmp(param, MNC_PARAM)){
                err = set_mnc(value);
            }else if(!strcasecmp(param, CELL_ID_PARAM)){
                err = set_param(&sib1.cell_id, value, 0, 268435455);
            }else if(!strcasecmp(param, TRACKING_AREA_CODE_PARAM)){
                err = set_param(&sib1.tracking_area_code, value, 0, 65535);
            }else if(!strcasecmp(param, Q_RX_LEV_MIN_PARAM)){
                err = set_param(&sib1.q_rx_lev_min, value, -140, -44);
                sib3.q_rx_lev_min = sib1.q_rx_lev_min;
            }else if(!strcasecmp(param, P0_NOMINAL_PUSCH_PARAM)){
                err = set_param(&sib2.rr_config_common_sib.ul_pwr_ctrl.p0_nominal_pusch, value, -126, 24);
            }else if(!strcasecmp(param, P0_NOMINAL_PUCCH_PARAM)){
                err = set_param(&sib2.rr_config_common_sib.ul_pwr_ctrl.p0_nominal_pucch, value, -127, -96);
            }else if(!strcasecmp(param, SIB3_PRESENT_PARAM)){
                err = set_param(&sib3_present, value, 0, 1);
                recreate_sched_info();
            }else if(!strcasecmp(param, Q_HYST_PARAM)){
                err = set_q_hyst(value);
            }else if(!strcasecmp(param, SIB4_PRESENT_PARAM)){
                err = set_param(&sib4_present, value, 0, 1);
                recreate_sched_info();
            }else if(!strcasecmp(param, NEIGH_CELL_LIST_PARAM)){
                err = set_neigh_cell_list(value);
            }else if(!strcasecmp(param, SIB8_PRESENT_PARAM)){
                err = set_param(&sib8_present, value, 0, 1);
                recreate_sched_info();
            }else if(!strcasecmp(param, SEARCH_WIN_SIZE_PARAM)){
                err = set_param(&sib8.search_win_size, value, 0, 15);
            }else if(!strcasecmp(param, PERCENT_LOAD_PARAM)){
                err = set_param(&percent_load, value, 0, 66); // FIXME: Decode issues if load is greater than 66%
            }else{
                printf("Invalid parameter (%s)\n", param);
            }

            if(err)
            {
                printf("Invalid value\n");
            }
        }else{
            printf("Invalid value\n");
        }
    }
}

bool LTE_fdd_dl_fg_samp_buf::set_bandwidth(char *char_value)
{
    bool err = false;

    if(!strcasecmp(char_value, "1.4"))
    {
        bandwidth = 1.4;
        N_rb_dl   = LIBLTE_PHY_N_RB_DL_1_4MHZ;
        mib.dl_bw = LIBLTE_RRC_DL_BANDWIDTH_6;
    }else if(!strcasecmp(char_value, "3") &&
             fs >= LIBLTE_PHY_FS_3_84MHZ){
        bandwidth = 3;
        N_rb_dl   = LIBLTE_PHY_N_RB_DL_3MHZ;
        mib.dl_bw = LIBLTE_RRC_DL_BANDWIDTH_15;
    }else if(!strcasecmp(char_value, "5") &&
             fs >= LIBLTE_PHY_FS_7_68MHZ){
        bandwidth = 5;
        N_rb_dl   = LIBLTE_PHY_N_RB_DL_5MHZ;
        mib.dl_bw = LIBLTE_RRC_DL_BANDWIDTH_25;
    }else if(!strcasecmp(char_value, "10") &&
             fs >= LIBLTE_PHY_FS_15_36MHZ){
        bandwidth = 10;
        N_rb_dl   = LIBLTE_PHY_N_RB_DL_10MHZ;
        mib.dl_bw = LIBLTE_RRC_DL_BANDWIDTH_50;
    }else if(!strcasecmp(char_value, "15") &&
             fs == LIBLTE_PHY_FS_30_72MHZ){
        bandwidth = 15;
        N_rb_dl   = LIBLTE_PHY_N_RB_DL_15MHZ;
        mib.dl_bw = LIBLTE_RRC_DL_BANDWIDTH_75;
    }else if(!strcasecmp(char_value, "20") &&
             fs == LIBLTE_PHY_FS_30_72MHZ){
        bandwidth = 20;
        N_rb_dl   = LIBLTE_PHY_N_RB_DL_20MHZ;
        mib.dl_bw = LIBLTE_RRC_DL_BANDWIDTH_100;
    }else{
        err = true;
    }

    recreate_sched_info();

    return(err);
}

bool LTE_fdd_dl_fg_samp_buf::set_fs(char *char_value)
{
    bool err = false;

    if(!strcasecmp(char_value, "1.92") &&
       N_rb_dl == LIBLTE_PHY_N_RB_DL_1_4MHZ)
    {
        fs = LIBLTE_PHY_FS_1_92MHZ;
    }else if(!strcasecmp(char_value, "3.84") &&
             bandwidth <= 3){
        fs = LIBLTE_PHY_FS_3_84MHZ;
    }else if(!strcasecmp(char_value, "7.68") &&
             bandwidth <= 5){
        fs = LIBLTE_PHY_FS_7_68MHZ;
    }else if(!strcasecmp(char_value, "15.36") &&
             bandwidth <= 10){
        fs = LIBLTE_PHY_FS_15_36MHZ;
    }else if(!strcasecmp(char_value, "30.72")){
        fs = LIBLTE_PHY_FS_30_72MHZ;
    }else{
        err = true;
    }

    return(err);
}

bool LTE_fdd_dl_fg_samp_buf::set_n_ant(char *char_value)
{
    uint32 value;
    bool   err = true;

    if(false  == char_to_uint32(char_value, &value) &&
       (value == 1                                  ||
        value == 2                                  ||
        value == 4))
    {
        N_ant = value;
        err   = false;

        // Set the SIB tx_mode
        if(1 == N_ant)
        {
            sib_tx_mode = 1;
        }else{
            sib_tx_mode = 2;
        }
    }

    return(err);
}

bool LTE_fdd_dl_fg_samp_buf::set_n_id_cell(char *char_value)
{
    uint32 value;
    bool   err = true;

    if(false == char_to_uint32(char_value, &value) &&
       value <= 503)
    {
        N_id_cell = value;
        N_id_2    = (N_id_cell % 3);
        N_id_1    = (N_id_cell - N_id_2)/3;
        err       = false;
    }

    return(err);
}

bool LTE_fdd_dl_fg_samp_buf::set_mcc(char *char_value)
{
    uint32 i;
    uint32 length = strlen(char_value);
    bool   err    = true;

    if(3 >= length)
    {
        sib1.plmn_id[0].id.mcc = 0xF000;
        for(i=0; i<length; i++)
        {
            sib1.plmn_id[0].id.mcc |= ((char_value[i] & 0x0F) << ((length-i-1)*4));
        }
        err = false;
    }

    return(err);
}

bool LTE_fdd_dl_fg_samp_buf::set_mnc(char *char_value)
{
    uint32 i;
    uint32 length = strlen(char_value);
    bool   err    = true;

    if(3 >= length)
    {
        sib1.plmn_id[0].id.mnc = 0x0000;
        for(i=0; i<length; i++)
        {
            sib1.plmn_id[0].id.mnc |= ((char_value[i] & 0x0F) << ((length-i-1)*4));
        }
        if(2 >= length)
        {
            sib1.plmn_id[0].id.mnc |= 0xFF00;
        }else{
            sib1.plmn_id[0].id.mnc |= 0xF000;
        }
        err = false;
    }

    return(err);
}

bool LTE_fdd_dl_fg_samp_buf::set_q_hyst(char *char_value)
{
    uint32 i;
    bool   err = false;

    for(i=0; i<LIBLTE_RRC_Q_HYST_N_ITEMS; i++)
    {
        if(!strcasecmp(char_value, liblte_rrc_q_hyst_text[i]))
        {
            sib3.q_hyst = (LIBLTE_RRC_Q_HYST_ENUM)i;
            break;
        }
    }
    if(LIBLTE_RRC_Q_HYST_N_ITEMS == i)
    {
        err = true;
    }

    return(err);
}

bool LTE_fdd_dl_fg_samp_buf::set_neigh_cell_list(char *char_value)
{
    uint32  i;
    char   *token1;
    char   *token2;
    bool    err = false;

    token1 = strtok(char_value, ":");
    if(NULL  != token1 &&
       false == set_param(&sib4.intra_freq_neigh_cell_list_size, token1, 0, LIBLTE_RRC_MAX_CELL_INTRA))
    {
        for(i=0; i<sib4.intra_freq_neigh_cell_list_size; i++)
        {
            token1 = strtok(NULL, ",");
            token2 = strtok(NULL, ";");
            if(!(NULL  != token1                                                                      &&
                 NULL  != token2                                                                      &&
                 false == set_param(&sib4.intra_freq_neigh_cell_list[i].phys_cell_id, token1, 0, 503) &&
                 false == set_q_offset_range(&sib4.intra_freq_neigh_cell_list[i].q_offset_range, token2)))
            {
                err = true;
                break;
            }
        }
    }else{
        err = true;
    }

    if(true == err)
    {
        sib4.intra_freq_neigh_cell_list_size = 0;
    }

    return(err);
}

bool LTE_fdd_dl_fg_samp_buf::set_q_offset_range(LIBLTE_RRC_Q_OFFSET_RANGE_ENUM *q_offset_range,
                                                char                           *char_value)
{
    uint32 i;
    bool   err = false;

    for(i=0; i<LIBLTE_RRC_Q_OFFSET_RANGE_N_ITEMS; i++)
    {
        if(!strcasecmp(char_value, liblte_rrc_q_offset_range_text[i]))
        {
            *q_offset_range = (LIBLTE_RRC_Q_OFFSET_RANGE_ENUM)i;
            break;
        }
    }
    if(LIBLTE_RRC_Q_OFFSET_RANGE_N_ITEMS == i)
    {
        err = true;
    }

    return(err);
}

bool LTE_fdd_dl_fg_samp_buf::set_param(uint32 *param,
                                       char   *char_value,
                                       uint32  llimit,
                                       uint32  ulimit)
{
    uint32 value;
    bool   err = true;

    if(param != NULL                               &&
       false == char_to_uint32(char_value, &value) &&
       value >= llimit                             &&
       value <= ulimit)
    {
        *param = value;
        err    = false;
    }

    return(err);
}

bool LTE_fdd_dl_fg_samp_buf::set_param(uint16 *param,
                                       char   *char_value,
                                       uint16  llimit,
                                       uint16  ulimit)
{
    uint32 value;
    bool   err = true;

    if(param != NULL                               &&
       false == char_to_uint32(char_value, &value) &&
       value >= llimit                             &&
       value <= ulimit)
    {
        *param = (uint16)value;
        err    = false;
    }

    return(err);
}

bool LTE_fdd_dl_fg_samp_buf::set_param(uint8 *param,
                                       char  *char_value,
                                       uint8  llimit,
                                       uint8  ulimit)
{
    uint32 value;
    bool   err = true;

    if(param != NULL                               &&
       false == char_to_uint32(char_value, &value) &&
       value >= llimit                             &&
       value <= ulimit)
    {
        *param = (uint8)value;
        err    = false;
    }

    return(err);
}

bool LTE_fdd_dl_fg_samp_buf::set_param(int32 *param,
                                       char  *char_value,
                                       int32  llimit,
                                       int32  ulimit)
{
    int32 value;
    bool  err = true;

    if(param != NULL                              &&
       false == char_to_int32(char_value, &value) &&
       value >= llimit                            &&
       value <= ulimit)
    {
        *param = value;
        err    = false;
    }

    return(err);
}

bool LTE_fdd_dl_fg_samp_buf::set_param(int16 *param,
                                       char  *char_value,
                                       int16  llimit,
                                       int16  ulimit)
{
    int32 value;
    bool  err = true;

    if(param != NULL                              &&
       false == char_to_int32(char_value, &value) &&
       value >= llimit                            &&
       value <= ulimit)
    {
        *param = (int16)value;
        err    = false;
    }

    return(err);
}

bool LTE_fdd_dl_fg_samp_buf::set_param(int8 *param,
                                       char *char_value,
                                       int8  llimit,
                                       int8  ulimit)
{
    int32 value;
    bool  err = true;

    if(param != NULL                              &&
       false == char_to_int32(char_value, &value) &&
       value >= llimit                            &&
       value <= ulimit)
    {
        *param = (int8)value;
        err    = false;
    }

    return(err);
}

bool LTE_fdd_dl_fg_samp_buf::char_to_uint32(char   *char_value,
                                            uint32 *uint32_value)
{
    uint32  tmp_value;
    char   *endptr;
    bool    err = true;

    errno     = 0;
    tmp_value = strtoul(char_value, &endptr, 10);

    if(errno          == 0 &&
       strlen(endptr) == 0)
    {
        *uint32_value = tmp_value;
        err           = false;
    }

    return(err);
}

bool LTE_fdd_dl_fg_samp_buf::char_to_int32(char  *char_value,
                                           int32 *int32_value)
{
    int32  tmp_value;
    char  *endptr;
    bool   err = true;

    errno     = 0;
    tmp_value = strtol(char_value, &endptr, 10);

    if(errno          == 0 &&
       strlen(endptr) == 0)
    {
        *int32_value = tmp_value;
        err          = false;
    }

    return(err);
}
