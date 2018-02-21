/*******************************************************************************

    Copyright 2012-2014 Ben Wojtowicz

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

    File: LTE_fdd_dl_fs_samp_buf.cc

    Description: Contains all the implementations for the LTE FDD DL File
                 Scanner sample buffer block.

    Revision History
    ----------    -------------    --------------------------------------------
    03/23/2012    Ben Wojtowicz    Created file.
    04/21/2012    Ben Wojtowicz    Fixed bug in frequency offset removal, added
                                   PDSCH decode, general cleanup
    06/28/2012    Ben Wojtowicz    Fixed a bug that was causing I and Q to swap
    08/19/2012    Ben Wojtowicz    Added states and state memory and added
                                   decoding of all SIBs.
    10/06/2012    Ben Wojtowicz    Updated to use the latest LTE library.
    11/10/2012    Ben Wojtowicz    Using the latest libraries to decode more
                                   than 1 eNB
    12/01/2012    Ben Wojtowicz    Using the latest liblte library
    12/26/2012    Ben Wojtowicz    Added more detail printing of RACH
                                   configuration
    03/03/2013    Ben Wojtowicz    Added support for SIB5, SIB6, SIB7, and test
                                   load decoding, using the latest libraries,
                                   fixed a bug that allowed multiple decodes
                                   of the same channel, and fixed a frequency
                                   offset correction bug.
    03/17/2013    Ben Wojtowicz    Added paging message printing.
    07/21/2013    Ben Wojtowicz    Fixed a bug and using the latest LTE library.
    08/26/2013    Ben Wojtowicz    Updates to support GnuRadio 3.7 and the
                                   latest LTE library.
    09/28/2013    Ben Wojtowicz    Added support for setting the sample rate
                                   and input data type.
    03/26/2014    Ben Wojtowicz    Using the latest LTE library.
    11/01/2014    Ben Wojtowicz    Using the latest LTE library.
    11/09/2014    Ben Wojtowicz    Added SIB13 printing.

*******************************************************************************/

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_fdd_dl_fs_samp_buf.h"
#include "liblte_mac.h"
#include "liblte_mcc_mnc_list.h"
#include <gnuradio/io_signature.h>

/*******************************************************************************
                              DEFINES
*******************************************************************************/

#define COARSE_TIMING_N_SLOTS                    (160)
#define COARSE_TIMING_SEARCH_NUM_SUBFRAMES       ((COARSE_TIMING_N_SLOTS/2)+2)
#define PSS_AND_FINE_TIMING_SEARCH_NUM_SUBFRAMES (COARSE_TIMING_SEARCH_NUM_SUBFRAMES)
#define SSS_SEARCH_NUM_SUBFRAMES                 (COARSE_TIMING_SEARCH_NUM_SUBFRAMES)
#define BCH_DECODE_NUM_FRAMES                    (2)
#define PDSCH_DECODE_SIB1_NUM_FRAMES             (2)
#define PDSCH_DECODE_SI_GENERIC_NUM_FRAMES       (1)

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

LTE_fdd_dl_fs_samp_buf_sptr LTE_fdd_dl_fs_make_samp_buf(size_t in_size_val)
{
    return LTE_fdd_dl_fs_samp_buf_sptr(new LTE_fdd_dl_fs_samp_buf(in_size_val));
}

LTE_fdd_dl_fs_samp_buf::LTE_fdd_dl_fs_samp_buf(size_t in_size_val)
    : gr::sync_block ("samp_buf",
                      gr::io_signature::make(MIN_IN,  MAX_IN,  in_size_val),
                      gr::io_signature::make(MIN_OUT, MAX_OUT, sizeof(int8)))
{
    uint32 i;

    // Parse the inputs
    if(in_size_val == sizeof(gr_complex))
    {
        in_size = LTE_FDD_DL_FS_IN_SIZE_GR_COMPLEX;
    }else if(in_size_val == sizeof(int8)){
        in_size = LTE_FDD_DL_FS_IN_SIZE_INT8;
    }else{
        in_size = LTE_FDD_DL_FS_IN_SIZE_INT8;
    }

    // Initialize the LTE parameters
    fs = LIBLTE_PHY_FS_30_72MHZ;

    // Initialize the configuration
    need_config = true;

    // Initialize the sample buffer
    i_buf           = (float *)malloc(LTE_FDD_DL_FS_SAMP_BUF_SIZE*sizeof(float));
    q_buf           = (float *)malloc(LTE_FDD_DL_FS_SAMP_BUF_SIZE*sizeof(float));
    samp_buf_w_idx  = 0;
    samp_buf_r_idx  = 0;
    last_samp_was_i = false;

    // Variables
    init();
    N_decoded_chans = 0;
    corr_peak_idx   = 0;
    for(i=0; i<LIBLTE_PHY_N_MAX_ROUGH_CORR_SEARCH_PEAKS; i++)
    {
        timing_struct.freq_offset[i] = 0;
    }
}
LTE_fdd_dl_fs_samp_buf::~LTE_fdd_dl_fs_samp_buf()
{
    // Cleanup the LTE library
    liblte_phy_cleanup(phy_struct);

    // Free the sample buffer
    free(i_buf);
    free(q_buf);
}

