/*******************************************************************************

    Copyright 2013-2015 Ben Wojtowicz

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

    File: LTE_fdd_dl_scan_state_machine.cc

    Description: Contains all the implementations for the LTE FDD DL Scanner
                 state machine block.

    Revision History
    ----------    -------------    --------------------------------------------
    02/26/2013    Ben Wojtowicz    Created file
    07/21/2013    Ben Wojtowicz    Added support for multiple sample rates and
                                   for decoding SIBs.
    08/26/2013    Ben Wojtowicz    Updates to support GnuRadio 3.7 and the
                                   latest LTE library.
    03/26/2014    Ben Wojtowicz    Using the latest LTE library.
    06/15/2014    Ben Wojtowicz    Added PCAP support.
    03/11/2015    Ben Wojtowicz    Added 7.68MHz support.

*******************************************************************************/

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_fdd_dl_scan_state_machine.h"
#include "LTE_fdd_dl_scan_flowgraph.h"
#include "liblte_mac.h"
#include <gnuradio/io_signature.h>

/*******************************************************************************
                              DEFINES
*******************************************************************************/

// Generic defines
#define COARSE_TIMING_N_SLOTS (160)
#define MAX_ATTEMPTS          (5)
#define SAMP_BUF_SIZE         (307200*20)
#define MAX_PDSCH_ATTEMPTS    (20*10)

// Sample rate 1.92MHZ defines
#define ONE_SUBFRAME_NUM_SAMPS_1_92MHZ               (LIBLTE_PHY_N_SAMPS_PER_SUBFR_1_92MHZ)
#define ONE_FRAME_NUM_SAMPS_1_92MHZ                  (10 * ONE_SUBFRAME_NUM_SAMPS_1_92MHZ)
#define FREQ_CHANGE_WAIT_NUM_SAMPS_1_92MHZ           (100 * ONE_FRAME_NUM_SAMPS_1_92MHZ)
#define COARSE_TIMING_SEARCH_NUM_SAMPS_1_92MHZ       (((COARSE_TIMING_N_SLOTS/2)+2) * ONE_SUBFRAME_NUM_SAMPS_1_92MHZ)
#define PSS_AND_FINE_TIMING_SEARCH_NUM_SAMPS_1_92MHZ (COARSE_TIMING_SEARCH_NUM_SAMPS_1_92MHZ)
#define SSS_SEARCH_NUM_SAMPS_1_92MHZ                 (COARSE_TIMING_SEARCH_NUM_SAMPS_1_92MHZ)
#define BCH_DECODE_NUM_SAMPS_1_92MHZ                 (2 * ONE_FRAME_NUM_SAMPS_1_92MHZ)
#define PDSCH_DECODE_SIB1_NUM_SAMPS_1_92MHZ          (2 * ONE_FRAME_NUM_SAMPS_1_92MHZ)
#define PDSCH_DECODE_SI_GENERIC_NUM_SAMPS_1_92MHZ    (ONE_FRAME_NUM_SAMPS_1_92MHZ)

// Sample rate 7.68MHZ defines
#define ONE_SUBFRAME_NUM_SAMPS_7_68MHZ               (LIBLTE_PHY_N_SAMPS_PER_SUBFR_7_68MHZ)
#define ONE_FRAME_NUM_SAMPS_7_68MHZ                  (10 * ONE_SUBFRAME_NUM_SAMPS_7_68MHZ)
#define FREQ_CHANGE_WAIT_NUM_SAMPS_7_68MHZ           (100 * ONE_FRAME_NUM_SAMPS_7_68MHZ)
#define COARSE_TIMING_SEARCH_NUM_SAMPS_7_68MHZ       (((COARSE_TIMING_N_SLOTS/2)+2) * ONE_SUBFRAME_NUM_SAMPS_7_68MHZ)
#define PSS_AND_FINE_TIMING_SEARCH_NUM_SAMPS_7_68MHZ (COARSE_TIMING_SEARCH_NUM_SAMPS_7_68MHZ)
#define SSS_SEARCH_NUM_SAMPS_7_68MHZ                 (COARSE_TIMING_SEARCH_NUM_SAMPS_7_68MHZ)
#define BCH_DECODE_NUM_SAMPS_7_68MHZ                 (2 * ONE_FRAME_NUM_SAMPS_7_68MHZ)
#define PDSCH_DECODE_SIB1_NUM_SAMPS_7_68MHZ          (2 * ONE_FRAME_NUM_SAMPS_7_68MHZ)
#define PDSCH_DECODE_SI_GENERIC_NUM_SAMPS_7_68MHZ    (ONE_FRAME_NUM_SAMPS_7_68MHZ)