int32 LTE_fdd_dl_fs_samp_buf::work(int32                      ninput_items,
                                   gr_vector_const_void_star &input_items,
                                   gr_vector_void_star       &output_items)
{
    LIBLTE_PHY_SUBFRAME_STRUCT  subframe;
    LIBLTE_PHY_PCFICH_STRUCT    pcfich;
    LIBLTE_PHY_PHICH_STRUCT     phich;
    LIBLTE_PHY_PDCCH_STRUCT     pdcch;
    float                       pss_thresh;
    float                       freq_offset;
    int32                       done_flag = 0;
    uint32                      i;
    uint32                      pss_symb;
    uint32                      frame_start_idx;
    uint32                      num_samps_needed = 0;
    uint32                      samps_to_copy;
    uint32                      N_rb_dl;
    size_t                      line_size = LINE_MAX;
    ssize_t                     N_line_chars;
    char                       *line;
    uint8                       sfn_offset;
    bool                        process_samples = false;
    bool                        copy_input      = false;

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
                            LIBLTE_PHY_INIT_N_ID_CELL_UNKNOWN,
                            4,
                            LIBLTE_PHY_N_RB_DL_1_4MHZ,
                            LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP,
                            liblte_rrc_phich_resource_num[LIBLTE_RRC_PHICH_RESOURCE_1]);
            num_samps_needed = phy_struct->N_samps_per_subfr * COARSE_TIMING_SEARCH_NUM_SUBFRAMES;
        }
    }
    free(line);

    if(LTE_FDD_DL_FS_IN_SIZE_INT8 == in_size)
    {
        if(samp_buf_w_idx < (LTE_FDD_DL_FS_SAMP_BUF_NUM_FRAMES*phy_struct->N_samps_per_frame - ((ninput_items+1)/2)))
        {
            copy_input_to_samp_buf(input_items, ninput_items);

            // Check if buffer is full enough
            if(samp_buf_w_idx >= (LTE_FDD_DL_FS_SAMP_BUF_NUM_FRAMES*phy_struct->N_samps_per_frame - ((ninput_items+1)/2)))
            {
                process_samples = true;
                copy_input      = false;
            }
        }else{
            // Buffer is too full process samples, then copy
            process_samples = true;
            copy_input      = true;
        }
    }else{ // LTE_FDD_DL_FS_IN_SIZE_GR_COMPLEX == in_size
        if(samp_buf_w_idx < (LTE_FDD_DL_FS_SAMP_BUF_NUM_FRAMES*phy_struct->N_samps_per_frame - (ninput_items+1)))
        {
            copy_input_to_samp_buf(input_items, ninput_items);

            // Check if buffer is full enough
            if(samp_buf_w_idx >= (LTE_FDD_DL_FS_SAMP_BUF_NUM_FRAMES*phy_struct->N_samps_per_frame - (ninput_items+1)))
            {
                process_samples = true;
                copy_input      = false;
            }
        }else{
            // Buffer is too full process samples, then copy
            process_samples = true;
            copy_input      = true;
        }
    }

    if(process_samples)
    {
        if(LTE_FDD_DL_FS_SAMP_BUF_STATE_COARSE_TIMING_SEARCH != state)
        {
            // Correct frequency error
            freq_shift(0, LTE_FDD_DL_FS_SAMP_BUF_SIZE, timing_struct.freq_offset[corr_peak_idx]);
        }

        // Get number of samples needed for each state
        switch(state)
        {
        case LTE_FDD_DL_FS_SAMP_BUF_STATE_COARSE_TIMING_SEARCH:
            num_samps_needed = phy_struct->N_samps_per_subfr * COARSE_TIMING_SEARCH_NUM_SUBFRAMES;
            break;
        case LTE_FDD_DL_FS_SAMP_BUF_STATE_PSS_AND_FINE_TIMING_SEARCH:
            num_samps_needed = phy_struct->N_samps_per_subfr * PSS_AND_FINE_TIMING_SEARCH_NUM_SUBFRAMES;
            break;
        case LTE_FDD_DL_FS_SAMP_BUF_STATE_SSS_SEARCH:
            num_samps_needed = phy_struct->N_samps_per_subfr * SSS_SEARCH_NUM_SUBFRAMES;
            break;
        case LTE_FDD_DL_FS_SAMP_BUF_STATE_BCH_DECODE:
            num_samps_needed = phy_struct->N_samps_per_frame * BCH_DECODE_NUM_FRAMES;
            break;
        case LTE_FDD_DL_FS_SAMP_BUF_STATE_PDSCH_DECODE_SIB1:
            num_samps_needed = phy_struct->N_samps_per_frame * PDSCH_DECODE_SIB1_NUM_FRAMES;
            break;
        case LTE_FDD_DL_FS_SAMP_BUF_STATE_PDSCH_DECODE_SI_GENERIC:
            num_samps_needed = phy_struct->N_samps_per_frame * PDSCH_DECODE_SI_GENERIC_NUM_FRAMES;
            break;
        }

        while(samp_buf_r_idx < (samp_buf_w_idx - num_samps_needed))
        {
            if(mib_printed   == true          &&
               sib1_printed  == true          &&
               sib2_printed  == true          &&
               sib3_printed  == sib3_expected &&
               sib4_printed  == sib4_expected &&
               sib5_printed  == sib5_expected &&
               sib6_printed  == sib6_expected &&
               sib7_printed  == sib7_expected &&
               sib8_printed  == sib8_expected &&
               sib13_printed == sib13_expected)
            {
                corr_peak_idx++;
                init();
            }

            switch(state)
            {
            case LTE_FDD_DL_FS_SAMP_BUF_STATE_COARSE_TIMING_SEARCH:
                if(LIBLTE_SUCCESS == liblte_phy_dl_find_coarse_timing_and_freq_offset(phy_struct,
                                                                                      i_buf,
                                                                                      q_buf,
                                                                                      COARSE_TIMING_N_SLOTS,
                                                                                      &timing_struct))
                {
                    if(corr_peak_idx < timing_struct.n_corr_peaks)
                    {
                        // Correct frequency error
                        freq_shift(0, LTE_FDD_DL_FS_SAMP_BUF_SIZE, timing_struct.freq_offset[corr_peak_idx]);

                        // Search for PSS and fine timing
                        state            = LTE_FDD_DL_FS_SAMP_BUF_STATE_PSS_AND_FINE_TIMING_SEARCH;
                        num_samps_needed = phy_struct->N_samps_per_subfr * PSS_AND_FINE_TIMING_SEARCH_NUM_SUBFRAMES;
                    }else{
                        // No more peaks, so signal that we are done
                        done_flag = -1;
                    }
                }else{
                    // Stay in coarse timing search
                    samp_buf_r_idx   += phy_struct->N_samps_per_subfr * COARSE_TIMING_SEARCH_NUM_SUBFRAMES;
                    num_samps_needed  = phy_struct->N_samps_per_subfr * COARSE_TIMING_SEARCH_NUM_SUBFRAMES;
                }
                break;
            case LTE_FDD_DL_FS_SAMP_BUF_STATE_PSS_AND_FINE_TIMING_SEARCH:
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
                        freq_shift(0, LTE_FDD_DL_FS_SAMP_BUF_SIZE, freq_offset);
                        timing_struct.freq_offset[corr_peak_idx] += freq_offset;
                    }

                    // Search for SSS
                    state            = LTE_FDD_DL_FS_SAMP_BUF_STATE_SSS_SEARCH;
                    num_samps_needed = phy_struct->N_samps_per_subfr * SSS_SEARCH_NUM_SUBFRAMES;
                }else{
                    // Go back to coarse timing search
                    state             = LTE_FDD_DL_FS_SAMP_BUF_STATE_COARSE_TIMING_SEARCH;
                    samp_buf_r_idx   += phy_struct->N_samps_per_subfr * COARSE_TIMING_SEARCH_NUM_SUBFRAMES;
                    num_samps_needed  = phy_struct->N_samps_per_subfr * COARSE_TIMING_SEARCH_NUM_SUBFRAMES;
                }
                break;
            case LTE_FDD_DL_FS_SAMP_BUF_STATE_SSS_SEARCH:
                if(LIBLTE_SUCCESS == liblte_phy_find_sss(phy_struct,
                                                         i_buf,
                                                         q_buf,
                                                         N_id_2,
                                                         timing_struct.symb_starts[corr_peak_idx],
                                                         pss_thresh,
                                                         &N_id_1,
                                                         &frame_start_idx))
                {
                    N_id_cell = 3*N_id_1 + N_id_2;

                    for(i=0; i<N_decoded_chans; i++)
                    {
                        if(N_id_cell == decoded_chans[i])
                        {
                            break;
                        }
                    }
                    if(i != N_decoded_chans)
                    {
                        // Go back to coarse timing search
                        state = LTE_FDD_DL_FS_SAMP_BUF_STATE_COARSE_TIMING_SEARCH;
                        corr_peak_idx++;
                        init();
                    }else{
                        // Decode BCH
                        state = LTE_FDD_DL_FS_SAMP_BUF_STATE_BCH_DECODE;
                        while(frame_start_idx < samp_buf_r_idx)
                        {
                            frame_start_idx += phy_struct->N_samps_per_frame;
                        }
                        samp_buf_r_idx   = frame_start_idx;
                        num_samps_needed = phy_struct->N_samps_per_frame * BCH_DECODE_NUM_FRAMES;
                    }
                }else{
                    // Go back to coarse timing search
                    state             = LTE_FDD_DL_FS_SAMP_BUF_STATE_COARSE_TIMING_SEARCH;
                    samp_buf_r_idx   += phy_struct->N_samps_per_subfr * COARSE_TIMING_SEARCH_NUM_SUBFRAMES;
                    num_samps_needed  = phy_struct->N_samps_per_subfr * COARSE_TIMING_SEARCH_NUM_SUBFRAMES;
                }
                break;
            case LTE_FDD_DL_FS_SAMP_BUF_STATE_BCH_DECODE:
                if(LIBLTE_SUCCESS == liblte_phy_get_dl_subframe_and_ce(phy_struct,
                                                                       i_buf,
                                                                       q_buf,
                                                                       samp_buf_r_idx,
                                                                       0,
                                                                       N_id_cell,
                                                                       4,
                                                                       &subframe) &&
                   LIBLTE_SUCCESS == liblte_phy_bch_channel_decode(phy_struct,
                                                                   &subframe,
                                                                   N_id_cell,
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
                    liblte_phy_update_n_rb_dl(phy_struct, N_rb_dl);
                    sfn       = (mib.sfn_div_4 << 2) + sfn_offset;
                    phich_res = liblte_rrc_phich_resource_num[mib.phich_config.res];
                    print_mib(&mib);

                    // Add this channel to the list of decoded channels
                    decoded_chans[N_decoded_chans++] = N_id_cell;
                    if(LTE_FDD_DL_FS_SAMP_BUF_N_DECODED_CHANS_MAX == N_decoded_chans)
                    {
                        done_flag = -1;
                    }

                    // Decode PDSCH for SIB1
                    state = LTE_FDD_DL_FS_SAMP_BUF_STATE_PDSCH_DECODE_SIB1;
                    if((sfn % 2) != 0)
                    {
                        samp_buf_r_idx += phy_struct->N_samps_per_frame;
                        sfn++;
                    }
                    num_samps_needed = phy_struct->N_samps_per_frame * PDSCH_DECODE_SIB1_NUM_FRAMES;
                }else{
                    // Go back to coarse timing search
                    state             = LTE_FDD_DL_FS_SAMP_BUF_STATE_COARSE_TIMING_SEARCH;
                    samp_buf_r_idx   += phy_struct->N_samps_per_subfr * COARSE_TIMING_SEARCH_NUM_SUBFRAMES;
                    num_samps_needed  = phy_struct->N_samps_per_subfr * COARSE_TIMING_SEARCH_NUM_SUBFRAMES;
                }
                break;
            case LTE_FDD_DL_FS_SAMP_BUF_STATE_PDSCH_DECODE_SIB1:
                if(LIBLTE_SUCCESS == liblte_phy_get_dl_subframe_and_ce(phy_struct,
                                                                       i_buf,
                                                                       q_buf,
                                                                       samp_buf_r_idx,
                                                                       5,
                                                                       N_id_cell,
                                                                       N_ant,
                                                                       &subframe) &&
                   LIBLTE_SUCCESS == liblte_phy_pdcch_channel_decode(phy_struct,
                                                                     &subframe,
                                                                     N_id_cell,
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
                                                                     N_id_cell,
                                                                     N_ant,
                                                                     rrc_msg.msg,
                                                                     &rrc_msg.N_bits) &&
                   LIBLTE_SUCCESS == liblte_rrc_unpack_bcch_dlsch_msg(&rrc_msg,
                                                                      &bcch_dlsch_msg))
                {
                    if(1                                == bcch_dlsch_msg.N_sibs &&
                       LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_1 == bcch_dlsch_msg.sibs[0].sib_type)
                    {
                        print_sib1((LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_1_STRUCT *)&bcch_dlsch_msg.sibs[0].sib);
                    }

                    // Decode all PDSCHs
                    state            = LTE_FDD_DL_FS_SAMP_BUF_STATE_PDSCH_DECODE_SI_GENERIC;
                    N_sfr            = 0;
                    num_samps_needed = phy_struct->N_samps_per_frame * PDSCH_DECODE_SI_GENERIC_NUM_FRAMES;
                }else{
                    // Try to decode SIB1 again
                    samp_buf_r_idx   += phy_struct->N_samps_per_frame * PDSCH_DECODE_SIB1_NUM_FRAMES;
                    sfn              += 2;
                    num_samps_needed  = phy_struct->N_samps_per_frame * PDSCH_DECODE_SIB1_NUM_FRAMES;
                }
                break;
            case LTE_FDD_DL_FS_SAMP_BUF_STATE_PDSCH_DECODE_SI_GENERIC:
                if(LIBLTE_SUCCESS == liblte_phy_get_dl_subframe_and_ce(phy_struct,
                                                                       i_buf,
                                                                       q_buf,
                                                                       samp_buf_r_idx,
                                                                       N_sfr,
                                                                       N_id_cell,
                                                                       N_ant,
                                                                       &subframe) &&
                   LIBLTE_SUCCESS == liblte_phy_pdcch_channel_decode(phy_struct,
                                                                     &subframe,
                                                                     N_id_cell,
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
                                                                     N_id_cell,
                                                                     N_ant,
                                                                     rrc_msg.msg,
                                                                     &rrc_msg.N_bits))
                {
                    if(LIBLTE_MAC_SI_RNTI == pdcch.alloc[0].rnti &&
                       LIBLTE_SUCCESS     == liblte_rrc_unpack_bcch_dlsch_msg(&rrc_msg,
                                                                              &bcch_dlsch_msg))
                    {
                        for(i=0; i<bcch_dlsch_msg.N_sibs; i++)
                        {
                            switch(bcch_dlsch_msg.sibs[i].sib_type)
                            {
                            case LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_1:
                                print_sib1((LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_1_STRUCT *)&bcch_dlsch_msg.sibs[i].sib);
                                break;
                            case LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_2:
                                print_sib2((LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_2_STRUCT *)&bcch_dlsch_msg.sibs[i].sib);
                                break;
                            case LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_3:
                                print_sib3((LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_3_STRUCT *)&bcch_dlsch_msg.sibs[i].sib);
                                break;
                            case LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_4:
                                print_sib4((LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_4_STRUCT *)&bcch_dlsch_msg.sibs[i].sib);
                                break;
                            case LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_5:
                                print_sib5((LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_5_STRUCT *)&bcch_dlsch_msg.sibs[i].sib);
                                break;
                            case LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_6:
                                print_sib6((LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_6_STRUCT *)&bcch_dlsch_msg.sibs[i].sib);
                                break;
                            case LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_7:
                                print_sib7((LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_7_STRUCT *)&bcch_dlsch_msg.sibs[i].sib);
                                break;
                            case LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_8:
                                print_sib8((LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_8_STRUCT *)&bcch_dlsch_msg.sibs[i].sib);
                                break;
                            case LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_13:
                                print_sib13((LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_13_STRUCT *)&bcch_dlsch_msg.sibs[i].sib);
                                break;
                            default:
                                printf("Not handling SIB %u\n", bcch_dlsch_msg.sibs[i].sib_type);
                                break;
                            }
                        }
                    }else if(LIBLTE_MAC_P_RNTI == pdcch.alloc[0].rnti){
                        for(i=0; i<8; i++)
                        {
                            if(rrc_msg.msg[i] != liblte_rrc_test_fill[i])
                            {
                                break;
                            }
                        }
                        if(i == 16)
                        {
                            printf("TEST FILL RECEIVED\n");
                        }else if(LIBLTE_SUCCESS == liblte_rrc_unpack_pcch_msg(&rrc_msg,
                                                                              &pcch_msg)){
                            print_page(&pcch_msg);
                        }
                    }else{
                        printf("MESSAGE RECEIVED FOR RNTI=%04X: ", pdcch.alloc[0].rnti);
                        for(i=0; i<rrc_msg.N_bits; i++)
                        {
                            printf("%u", rrc_msg.msg[i]);
                        }
                        printf("\n");
                    }
                }

                // Keep trying to decode PDSCHs
                state            = LTE_FDD_DL_FS_SAMP_BUF_STATE_PDSCH_DECODE_SI_GENERIC;
                num_samps_needed = phy_struct->N_samps_per_frame * PDSCH_DECODE_SI_GENERIC_NUM_FRAMES;
                N_sfr++;
                if(N_sfr >= 10)
                {
                    N_sfr = 0;
                    sfn++;
                    samp_buf_r_idx += phy_struct->N_samps_per_frame * PDSCH_DECODE_SI_GENERIC_NUM_FRAMES;
                }
                break;
            }

            if(-1 == done_flag)
            {
                break;
            }
        }

        // Copy remaining samples to beginning of buffer
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

        if(true == copy_input)
        {
            copy_input_to_samp_buf(input_items, ninput_items);
        }
    }

    // Tell runtime system how many input items we consumed
    consume_each(ninput_items);

    // Tell runtime system how many output items we produced.
    return(done_flag);
}

void LTE_fdd_dl_fs_samp_buf::init(void)
{
    state                   = LTE_FDD_DL_FS_SAMP_BUF_STATE_COARSE_TIMING_SEARCH;
    phich_res               = 0;
    sfn                     = 0;
    N_sfr                   = 0;
    N_ant                   = 0;
    N_id_cell               = 0;
    N_id_1                  = 0;
    N_id_2                  = 0;
    prev_si_value_tag       = 0;
    prev_si_value_tag_valid = false;
    mib_printed             = false;
    sib1_printed            = false;
    sib2_printed            = false;
    sib3_printed            = false;
    sib3_expected           = false;
    sib4_printed            = false;
    sib4_expected           = false;
    sib5_printed            = false;
    sib5_expected           = false;
    sib6_printed            = false;
    sib6_expected           = false;
    sib7_printed            = false;
    sib7_expected           = false;
    sib8_printed            = false;
    sib8_expected           = false;
    sib13_printed           = false;
    sib13_expected          = false;
}

void LTE_fdd_dl_fs_samp_buf::copy_input_to_samp_buf(gr_vector_const_void_star &input_items, int32 ninput_items)
{
    const gr_complex *gr_complex_in = (gr_complex *)input_items[0];
    uint32            i;
    uint32            offset;
    const int8       *int8_in = (int8 *)input_items[0];

    if(LTE_FDD_DL_FS_IN_SIZE_INT8 == in_size)
    {
        if(true == last_samp_was_i)
        {
            q_buf[samp_buf_w_idx++] = (float)int8_in[0];
            offset                  = 1;
        }else{
            offset = 0;
        }

        for(i=0; i<(ninput_items-offset)/2; i++)
        {
            i_buf[samp_buf_w_idx]   = (float)int8_in[i*2+offset];
            q_buf[samp_buf_w_idx++] = (float)int8_in[i*2+offset+1];
        }

        if(((ninput_items-offset) % 2) != 0)
        {
            i_buf[samp_buf_w_idx] = (float)int8_in[ninput_items-1];
            last_samp_was_i       = true;
        }else{
            last_samp_was_i = false;
        }
    }else{ // LTE_FDD_DL_FS_IN_SIZE_GR_COMPLEX == in_size
        for(i=0; i<ninput_items; i++)
        {
            i_buf[samp_buf_w_idx]   = gr_complex_in[i].real();
            q_buf[samp_buf_w_idx++] = gr_complex_in[i].imag();
        }
    }
}

void LTE_fdd_dl_fs_samp_buf::freq_shift(uint32 start_idx, uint32 num_samps, float freq_offset)
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

void LTE_fdd_dl_fs_samp_buf::print_mib(LIBLTE_RRC_MIB_STRUCT *mib)
{
    if(false == mib_printed)
    {
        printf("DL LTE Channel found [%u]:\n", corr_peak_idx);
        printf("\tMIB Decoded:\n");
        printf("\t\t%-40s=%20.2f\n", "Frequency Offset", timing_struct.freq_offset[corr_peak_idx]);
        printf("\t\t%-40s=%20u\n", "System Frame Number", sfn);
        printf("\t\t%-40s=%20u\n", "Physical Cell ID", N_id_cell);
        printf("\t\t%-40s=%20u\n", "Number of TX Antennas", N_ant);
        printf("\t\t%-40s=%17sMHz\n", "Bandwidth", liblte_rrc_dl_bandwidth_text[mib->dl_bw]);
        printf("\t\t%-40s=%20s\n", "PHICH Duration", liblte_rrc_phich_duration_text[mib->phich_config.dur]);
        printf("\t\t%-40s=%20s\n", "PHICH Resource", liblte_rrc_phich_resource_text[mib->phich_config.res]);

        mib_printed = true;
    }
}

void LTE_fdd_dl_fs_samp_buf::print_sib1(LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_1_STRUCT *sib1)
{
    uint32 i;
    uint32 j;
    uint32 si_win_len;
    uint32 si_periodicity_T;
    uint16 mnc;

    if(true              == prev_si_value_tag_valid &&
       prev_si_value_tag != sib1->system_info_value_tag)
    {
        printf("\tSystem Info value tag changed\n");
        sib1_printed  = false;
        sib2_printed  = false;
        sib3_printed  = false;
        sib4_printed  = false;
        sib5_printed  = false;
        sib6_printed  = false;
        sib7_printed  = false;
        sib8_printed  = false;
        sib13_printed = false;
    }

    if(false == sib1_printed)
    {
        printf("\tSIB1 Decoded:\n");
        printf("\t\t%-40s\n", "PLMN Identity List:");
        for(i=0; i<sib1->N_plmn_ids; i++)
        {
            printf("\t\t\t%03X-", sib1->plmn_id[i].id.mcc & 0x0FFF);
            if((sib1->plmn_id[i].id.mnc & 0xFF00) == 0xFF00)
            {
                mnc = sib1->plmn_id[i].id.mnc & 0x00FF;
                printf("%02X, ", mnc);
            }else{
                mnc = sib1->plmn_id[i].id.mnc & 0x0FFF;
                printf("%03X, ", mnc);
            }
            for(j=0; j<LIBLTE_MCC_MNC_LIST_N_ITEMS; j++)
            {
                if(liblte_mcc_mnc_list[j].mcc == (sib1->plmn_id[i].id.mcc & 0x0FFF) &&
                   liblte_mcc_mnc_list[j].mnc == mnc)
                {
                    printf("%s, ", liblte_mcc_mnc_list[j].net_name);
                    break;
                }
            }
            if(LIBLTE_RRC_RESV_FOR_OPER == sib1->plmn_id[i].resv_for_oper)
            {
                printf("reserved for operator use\n");
            }else{
                printf("not reserved for operator use\n");
            }
        }
        printf("\t\t%-40s=%20u\n", "Tracking Area Code", sib1->tracking_area_code);
        printf("\t\t%-40s=%20u\n", "Cell Identity", sib1->cell_id);
        switch(sib1->cell_barred)
        {
        case LIBLTE_RRC_CELL_BARRED:
            printf("\t\t%-40s=%20s\n", "Cell Barred", "Barred");
            break;
        case LIBLTE_RRC_CELL_NOT_BARRED:
            printf("\t\t%-40s=%20s\n", "Cell Barred", "Not Barred");
            break;
        }
        switch(sib1->intra_freq_reselection)
        {
        case LIBLTE_RRC_INTRA_FREQ_RESELECTION_ALLOWED:
            printf("\t\t%-40s=%20s\n", "Intra Frequency Reselection", "Allowed");
            break;
        case LIBLTE_RRC_INTRA_FREQ_RESELECTION_NOT_ALLOWED:
            printf("\t\t%-40s=%20s\n", "Intra Frequency Reselection", "Not Allowed");
            break;
        }
        if(true == sib1->csg_indication)
        {
            printf("\t\t%-40s=%20s\n", "CSG Indication", "TRUE");
        }else{
            printf("\t\t%-40s=%20s\n", "CSG Indication", "FALSE");
        }
        if(LIBLTE_RRC_CSG_IDENTITY_NOT_PRESENT != sib1->csg_id)
        {
            printf("\t\t%-40s=%20u\n", "CSG Identity", sib1->csg_id);
        }
        printf("\t\t%-40s=%17ddBm\n", "Q Rx Lev Min", sib1->q_rx_lev_min);
        printf("\t\t%-40s=%18udB\n", "Q Rx Lev Min Offset", sib1->q_rx_lev_min_offset);
        if(true == sib1->p_max_present)
        {
            printf("\t\t%-40s=%17ddBm\n", "P Max", sib1->p_max);
        }
        printf("\t\t%-40s=%20u\n", "Frequency Band", sib1->freq_band_indicator);
        printf("\t\t%-40s=%18sms\n", "SI Window Length", liblte_rrc_si_window_length_text[sib1->si_window_length]);
        si_win_len = liblte_rrc_si_window_length_num[sib1->si_window_length];
        printf("\t\t%-40s\n", "Scheduling Info List:");
        for(i=0; i<sib1->N_sched_info; i++)
        {
            printf("\t\t\t%s = %s frames\n", "SI Periodicity", liblte_rrc_si_periodicity_text[sib1->sched_info[i].si_periodicity]);
            si_periodicity_T = liblte_rrc_si_periodicity_num[sib1->sched_info[i].si_periodicity];
            printf("\t\t\tSI Window Starts at N_subframe = %u, SFN mod %u = %u\n", (i * si_win_len) % 10, si_periodicity_T, (i * si_win_len)/10);
            if(0 == i)
            {
                printf("\t\t\t\t%s = %s\n", "SIB Type", "2");
            }
            for(j=0; j<sib1->sched_info[i].N_sib_mapping_info; j++)
            {
                printf("\t\t\t\t%s = %u\n", "SIB Type", liblte_rrc_sib_type_num[sib1->sched_info[i].sib_mapping_info[j].sib_type]);
                switch(sib1->sched_info[i].sib_mapping_info[j].sib_type)
                {
                case LIBLTE_RRC_SIB_TYPE_3:
                    sib3_expected = true;
                    break;
                case LIBLTE_RRC_SIB_TYPE_4:
                    sib4_expected = true;
                    break;
                case LIBLTE_RRC_SIB_TYPE_5:
                    sib5_expected = true;
                    break;
                case LIBLTE_RRC_SIB_TYPE_6:
                    sib6_expected = true;
                    break;
                case LIBLTE_RRC_SIB_TYPE_7:
                    sib7_expected = true;
                    break;
                case LIBLTE_RRC_SIB_TYPE_8:
                    sib8_expected = true;
                    break;
                case LIBLTE_RRC_SIB_TYPE_13_v920:
                    sib13_expected = true;
                    break;
                }
            }
        }
        if(false == sib1->tdd)
        {
            printf("\t\t%-40s=%20s\n", "Duplexing Mode", "FDD");
        }else{
            printf("\t\t%-40s=%20s\n", "Duplexing Mode", "TDD");
            printf("\t\t%-40s=%20s\n", "Subframe Assignment", liblte_rrc_subframe_assignment_text[sib1->tdd_cnfg.sf_assignment]);
            printf("\t\t%-40s=%20s\n", "Special Subframe Patterns", liblte_rrc_special_subframe_patterns_text[sib1->tdd_cnfg.special_sf_patterns]);
        }
        printf("\t\t%-40s=%20u\n", "SI Value Tag", sib1->system_info_value_tag);
        prev_si_value_tag       = sib1->system_info_value_tag;
        prev_si_value_tag_valid = true;

        sib1_printed = true;
    }
}