// Sample rate 15.36MHZ defines
#define ONE_SUBFRAME_NUM_SAMPS_15_36MHZ               (LIBLTE_PHY_N_SAMPS_PER_SUBFR_15_36MHZ)
#define ONE_FRAME_NUM_SAMPS_15_36MHZ                  (10 * ONE_SUBFRAME_NUM_SAMPS_15_36MHZ)
#define FREQ_CHANGE_WAIT_NUM_SAMPS_15_36MHZ           (100 * ONE_FRAME_NUM_SAMPS_15_36MHZ)
#define COARSE_TIMING_SEARCH_NUM_SAMPS_15_36MHZ       (((COARSE_TIMING_N_SLOTS/2)+2) * ONE_SUBFRAME_NUM_SAMPS_15_36MHZ)
#define PSS_AND_FINE_TIMING_SEARCH_NUM_SAMPS_15_36MHZ (COARSE_TIMING_SEARCH_NUM_SAMPS_15_36MHZ)
#define SSS_SEARCH_NUM_SAMPS_15_36MHZ                 (COARSE_TIMING_SEARCH_NUM_SAMPS_15_36MHZ)
#define BCH_DECODE_NUM_SAMPS_15_36MHZ                 (2 * ONE_FRAME_NUM_SAMPS_15_36MHZ)
#define PDSCH_DECODE_SIB1_NUM_SAMPS_15_36MHZ          (2 * ONE_FRAME_NUM_SAMPS_15_36MHZ)
#define PDSCH_DECODE_SI_GENERIC_NUM_SAMPS_15_36MHZ    (ONE_FRAME_NUM_SAMPS_15_36MHZ)

/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/


/*******************************************************************************
                              GLOBAL VARIABLES
*******************************************************************************/

// minimum and maximum number of input and output streams
static const int32 MIN_IN  = 1;
static const int32 MAX_IN  = 1;
static const int32 MIN_OUT = 0;
static const int32 MAX_OUT = 0;

/*******************************************************************************
                              CLASS IMPLEMENTATIONS
*******************************************************************************/

LTE_fdd_dl_scan_state_machine_sptr LTE_fdd_dl_scan_make_state_machine(uint32 samp_rate)
{
    return LTE_fdd_dl_scan_state_machine_sptr(new LTE_fdd_dl_scan_state_machine(samp_rate));
}