void LTE_fdd_dl_fs_samp_buf::print_sib2(LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_2_STRUCT *sib2)
{
    uint32 coeff = 0;
    uint32 T     = 0;
    uint32 i;

    if(false == sib2_printed)
    {
        printf("\tSIB2 Decoded:\n");
        if(true == sib2->ac_barring_info_present)
        {
            if(true == sib2->ac_barring_for_emergency)
            {
                printf("\t\t%-40s=%20s\n", "AC Barring for Emergency", "Barred");
            }else{
                printf("\t\t%-40s=%20s\n", "AC Barring for Emergency", "Not Barred");
            }
            if(true == sib2->ac_barring_for_mo_signalling.enabled)
            {
                printf("\t\t%-40s=%20s\n", "AC Barring for MO Signalling", "Barred");
                printf("\t\t\t%-40s=%20s\n", "Factor", liblte_rrc_ac_barring_factor_text[sib2->ac_barring_for_mo_signalling.factor]);
                printf("\t\t\t%-40s=%19ss\n", "Time", liblte_rrc_ac_barring_time_text[sib2->ac_barring_for_mo_signalling.time]);
                printf("\t\t\t%-40s=%20u\n", "Special AC", sib2->ac_barring_for_mo_signalling.for_special_ac);
            }else{
                printf("\t\t%-40s=%20s\n", "AC Barring for MO Signalling", "Not Barred");
            }
            if(true == sib2->ac_barring_for_mo_data.enabled)
            {
                printf("\t\t%-40s=%20s\n", "AC Barring for MO Data", "Barred");
                printf("\t\t\t%-40s=%20s\n", "Factor", liblte_rrc_ac_barring_factor_text[sib2->ac_barring_for_mo_data.factor]);
                printf("\t\t\t%-40s=%19ss\n", "Time", liblte_rrc_ac_barring_time_text[sib2->ac_barring_for_mo_data.time]);
                printf("\t\t\t%-40s=%20u\n", "Special AC", sib2->ac_barring_for_mo_data.for_special_ac);
            }else{
                printf("\t\t%-40s=%20s\n", "AC Barring for MO Data", "Not Barred");
            }
        }
        printf("\t\t%-40s=%20s\n", "Number of RACH Preambles", liblte_rrc_number_of_ra_preambles_text[sib2->rr_config_common_sib.rach_cnfg.num_ra_preambles]);
        if(true == sib2->rr_config_common_sib.rach_cnfg.preambles_group_a_cnfg.present)
        {
            printf("\t\t%-40s=%20s\n", "Size of RACH Preambles Group A", liblte_rrc_size_of_ra_preambles_group_a_text[sib2->rr_config_common_sib.rach_cnfg.preambles_group_a_cnfg.size_of_ra]);
            printf("\t\t%-40s=%15s bits\n", "Message Size Group A", liblte_rrc_message_size_group_a_text[sib2->rr_config_common_sib.rach_cnfg.preambles_group_a_cnfg.msg_size]);
            printf("\t\t%-40s=%18sdB\n", "Message Power Offset Group B", liblte_rrc_message_power_offset_group_b_text[sib2->rr_config_common_sib.rach_cnfg.preambles_group_a_cnfg.msg_pwr_offset_group_b]);
        }
        printf("\t\t%-40s=%18sdB\n", "Power Ramping Step", liblte_rrc_power_ramping_step_text[sib2->rr_config_common_sib.rach_cnfg.pwr_ramping_step]);
        printf("\t\t%-40s=%17sdBm\n", "Preamble init target RX power", liblte_rrc_preamble_initial_received_target_power_text[sib2->rr_config_common_sib.rach_cnfg.preamble_init_rx_target_pwr]);
        printf("\t\t%-40s=%20s\n", "Preamble TX Max", liblte_rrc_preamble_trans_max_text[sib2->rr_config_common_sib.rach_cnfg.preamble_trans_max]);
        printf("\t\t%-40s=%10s Subframes\n", "RA Response Window Size", liblte_rrc_ra_response_window_size_text[sib2->rr_config_common_sib.rach_cnfg.ra_resp_win_size]);
        printf("\t\t%-40s=%10s Subframes\n", "MAC Contention Resolution Timer", liblte_rrc_mac_contention_resolution_timer_text[sib2->rr_config_common_sib.rach_cnfg.mac_con_res_timer]);
        printf("\t\t%-40s=%20u\n", "Max num HARQ TX for Message 3", sib2->rr_config_common_sib.rach_cnfg.max_harq_msg3_tx);
        printf("\t\t%-40s=%20s\n", "Modification Period Coeff", liblte_rrc_modification_period_coeff_text[sib2->rr_config_common_sib.bcch_cnfg.modification_period_coeff]);
        coeff = liblte_rrc_modification_period_coeff_num[sib2->rr_config_common_sib.bcch_cnfg.modification_period_coeff];
        printf("\t\t%-40s=%13s Frames\n", "Default Paging Cycle", liblte_rrc_default_paging_cycle_text[sib2->rr_config_common_sib.pcch_cnfg.default_paging_cycle]);
        T = liblte_rrc_default_paging_cycle_num[sib2->rr_config_common_sib.pcch_cnfg.default_paging_cycle];
        printf("\t\t%-40s=%13u Frames\n", "Modification Period", coeff * T);
        printf("\t\t%-40s=%13u Frames\n", "nB", (uint32)(T * liblte_rrc_nb_num[sib2->rr_config_common_sib.pcch_cnfg.nB]));
        printf("\t\t%-40s=%20u\n", "Root Sequence Index", sib2->rr_config_common_sib.prach_cnfg.root_sequence_index);
        printf("\t\t%-40s=%20u\n", "PRACH Config Index", sib2->rr_config_common_sib.prach_cnfg.prach_cnfg_info.prach_config_index);
        switch(sib2->rr_config_common_sib.prach_cnfg.prach_cnfg_info.prach_config_index)
        {
        case 0:
            printf("\t\t\tPreamble Format = 0, RACH SFN = Even, RACH Subframe Number = 1\n");
            break;
        case 1:
            printf("\t\t\tPreamble Format = 0, RACH SFN = Even, RACH Subframe Number = 4\n");
            break;
        case 2:
            printf("\t\t\tPreamble Format = 0, RACH SFN = Even, RACH Subframe Number = 7\n");
            break;
        case 3:
            printf("\t\t\tPreamble Format = 0, RACH SFN = Any, RACH Subframe Number = 1\n");
            break;
        case 4:
            printf("\t\t\tPreamble Format = 0, RACH SFN = Any, RACH Subframe Number = 4\n");
            break;
        case 5:
            printf("\t\t\tPreamble Format = 0, RACH SFN = Any, RACH Subframe Number = 7\n");
            break;
        case 6:
            printf("\t\t\tPreamble Format = 0, RACH SFN = Any, RACH Subframe Number = 1,6\n");
            break;
        case 7:
            printf("\t\t\tPreamble Format = 0, RACH SFN = Any, RACH Subframe Number = 2,7\n");
            break;
        case 8:
            printf("\t\t\tPreamble Format = 0, RACH SFN = Any, RACH Subframe Number = 3,8\n");
            break;
        case 9:
            printf("\t\t\tPreamble Format = 0, RACH SFN = Any, RACH Subframe Number = 1,4,7\n");
            break;
        case 10:
            printf("\t\t\tPreamble Format = 0, RACH SFN = Any, RACH Subframe Number = 2,5,8\n");
            break;
        case 11:
            printf("\t\t\tPreamble Format = 0, RACH SFN = Any, RACH Subframe Number = 3,6,9\n");
            break;
        case 12:
            printf("\t\t\tPreamble Format = 0, RACH SFN = Any, RACH Subframe Number = 0,2,4,6,8\n");
            break;
        case 13:
            printf("\t\t\tPreamble Format = 0, RACH SFN = Any, RACH Subframe Number = 1,3,5,7,9\n");
            break;
        case 14:
            printf("\t\t\tPreamble Format = 0, RACH SFN = Any, RACH Subframe Number = 0,1,2,3,4,5,6,7,8,9\n");
            break;
        case 15:
            printf("\t\t\tPreamble Format = 0, RACH SFN = Even, RACH Subframe Number = 9\n");
            break;
        case 16:
            printf("\t\t\tPreamble Format = 1, RACH SFN = Even, RACH Subframe Number = 1\n");
            break;
        case 17:
            printf("\t\t\tPreamble Format = 1, RACH SFN = Even, RACH Subframe Number = 4\n");
            break;
        case 18:
            printf("\t\t\tPreamble Format = 1, RACH SFN = Even, RACH Subframe Number = 7\n");
            break;
        case 19:
            printf("\t\t\tPreamble Format = 1, RACH SFN = Any, RACH Subframe Number = 1\n");
            break;
        case 20:
            printf("\t\t\tPreamble Format = 1, RACH SFN = Any, RACH Subframe Number = 4\n");
            break;
        case 21:
            printf("\t\t\tPreamble Format = 1, RACH SFN = Any, RACH Subframe Number = 7\n");
            break;
        case 22:
            printf("\t\t\tPreamble Format = 1, RACH SFN = Any, RACH Subframe Number = 1,6\n");
            break;
        case 23:
            printf("\t\t\tPreamble Format = 1, RACH SFN = Any, RACH Subframe Number = 2,7\n");
            break;
        case 24:
            printf("\t\t\tPreamble Format = 1, RACH SFN = Any, RACH Subframe Number = 3,8\n");
            break;
        case 25:
            printf("\t\t\tPreamble Format = 1, RACH SFN = Any, RACH Subframe Number = 1,4,7\n");
            break;
        case 26:
            printf("\t\t\tPreamble Format = 1, RACH SFN = Any, RACH Subframe Number = 2,5,8\n");
            break;
        case 27:
            printf("\t\t\tPreamble Format = 1, RACH SFN = Any, RACH Subframe Number = 3,6,9\n");
            break;
        case 28:
            printf("\t\t\tPreamble Format = 1, RACH SFN = Any, RACH Subframe Number = 0,2,4,6,8\n");
            break;
        case 29:
            printf("\t\t\tPreamble Format = 1, RACH SFN = Any, RACH Subframe Number = 1,3,5,7,9\n");
            break;
        case 30:
            printf("\t\t\tPreamble Format = N/A, RACH SFN = N/A, RACH Subframe Number = N/A\n");
            break;
        case 31:
            printf("\t\t\tPreamble Format = 1, RACH SFN = Even, RACH Subframe Number = 9\n");
            break;
        case 32:
            printf("\t\t\tPreamble Format = 2, RACH SFN = Even, RACH Subframe Number = 1\n");
            break;
        case 33:
            printf("\t\t\tPreamble Format = 2, RACH SFN = Even, RACH Subframe Number = 4\n");
            break;
        case 34:
            printf("\t\t\tPreamble Format = 2, RACH SFN = Even, RACH Subframe Number = 7\n");
            break;
        case 35:
            printf("\t\t\tPreamble Format = 2, RACH SFN = Any, RACH Subframe Number = 1\n");
            break;
        case 36:
            printf("\t\t\tPreamble Format = 2, RACH SFN = Any, RACH Subframe Number = 4\n");
            break;
        case 37:
            printf("\t\t\tPreamble Format = 2, RACH SFN = Any, RACH Subframe Number = 7\n");
            break;
        case 38:
            printf("\t\t\tPreamble Format = 2, RACH SFN = Any, RACH Subframe Number = 1,6\n");
            break;
        case 39:
            printf("\t\t\tPreamble Format = 2, RACH SFN = Any, RACH Subframe Number = 2,7\n");
            break;
        case 40:
            printf("\t\t\tPreamble Format = 2, RACH SFN = Any, RACH Subframe Number = 3,8\n");
            break;
        case 41:
            printf("\t\t\tPreamble Format = 2, RACH SFN = Any, RACH Subframe Number = 1,4,7\n");
            break;
        case 42:
            printf("\t\t\tPreamble Format = 2, RACH SFN = Any, RACH Subframe Number = 2,5,8\n");
            break;
        case 43:
            printf("\t\t\tPreamble Format = 2, RACH SFN = Any, RACH Subframe Number = 3,6,9\n");
            break;
        case 44:
            printf("\t\t\tPreamble Format = 2, RACH SFN = Any, RACH Subframe Number = 0,2,4,6,8\n");
            break;
        case 45:
            printf("\t\t\tPreamble Format = 2, RACH SFN = Any, RACH Subframe Number = 1,3,5,7,9\n");
            break;
        case 46:
            printf("\t\t\tPreamble Format = N/A, RACH SFN = N/A, RACH Subframe Number = N/A\n");
            break;
        case 47:
            printf("\t\t\tPreamble Format = 2, RACH SFN = Even, RACH Subframe Number = 9\n");
            break;
        case 48:
            printf("\t\t\tPreamble Format = 3, RACH SFN = Even, RACH Subframe Number = 1\n");
            break;
        case 49:
            printf("\t\t\tPreamble Format = 3, RACH SFN = Even, RACH Subframe Number = 4\n");
            break;
        case 50:
            printf("\t\t\tPreamble Format = 3, RACH SFN = Even, RACH Subframe Number = 7\n");
            break;
        case 51:
            printf("\t\t\tPreamble Format = 3, RACH SFN = Any, RACH Subframe Number = 1\n");
            break;
        case 52:
            printf("\t\t\tPreamble Format = 3, RACH SFN = Any, RACH Subframe Number = 4\n");
            break;
        case 53:
            printf("\t\t\tPreamble Format = 3, RACH SFN = Any, RACH Subframe Number = 7\n");
            break;
        case 54:
            printf("\t\t\tPreamble Format = 3, RACH SFN = Any, RACH Subframe Number = 1,6\n");
            break;
        case 55:
            printf("\t\t\tPreamble Format = 3, RACH SFN = Any, RACH Subframe Number = 2,7\n");
            break;
        case 56:
            printf("\t\t\tPreamble Format = 3, RACH SFN = Any, RACH Subframe Number = 3,8\n");
            break;
        case 57:
            printf("\t\t\tPreamble Format = 3, RACH SFN = Any, RACH Subframe Number = 1,4,7\n");
            break;
        case 58:
            printf("\t\t\tPreamble Format = 3, RACH SFN = Any, RACH Subframe Number = 2,5,8\n");
            break;
        case 59:
            printf("\t\t\tPreamble Format = 3, RACH SFN = Any, RACH Subframe Number = 3,6,9\n");
            break;
        case 60:
            printf("\t\t\tPreamble Format = N/A, RACH SFN = N/A, RACH Subframe Number = N/A\n");
            break;
        case 61:
            printf("\t\t\tPreamble Format = N/A, RACH SFN = N/A, RACH Subframe Number = N/A\n");
            break;
        case 62:
            printf("\t\t\tPreamble Format = N/A, RACH SFN = N/A, RACH Subframe Number = N/A\n");
            break;
        case 63:
            printf("\t\t\tPreamble Format = 3, RACH SFN = Even, RACH Subframe Number = 9\n");
            break;
        }
        if(true == sib2->rr_config_common_sib.prach_cnfg.prach_cnfg_info.high_speed_flag)
        {
            printf("\t\t%-40s=%20s\n", "High Speed Flag", "Restricted Set");
        }else{
            printf("\t\t%-40s=%20s\n", "High Speed Flag", "Unrestricted Set");
        }
        printf("\t\t%-40s=%20u\n", "Ncs Configuration", sib2->rr_config_common_sib.prach_cnfg.prach_cnfg_info.zero_correlation_zone_config);
        printf("\t\t%-40s=%20u\n", "PRACH Freq Offset", sib2->rr_config_common_sib.prach_cnfg.prach_cnfg_info.prach_freq_offset);
        printf("\t\t%-40s=%17ddBm\n", "Reference Signal Power", sib2->rr_config_common_sib.pdsch_cnfg.rs_power);
        printf("\t\t%-40s=%20u\n", "Pb", sib2->rr_config_common_sib.pdsch_cnfg.p_b);
        printf("\t\t%-40s=%20u\n", "Nsb", sib2->rr_config_common_sib.pusch_cnfg.n_sb);
        switch(sib2->rr_config_common_sib.pusch_cnfg.hopping_mode)
        {
        case LIBLTE_RRC_HOPPING_MODE_INTER_SUBFRAME:
            printf("\t\t%-40s=%20s\n", "Hopping Mode", "Inter Subframe");
            break;
        case LIBLTE_RRC_HOPPING_MODE_INTRA_AND_INTER_SUBFRAME:
            printf("\t\t%-40s= %s\n", "Hopping Mode", "Intra and Inter Subframe");
            break;
        }
        printf("\t\t%-40s=%20u\n", "PUSCH Nrb Hopping Offset", sib2->rr_config_common_sib.pusch_cnfg.pusch_hopping_offset);
        if(true == sib2->rr_config_common_sib.pusch_cnfg.enable_64_qam)
        {
            printf("\t\t%-40s=%20s\n", "64QAM", "Allowed");
        }else{
            printf("\t\t%-40s=%20s\n", "64QAM", "Not Allowed");
        }
        if(true == sib2->rr_config_common_sib.pusch_cnfg.ul_rs.group_hopping_enabled)
        {
            printf("\t\t%-40s=%20s\n", "Group Hopping", "Enabled");
        }else{
            printf("\t\t%-40s=%20s\n", "Group Hopping", "Disabled");
        }
        printf("\t\t%-40s=%20u\n", "Group Assignment PUSCH", sib2->rr_config_common_sib.pusch_cnfg.ul_rs.group_assignment_pusch);
        if(true == sib2->rr_config_common_sib.pusch_cnfg.ul_rs.sequence_hopping_enabled)
        {
            printf("\t\t%-40s=%20s\n", "Sequence Hopping", "Enabled");
        }else{
            printf("\t\t%-40s=%20s\n", "Sequence Hopping", "Disabled");
        }
        printf("\t\t%-40s=%20u\n", "Cyclic Shift", sib2->rr_config_common_sib.pusch_cnfg.ul_rs.cyclic_shift);
        printf("\t\t%-40s=%20s\n", "Delta PUCCH Shift", liblte_rrc_delta_pucch_shift_text[sib2->rr_config_common_sib.pucch_cnfg.delta_pucch_shift]);
        printf("\t\t%-40s=%20u\n", "N_rb_cqi", sib2->rr_config_common_sib.pucch_cnfg.n_rb_cqi);
        printf("\t\t%-40s=%20u\n", "N_cs_an", sib2->rr_config_common_sib.pucch_cnfg.n_cs_an);
        printf("\t\t%-40s=%20u\n", "N1 PUCCH AN", sib2->rr_config_common_sib.pucch_cnfg.n1_pucch_an);
        if(true == sib2->rr_config_common_sib.srs_ul_cnfg.present)
        {
            printf("\t\t%-40s=%20s\n", "SRS Bandwidth Config", liblte_rrc_srs_bw_config_text[sib2->rr_config_common_sib.srs_ul_cnfg.bw_cnfg]);
            printf("\t\t%-40s=%20s\n", "SRS Subframe Config", liblte_rrc_srs_subfr_config_text[sib2->rr_config_common_sib.srs_ul_cnfg.subfr_cnfg]);
            if(true == sib2->rr_config_common_sib.srs_ul_cnfg.ack_nack_simul_tx)
            {
                printf("\t\t%-40s=%20s\n", "Simultaneous AN and SRS", "True");
            }else{
                printf("\t\t%-40s=%20s\n", "Simultaneous AN and SRS", "False");
            }
            if(true == sib2->rr_config_common_sib.srs_ul_cnfg.max_up_pts_present)
            {
                printf("\t\t%-40s=%20s\n", "SRS Max Up PTS", "True");
            }else{
                printf("\t\t%-40s=%20s\n", "SRS Max Up PTS", "False");
            }
        }
        printf("\t\t%-40s=%17ddBm\n", "P0 Nominal PUSCH", sib2->rr_config_common_sib.ul_pwr_ctrl.p0_nominal_pusch);
        printf("\t\t%-40s=%20s\n", "Alpha", liblte_rrc_ul_power_control_alpha_text[sib2->rr_config_common_sib.ul_pwr_ctrl.alpha]);
        printf("\t\t%-40s=%17ddBm\n", "P0 Nominal PUCCH", sib2->rr_config_common_sib.ul_pwr_ctrl.p0_nominal_pucch);
        printf("\t\t%-40s=%18sdB\n", "Delta F PUCCH Format 1", liblte_rrc_delta_f_pucch_format_1_text[sib2->rr_config_common_sib.ul_pwr_ctrl.delta_flist_pucch.format_1]);
        printf("\t\t%-40s=%18sdB\n", "Delta F PUCCH Format 1B", liblte_rrc_delta_f_pucch_format_1b_text[sib2->rr_config_common_sib.ul_pwr_ctrl.delta_flist_pucch.format_1b]);
        printf("\t\t%-40s=%18sdB\n", "Delta F PUCCH Format 2", liblte_rrc_delta_f_pucch_format_2_text[sib2->rr_config_common_sib.ul_pwr_ctrl.delta_flist_pucch.format_2]);
        printf("\t\t%-40s=%18sdB\n", "Delta F PUCCH Format 2A", liblte_rrc_delta_f_pucch_format_2a_text[sib2->rr_config_common_sib.ul_pwr_ctrl.delta_flist_pucch.format_2a]);
        printf("\t\t%-40s=%18sdB\n", "Delta F PUCCH Format 2B", liblte_rrc_delta_f_pucch_format_2b_text[sib2->rr_config_common_sib.ul_pwr_ctrl.delta_flist_pucch.format_2b]);
        printf("\t\t%-40s=%18ddB\n", "Delta Preamble Message 3", sib2->rr_config_common_sib.ul_pwr_ctrl.delta_preamble_msg3);
        switch(sib2->rr_config_common_sib.ul_cp_length)
        {
        case LIBLTE_RRC_UL_CP_LENGTH_1:
            printf("\t\t%-40s=%20s\n", "UL CP Length", "Normal");
            break;
        case LIBLTE_RRC_UL_CP_LENGTH_2:
            printf("\t\t%-40s=%20s\n", "UL CP Length", "Extended");
            break;
        }
        printf("\t\t%-40s=%18sms\n", "T300", liblte_rrc_t300_text[sib2->ue_timers_and_constants.t300]);
        printf("\t\t%-40s=%18sms\n", "T301", liblte_rrc_t301_text[sib2->ue_timers_and_constants.t301]);
        printf("\t\t%-40s=%18sms\n", "T310", liblte_rrc_t310_text[sib2->ue_timers_and_constants.t310]);
        printf("\t\t%-40s=%20s\n", "N310", liblte_rrc_n310_text[sib2->ue_timers_and_constants.n310]);
        printf("\t\t%-40s=%18sms\n", "T311", liblte_rrc_t311_text[sib2->ue_timers_and_constants.t311]);
        printf("\t\t%-40s=%20s\n", "N311", liblte_rrc_n311_text[sib2->ue_timers_and_constants.n311]);
        if(true == sib2->arfcn_value_eutra.present)
        {
            printf("\t\t%-40s=%20u\n", "UL ARFCN", sib2->arfcn_value_eutra.value);
        }
        if(true == sib2->ul_bw.present)
        {
            printf("\t\t%-40s=%17sMHz\n", "UL Bandwidth", liblte_rrc_ul_bw_text[sib2->ul_bw.bw]);
        }
        printf("\t\t%-40s=%20u\n", "Additional Spectrum Emission", sib2->additional_spectrum_emission);
        if(0 != sib2->mbsfn_subfr_cnfg_list_size)
        {
            printf("\t\t%s:\n", "MBSFN Subframe Config List");
        }
        for(i=0; i<sib2->mbsfn_subfr_cnfg_list_size; i++)
        {
            printf("\t\t\t%-40s=%20s\n", "Radio Frame Alloc Period", liblte_rrc_radio_frame_allocation_period_text[sib2->mbsfn_subfr_cnfg[i].radio_fr_alloc_period]);
            printf("\t\t\t%-40s=%20u\n", "Radio Frame Alloc Offset", sib2->mbsfn_subfr_cnfg[i].subfr_alloc);
            printf("\t\t\tSubframe Alloc%-26s=%20u\n", liblte_rrc_subframe_allocation_num_frames_text[sib2->mbsfn_subfr_cnfg[i].subfr_alloc_num_frames], sib2->mbsfn_subfr_cnfg[i].subfr_alloc);
        }
        printf("\t\t%-40s=%10s Subframes\n", "Time Alignment Timer", liblte_rrc_time_alignment_timer_text[sib2->time_alignment_timer]);

        sib2_printed = true;
    }
}