LTE_fdd_dl_scan_state_machine::LTE_fdd_dl_scan_state_machine(uint32 samp_rate)
    : gr::sync_block ("LTE_fdd_dl_scan_state_machine",
                      gr::io_signature::make(MIN_IN,  MAX_IN,  sizeof(gr_complex)),
                      gr::io_signature::make(MIN_OUT, MAX_OUT, sizeof(gr_complex)))
{
    uint32 i;

    // Initialize the LTE library
    if(samp_rate == 1920000)
    {
        liblte_phy_init(&phy_struct,
                        LIBLTE_PHY_FS_1_92MHZ,
                        LIBLTE_PHY_INIT_N_ID_CELL_UNKNOWN,
                        4,
                        LIBLTE_PHY_N_RB_DL_1_4MHZ,
                        LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP,
                        liblte_rrc_phich_resource_num[LIBLTE_RRC_PHICH_RESOURCE_1]);
        one_subframe_num_samps               = ONE_SUBFRAME_NUM_SAMPS_1_92MHZ;
        one_frame_num_samps                  = ONE_FRAME_NUM_SAMPS_1_92MHZ;
        freq_change_wait_num_samps           = FREQ_CHANGE_WAIT_NUM_SAMPS_1_92MHZ;
        coarse_timing_search_num_samps       = COARSE_TIMING_SEARCH_NUM_SAMPS_1_92MHZ;
        pss_and_fine_timing_search_num_samps = PSS_AND_FINE_TIMING_SEARCH_NUM_SAMPS_1_92MHZ;
        sss_search_num_samps                 = SSS_SEARCH_NUM_SAMPS_1_92MHZ;
        bch_decode_num_samps                 = BCH_DECODE_NUM_SAMPS_1_92MHZ;
        pdsch_decode_sib1_num_samps          = PDSCH_DECODE_SIB1_NUM_SAMPS_1_92MHZ;
        pdsch_decode_si_generic_num_samps    = PDSCH_DECODE_SI_GENERIC_NUM_SAMPS_1_92MHZ;
    }else if(samp_rate == 7680000){
        liblte_phy_init(&phy_struct,
                        LIBLTE_PHY_FS_7_68MHZ,
                        LIBLTE_PHY_INIT_N_ID_CELL_UNKNOWN,
                        4,
                        LIBLTE_PHY_N_RB_DL_5MHZ,
                        LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP,
                        liblte_rrc_phich_resource_num[LIBLTE_RRC_PHICH_RESOURCE_1]);
        one_subframe_num_samps               = ONE_SUBFRAME_NUM_SAMPS_7_68MHZ;
        one_frame_num_samps                  = ONE_FRAME_NUM_SAMPS_7_68MHZ;
        freq_change_wait_num_samps           = FREQ_CHANGE_WAIT_NUM_SAMPS_7_68MHZ;
        coarse_timing_search_num_samps       = COARSE_TIMING_SEARCH_NUM_SAMPS_7_68MHZ;
        pss_and_fine_timing_search_num_samps = PSS_AND_FINE_TIMING_SEARCH_NUM_SAMPS_7_68MHZ;
        sss_search_num_samps                 = SSS_SEARCH_NUM_SAMPS_7_68MHZ;
        bch_decode_num_samps                 = BCH_DECODE_NUM_SAMPS_7_68MHZ;
        pdsch_decode_sib1_num_samps          = PDSCH_DECODE_SIB1_NUM_SAMPS_7_68MHZ;
        pdsch_decode_si_generic_num_samps    = PDSCH_DECODE_SI_GENERIC_NUM_SAMPS_7_68MHZ;
    }else{
        liblte_phy_init(&phy_struct,
                        LIBLTE_PHY_FS_15_36MHZ,
                        LIBLTE_PHY_INIT_N_ID_CELL_UNKNOWN,
                        4,
                        LIBLTE_PHY_N_RB_DL_10MHZ,
                        LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP,
                        liblte_rrc_phich_resource_num[LIBLTE_RRC_PHICH_RESOURCE_1]);
        one_subframe_num_samps               = ONE_SUBFRAME_NUM_SAMPS_15_36MHZ;
        one_frame_num_samps                  = ONE_FRAME_NUM_SAMPS_15_36MHZ;
        freq_change_wait_num_samps           = FREQ_CHANGE_WAIT_NUM_SAMPS_15_36MHZ;
        coarse_timing_search_num_samps       = COARSE_TIMING_SEARCH_NUM_SAMPS_15_36MHZ;
        pss_and_fine_timing_search_num_samps = PSS_AND_FINE_TIMING_SEARCH_NUM_SAMPS_15_36MHZ;
        sss_search_num_samps                 = SSS_SEARCH_NUM_SAMPS_15_36MHZ;
        bch_decode_num_samps                 = BCH_DECODE_NUM_SAMPS_15_36MHZ;
        pdsch_decode_sib1_num_samps          = PDSCH_DECODE_SIB1_NUM_SAMPS_15_36MHZ;
        pdsch_decode_si_generic_num_samps    = PDSCH_DECODE_SI_GENERIC_NUM_SAMPS_15_36MHZ;
    }

    // Initialize the sample buffer
    i_buf          = (float *)malloc(SAMP_BUF_SIZE*sizeof(float));
    q_buf          = (float *)malloc(SAMP_BUF_SIZE*sizeof(float));
    samp_buf_w_idx = 0;
    samp_buf_r_idx = 0;

    // Variables
    init();
    send_cnf        = true;
    N_decoded_chans = 0;
    corr_peak_idx   = 0;
    for(i=0; i<LIBLTE_PHY_N_MAX_ROUGH_CORR_SEARCH_PEAKS; i++)
    {
        timing_struct.freq_offset[i] = 0;
    }
}
LTE_fdd_dl_scan_state_machine::~LTE_fdd_dl_scan_state_machine()
{
    // Cleanup the LTE library
    liblte_phy_cleanup(phy_struct);

    // Free the sample buffer
    free(i_buf);
    free(q_buf);
}