void LTE_fdd_dl_fs_samp_buf::print_sib3(LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_3_STRUCT *sib3)
{
    if(false == sib3_printed)
    {
        printf("\tSIB3 Decoded:\n");
        printf("\t\t%-40s=%18sdB\n", "Q-Hyst", liblte_rrc_q_hyst_text[sib3->q_hyst]);
        if(true == sib3->speed_state_resel_params.present)
        {
            printf("\t\t%-40s=%19ss\n", "T-Evaluation", liblte_rrc_t_evaluation_text[sib3->speed_state_resel_params.mobility_state_params.t_eval]);
            printf("\t\t%-40s=%19ss\n", "T-Hyst Normal", liblte_rrc_t_hyst_normal_text[sib3->speed_state_resel_params.mobility_state_params.t_hyst_normal]);
            printf("\t\t%-40s=%20u\n", "N-Cell Change Medium", sib3->speed_state_resel_params.mobility_state_params.n_cell_change_medium);
            printf("\t\t%-40s=%20u\n", "N-Cell Change High", sib3->speed_state_resel_params.mobility_state_params.n_cell_change_high);
            printf("\t\t%-40s=%18sdB\n", "Q-Hyst SF Medium", liblte_rrc_sf_medium_text[sib3->speed_state_resel_params.q_hyst_sf.medium]);
            printf("\t\t%-40s=%18sdB\n", "Q-Hyst SF High", liblte_rrc_sf_high_text[sib3->speed_state_resel_params.q_hyst_sf.high]);
        }
        if(true == sib3->s_non_intra_search_present)
        {
            printf("\t\t%-40s=%18udB\n", "S-Non Intra Search", sib3->s_non_intra_search);
        }
        printf("\t\t%-40s=%18udB\n", "Threshold Serving Low", sib3->thresh_serving_low);
        printf("\t\t%-40s=%20u\n", "Cell Reselection Priority", sib3->cell_resel_prio);
        printf("\t\t%-40s=%17ddBm\n", "Q Rx Lev Min", sib3->q_rx_lev_min);
        if(true == sib3->p_max_present)
        {
            printf("\t\t%-40s=%17ddBm\n", "P Max", sib3->p_max);
        }
        if(true == sib3->s_intra_search_present)
        {
            printf("\t\t%-40s=%18udB\n", "S-Intra Search", sib3->s_intra_search);
        }
        if(true == sib3->allowed_meas_bw_present)
        {
            printf("\t\t%-40s=%17sMHz\n", "Allowed Meas Bandwidth", liblte_rrc_allowed_meas_bandwidth_text[sib3->allowed_meas_bw]);
        }
        if(true == sib3->presence_ant_port_1)
        {
            printf("\t\t%-40s=%20s\n", "Presence Antenna Port 1", "True");
        }else{
            printf("\t\t%-40s=%20s\n", "Presence Antenna Port 1", "False");
        }
        switch(sib3->neigh_cell_cnfg)
        {
        case 0:
            printf("\t\t%-40s= %s\n", "Neighbor Cell Config", "Not all neighbor cells have the same MBSFN alloc");
            break;
        case 1:
            printf("\t\t%-40s= %s\n", "Neighbor Cell Config", "MBSFN allocs are identical for all neighbor cells");
            break;
        case 2:
            printf("\t\t%-40s= %s\n", "Neighbor Cell Config", "No MBSFN allocs are present in neighbor cells");
            break;
        case 3:
            printf("\t\t%-40s= %s\n", "Neighbor Cell Config", "Different UL/DL allocs in neighbor cells for TDD");
            break;
        }
        printf("\t\t%-40s=%19us\n", "T-Reselection EUTRA", sib3->t_resel_eutra);
        if(true == sib3->t_resel_eutra_sf_present)
        {
            printf("\t\t%-40s=%20s\n", "T-Reselection EUTRA SF Medium", liblte_rrc_sssf_medium_text[sib3->t_resel_eutra_sf.sf_medium]);
            printf("\t\t%-40s=%20s\n", "T-Reselection EUTRA SF High", liblte_rrc_sssf_high_text[sib3->t_resel_eutra_sf.sf_high]);
        }

        sib3_printed = true;
    }
}