int32 LTE_fdd_dl_scan_state_machine::work(int32                      ninput_items,
                                          gr_vector_const_void_star &input_items,
                                          gr_vector_void_star       &output_items)
{
    LTE_fdd_dl_scan_interface  *interface = LTE_fdd_dl_scan_interface::get_instance();
    LTE_fdd_dl_scan_flowgraph  *flowgraph = LTE_fdd_dl_scan_flowgraph::get_instance();
    LIBLTE_PHY_SUBFRAME_STRUCT  subframe;
    LIBLTE_PHY_PCFICH_STRUCT    pcfich;
    LIBLTE_PHY_PHICH_STRUCT     phich;
    LIBLTE_PHY_PDCCH_STRUCT     pdcch;
    const gr_complex           *in  = (const gr_complex *)input_items[0];
    float                       pss_thresh;
    float                       freq_offset;
    int32                       done_flag = 0;
    uint32                      i;
    uint32                      pss_symb;
    uint32                      frame_start_idx;
    uint32                      samps_to_copy;
    uint32                      N_rb_dl;
    uint8                       sfn_offset;
    bool                        process_samples = false;
    bool                        copy_input      = false;
    bool                        switch_freq     = false;

    if(freq_change_wait_done)
    {
        if(samp_buf_w_idx < (SAMP_BUF_SIZE-(ninput_items+1)))
        {
            copy_input_to_samp_buf(in, ninput_items);

            // Check if buffer is full enough
            if(samp_buf_w_idx >= (SAMP_BUF_SIZE-(ninput_items+1)))
            {
                process_samples = true;
                copy_input      = false;
            }
        }else{
            // Buffer is too full process samples, then copy
            process_samples = true;
            copy_input      = true;
        }
    }else{
        freq_change_wait_cnt += ninput_items;
        if(freq_change_wait_cnt >= freq_change_wait_num_samps)
        {
            freq_change_wait_done = true;
        }
    }

    if(process_samples)
    {
        if(LTE_FDD_DL_SCAN_STATE_MACHINE_STATE_COARSE_TIMING_SEARCH != state)
        {
            // Correct frequency error
            freq_shift(0, SAMP_BUF_SIZE, timing_struct.freq_offset[corr_peak_idx]);
        }

        while(samp_buf_r_idx < (samp_buf_w_idx - N_samps_needed) &&
              samp_buf_w_idx != 0)
        {
            switch(state)
            {
            case LTE_FDD_DL_SCAN_STATE_MACHINE_STATE_COARSE_TIMING_SEARCH:
                if(LIBLTE_SUCCESS == liblte_phy_dl_find_coarse_timing_and_freq_offset(phy_struct,
                                                                                      i_buf,
                                                                                      q_buf,
                                                                                      COARSE_TIMING_N_SLOTS,
                                                                                      &timing_struct))
                {
                    if(corr_peak_idx < timing_struct.n_corr_peaks)
                    {
                        // Correct frequency error
                        freq_shift(0, SAMP_BUF_SIZE, timing_struct.freq_offset[corr_peak_idx]);

                        // Search for PSS and fine timing
                        state          = LTE_FDD_DL_SCAN_STATE_MACHINE_STATE_PSS_AND_FINE_TIMING_SEARCH;
                        N_samps_needed = pss_and_fine_timing_search_num_samps;
                    }else{
                        channel_not_found(switch_freq, done_flag);
                    }
                }else{
                    // Stay in coarse timing search
                    samp_buf_r_idx = 0;
                    samp_buf_w_idx = 0;
                    N_samps_needed = coarse_timing_search_num_samps;
                    N_attempts++;
                    if(N_attempts > MAX_ATTEMPTS)
                    {
                        channel_not_found(switch_freq, done_flag);
                    }
                }
                break;
            case LTE_FDD_DL_SCAN_STATE_MACHINE_STATE_PSS_AND_FINE_TIMING_SEARCH:
                if(LIBLTE_SUCCESS == liblte_phy_find_pss_and_fine_timing(phy_struct,
                                                                         i_buf,
                                                                         q_buf,
                                                                         timing_struct.symb_starts[corr_peak_idx],
                                                                         &N_id_2,
                                                                         &pss_symb,
                                                                         &pss_thresh,
                                                                         &freq_offset))
                {
                    if(fabs(freq_offset) > 100)
                    {
                        freq_shift(0, SAMP_BUF_SIZE, freq_offset);
                        timing_struct.freq_offset[corr_peak_idx] += freq_offset;
                    }

                    // Search for SSS
                    state          = LTE_FDD_DL_SCAN_STATE_MACHINE_STATE_SSS_SEARCH;
                    N_samps_needed = sss_search_num_samps;
                }else{
                    // Go back to coarse timing search
                    state          = LTE_FDD_DL_SCAN_STATE_MACHINE_STATE_COARSE_TIMING_SEARCH;
                    samp_buf_r_idx = 0;
                    samp_buf_w_idx = 0;
                    N_samps_needed = coarse_timing_search_num_samps;
                    N_attempts++;
                    if(N_attempts > MAX_ATTEMPTS)
                    {
                        channel_not_found(switch_freq, done_flag);
                    }
                }
                break;
            case LTE_FDD_DL_SCAN_STATE_MACHINE_STATE_SSS_SEARCH:
                if(LIBLTE_SUCCESS == liblte_phy_find_sss(phy_struct,
                                                         i_buf,
                                                         q_buf,
                                                         N_id_2,
                                                         timing_struct.symb_starts[corr_peak_idx],
                                                         pss_thresh,
                                                         &N_id_1,
                                                         &frame_start_idx))
                {
                    chan_data.N_id_cell = 3*N_id_1 + N_id_2;

                    for(i=0; i<N_decoded_chans; i++)
                    {
                        if(chan_data.N_id_cell == decoded_chans[i])
                        {
                            break;
                        }
                    }
                    if(i != N_decoded_chans)
                    {
                        // Go back to coarse timing search
                        state = LTE_FDD_DL_SCAN_STATE_MACHINE_STATE_COARSE_TIMING_SEARCH;
                        corr_peak_idx++;
                        init();
                    }else{
                        // Decode BCH
                        state = LTE_FDD_DL_SCAN_STATE_MACHINE_STATE_BCH_DECODE;
                        while(frame_start_idx < samp_buf_r_idx)
                        {
                            frame_start_idx += one_frame_num_samps;
                        }
                        samp_buf_r_idx = frame_start_idx;
                        N_samps_needed = bch_decode_num_samps;
                    }
                }else{
                    // Go back to coarse timing search
                    state          = LTE_FDD_DL_SCAN_STATE_MACHINE_STATE_COARSE_TIMING_SEARCH;
                    samp_buf_r_idx = 0;
                    samp_buf_w_idx = 0;
                    N_samps_needed = coarse_timing_search_num_samps;
                    N_attempts++;
                    if(N_attempts > MAX_ATTEMPTS)
                    {
                        channel_not_found(switch_freq, done_flag);
                    }
                }
                break;
            case LTE_FDD_DL_SCAN_STATE_MACHINE_STATE_BCH_DECODE:
                if(LIBLTE_SUCCESS == liblte_phy_get_dl_subframe_and_ce(phy_struct,
                                                                       i_buf,
                                                                       q_buf,
                                                                       samp_buf_r_idx,
                                                                       0,
                                                                       chan_data.N_id_cell,
                                                                       4,
                                                                       &subframe) &&
                   LIBLTE_SUCCESS == liblte_phy_bch_channel_decode(phy_struct,
                                                                   &subframe,
                                                                   chan_data.N_id_cell,
                                                                   &N_ant,
                                                                   rrc_msg.msg,
                                                                   &rrc_msg.N_bits,
                                                                   &sfn_offset) &&
                   LIBLTE_SUCCESS == liblte_rrc_unpack_bcch_bch_msg(&rrc_msg,
                                                                    &mib))
                {
                    switch(mib.dl_bw)
                    {
                    case LIBLTE_RRC_DL_BANDWIDTH_6:
                        N_rb_dl = LIBLTE_PHY_N_RB_DL_1_4MHZ;
                        break;
                    case LIBLTE_RRC_DL_BANDWIDTH_15:
                        N_rb_dl = LIBLTE_PHY_N_RB_DL_3MHZ;
                        break;
                    case LIBLTE_RRC_DL_BANDWIDTH_25:
                        N_rb_dl = LIBLTE_PHY_N_RB_DL_5MHZ;
                        break;
                    case LIBLTE_RRC_DL_BANDWIDTH_50:
                        N_rb_dl = LIBLTE_PHY_N_RB_DL_10MHZ;
                        break;
                    case LIBLTE_RRC_DL_BANDWIDTH_75:
                        N_rb_dl = LIBLTE_PHY_N_RB_DL_15MHZ;
                        break;
                    case LIBLTE_RRC_DL_BANDWIDTH_100:
                        N_rb_dl = LIBLTE_PHY_N_RB_DL_20MHZ;
                        break;
                    }
                    sfn       = (mib.sfn_div_4 << 2) + sfn_offset;
                    phich_res = liblte_rrc_phich_resource_num[mib.phich_config.res];

                    // Send a PCAP message
                    interface->send_pcap_msg(0xFFFFFFFF, sfn*10 + subframe.num, &rrc_msg);

                    // Send channel found start and mib decoded messages
                    chan_data.freq_offset = timing_struct.freq_offset[corr_peak_idx];
                    interface->send_ctrl_channel_found_begin_msg(&chan_data, &mib, sfn, N_ant);

                    if(LIBLTE_SUCCESS == liblte_phy_update_n_rb_dl(phy_struct, N_rb_dl))
                    {
                        // Add this channel to the list of decoded channels
                        decoded_chans[N_decoded_chans++] = chan_data.N_id_cell;
                        if(LTE_FDD_DL_SCAN_STATE_MACHINE_N_DECODED_CHANS_MAX == N_decoded_chans)
                        {
                            channel_not_found(switch_freq, done_flag);
                        }

                        // Decode PDSCH for SIB1
                        state = LTE_FDD_DL_SCAN_STATE_MACHINE_STATE_PDSCH_DECODE_SIB1;
                        if((sfn % 2) != 0)
                        {
                            samp_buf_r_idx += one_frame_num_samps;
                            sfn++;
                        }
                        N_samps_needed = pdsch_decode_sib1_num_samps;
                    }else{
                        chan_data.freq_offset = timing_struct.freq_offset[corr_peak_idx];
                        channel_found(switch_freq, done_flag);
                    }
                }else{
                    // Try next MIB
                    state            = LTE_FDD_DL_SCAN_STATE_MACHINE_STATE_BCH_DECODE;
                    frame_start_idx += one_frame_num_samps;
                    samp_buf_r_idx   = frame_start_idx;
                    N_samps_needed   = bch_decode_num_samps;
                    N_bch_attempts++;
                    if(N_bch_attempts > MAX_ATTEMPTS)
                    {
                        // Go back to coarse timing
                        state          = LTE_FDD_DL_SCAN_STATE_MACHINE_STATE_COARSE_TIMING_SEARCH;
                        samp_buf_r_idx = 0;
                        samp_buf_w_idx = 0;
                        N_samps_needed = coarse_timing_search_num_samps;
                        N_attempts++;
                        if(N_attempts > MAX_ATTEMPTS)
                        {
                            channel_not_found(switch_freq, done_flag);
                        }
                    }
                }
                break;
            case LTE_FDD_DL_SCAN_STATE_MACHINE_STATE_PDSCH_DECODE_SIB1:
                if(LIBLTE_SUCCESS == liblte_phy_get_dl_subframe_and_ce(phy_struct,
                                                                       i_buf,
                                                                       q_buf,
                                                                       samp_buf_r_idx,
                                                                       5,
                                                                       chan_data.N_id_cell,
                                                                       N_ant,
                                                                       &subframe) &&
                   LIBLTE_SUCCESS == liblte_phy_pdcch_channel_decode(phy_struct,
                                                                     &subframe,
                                                                     chan_data.N_id_cell,
                                                                     N_ant,
                                                                     phich_res,
                                                                     mib.phich_config.dur,
                                                                     &pcfich,
                                                                     &phich,
                                                                     &pdcch) &&
                   LIBLTE_SUCCESS == liblte_phy_pdsch_channel_decode(phy_struct,
                                                                     &subframe,
                                                                     &pdcch.alloc[0],
                                                                     pdcch.N_symbs,
                                                                     chan_data.N_id_cell,
                                                                     N_ant,
                                                                     rrc_msg.msg,
                                                                     &rrc_msg.N_bits) &&
                   LIBLTE_SUCCESS == liblte_rrc_unpack_bcch_dlsch_msg(&rrc_msg,
                                                                      &bcch_dlsch_msg))
                {
                    // Send a PCAP message
                    interface->send_pcap_msg(pdcch.alloc[0].rnti, sfn*10 + subframe.num, &rrc_msg);

                    if(1                                == bcch_dlsch_msg.N_sibs &&
                       LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_1 == bcch_dlsch_msg.sibs[0].sib_type)
                    {
                        if(!sib1_sent)
                        {
                            interface->send_ctrl_sib1_decoded_msg(&chan_data, (LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_1_STRUCT *)&bcch_dlsch_msg.sibs[0].sib, sfn);
                            sib1_sent = true;
                        }
                    }

                    // Decode all PDSCHs
                    state          = LTE_FDD_DL_SCAN_STATE_MACHINE_STATE_PDSCH_DECODE_SI_GENERIC;
                    N_sfr          = 0;
                    N_samps_needed = pdsch_decode_si_generic_num_samps;
                }else{
                    // Try to decode SIB1 again
                    samp_buf_r_idx += pdsch_decode_sib1_num_samps;
                    sfn            += 2;
                    N_samps_needed  = pdsch_decode_sib1_num_samps;
                }

                // Determine if PDSCH search is done
                N_pdsch_attempts++;
                if(N_pdsch_attempts >= MAX_PDSCH_ATTEMPTS)
                {
                    channel_found(switch_freq, done_flag);
                }
                break;
            case LTE_FDD_DL_SCAN_STATE_MACHINE_STATE_PDSCH_DECODE_SI_GENERIC:
                if(LIBLTE_SUCCESS == liblte_phy_get_dl_subframe_and_ce(phy_struct,
                                                                       i_buf,
                                                                       q_buf,
                                                                       samp_buf_r_idx,
                                                                       N_sfr,
                                                                       chan_data.N_id_cell,
                                                                       N_ant,
                                                                       &subframe) &&
                   LIBLTE_SUCCESS == liblte_phy_pdcch_channel_decode(phy_struct,
                                                                     &subframe,
                                                                     chan_data.N_id_cell,
                                                                     N_ant,
                                                                     phich_res,
                                                                     mib.phich_config.dur,
                                                                     &pcfich,
                                                                     &phich,
                                                                     &pdcch) &&
                   LIBLTE_SUCCESS == liblte_phy_pdsch_channel_decode(phy_struct,
                                                                     &subframe,
                                                                     &pdcch.alloc[0],
                                                                     pdcch.N_symbs,
                                                                     chan_data.N_id_cell,
                                                                     N_ant,
                                                                     rrc_msg.msg,
                                                                     &rrc_msg.N_bits))
                {
                    // Send a PCAP message
                    interface->send_pcap_msg(pdcch.alloc[0].rnti, sfn*10 + subframe.num, &rrc_msg);

                    if(LIBLTE_MAC_SI_RNTI == pdcch.alloc[0].rnti &&
                       LIBLTE_SUCCESS     == liblte_rrc_unpack_bcch_dlsch_msg(&rrc_msg,
                                                                              &bcch_dlsch_msg))
                    {
                        for(i=0; i<bcch_dlsch_msg.N_sibs; i++)
                        {
                            switch(bcch_dlsch_msg.sibs[i].sib_type)
                            {
                            case LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_1:
                                if(!sib1_sent)
                                {
                                    interface->send_ctrl_sib1_decoded_msg(&chan_data, (LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_1_STRUCT *)&bcch_dlsch_msg.sibs[i].sib, sfn);
                                    sib1_sent = true;
                                }
                                break;
                            case LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_2:
                                if(!sib2_sent)
                                {
                                    interface->send_ctrl_sib2_decoded_msg(&chan_data, (LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_2_STRUCT *)&bcch_dlsch_msg.sibs[i].sib, sfn);
                                    sib2_sent = true;
                                }
                                break;
                            case LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_3:
                                if(!sib3_sent)
                                {
                                    interface->send_ctrl_sib3_decoded_msg(&chan_data, (LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_3_STRUCT *)&bcch_dlsch_msg.sibs[i].sib, sfn);
                                    sib3_sent = true;
                                }
                                break;
                            case LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_4:
                                if(!sib4_sent)
                                {
                                    interface->send_ctrl_sib4_decoded_msg(&chan_data, (LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_4_STRUCT *)&bcch_dlsch_msg.sibs[i].sib, sfn);
                                    sib4_sent = true;
                                }
                                break;
                            case LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_5:
                                if(!sib5_sent)
                                {
                                    interface->send_ctrl_sib5_decoded_msg(&chan_data, (LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_5_STRUCT *)&bcch_dlsch_msg.sibs[i].sib, sfn);
                                    sib5_sent = true;
                                }
                                break;
                            case LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_6:
                                if(!sib6_sent)
                                {
                                    interface->send_ctrl_sib6_decoded_msg(&chan_data, (LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_6_STRUCT *)&bcch_dlsch_msg.sibs[i].sib, sfn);
                                    sib6_sent = true;
                                }
                                break;
                            case LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_7:
                                if(!sib7_sent)
                                {
                                    interface->send_ctrl_sib7_decoded_msg(&chan_data, (LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_7_STRUCT *)&bcch_dlsch_msg.sibs[i].sib, sfn);
                                    sib7_sent = true;
                                }
                                break;
                            case LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_8:
                                if(!sib8_sent)
                                {
                                    interface->send_ctrl_sib8_decoded_msg(&chan_data, (LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_8_STRUCT *)&bcch_dlsch_msg.sibs[i].sib, sfn);
                                    sib8_sent = true;
                                }
                                break;
                            default:
                                printf("Not handling SIB %u\n", bcch_dlsch_msg.sibs[i].sib_type);
                                break;
                            }
                        }
                    }
                }

                // Determine if PDSCH search is done
                N_pdsch_attempts++;
                if(N_pdsch_attempts >= MAX_PDSCH_ATTEMPTS)
                {
                    channel_found(switch_freq, done_flag);
                }else{
                    state          = LTE_FDD_DL_SCAN_STATE_MACHINE_STATE_PDSCH_DECODE_SI_GENERIC;
                    N_samps_needed = pdsch_decode_si_generic_num_samps;
                    N_sfr++;
                    if(N_sfr >= 10)
                    {
                        N_sfr = 0;
                        sfn++;
                        samp_buf_r_idx += pdsch_decode_si_generic_num_samps;
                    }
                }
                break;
            }

            if(switch_freq)
            {
                samp_buf_w_idx = 0;
                samp_buf_r_idx = 0;
                init();
                N_decoded_chans = 0;
                corr_peak_idx   = 0;
                for(i=0; i<LIBLTE_PHY_N_MAX_ROUGH_CORR_SEARCH_PEAKS; i++)
                {
                    timing_struct.freq_offset[i] = 0;
                }
                break;
            }

            if(-1 == done_flag)
            {
                break;
            }
        }

        // Copy remaining samples to beginning of buffer
        if(samp_buf_r_idx > 100)
        {
            samp_buf_r_idx -= 100;
            samps_to_copy   = samp_buf_w_idx - samp_buf_r_idx;
            samp_buf_w_idx  = 0;
            freq_shift(samp_buf_r_idx, samps_to_copy, -timing_struct.freq_offset[corr_peak_idx]);
            for(i=0; i<samps_to_copy; i++)
            {
                i_buf[samp_buf_w_idx]   = i_buf[samp_buf_r_idx];
                q_buf[samp_buf_w_idx++] = q_buf[samp_buf_r_idx++];
            }
            samp_buf_r_idx = 100;
        }

        if(true == copy_input)
        {
            copy_input_to_samp_buf(in, ninput_items);
        }
    }

    if(!flowgraph->is_started())
    {
        done_flag = -1;
    }

    // Tell runtime system how many input items we consumed
    consume_each(ninput_items);

    // Tell runtime system how many output items we produced.
    return(done_flag);
}

void LTE_fdd_dl_scan_state_machine::init(void)
{
    state                 = LTE_FDD_DL_SCAN_STATE_MACHINE_STATE_COARSE_TIMING_SEARCH;
    phich_res             = 0;
    sfn                   = 0;
    N_sfr                 = 0;
    N_ant                 = 0;
    chan_data.N_id_cell   = 0;
    N_id_1                = 0;
    N_id_2                = 0;
    N_attempts            = 0;
    N_bch_attempts        = 0;
    N_pdsch_attempts      = 0;
    N_samps_needed        = coarse_timing_search_num_samps;
    freq_change_wait_cnt  = 0;
    freq_change_wait_done = false;
    sib1_sent             = false;
    sib2_sent             = false;
    sib3_sent             = false;
    sib4_sent             = false;
    sib5_sent             = false;
    sib6_sent             = false;
    sib7_sent             = false;
    sib8_sent             = false;
}

void LTE_fdd_dl_scan_state_machine::copy_input_to_samp_buf(const gr_complex *in, int32 ninput_items)
{
    uint32 i;

    for(i=0; i<ninput_items; i++)
    {
        i_buf[samp_buf_w_idx]   = in[i].real();
        q_buf[samp_buf_w_idx++] = in[i].imag();
    }
}

void LTE_fdd_dl_scan_state_machine::freq_shift(uint32 start_idx, uint32 num_samps, float freq_offset)
{
    float  f_samp_re;
    float  f_samp_im;
    float  tmp_i;
    float  tmp_q;
    uint32 i;

    for(i=start_idx; i<(start_idx+num_samps); i++)
    {
        f_samp_re = cosf((i+1)*(freq_offset)*2*M_PI/phy_struct->fs);
        f_samp_im = sinf((i+1)*(freq_offset)*2*M_PI/phy_struct->fs);
        tmp_i     = i_buf[i];
        tmp_q     = q_buf[i];
        i_buf[i]  = tmp_i*f_samp_re + tmp_q*f_samp_im;
        q_buf[i]  = tmp_q*f_samp_re - tmp_i*f_samp_im;
    }
}