void LTE_fdd_dl_fs_samp_buf::print_sib4(LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_4_STRUCT *sib4)
{
    uint32 i;
    uint32 stop;

    if(false == sib4_printed)
    {
        printf("\tSIB4 Decoded:\n");
        if(0 != sib4->intra_freq_neigh_cell_list_size)
        {
            printf("\t\tList of intra-frequency neighboring cells:\n");
        }
        for(i=0; i<sib4->intra_freq_neigh_cell_list_size; i++)
        {
            printf("\t\t\t%s = %u\n", "Physical Cell ID", sib4->intra_freq_neigh_cell_list[i].phys_cell_id);
            printf("\t\t\t\t%s = %sdB\n", "Q Offset Range", liblte_rrc_q_offset_range_text[sib4->intra_freq_neigh_cell_list[i].q_offset_range]);
        }
        if(0 != sib4->intra_freq_black_cell_list_size)
        {
            printf("\t\tList of blacklisted intra-frequency neighboring cells:\n");
        }
        for(i=0; i<sib4->intra_freq_black_cell_list_size; i++)
        {
            printf("\t\t\t%u - %u\n", sib4->intra_freq_black_cell_list[i].start, sib4->intra_freq_black_cell_list[i].start + liblte_rrc_phys_cell_id_range_num[sib4->intra_freq_black_cell_list[i].range]);
        }
        if(true == sib4->csg_phys_cell_id_range_present)
        {
            printf("\t\t%-40s= %u - %u\n", "CSG Phys Cell ID Range", sib4->csg_phys_cell_id_range.start, sib4->csg_phys_cell_id_range.start + liblte_rrc_phys_cell_id_range_num[sib4->csg_phys_cell_id_range.range]);
        }

        sib4_printed = true;
    }
}

void LTE_fdd_dl_fs_samp_buf::print_sib5(LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_5_STRUCT *sib5)
{
    uint32 i;
    uint32 j;
    uint16 stop;

    if(false == sib5_printed)
    {
        printf("\tSIB5 Decoded:\n");
        printf("\t\tList of inter-frequency neighboring cells:\n");
        for(i=0; i<sib5->inter_freq_carrier_freq_list_size; i++)
        {
            printf("\t\t\t%-40s=%20u\n", "ARFCN", sib5->inter_freq_carrier_freq_list[i].dl_carrier_freq);
            printf("\t\t\t%-40s=%17ddBm\n", "Q Rx Lev Min", sib5->inter_freq_carrier_freq_list[i].q_rx_lev_min);
            if(true == sib5->inter_freq_carrier_freq_list[i].p_max_present)
            {
                printf("\t\t\t%-40s=%17ddBm\n", "P Max", sib5->inter_freq_carrier_freq_list[i].p_max);
            }
            printf("\t\t\t%-40s=%19us\n", "T-Reselection EUTRA", sib5->inter_freq_carrier_freq_list[i].t_resel_eutra);
            if(true == sib5->inter_freq_carrier_freq_list[i].t_resel_eutra_sf_present)
            {
                printf("\t\t\t%-40s=%20s\n", "T-Reselection EUTRA SF Medium", liblte_rrc_sssf_medium_text[sib5->inter_freq_carrier_freq_list[i].t_resel_eutra_sf.sf_medium]);
                printf("\t\t\t%-40s=%20s\n", "T-Reselection EUTRA SF High", liblte_rrc_sssf_high_text[sib5->inter_freq_carrier_freq_list[i].t_resel_eutra_sf.sf_high]);
            }
            printf("\t\t\t%-40s=%20u\n", "Threshold X High", sib5->inter_freq_carrier_freq_list[i].threshx_high);
            printf("\t\t\t%-40s=%20u\n", "Threshold X Low", sib5->inter_freq_carrier_freq_list[i].threshx_low);
            printf("\t\t\t%-40s=%17sMHz\n", "Allowed Meas Bandwidth", liblte_rrc_allowed_meas_bandwidth_text[sib5->inter_freq_carrier_freq_list[i].allowed_meas_bw]);
            if(true == sib5->inter_freq_carrier_freq_list[i].presence_ant_port_1)
            {
                printf("\t\t\t%-40s=%20s\n", "Presence Antenna Port 1", "True");
            }else{
                printf("\t\t\t%-40s=%20s\n", "Presence Antenna Port 1", "False");
            }
            if(true == sib5->inter_freq_carrier_freq_list[i].cell_resel_prio_present)
            {
                printf("\t\t\t%-40s=%20u\n", "Cell Reselection Priority", sib5->inter_freq_carrier_freq_list[i].cell_resel_prio);
            }
            switch(sib5->inter_freq_carrier_freq_list[i].neigh_cell_cnfg)
            {
            case 0:
                printf("\t\t\t%-40s= %s\n", "Neighbor Cell Config", "Not all neighbor cells have the same MBSFN alloc");
                break;
            case 1:
                printf("\t\t\t%-40s= %s\n", "Neighbor Cell Config", "MBSFN allocs are identical for all neighbor cells");
                break;
            case 2:
                printf("\t\t\t%-40s= %s\n", "Neighbor Cell Config", "No MBSFN allocs are present in neighbor cells");
                break;
            case 3:
                printf("\t\t\t%-40s= %s\n", "Neighbor Cell Config", "Different UL/DL allocs in neighbor cells for TDD");
                break;
            }
            printf("\t\t\t%-40s=%18sdB\n", "Q Offset Freq", liblte_rrc_q_offset_range_text[sib5->inter_freq_carrier_freq_list[i].q_offset_freq]);
            if(0 != sib5->inter_freq_carrier_freq_list[i].inter_freq_neigh_cell_list_size)
            {
                printf("\t\t\tList of inter-frequency neighboring cells with specific cell reselection parameters:\n");
                for(j=0; j<sib5->inter_freq_carrier_freq_list[i].inter_freq_neigh_cell_list_size; j++)
                {
                    printf("\t\t\t\t%-40s=%20u\n", "Physical Cell ID", sib5->inter_freq_carrier_freq_list[i].inter_freq_neigh_cell_list[j].phys_cell_id);
                    printf("\t\t\t\t%-40s=%18sdB\n", "Q Offset Cell", liblte_rrc_q_offset_range_text[sib5->inter_freq_carrier_freq_list[i].inter_freq_neigh_cell_list[j].q_offset_cell]);
                }
            }
            if(0 != sib5->inter_freq_carrier_freq_list[i].inter_freq_black_cell_list_size)
            {
                printf("\t\t\tList of blacklisted inter-frequency neighboring cells\n");
                for(j=0; j<sib5->inter_freq_carrier_freq_list[i].inter_freq_black_cell_list_size; j++)
                {
                    printf("\t\t\t\t%u - %u\n", sib5->inter_freq_carrier_freq_list[i].inter_freq_black_cell_list[j].start, sib5->inter_freq_carrier_freq_list[i].inter_freq_black_cell_list[j].start + liblte_rrc_phys_cell_id_range_num[sib5->inter_freq_carrier_freq_list[i].inter_freq_black_cell_list[j].range]);
                }
            }
        }

        sib5_printed = true;
    }
}

void LTE_fdd_dl_fs_samp_buf::print_sib6(LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_6_STRUCT *sib6)
{
    uint32 i;

    if(false == sib6_printed)
    {
        printf("\tSIB6 Decoded:\n");
        if(0 != sib6->carrier_freq_list_utra_fdd_size)
        {
            printf("\t\t%s:\n", "Carrier Freq List UTRA FDD");
        }
        for(i=0; i<sib6->carrier_freq_list_utra_fdd_size; i++)
        {
            printf("\t\t\t%-40s=%20u\n", "ARFCN", sib6->carrier_freq_list_utra_fdd[i].carrier_freq);
            if(true == sib6->carrier_freq_list_utra_fdd[i].cell_resel_prio_present)
            {
                printf("\t\t\t%-40s=%20u\n", "Cell Reselection Priority", sib6->carrier_freq_list_utra_fdd[i].cell_resel_prio);
            }
            printf("\t\t\t%-40s=%20u\n", "Threshold X High", sib6->carrier_freq_list_utra_fdd[i].threshx_high);
            printf("\t\t\t%-40s=%20u\n", "Threshold X Low", sib6->carrier_freq_list_utra_fdd[i].threshx_low);
            printf("\t\t\t%-40s=%17ddBm\n", "Q Rx Lev Min", sib6->carrier_freq_list_utra_fdd[i].q_rx_lev_min);
            printf("\t\t\t%-40s=%17ddBm\n", "P Max UTRA", sib6->carrier_freq_list_utra_fdd[i].p_max_utra);
            printf("\t\t\t%-40s=%18dB\n", "Q Qual Min", sib6->carrier_freq_list_utra_fdd[i].q_qual_min);
        }
        if(0 != sib6->carrier_freq_list_utra_tdd_size)
        {
            printf("\t\t%s:\n", "Carrier Freq List UTRA TDD");
        }
        for(i=0; i<sib6->carrier_freq_list_utra_tdd_size; i++)
        {
            printf("\t\t\t%-40s=%20u\n", "ARFCN", sib6->carrier_freq_list_utra_tdd[i].carrier_freq);
            if(true == sib6->carrier_freq_list_utra_tdd[i].cell_resel_prio_present)
            {
                printf("\t\t\t%-40s=%20u\n", "Cell Reselection Priority", sib6->carrier_freq_list_utra_tdd[i].cell_resel_prio);
            }
            printf("\t\t\t%-40s=%20u\n", "Threshold X High", sib6->carrier_freq_list_utra_tdd[i].threshx_high);
            printf("\t\t\t%-40s=%20u\n", "Threshold X Low", sib6->carrier_freq_list_utra_tdd[i].threshx_low);
            printf("\t\t\t%-40s=%17ddBm\n", "Q Rx Lev Min", sib6->carrier_freq_list_utra_tdd[i].q_rx_lev_min);
            printf("\t\t\t%-40s=%17ddBm\n", "P Max UTRA", sib6->carrier_freq_list_utra_tdd[i].p_max_utra);
        }
        printf("\t\t%-40s=%19us\n", "T-Reselection UTRA", sib6->t_resel_utra);
        if(true == sib6->t_resel_utra_sf_present)
        {
            printf("\t\t%-40s=%20s\n", "T-Reselection UTRA SF Medium", liblte_rrc_sssf_medium_text[sib6->t_resel_utra_sf.sf_medium]);
            printf("\t\t%-40s=%20s\n", "T-Reselection UTRA SF High", liblte_rrc_sssf_high_text[sib6->t_resel_utra_sf.sf_high]);
        }

        sib6_printed = true;
    }
}

void LTE_fdd_dl_fs_samp_buf::print_sib7(LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_7_STRUCT *sib7)
{
    uint32 i;
    uint32 j;

    if(false == sib7_printed)
    {
        printf("\tSIB7 Decoded:\n");
        printf("\t\t%-40s=%19us\n", "T-Reselection GERAN", sib7->t_resel_geran);
        if(true == sib7->t_resel_geran_sf_present)
        {
            printf("\t\t%-40s=%20s\n", "T-Reselection GERAN SF Medium", liblte_rrc_sssf_medium_text[sib7->t_resel_geran_sf.sf_medium]);
            printf("\t\t%-40s=%20s\n", "T-Reselection GERAN SF High", liblte_rrc_sssf_high_text[sib7->t_resel_geran_sf.sf_high]);
        }
        if(0 != sib7->carrier_freqs_info_list_size)
        {
            printf("\t\tList of neighboring GERAN carrier frequencies\n");
        }
        for(i=0; i<sib7->carrier_freqs_info_list_size; i++)
        {
            printf("\t\t\t%-40s=%20u\n", "Starting ARFCN", sib7->carrier_freqs_info_list[i].carrier_freqs.starting_arfcn);
            printf("\t\t\t%-40s=%20s\n", "Band Indicator", liblte_rrc_band_indicator_geran_text[sib7->carrier_freqs_info_list[i].carrier_freqs.band_indicator]);
            if(LIBLTE_RRC_FOLLOWING_ARFCNS_EXPLICIT_LIST == sib7->carrier_freqs_info_list[i].carrier_freqs.following_arfcns)
            {
                printf("\t\t\tFollowing ARFCNs Explicit List\n");
                for(j=0; j<sib7->carrier_freqs_info_list[i].carrier_freqs.explicit_list_of_arfcns_size; j++)
                {
                    printf("\t\t\t\t%u\n", sib7->carrier_freqs_info_list[i].carrier_freqs.explicit_list_of_arfcns[j]);
                }
            }else if(LIBLTE_RRC_FOLLOWING_ARFCNS_EQUALLY_SPACED == sib7->carrier_freqs_info_list[i].carrier_freqs.following_arfcns){
                printf("\t\t\tFollowing ARFCNs Equally Spaced\n");
                printf("\t\t\t\t%u, %u\n", sib7->carrier_freqs_info_list[i].carrier_freqs.equally_spaced_arfcns.arfcn_spacing, sib7->carrier_freqs_info_list[i].carrier_freqs.equally_spaced_arfcns.number_of_arfcns);
            }else{
                printf("\t\t\tFollowing ARFCNs Variable Bit Map\n");
                printf("\t\t\t\t%02X\n", sib7->carrier_freqs_info_list[i].carrier_freqs.variable_bit_map_of_arfcns);
            }
            if(true == sib7->carrier_freqs_info_list[i].cell_resel_prio_present)
            {
                printf("\t\t\t%-40s=%20u\n", "Cell Reselection Priority", sib7->carrier_freqs_info_list[i].cell_resel_prio);
            }
            printf("\t\t\t%-40s=%20u\n", "NCC Permitted", sib7->carrier_freqs_info_list[i].ncc_permitted);
            printf("\t\t\t%-40s=%17ddBm\n", "Q Rx Lev Min", sib7->carrier_freqs_info_list[i].q_rx_lev_min);
            if(true == sib7->carrier_freqs_info_list[i].p_max_geran_present)
            {
                printf("\t\t\t%-40s=%17udBm\n", "P Max GERAN", sib7->carrier_freqs_info_list[i].p_max_geran);
            }
            printf("\t\t\t%-40s=%20u\n", "Threshold X High", sib7->carrier_freqs_info_list[i].threshx_high);
            printf("\t\t\t%-40s=%20u\n", "Threshold X Low", sib7->carrier_freqs_info_list[i].threshx_low);
        }

        sib7_printed = true;
    }
}