void LTE_fdd_dl_scan_state_machine::channel_found(bool  &switch_freq,
                                                  int32 &done_flag)
{
    LTE_fdd_dl_scan_interface *interface = LTE_fdd_dl_scan_interface::get_instance();

    // Send the channel information
    interface->send_ctrl_channel_found_end_msg(&chan_data);

    // Initialize for the next channel
    init();

    // Determine if a frequency change is needed
    corr_peak_idx++;
    if(corr_peak_idx >= timing_struct.n_corr_peaks)
    {
        // Change frequency
        corr_peak_idx = 0;
        if(LTE_FDD_DL_SCAN_STATUS_OK == interface->switch_to_next_freq())
        {
            switch_freq = true;
            send_cnf    = true;
        }else{
            done_flag = -1;
        }
    }else{
        send_cnf = false;
    }
}

void LTE_fdd_dl_scan_state_machine::channel_not_found(bool  &switch_freq,
                                                      int32 &done_flag)
{
    LTE_fdd_dl_scan_interface *interface = LTE_fdd_dl_scan_interface::get_instance();

    if(send_cnf)
    {
        // Send the channel information
        interface->send_ctrl_channel_not_found_msg();
    }

    // Initialize for the next channel
    init();

    // Change frequency
    corr_peak_idx = 0;
    if(LTE_FDD_DL_SCAN_STATUS_OK == interface->switch_to_next_freq())
    {
        switch_freq = true;
        send_cnf    = true;
    }else{
        done_flag = -1;
    }
}