void LTE_fdd_dl_fs_samp_buf::print_sib8(LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_8_STRUCT *sib8)
{
    uint32 i;
    uint32 j;
    uint32 k;

    if(false == sib8_printed)
    {
        printf("\tSIB8 Decoded:\n");
        if(true == sib8->sys_time_info_present)
        {
            if(true == sib8->sys_time_info_cdma2000.cdma_eutra_sync)
            {
                printf("\t\t%-40s=%20s\n", "CDMA EUTRA sync", "True");
            }else{
                printf("\t\t%-40s=%20s\n", "CDMA EUTRA sync", "False");
            }
            if(true == sib8->sys_time_info_cdma2000.system_time_async)
            {
                printf("\t\t%-40s=%14llu chips\n", "System Time", sib8->sys_time_info_cdma2000.system_time * 8);
            }else{
                printf("\t\t%-40s=%17llu ms\n", "System Time", sib8->sys_time_info_cdma2000.system_time * 10);
            }
        }
        if(true == sib8->search_win_size_present)
        {
            printf("\t\t%-40s=%20u\n", "Search Window Size", sib8->search_win_size);
        }
        if(true == sib8->params_hrpd_present)
        {
            if(true == sib8->pre_reg_info_hrpd.pre_reg_allowed)
            {
                printf("\t\t%-40s=%20s\n", "Pre Registration", "Allowed");
            }else{
                printf("\t\t%-40s=%20s\n", "Pre Registration", "Not Allowed");
            }
            if(true == sib8->pre_reg_info_hrpd.pre_reg_zone_id_present)
            {
                printf("\t\t%-40s=%20u\n", "Pre Registration Zone ID", sib8->pre_reg_info_hrpd.pre_reg_zone_id);
            }
            if(0 != sib8->pre_reg_info_hrpd.secondary_pre_reg_zone_id_list_size)
            {
                printf("\t\tSecondary Pre Registration Zone IDs:\n");
            }
            for(i=0; i<sib8->pre_reg_info_hrpd.secondary_pre_reg_zone_id_list_size; i++)
            {
                printf("\t\t\t%u\n", sib8->pre_reg_info_hrpd.secondary_pre_reg_zone_id_list[i]);
            }
            if(true == sib8->cell_resel_params_hrpd_present)
            {
                printf("\t\tBand Class List:\n");
                for(i=0; i<sib8->cell_resel_params_hrpd.band_class_list_size; i++)
                {
                    printf("\t\t\t%-40s=%20s\n", "Band Class", liblte_rrc_band_class_cdma2000_text[sib8->cell_resel_params_hrpd.band_class_list[i].band_class]);
                    if(true == sib8->cell_resel_params_hrpd.band_class_list[i].cell_resel_prio_present)
                    {
                        printf("\t\t\t%-40s=%20u\n", "Cell Reselection Priority", sib8->cell_resel_params_hrpd.band_class_list[i].cell_resel_prio);
                    }
                    printf("\t\t\t%-40s=%20u\n", "Threshold X High", sib8->cell_resel_params_hrpd.band_class_list[i].thresh_x_high);
                    printf("\t\t\t%-40s=%20u\n", "Threshold X Low", sib8->cell_resel_params_hrpd.band_class_list[i].thresh_x_low);
                }
                printf("\t\tNeighbor Cell List:\n");
                for(i=0; i<sib8->cell_resel_params_hrpd.neigh_cell_list_size; i++)
                {
                    printf("\t\t\t%-40s=%20s\n", "Band Class", liblte_rrc_band_class_cdma2000_text[sib8->cell_resel_params_hrpd.neigh_cell_list[i].band_class]);
                    printf("\t\t\tNeighbor Cells Per Frequency List\n");
                    for(j=0; j<sib8->cell_resel_params_hrpd.neigh_cell_list[i].neigh_cells_per_freq_list_size; j++)
                    {
                        printf("\t\t\t\t%-40s=%20u\n", "ARFCN", sib8->cell_resel_params_hrpd.neigh_cell_list[i].neigh_cells_per_freq_list[j].arfcn);
                        printf("\t\t\t\tPhys Cell ID List\n");
                        for(k=0; k<sib8->cell_resel_params_hrpd.neigh_cell_list[i].neigh_cells_per_freq_list[j].phys_cell_id_list_size; k++)
                        {
                            printf("\t\t\t\t\t%u\n", sib8->cell_resel_params_hrpd.neigh_cell_list[i].neigh_cells_per_freq_list[j].phys_cell_id_list[k]);
                        }
                    }
                }
                printf("\t\t%-40s=%19us\n", "T Reselection", sib8->cell_resel_params_hrpd.t_resel_cdma2000);
                if(true == sib8->cell_resel_params_hrpd.t_resel_cdma2000_sf_present)
                {
                    printf("\t\t%-40s=%20s\n", "T-Reselection Scale Factor Medium", liblte_rrc_sssf_medium_text[sib8->cell_resel_params_hrpd.t_resel_cdma2000_sf.sf_medium]);
                    printf("\t\t%-40s=%20s\n", "T-Reselection Scale Factor High", liblte_rrc_sssf_high_text[sib8->cell_resel_params_hrpd.t_resel_cdma2000_sf.sf_high]);
                }
            }
        }
        if(true == sib8->params_1xrtt_present)
        {
            printf("\t\tCSFB Registration Parameters\n");
            if(true == sib8->csfb_reg_param_1xrtt_present)
            {
                printf("\t\t\t%-40s=%20u\n", "SID", sib8->csfb_reg_param_1xrtt.sid);
                printf("\t\t\t%-40s=%20u\n", "NID", sib8->csfb_reg_param_1xrtt.nid);
                if(true == sib8->csfb_reg_param_1xrtt.multiple_sid)
                {
                    printf("\t\t\t%-40s=%20s\n", "Multiple SIDs", "True");
                }else{
                    printf("\t\t\t%-40s=%20s\n", "Multiple SIDs", "False");
                }
                if(true == sib8->csfb_reg_param_1xrtt.multiple_nid)
                {
                    printf("\t\t\t%-40s=%20s\n", "Multiple NIDs", "True");
                }else{
                    printf("\t\t\t%-40s=%20s\n", "Multiple NIDs", "False");
                }
                if(true == sib8->csfb_reg_param_1xrtt.home_reg)
                {
                    printf("\t\t\t%-40s=%20s\n", "Home Reg", "True");
                }else{
                    printf("\t\t\t%-40s=%20s\n", "Home Reg", "False");
                }
                if(true == sib8->csfb_reg_param_1xrtt.foreign_sid_reg)
                {
                    printf("\t\t\t%-40s=%20s\n", "Foreign SID Reg", "True");
                }else{
                    printf("\t\t\t%-40s=%20s\n", "Foreign SID Reg", "False");
                }
                if(true == sib8->csfb_reg_param_1xrtt.foreign_nid_reg)
                {
                    printf("\t\t\t%-40s=%20s\n", "Foreign NID Reg", "True");
                }else{
                    printf("\t\t\t%-40s=%20s\n", "Foreign NID Reg", "False");
                }
                if(true == sib8->csfb_reg_param_1xrtt.param_reg)
                {
                    printf("\t\t\t%-40s=%20s\n", "Parameter Reg", "True");
                }else{
                    printf("\t\t\t%-40s=%20s\n", "Parameter Reg", "False");
                }
                if(true == sib8->csfb_reg_param_1xrtt.power_up_reg)
                {
                    printf("\t\t\t%-40s=%20s\n", "Power Up Reg", "True");
                }else{
                    printf("\t\t\t%-40s=%20s\n", "Power Up Reg", "False");
                }
                printf("\t\t\t%-40s=%20u\n", "Registration Period", sib8->csfb_reg_param_1xrtt.reg_period);
                printf("\t\t\t%-40s=%20u\n", "Registration Zone", sib8->csfb_reg_param_1xrtt.reg_zone);
                printf("\t\t\t%-40s=%20u\n", "Total Zones", sib8->csfb_reg_param_1xrtt.total_zone);
                printf("\t\t\t%-40s=%20u\n", "Zone Timer", sib8->csfb_reg_param_1xrtt.zone_timer);
            }
            if(true == sib8->long_code_state_1xrtt_present)
            {
                printf("\t\t%-40s=%20llu\n", "Long Code State", sib8->long_code_state_1xrtt);
            }
            if(true == sib8->cell_resel_params_1xrtt_present)
            {
                printf("\t\tBand Class List:\n");
                for(i=0; i<sib8->cell_resel_params_1xrtt.band_class_list_size; i++)
                {
                    printf("\t\t\t%-40s=%20s\n", "Band Class", liblte_rrc_band_class_cdma2000_text[sib8->cell_resel_params_1xrtt.band_class_list[i].band_class]);
                    if(true == sib8->cell_resel_params_1xrtt.band_class_list[i].cell_resel_prio_present)
                    {
                        printf("\t\t\t%-40s=%20u\n", "Cell Reselection Priority", sib8->cell_resel_params_1xrtt.band_class_list[i].cell_resel_prio);
                    }
                    printf("\t\t\t%-40s=%20u\n", "Threshold X High", sib8->cell_resel_params_1xrtt.band_class_list[i].thresh_x_high);
                    printf("\t\t\t%-40s=%20u\n", "Threshold X Low", sib8->cell_resel_params_1xrtt.band_class_list[i].thresh_x_low);
                }
                printf("\t\tNeighbor Cell List:\n");
                for(i=0; i<sib8->cell_resel_params_1xrtt.neigh_cell_list_size; i++)
                {
                    printf("\t\t\t%-40s=%20s\n", "Band Class", liblte_rrc_band_class_cdma2000_text[sib8->cell_resel_params_1xrtt.neigh_cell_list[i].band_class]);
                    printf("\t\t\tNeighbor Cells Per Frequency List\n");
                    for(j=0; j<sib8->cell_resel_params_1xrtt.neigh_cell_list[i].neigh_cells_per_freq_list_size; j++)
                    {
                        printf("\t\t\t\t%-40s=%20u\n", "ARFCN", sib8->cell_resel_params_1xrtt.neigh_cell_list[i].neigh_cells_per_freq_list[j].arfcn);
                        printf("\t\t\t\tPhys Cell ID List\n");
                        for(k=0; k<sib8->cell_resel_params_1xrtt.neigh_cell_list[i].neigh_cells_per_freq_list[j].phys_cell_id_list_size; k++)
                        {
                            printf("\t\t\t\t\t%u\n", sib8->cell_resel_params_1xrtt.neigh_cell_list[i].neigh_cells_per_freq_list[j].phys_cell_id_list[k]);
                        }
                    }
                }
                printf("\t\t%-40s=%19us\n", "T Reselection", sib8->cell_resel_params_1xrtt.t_resel_cdma2000);
                if(true == sib8->cell_resel_params_1xrtt.t_resel_cdma2000_sf_present)
                {
                    printf("\t\t%-40s=%20s\n", "T-Reselection Scale Factor Medium", liblte_rrc_sssf_medium_text[sib8->cell_resel_params_1xrtt.t_resel_cdma2000_sf.sf_medium]);
                    printf("\t\t%-40s=%20s\n", "T-Reselection Scale Factor High", liblte_rrc_sssf_high_text[sib8->cell_resel_params_1xrtt.t_resel_cdma2000_sf.sf_high]);
                }
            }
        }

        sib8_printed = true;
    }
}

void LTE_fdd_dl_fs_samp_buf::print_sib13(LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_13_STRUCT *sib13)
{
    uint32 i;

    if(false == sib13_printed)
    {
        printf("\tSIB13 Decoded:\n");

        printf("\t\tMBSFN Area Info List R9:\n");
        for(i=0; i<sib13->mbsfn_area_info_list_r9_size; i++)
        {
            printf("\t\t\t%-40s=%20u\n", "MBSFN Area ID R9", sib13->mbsfn_area_info_list_r9[i].mbsfn_area_id_r9);
            printf("\t\t\t%-40s=%20s\n", "Non-MBSFN Region Length", liblte_rrc_non_mbsfn_region_length_text[sib13->mbsfn_area_info_list_r9[i].non_mbsfn_region_length]);
            printf("\t\t\t%-40s=%20u\n", "Notification Indicator R9", sib13->mbsfn_area_info_list_r9[i].notification_indicator_r9);
            printf("\t\t\t%-40s=%20s\n", "MCCH Repetition Period R9", liblte_rrc_mcch_repetition_period_r9_text[sib13->mbsfn_area_info_list_r9[i].mcch_repetition_period_r9]);
            printf("\t\t\t%-40s=%20u\n", "MCCH Offset R9", sib13->mbsfn_area_info_list_r9[i].mcch_offset_r9);
            printf("\t\t\t%-40s=%20s\n", "MCCH Modification Period R9", liblte_rrc_mcch_modification_period_r9_text[sib13->mbsfn_area_info_list_r9[i].mcch_modification_period_r9]);
            printf("\t\t\t%-40s=%20u\n", "SF Alloc Info R9", sib13->mbsfn_area_info_list_r9[i].sf_alloc_info_r9);
            printf("\t\t\t%-40s=%20s\n", "Signalling MCS R9", liblte_rrc_mcch_signalling_mcs_r9_text[sib13->mbsfn_area_info_list_r9[i].signalling_mcs_r9]);
        }

        printf("\t\t%-40s=%20s\n", "Repetition Coeff", liblte_rrc_notification_repetition_coeff_r9_text[sib13->mbms_notification_config.repetition_coeff]);
        printf("\t\t%-40s=%20u\n", "Offset", sib13->mbms_notification_config.offset);
        printf("\t\t%-40s=%20u\n", "SF Index", sib13->mbms_notification_config.sf_index);

        sib13_printed = true;
    }
}

void LTE_fdd_dl_fs_samp_buf::print_page(LIBLTE_RRC_PAGING_STRUCT *page)
{
    uint32 i;
    uint32 j;

    printf("\tPAGE Decoded:\n");
    if(0 != page->paging_record_list_size)
    {
        printf("\t\tNumber of paging records: %u\n", page->paging_record_list_size);
        for(i=0; i<page->paging_record_list_size; i++)
        {
            if(LIBLTE_RRC_PAGING_UE_IDENTITY_TYPE_S_TMSI == page->paging_record_list[i].ue_identity.ue_identity_type)
            {
                printf("\t\t\t%s\n", "S-TMSI");
                printf("\t\t\t\t%-40s= %08X\n", "M-TMSI", page->paging_record_list[i].ue_identity.s_tmsi.m_tmsi);
                printf("\t\t\t\t%-40s= %u\n", "MMEC", page->paging_record_list[i].ue_identity.s_tmsi.mmec);
            }else{
                printf("\t\t\t%-40s=", "IMSI");
                for(j=0; j<page->paging_record_list[i].ue_identity.imsi_size; j++)
                {
                    printf("%u", page->paging_record_list[i].ue_identity.imsi[j]);
                }
                printf("\n");
            }
            printf("\t\t\t%-40s=%20s\n", "CN Domain", liblte_rrc_cn_domain_text[page->paging_record_list[i].cn_domain]);
        }
    }
    if(true == page->system_info_modification_present)
    {
        printf("\t\t%-40s=%20s\n", "System Info Modification", liblte_rrc_system_info_modification_text[page->system_info_modification]);
    }
    if(true == page->etws_indication_present)
    {
        printf("\t\t%-40s=%20s\n", "ETWS Indication", liblte_rrc_etws_indication_text[page->etws_indication]);
    }
}

void LTE_fdd_dl_fs_samp_buf::print_config(void)
{
    uint32 i;

    printf("***System Configuration Parameters***\n");
    printf("\tType 'help' to reprint this menu\n");
    printf("\tHit enter to finish config and scan file\n");
    printf("\tSet parameters using <param>=<value> format\n");

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
}

void LTE_fdd_dl_fs_samp_buf::change_config(char *line)
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
            if(!strcasecmp(param, FS_PARAM))
            {
                err = set_fs(value);
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

bool LTE_fdd_dl_fs_samp_buf::set_fs(char *char_value)
{
    bool err = false;

    if(!strcasecmp(char_value, "30.72"))
    {
        fs = LIBLTE_PHY_FS_30_72MHZ;
    }else if(!strcasecmp(char_value, "15.36")){
        fs = LIBLTE_PHY_FS_15_36MHZ;
    }else if(!strcasecmp(char_value, "7.68")){
        fs = LIBLTE_PHY_FS_7_68MHZ;
    }else if(!strcasecmp(char_value, "3.84")){
        fs = LIBLTE_PHY_FS_3_84MHZ;
    }else if(!strcasecmp(char_value, "1.92")){
        fs = LIBLTE_PHY_FS_1_92MHZ;
    }else{
        err = true;
    }

    return(err);
}
