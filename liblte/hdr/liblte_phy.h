/*******************************************************************************

    Copyright 2012-2017 Ben Wojtowicz

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

    File: liblte_phy.h

    Description: Contains all the definitions for the LTE Physical Layer
                 library.

    Revision History
    ----------    -------------    --------------------------------------------
    02/26/2012    Ben Wojtowicz    Created file.
    04/21/2012    Ben Wojtowicz    Added Turbo encode/decode and PDSCH decode
    05/28/2012    Ben Wojtowicz    Rearranged to match spec order, added
                                   initial transmit functionality, and fixed
                                   a bug related to PDCCH sizes.
    06/11/2012    Ben Wojtowicz    Enabled fftw input, output, and plan in
                                   phy_struct.
    10/06/2012    Ben Wojtowicz    Added random access and paging PDCCH
                                   decoding, soft turbo decoding, and PDSCH
                                   decoding per allocation.
    11/10/2012    Ben Wojtowicz    Added TBS, MCS, and N_prb calculations for
                                   SI PDSCH messages, added more sample defines,
                                   and re-factored the coarse timing and freq
                                   search to find more than 1 eNB.
    12/01/2012    Ben Wojtowicz    Added ability to preconfigure CRS.
    12/26/2012    Ben Wojtowicz    Started supporting N_sc_rb for normal and
                                   extended CP and fixed several FIXMEs.
    03/03/2013    Ben Wojtowicz    Added support for multiple sampling rates,
                                   pre-permutation of PDCCH encoding, and
                                   integer frequency offset detection.
    03/17/2013    Ben Wojtowicz    Moved to single float version of fftw.
    07/21/2013    Ben Wojtowicz    Added routines for determining TBS, MCS,
                                   N_prb, and N_cce.
    08/26/2013    Ben Wojtowicz    Added PRACH generation and detection support
                                   and changed ambiguous routines/variables to
                                   be non-ambiguous.
    09/16/2013    Ben Wojtowicz    Implemented routines for determine TBS, MCS,
                                   N_prb, and N_cce.
    09/28/2013    Ben Wojtowicz    Reordered sample rate enum and added a text
                                   version.
    11/13/2013    Ben Wojtowicz    Started adding PUSCH functionality.
    03/26/2014    Ben Wojtowicz    Added PUSCH functionality.
    04/12/2014    Ben Wojtowicz    Added support for PRB allocation differences
                                   in each slot.
    05/04/2014    Ben Wojtowicz    Added PHICH and TPC support.
    06/15/2014    Ben Wojtowicz    Added TPC values for DCI 0, 3, and 4.
    07/14/2015    Ben Wojtowicz    Added a constant definition of Fs as an
                                   integer.
    12/06/2015    Ben Wojtowicz    Added defines for many of the array sizes in
                                   LIBLTE_PHY_STRUCT and corrected the order of
                                   sizes in the rate match/unmatch arrays in
                                   LIBLTE_PHY_STRUCT (thanks to Ziming He for
                                   finding this).
    02/13/2016    Ben Wojtowicz    Moved turbo coder rate match/unmatch and
                                   code block segmentation/desegmentation to
                                   globally available routines to support unit
                                   tests.
    03/12/2016    Ben Wojtowicz    Added PUCCH channel decode support.
    07/03/2016    Ben Wojtowicz    Added PDCCH size defines.
    07/31/2016    Ben Wojtowicz    Added harq_retx_count to allocation struct.
    07/29/2017    Ben Wojtowicz    Added two codeword support, refactored PUCCH
                                   channel decoding for PUCCH types 1, 1A, and
                                   1B, and added a function to map the SR
                                   configuration index.

*******************************************************************************/

#ifndef __LIBLTE_PHY_H__
#define __LIBLTE_PHY_H__

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "liblte_common.h"
#include "liblte_rrc.h"
#include "fftw3.h"

/*******************************************************************************
                              DEFINES
*******************************************************************************/

// FFT sizes
#define LIBLTE_PHY_FFT_SIZE_1_92MHZ  128
#define LIBLTE_PHY_FFT_SIZE_3_84MHZ  256
#define LIBLTE_PHY_FFT_SIZE_7_68MHZ  512
#define LIBLTE_PHY_FFT_SIZE_15_36MHZ 1024
#define LIBLTE_PHY_FFT_SIZE_30_72MHZ 2048

// N_rb_dl
#define LIBLTE_PHY_N_RB_DL_1_4MHZ 6
#define LIBLTE_PHY_N_RB_DL_3MHZ   15
#define LIBLTE_PHY_N_RB_DL_5MHZ   25
#define LIBLTE_PHY_N_RB_DL_10MHZ  50
#define LIBLTE_PHY_N_RB_DL_15MHZ  75
#define LIBLTE_PHY_N_RB_DL_20MHZ  100
#define LIBLTE_PHY_N_RB_DL_MAX    110

// N_rb_ul
#define LIBLTE_PHY_N_RB_UL_1_4MHZ 6
#define LIBLTE_PHY_N_RB_UL_3MHZ   15
#define LIBLTE_PHY_N_RB_UL_5MHZ   25
#define LIBLTE_PHY_N_RB_UL_10MHZ  50
#define LIBLTE_PHY_N_RB_UL_15MHZ  75
#define LIBLTE_PHY_N_RB_UL_20MHZ  100
#define LIBLTE_PHY_N_RB_UL_MAX    110

// N_sc_rb
#define LIBLTE_PHY_N_SC_RB_UL           12
#define LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP 12
// FIXME: Add Extended CP

// M_pucch_rs
#define LIBLTE_PHY_M_PUCCH_RS 3
// FIXME: Add Extended CP

// N_ant
#define LIBLTE_PHY_N_ANT_MAX 4

// Symbol, CP, Slot, Subframe, and Frame timing
// Generic
#define LIBLTE_PHY_SFN_MAX           1023
#define LIBLTE_PHY_N_SLOTS_PER_SUBFR 2
#define LIBLTE_PHY_N_SUBFR_PER_FRAME 10
// 20MHz and 15MHz bandwidths
#define LIBLTE_PHY_N_SAMPS_PER_SYMB_30_72MHZ  2048
#define LIBLTE_PHY_N_SAMPS_CP_L_0_30_72MHZ    160
#define LIBLTE_PHY_N_SAMPS_CP_L_ELSE_30_72MHZ 144
#define LIBLTE_PHY_N_SAMPS_PER_SLOT_30_72MHZ  15360
#define LIBLTE_PHY_N_SAMPS_PER_SUBFR_30_72MHZ (LIBLTE_PHY_N_SAMPS_PER_SLOT_30_72MHZ*LIBLTE_PHY_N_SLOTS_PER_SUBFR)
#define LIBLTE_PHY_N_SAMPS_PER_FRAME_30_72MHZ (LIBLTE_PHY_N_SAMPS_PER_SUBFR_30_72MHZ*LIBLTE_PHY_N_SUBFR_PER_FRAME)
// 10MHz bandwidth
#define LIBLTE_PHY_N_SAMPS_PER_SYMB_15_36MHZ  1024
#define LIBLTE_PHY_N_SAMPS_CP_L_0_15_36MHZ    80
#define LIBLTE_PHY_N_SAMPS_CP_L_ELSE_15_36MHZ 72
#define LIBLTE_PHY_N_SAMPS_PER_SLOT_15_36MHZ  7680
#define LIBLTE_PHY_N_SAMPS_PER_SUBFR_15_36MHZ (LIBLTE_PHY_N_SAMPS_PER_SLOT_15_36MHZ*LIBLTE_PHY_N_SLOTS_PER_SUBFR)
#define LIBLTE_PHY_N_SAMPS_PER_FRAME_15_36MHZ (LIBLTE_PHY_N_SAMPS_PER_SUBFR_15_36MHZ*LIBLTE_PHY_N_SUBFR_PER_FRAME)
// 5MHz bandwidth
#define LIBLTE_PHY_N_SAMPS_PER_SYMB_7_68MHZ  512
#define LIBLTE_PHY_N_SAMPS_CP_L_0_7_68MHZ    40
#define LIBLTE_PHY_N_SAMPS_CP_L_ELSE_7_68MHZ 36
#define LIBLTE_PHY_N_SAMPS_PER_SLOT_7_68MHZ  3840
#define LIBLTE_PHY_N_SAMPS_PER_SUBFR_7_68MHZ (LIBLTE_PHY_N_SAMPS_PER_SLOT_7_68MHZ*LIBLTE_PHY_N_SLOTS_PER_SUBFR)
#define LIBLTE_PHY_N_SAMPS_PER_FRAME_7_68MHZ (LIBLTE_PHY_N_SAMPS_PER_SUBFR_7_68MHZ*LIBLTE_PHY_N_SUBFR_PER_FRAME)
// 3MHz bandwidth
#define LIBLTE_PHY_N_SAMPS_PER_SYMB_3_84MHZ  256
#define LIBLTE_PHY_N_SAMPS_CP_L_0_3_84MHZ    20
#define LIBLTE_PHY_N_SAMPS_CP_L_ELSE_3_84MHZ 18
#define LIBLTE_PHY_N_SAMPS_PER_SLOT_3_84MHZ  1920
#define LIBLTE_PHY_N_SAMPS_PER_SUBFR_3_84MHZ (LIBLTE_PHY_N_SAMPS_PER_SLOT_3_84MHZ*LIBLTE_PHY_N_SLOTS_PER_SUBFR)
#define LIBLTE_PHY_N_SAMPS_PER_FRAME_3_84MHZ (LIBLTE_PHY_N_SAMPS_PER_SUBFR_3_84MHZ*LIBLTE_PHY_N_SUBFR_PER_FRAME)
// 1.4MHz bandwidth
#define LIBLTE_PHY_N_SAMPS_PER_SYMB_1_92MHZ  128
#define LIBLTE_PHY_N_SAMPS_CP_L_0_1_92MHZ    10
#define LIBLTE_PHY_N_SAMPS_CP_L_ELSE_1_92MHZ 9
#define LIBLTE_PHY_N_SAMPS_PER_SLOT_1_92MHZ  960
#define LIBLTE_PHY_N_SAMPS_PER_SUBFR_1_92MHZ (LIBLTE_PHY_N_SAMPS_PER_SLOT_1_92MHZ*LIBLTE_PHY_N_SLOTS_PER_SUBFR)
#define LIBLTE_PHY_N_SAMPS_PER_FRAME_1_92MHZ (LIBLTE_PHY_N_SAMPS_PER_SUBFR_1_92MHZ*LIBLTE_PHY_N_SUBFR_PER_FRAME)

// Turbo/Viterbi coding
#define LIBLTE_PHY_MAX_CODE_BLOCK_SIZE 6176
#define LIBLTE_PHY_MAX_VITERBI_STATES  128
#define LIBLTE_PHY_BASE_CODING_RATE    3

// Rate matching
#define LIBLTE_PHY_N_COLUMNS_RATE_MATCH 32

// PDCCH
#define LIBLTE_PHY_PDCCH_N_REGS_MAX 787
#define LIBLTE_PHY_PDCCH_N_REG_CCE  9
#define LIBLTE_PHY_PDCCH_N_RE_CCE   (LIBLTE_PHY_PDCCH_N_REG_CCE * 4)
#define LIBLTE_PHY_PDCCH_N_CCE_MAX  (LIBLTE_PHY_PDCCH_N_REGS_MAX / LIBLTE_PHY_PDCCH_N_REG_CCE)
#define LIBLTE_PHY_PDCCH_N_BITS_MAX 576

/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/

typedef enum{
    LIBLTE_PHY_FS_1_92MHZ = 0, // 1.4MHz bandwidth
    LIBLTE_PHY_FS_3_84MHZ,     // 3MHz bandwidth
    LIBLTE_PHY_FS_7_68MHZ,     // 5MHz bandwidth
    LIBLTE_PHY_FS_15_36MHZ,    // 10MHz bandwidth
    LIBLTE_PHY_FS_30_72MHZ,    // 20MHz and 15MHz bandwidths
    LIBLTE_PHY_FS_N_ITEMS,
}LIBLTE_PHY_FS_ENUM;
static const char liblte_phy_fs_text[LIBLTE_PHY_FS_N_ITEMS][20] = {"1.92", "3.84", "7.68", "15.36", "30.72"};
static const uint32 liblte_phy_fs_num[LIBLTE_PHY_FS_N_ITEMS] = {1920000, 3840000, 7680000, 15360000, 30720000};

typedef enum{
    LIBLTE_PHY_PRE_CODER_TYPE_TX_DIVERSITY = 0,
    LIBLTE_PHY_PRE_CODER_TYPE_SPATIAL_MULTIPLEXING,
}LIBLTE_PHY_PRE_CODER_TYPE_ENUM;

typedef enum{
    LIBLTE_PHY_MODULATION_TYPE_BPSK = 0,
    LIBLTE_PHY_MODULATION_TYPE_QPSK,
    LIBLTE_PHY_MODULATION_TYPE_16QAM,
    LIBLTE_PHY_MODULATION_TYPE_64QAM,
}LIBLTE_PHY_MODULATION_TYPE_ENUM;

typedef enum{
    LIBLTE_PHY_CHAN_TYPE_DLSCH = 0,
    LIBLTE_PHY_CHAN_TYPE_PCH,
    LIBLTE_PHY_CHAN_TYPE_ULSCH,
    LIBLTE_PHY_CHAN_TYPE_ULCCH,
}LIBLTE_PHY_CHAN_TYPE_ENUM;

typedef struct{
    // Receive
    float rx_symb_re[16][LIBLTE_PHY_N_RB_DL_20MHZ*LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP];
    float rx_symb_im[16][LIBLTE_PHY_N_RB_DL_20MHZ*LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP];
    float rx_ce_re[LIBLTE_PHY_N_ANT_MAX][16][LIBLTE_PHY_N_RB_DL_20MHZ*LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP];
    float rx_ce_im[LIBLTE_PHY_N_ANT_MAX][16][LIBLTE_PHY_N_RB_DL_20MHZ*LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP];

    // Transmit
    float tx_symb_re[LIBLTE_PHY_N_ANT_MAX][16][LIBLTE_PHY_N_RB_DL_20MHZ*LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP];
    float tx_symb_im[LIBLTE_PHY_N_ANT_MAX][16][LIBLTE_PHY_N_RB_DL_20MHZ*LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP];

    // Common
    uint32 num;
}LIBLTE_PHY_SUBFRAME_STRUCT;

/*******************************************************************************
                              DECLARATIONS
*******************************************************************************/

/*********************************************************************
    Name: liblte_phy_init

    Description: Initializes the LTE Physical Layer library.

    Document Reference: N/A
*********************************************************************/
// Defines
#define LIBLTE_PHY_INIT_N_ID_CELL_UNKNOWN 0xFFFF
// Enums
// Structs
typedef struct{
    // PUSCH
    fftwf_complex *transform_precoding_in;
    fftwf_complex *transform_precoding_out;
    fftwf_plan     transform_precoding_plan[LIBLTE_PHY_N_RB_UL_MAX];
    fftwf_plan     transform_pre_decoding_plan[LIBLTE_PHY_N_RB_UL_MAX];
    float          pusch_z_est_re[14400];
    float          pusch_z_est_im[14400];
    float          pusch_c_est_0_re[LIBLTE_PHY_N_RB_UL_MAX*LIBLTE_PHY_N_SC_RB_UL];
    float          pusch_c_est_0_im[LIBLTE_PHY_N_RB_UL_MAX*LIBLTE_PHY_N_SC_RB_UL];
    float          pusch_c_est_1_re[LIBLTE_PHY_N_RB_UL_MAX*LIBLTE_PHY_N_SC_RB_UL];
    float          pusch_c_est_1_im[LIBLTE_PHY_N_RB_UL_MAX*LIBLTE_PHY_N_SC_RB_UL];
    float          pusch_c_est_re[14400];
    float          pusch_c_est_im[14400];
    float          pusch_z_re[LIBLTE_PHY_N_ANT_MAX][14400];
    float          pusch_z_im[LIBLTE_PHY_N_ANT_MAX][14400];
    float          pusch_y_re[14400];
    float          pusch_y_im[14400];
    float          pusch_x_re[14400];
    float          pusch_x_im[14400];
    float          pusch_d_re[14400];
    float          pusch_d_im[14400];
    float          pusch_descramb_bits[28800];
    uint32         pusch_c[28800];
    uint8          pusch_encode_bits[28800];
    uint8          pusch_scramb_bits[28800];
    int8           pusch_soft_bits[28800];

    // PUCCH
    float pucch_z_est_re[LIBLTE_PHY_N_SC_RB_UL*14];
    float pucch_z_est_im[LIBLTE_PHY_N_SC_RB_UL*14];
    float pucch_c_est_0_re[LIBLTE_PHY_M_PUCCH_RS*LIBLTE_PHY_N_SC_RB_UL];
    float pucch_c_est_0_im[LIBLTE_PHY_M_PUCCH_RS*LIBLTE_PHY_N_SC_RB_UL];
    float pucch_c_est_1_re[LIBLTE_PHY_M_PUCCH_RS*LIBLTE_PHY_N_SC_RB_UL];
    float pucch_c_est_1_im[LIBLTE_PHY_M_PUCCH_RS*LIBLTE_PHY_N_SC_RB_UL];
    float pucch_c_est_re[LIBLTE_PHY_N_SC_RB_UL*14];
    float pucch_c_est_im[LIBLTE_PHY_N_SC_RB_UL*14];
    float pucch_z_re[LIBLTE_PHY_N_ANT_MAX][LIBLTE_PHY_N_SC_RB_UL*14];
    float pucch_z_im[LIBLTE_PHY_N_ANT_MAX][LIBLTE_PHY_N_SC_RB_UL*14];

    // UL Reference Signals
    float  ulrs_x_q_re[2048];
    float  ulrs_x_q_im[2048];
    float  ulrs_r_bar_u_v_re[2048];
    float  ulrs_r_bar_u_v_im[2048];
    uint32 ulrs_c[160];

    // DMRS
    float  pusch_dmrs_0_re[LIBLTE_PHY_N_SUBFR_PER_FRAME][LIBLTE_PHY_N_RB_UL_MAX][LIBLTE_PHY_N_RB_UL_MAX*LIBLTE_PHY_N_SC_RB_UL];
    float  pusch_dmrs_0_im[LIBLTE_PHY_N_SUBFR_PER_FRAME][LIBLTE_PHY_N_RB_UL_MAX][LIBLTE_PHY_N_RB_UL_MAX*LIBLTE_PHY_N_SC_RB_UL];
    float  pusch_dmrs_1_re[LIBLTE_PHY_N_SUBFR_PER_FRAME][LIBLTE_PHY_N_RB_UL_MAX][LIBLTE_PHY_N_RB_UL_MAX*LIBLTE_PHY_N_SC_RB_UL];
    float  pusch_dmrs_1_im[LIBLTE_PHY_N_SUBFR_PER_FRAME][LIBLTE_PHY_N_RB_UL_MAX][LIBLTE_PHY_N_RB_UL_MAX*LIBLTE_PHY_N_SC_RB_UL];
    float  pucch_dmrs_0_re[LIBLTE_PHY_N_SUBFR_PER_FRAME][LIBLTE_PHY_N_RB_UL_MAX/2][LIBLTE_PHY_M_PUCCH_RS*LIBLTE_PHY_N_SC_RB_UL];
    float  pucch_dmrs_0_im[LIBLTE_PHY_N_SUBFR_PER_FRAME][LIBLTE_PHY_N_RB_UL_MAX/2][LIBLTE_PHY_M_PUCCH_RS*LIBLTE_PHY_N_SC_RB_UL];
    float  pucch_dmrs_1_re[LIBLTE_PHY_N_SUBFR_PER_FRAME][LIBLTE_PHY_N_RB_UL_MAX/2][LIBLTE_PHY_M_PUCCH_RS*LIBLTE_PHY_N_SC_RB_UL];
    float  pucch_dmrs_1_im[LIBLTE_PHY_N_SUBFR_PER_FRAME][LIBLTE_PHY_N_RB_UL_MAX/2][LIBLTE_PHY_M_PUCCH_RS*LIBLTE_PHY_N_SC_RB_UL];
    float  pucch_r_u_v_alpha_p_re[LIBLTE_PHY_N_SUBFR_PER_FRAME][LIBLTE_PHY_N_RB_UL_MAX/2][2][7][LIBLTE_PHY_N_SC_RB_UL];
    float  pucch_r_u_v_alpha_p_im[LIBLTE_PHY_N_SUBFR_PER_FRAME][LIBLTE_PHY_N_RB_UL_MAX/2][2][7][LIBLTE_PHY_N_SC_RB_UL];
    uint32 pusch_dmrs_c[1120];
    uint32 pucch_dmrs_c[1120];
    uint32 pucch_n_prime_p[LIBLTE_PHY_N_SUBFR_PER_FRAME][LIBLTE_PHY_N_RB_UL_MAX/2][2];
    uint32 pucch_n_oc_p[LIBLTE_PHY_N_SUBFR_PER_FRAME][LIBLTE_PHY_N_RB_UL_MAX/2][2];

    // PRACH
    fftwf_complex *prach_dft_in;
    fftwf_complex *prach_dft_out;
    fftwf_complex *prach_fft_in;
    fftwf_complex *prach_fft_out;
    fftwf_plan     prach_dft_plan;
    fftwf_plan     prach_ifft_plan;
    fftwf_plan     prach_fft_plan;
    fftwf_plan     prach_idft_plan;
    float          prach_x_u_v_re[64][839];
    float          prach_x_u_v_im[64][839];
    float          prach_x_u_re[64][839];
    float          prach_x_u_im[64][839];
    float          prach_x_u_fft_re[64][839];
    float          prach_x_u_fft_im[64][839];
    float          prach_x_hat_re[839];
    float          prach_x_hat_im[839];
    uint32         prach_zczc;
    uint32         prach_preamble_format;
    uint32         prach_root_seq_idx;
    uint32         prach_N_x_u;
    uint32         prach_N_zc;
    uint32         prach_T_fft;
    uint32         prach_T_seq;
    uint32         prach_T_cp;
    uint32         prach_delta_f_RA;
    uint32         prach_phi;
    bool           prach_hs_flag;

    // PDSCH
    float  pdsch_y_est_re[5000];
    float  pdsch_y_est_im[5000];
    float  pdsch_c_est_re[LIBLTE_PHY_N_ANT_MAX][5000];
    float  pdsch_c_est_im[LIBLTE_PHY_N_ANT_MAX][5000];
    float  pdsch_y_re[LIBLTE_PHY_N_ANT_MAX][5000];
    float  pdsch_y_im[LIBLTE_PHY_N_ANT_MAX][5000];
    float  pdsch_x_re[10000];
    float  pdsch_x_im[10000];
    float  pdsch_d_re[10000];
    float  pdsch_d_im[10000];
    float  pdsch_descramb_bits[10000];
    uint32 pdsch_c[10000];
    uint8  pdsch_encode_bits[10000];
    uint8  pdsch_scramb_bits[10000];
    int8   pdsch_soft_bits[10000];

    // BCH
    float  bch_y_est_re[240];
    float  bch_y_est_im[240];
    float  bch_c_est_re[LIBLTE_PHY_N_ANT_MAX][240];
    float  bch_c_est_im[LIBLTE_PHY_N_ANT_MAX][240];
    float  bch_y_re[LIBLTE_PHY_N_ANT_MAX][240];
    float  bch_y_im[LIBLTE_PHY_N_ANT_MAX][240];
    float  bch_x_re[480];
    float  bch_x_im[480];
    float  bch_d_re[480];
    float  bch_d_im[480];
    float  bch_descramb_bits[1920];
    float  bch_rx_d_bits[1920];
    uint32 bch_c[1920];
    uint32 bch_N_bits;
    uint8  bch_tx_d_bits[1920];
    uint8  bch_c_bits[40];
    uint8  bch_encode_bits[1920];
    uint8  bch_scramb_bits[480];
    int8   bch_soft_bits[480];

    // PDCCH
    float  pdcch_reg_y_est_re[LIBLTE_PHY_PDCCH_N_REGS_MAX][4];
    float  pdcch_reg_y_est_im[LIBLTE_PHY_PDCCH_N_REGS_MAX][4];
    float  pdcch_reg_c_est_re[LIBLTE_PHY_N_ANT_MAX][LIBLTE_PHY_PDCCH_N_REGS_MAX][4];
    float  pdcch_reg_c_est_im[LIBLTE_PHY_N_ANT_MAX][LIBLTE_PHY_PDCCH_N_REGS_MAX][4];
    float  pdcch_shift_y_est_re[LIBLTE_PHY_PDCCH_N_REGS_MAX][4];
    float  pdcch_shift_y_est_im[LIBLTE_PHY_PDCCH_N_REGS_MAX][4];
    float  pdcch_shift_c_est_re[LIBLTE_PHY_N_ANT_MAX][LIBLTE_PHY_PDCCH_N_REGS_MAX][4];
    float  pdcch_shift_c_est_im[LIBLTE_PHY_N_ANT_MAX][LIBLTE_PHY_PDCCH_N_REGS_MAX][4];
    float  pdcch_perm_y_est_re[LIBLTE_PHY_PDCCH_N_REGS_MAX][4];
    float  pdcch_perm_y_est_im[LIBLTE_PHY_PDCCH_N_REGS_MAX][4];
    float  pdcch_perm_c_est_re[LIBLTE_PHY_N_ANT_MAX][LIBLTE_PHY_PDCCH_N_REGS_MAX][4];
    float  pdcch_perm_c_est_im[LIBLTE_PHY_N_ANT_MAX][LIBLTE_PHY_PDCCH_N_REGS_MAX][4];
    float  pdcch_cce_y_est_re[LIBLTE_PHY_PDCCH_N_CCE_MAX][LIBLTE_PHY_PDCCH_N_RE_CCE];
    float  pdcch_cce_y_est_im[LIBLTE_PHY_PDCCH_N_CCE_MAX][LIBLTE_PHY_PDCCH_N_RE_CCE];
    float  pdcch_cce_c_est_re[LIBLTE_PHY_N_ANT_MAX][LIBLTE_PHY_PDCCH_N_CCE_MAX][LIBLTE_PHY_PDCCH_N_RE_CCE];
    float  pdcch_cce_c_est_im[LIBLTE_PHY_N_ANT_MAX][LIBLTE_PHY_PDCCH_N_CCE_MAX][LIBLTE_PHY_PDCCH_N_RE_CCE];
    float  pdcch_y_est_re[LIBLTE_PHY_PDCCH_N_BITS_MAX / 2];
    float  pdcch_y_est_im[LIBLTE_PHY_PDCCH_N_BITS_MAX / 2];
    float  pdcch_c_est_re[LIBLTE_PHY_N_ANT_MAX][LIBLTE_PHY_PDCCH_N_BITS_MAX / 2];
    float  pdcch_c_est_im[LIBLTE_PHY_N_ANT_MAX][LIBLTE_PHY_PDCCH_N_BITS_MAX / 2];
    float  pdcch_y_re[LIBLTE_PHY_N_ANT_MAX][LIBLTE_PHY_PDCCH_N_BITS_MAX / 2];
    float  pdcch_y_im[LIBLTE_PHY_N_ANT_MAX][LIBLTE_PHY_PDCCH_N_BITS_MAX / 2];
    float  pdcch_cce_re[LIBLTE_PHY_N_ANT_MAX][LIBLTE_PHY_PDCCH_N_CCE_MAX][LIBLTE_PHY_PDCCH_N_RE_CCE];
    float  pdcch_cce_im[LIBLTE_PHY_N_ANT_MAX][LIBLTE_PHY_PDCCH_N_CCE_MAX][LIBLTE_PHY_PDCCH_N_RE_CCE];
    float  pdcch_reg_re[LIBLTE_PHY_N_ANT_MAX][LIBLTE_PHY_PDCCH_N_REGS_MAX][4];
    float  pdcch_reg_im[LIBLTE_PHY_N_ANT_MAX][LIBLTE_PHY_PDCCH_N_REGS_MAX][4];
    float  pdcch_perm_re[LIBLTE_PHY_N_ANT_MAX][LIBLTE_PHY_PDCCH_N_REGS_MAX][4];
    float  pdcch_perm_im[LIBLTE_PHY_N_ANT_MAX][LIBLTE_PHY_PDCCH_N_REGS_MAX][4];
    float  pdcch_shift_re[LIBLTE_PHY_N_ANT_MAX][LIBLTE_PHY_PDCCH_N_REGS_MAX][4];
    float  pdcch_shift_im[LIBLTE_PHY_N_ANT_MAX][LIBLTE_PHY_PDCCH_N_REGS_MAX][4];
    float  pdcch_x_re[LIBLTE_PHY_PDCCH_N_BITS_MAX / 2];
    float  pdcch_x_im[LIBLTE_PHY_PDCCH_N_BITS_MAX / 2];
    float  pdcch_d_re[LIBLTE_PHY_PDCCH_N_BITS_MAX / 2];
    float  pdcch_d_im[LIBLTE_PHY_PDCCH_N_BITS_MAX / 2];
    float  pdcch_descramb_bits[LIBLTE_PHY_PDCCH_N_BITS_MAX];
    uint32 pdcch_c[LIBLTE_PHY_PDCCH_N_BITS_MAX * 2];
    uint32 pdcch_permute_map[LIBLTE_PHY_PDCCH_N_REGS_MAX][LIBLTE_PHY_PDCCH_N_REGS_MAX];
    uint16 pdcch_reg_vec[LIBLTE_PHY_PDCCH_N_REGS_MAX];
    uint16 pdcch_reg_perm_vec[LIBLTE_PHY_PDCCH_N_REGS_MAX];
    uint8  pdcch_dci[LIBLTE_PHY_PDCCH_N_BITS_MAX];
    uint8  pdcch_encode_bits[LIBLTE_PHY_PDCCH_N_BITS_MAX];
    uint8  pdcch_scramb_bits[LIBLTE_PHY_PDCCH_N_BITS_MAX];
    int8   pdcch_soft_bits[LIBLTE_PHY_PDCCH_N_BITS_MAX];
    bool   pdcch_cce_used[LIBLTE_PHY_PDCCH_N_CCE_MAX];

    // PHICH
    uint32 N_group_phich;
    uint32 N_sf_phich;

    // CRS & Channel Estimate
    float crs_re[14][LIBLTE_PHY_N_RB_DL_20MHZ*LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP];
    float crs_im[14][LIBLTE_PHY_N_RB_DL_20MHZ*LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP];
    float dl_ce_crs_re[16][LIBLTE_PHY_N_RB_DL_20MHZ*LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP];
    float dl_ce_crs_im[16][LIBLTE_PHY_N_RB_DL_20MHZ*LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP];
    float dl_ce_mag[5][LIBLTE_PHY_N_RB_DL_20MHZ*LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP];
    float dl_ce_ang[5][LIBLTE_PHY_N_RB_DL_20MHZ*LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP];

    // PSS
    float pss_mod_re_n1[3][LIBLTE_PHY_N_RB_DL_20MHZ*LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP];
    float pss_mod_im_n1[3][LIBLTE_PHY_N_RB_DL_20MHZ*LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP];
    float pss_mod_re[3][LIBLTE_PHY_N_RB_DL_20MHZ*LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP];
    float pss_mod_im[3][LIBLTE_PHY_N_RB_DL_20MHZ*LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP];
    float pss_mod_re_p1[3][LIBLTE_PHY_N_RB_DL_20MHZ*LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP];
    float pss_mod_im_p1[3][LIBLTE_PHY_N_RB_DL_20MHZ*LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP];

    // SSS
    float sss_mod_re_0[168][LIBLTE_PHY_N_RB_DL_20MHZ*LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP];
    float sss_mod_im_0[168][LIBLTE_PHY_N_RB_DL_20MHZ*LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP];
    float sss_mod_re_5[168][LIBLTE_PHY_N_RB_DL_20MHZ*LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP];
    float sss_mod_im_5[168][LIBLTE_PHY_N_RB_DL_20MHZ*LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP];
    float sss_re_0[63];
    float sss_im_0[63];
    float sss_re_5[63];
    float sss_im_5[63];
    uint8 sss_x_s_tilda[31];
    uint8 sss_x_c_tilda[31];
    uint8 sss_x_z_tilda[31];
    int8  sss_s_tilda[31];
    int8  sss_c_tilda[31];
    int8  sss_z_tilda[31];
    int8  sss_s0_m0[31];
    int8  sss_s1_m1[31];
    int8  sss_c0[31];
    int8  sss_c1[31];
    int8  sss_z1_m0[31];
    int8  sss_z1_m1[31];

    // Timing
    float dl_timing_abs_corr[LIBLTE_PHY_N_SAMPS_PER_SLOT_30_72MHZ*2];

    // CRS Storage
    float  crs_re_storage[20][3][LIBLTE_PHY_N_RB_DL_20MHZ*LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP];
    float  crs_im_storage[20][3][LIBLTE_PHY_N_RB_DL_20MHZ*LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP];
    uint32 N_id_cell_crs;

    // Samples to Symbols & Symbols to Samples
    fftwf_complex *s2s_in;
    fftwf_complex *s2s_out;
    fftwf_plan     symbs_to_samps_dl_plan;
    fftwf_plan     samps_to_symbs_dl_plan;
    fftwf_plan     symbs_to_samps_ul_plan;
    fftwf_plan     samps_to_symbs_ul_plan;

    // Viterbi decode
    float vd_path_metric[LIBLTE_PHY_MAX_VITERBI_STATES][LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    float vd_br_metric[LIBLTE_PHY_MAX_VITERBI_STATES][2];
    float vd_p_metric[LIBLTE_PHY_MAX_VITERBI_STATES][2];
    float vd_br_weight[LIBLTE_PHY_MAX_VITERBI_STATES][2];
    float vd_w_metric[LIBLTE_PHY_MAX_VITERBI_STATES][LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    float vd_tb_state[LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    float vd_tb_weight[LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    uint8 vd_st_output[LIBLTE_PHY_MAX_VITERBI_STATES][2][3];

    // Turbo encode
    uint8 te_z[LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    uint8 te_fb1[LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    uint8 te_c_prime[LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    uint8 te_z_prime[LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    uint8 te_x_prime[LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];

    // Turbo decode
    int8 td_vitdec_in[LIBLTE_PHY_BASE_CODING_RATE*LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    int8 td_in_int[LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    int8 td_in_calc_1[LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    int8 td_in_calc_2[LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    int8 td_in_calc_3[LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    int8 td_in_int_1[LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    int8 td_int_calc_1[LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    int8 td_int_calc_2[LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    int8 td_in_act_1[LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    int8 td_fb_1[LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    int8 td_int_act_1[LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    int8 td_int_act_2[LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    int8 td_fb_int_1[LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    int8 td_fb_int_2[LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];

    // Rate Match Turbo
    uint8 rmt_tmp[LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    uint8 rmt_sb_mat[LIBLTE_PHY_MAX_CODE_BLOCK_SIZE/LIBLTE_PHY_N_COLUMNS_RATE_MATCH][LIBLTE_PHY_N_COLUMNS_RATE_MATCH];
    uint8 rmt_sb_perm_mat[LIBLTE_PHY_MAX_CODE_BLOCK_SIZE/LIBLTE_PHY_N_COLUMNS_RATE_MATCH][LIBLTE_PHY_N_COLUMNS_RATE_MATCH];
    uint8 rmt_y[LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    uint8 rmt_w[LIBLTE_PHY_BASE_CODING_RATE*LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];

    // Rate Unmatch Turbo
    float rut_tmp[LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    float rut_sb_mat[LIBLTE_PHY_MAX_CODE_BLOCK_SIZE/LIBLTE_PHY_N_COLUMNS_RATE_MATCH][LIBLTE_PHY_N_COLUMNS_RATE_MATCH];
    float rut_sb_perm_mat[LIBLTE_PHY_MAX_CODE_BLOCK_SIZE/LIBLTE_PHY_N_COLUMNS_RATE_MATCH][LIBLTE_PHY_N_COLUMNS_RATE_MATCH];
    float rut_y[LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    float rut_w_dum[LIBLTE_PHY_BASE_CODING_RATE*LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    float rut_w[LIBLTE_PHY_BASE_CODING_RATE*LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    float rut_v[LIBLTE_PHY_BASE_CODING_RATE][LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];

    // Rate Match Conv
    uint8 rmc_tmp[LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    uint8 rmc_sb_mat[LIBLTE_PHY_N_COLUMNS_RATE_MATCH][LIBLTE_PHY_MAX_CODE_BLOCK_SIZE/LIBLTE_PHY_N_COLUMNS_RATE_MATCH];
    uint8 rmc_sb_perm_mat[LIBLTE_PHY_N_COLUMNS_RATE_MATCH][LIBLTE_PHY_MAX_CODE_BLOCK_SIZE/LIBLTE_PHY_N_COLUMNS_RATE_MATCH];
    uint8 rmc_w[LIBLTE_PHY_BASE_CODING_RATE*LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];

    // Rate Unatch Conv
    float ruc_tmp[LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    float ruc_sb_mat[LIBLTE_PHY_N_COLUMNS_RATE_MATCH][LIBLTE_PHY_MAX_CODE_BLOCK_SIZE/LIBLTE_PHY_N_COLUMNS_RATE_MATCH];
    float ruc_sb_perm_mat[LIBLTE_PHY_N_COLUMNS_RATE_MATCH][LIBLTE_PHY_MAX_CODE_BLOCK_SIZE/LIBLTE_PHY_N_COLUMNS_RATE_MATCH];
    float ruc_w_dum[LIBLTE_PHY_BASE_CODING_RATE*LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    float ruc_w[LIBLTE_PHY_BASE_CODING_RATE*LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    float ruc_v[LIBLTE_PHY_BASE_CODING_RATE][LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];

    // ULSCH
    // FIXME: Sizes
    float  ulsch_y_idx[92160];
    float  ulsch_y_mat[92160];
    float  ulsch_rx_d_bits[75376];
    float  ulsch_rx_e_bits[5][LIBLTE_PHY_BASE_CODING_RATE*LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    float  ulsch_rx_f_bits[92160];
    float  ulsch_rx_g_bits[92160];
    uint32 ulsch_N_c_bits[5];
    uint32 ulsch_N_e_bits[5];
    uint8  ulsch_b_bits[30720];
    uint8  ulsch_c_bits[5][LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    uint8  ulsch_tx_d_bits[75376];
    uint8  ulsch_tx_e_bits[5][LIBLTE_PHY_BASE_CODING_RATE*LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    uint8  ulsch_tx_f_bits[92160];
    uint8  ulsch_tx_g_bits[92160];

    // DLSCH
    // FIXME: Sizes
    float  dlsch_rx_d_bits[75376];
    float  dlsch_rx_e_bits[5][LIBLTE_PHY_BASE_CODING_RATE*LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    uint32 dlsch_N_c_bits[5];
    uint32 dlsch_N_e_bits[5];
    uint8  dlsch_b_bits[30720];
    uint8  dlsch_c_bits[5][LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];
    uint8  dlsch_tx_d_bits[75376];
    uint8  dlsch_tx_e_bits[5][LIBLTE_PHY_BASE_CODING_RATE*LIBLTE_PHY_MAX_CODE_BLOCK_SIZE];

    // DCI
    float dci_rx_d_bits[576];
    uint8 dci_tx_d_bits[576];
    uint8 dci_c_bits[192];

    // Generic
    float  rx_symb_re[LIBLTE_PHY_N_RB_DL_20MHZ*LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP];
    float  rx_symb_im[LIBLTE_PHY_N_RB_DL_20MHZ*LIBLTE_PHY_N_SC_RB_DL_NORMAL_CP];
    uint32 fs;
    uint32 N_samps_per_symb;
    uint32 N_samps_cp_l_0;
    uint32 N_samps_cp_l_else;
    uint32 N_samps_per_slot;
    uint32 N_samps_per_subfr;
    uint32 N_samps_per_frame;
    uint32 N_rb_dl;
    uint32 N_rb_ul;
    uint32 N_sc_rb_dl;
    uint32 N_sc_rb_ul;
    uint32 FFT_pad_size;
    uint32 FFT_size;
    uint8  N_ant;
    bool   ul_init;
}LIBLTE_PHY_STRUCT;
// Functions
LIBLTE_ERROR_ENUM liblte_phy_init(LIBLTE_PHY_STRUCT  **phy_struct,
                                  LIBLTE_PHY_FS_ENUM   fs,
                                  uint16               N_id_cell,
                                  uint8                N_ant,
                                  uint32               N_rb_dl,
                                  uint32               N_sc_rb_dl,
                                  float                phich_res);
LIBLTE_ERROR_ENUM liblte_phy_ul_init(LIBLTE_PHY_STRUCT *phy_struct,
                                     uint16             N_id_cell,
                                     uint32             prach_root_seq_idx,
                                     uint32             prach_preamble_format,
                                     uint32             prach_zczc,
                                     bool               prach_hs_flag,
                                     uint8              group_assignment_pusch,
                                     bool               group_hopping_enabled,
                                     bool               sequence_hopping_enabled,
                                     uint8              cyclic_shift,
                                     uint8              cyclic_shift_dci,
                                     uint8              N_cs_an,
                                     uint8              delta_pucch_shift);

/*********************************************************************
    Name: liblte_phy_cleanup

    Description: Cleans up the LTE Physical Layer library.

    Document Reference: N/A
*********************************************************************/
// Defines
// Enums
// Structs
// Functions
LIBLTE_ERROR_ENUM liblte_phy_cleanup(LIBLTE_PHY_STRUCT *phy_struct);
LIBLTE_ERROR_ENUM liblte_phy_ul_cleanup(LIBLTE_PHY_STRUCT *phy_struct);

/*********************************************************************
    Name: liblte_phy_update_n_rb_dl

    Description: Updates N_rb_dl and all associated variables.

    Document Reference: N/A
*********************************************************************/
// Defines
// Enums
// Structs
// Functions
LIBLTE_ERROR_ENUM liblte_phy_update_n_rb_dl(LIBLTE_PHY_STRUCT *phy_struct,
                                            uint32             N_rb_dl);

/*********************************************************************
    Name: liblte_phy_pusch_channel_encode

    Description: Encodes and modulates the Physical Uplink Shared
                 Channel

    Document Reference: 3GPP TS 36.211 v10.1.0 section 5.3

    Notes: Only handling normal CP and N_ant=1
*********************************************************************/
// Defines
// Enums
typedef enum{
    LIBLTE_PHY_TPC_COMMAND_DCI_0_3_4_DB_NEG_1 = 0,
    LIBLTE_PHY_TPC_COMMAND_DCI_0_3_4_DB_ZERO,
    LIBLTE_PHY_TPC_COMMAND_DCI_0_3_4_DB_1,
    LIBLTE_PHY_TPC_COMMAND_DCI_0_3_4_DB_3,
    LIBLTE_PHY_TPC_COMMAND_DCI_0_3_4_N_ITEMS,
}LIBLTE_PHY_TPC_COMMAND_DCI_0_3_4_ENUM;
static const char liblte_phy_tpc_command_dci_0_3_4_text[LIBLTE_PHY_TPC_COMMAND_DCI_0_3_4_N_ITEMS][20] = {"-1dB", "0dB", "1dB", "3dB"};
typedef enum{
    LIBLTE_PHY_TPC_COMMAND_DCI_1_1A_1B_1D_2_3_DB_NEG_1 = 0,
    LIBLTE_PHY_TPC_COMMAND_DCI_1_1A_1B_1D_2_3_DB_ZERO,
    LIBLTE_PHY_TPC_COMMAND_DCI_1_1A_1B_1D_2_3_DB_1,
    LIBLTE_PHY_TPC_COMMAND_DCI_1_1A_1B_1D_2_3_DB_3,
    LIBLTE_PHY_TPC_COMMAND_DCI_1_1A_1B_1D_2_3_N_ITEMS,
}LIBLTE_PHY_TPC_COMMAND_DCI_1_1A_1B_1D_2_3_ENUM;
static const char liblte_phy_tpc_command_dci_1_1a_1b_1d_2_3_text[LIBLTE_PHY_TPC_COMMAND_DCI_1_1A_1B_1D_2_3_N_ITEMS][20] = {"-1dB", "0dB", "1dB", "3dB"};
// Structs
typedef struct{
    LIBLTE_BIT_MSG_STRUCT           msg[2];
    LIBLTE_PHY_PRE_CODER_TYPE_ENUM  pre_coder_type;
    LIBLTE_PHY_MODULATION_TYPE_ENUM mod_type;
    LIBLTE_PHY_CHAN_TYPE_ENUM       chan_type;
    uint32                          tbs;
    uint32                          rv_idx;
    uint32                          N_prb;
    uint32                          prb[LIBLTE_PHY_N_SLOTS_PER_SUBFR][LIBLTE_PHY_N_RB_DL_MAX];
    uint32                          N_codewords;
    uint32                          N_layers;
    uint32                          tx_mode;
    uint32                          harq_retx_count;
    uint16                          rnti;
    uint8                           mcs;
    uint8                           tpc;
    bool                            ndi;
    bool                            dl_alloc;
}LIBLTE_PHY_ALLOCATION_STRUCT;
// Functions
LIBLTE_ERROR_ENUM liblte_phy_pusch_channel_encode(LIBLTE_PHY_STRUCT            *phy_struct,
                                                  LIBLTE_PHY_ALLOCATION_STRUCT *alloc,
                                                  uint32                        N_id_cell,
                                                  uint8                         N_ant,
                                                  LIBLTE_PHY_SUBFRAME_STRUCT   *subframe);

/*********************************************************************
    Name: liblte_phy_pusch_channel_decode

    Description: Demodulates and decodes the Physical Uplink Shared
                 Channel

    Document Reference: 3GPP TS 36.211 v10.1.0 section 5.3
*********************************************************************/
// Defines
// Enums
// Structs
// Functions
LIBLTE_ERROR_ENUM liblte_phy_pusch_channel_decode(LIBLTE_PHY_STRUCT            *phy_struct,
                                                  LIBLTE_PHY_SUBFRAME_STRUCT   *subframe,
                                                  LIBLTE_PHY_ALLOCATION_STRUCT *alloc,
                                                  uint32                        N_id_cell,
                                                  uint8                         N_ant,
                                                  uint8                        *out_bits,
                                                  uint32                       *N_out_bits);

/*********************************************************************
    Name: liblte_phy_pucch_format_1_1a_1b_channel_encode

    Description: Encodes and modulates the Physical Uplink Control
                 Channel for formats 1, 1a, and 1b

    Document Reference: 3GPP TS 36.211 v10.1.0 section 5.4.1
                        3GPP TS 36.212 v10.1.0 section 5.2.3

    Notes: Only handling normal CP and N_ant=1
*********************************************************************/
// Defines
// Enums
// Structs
// Functions
// FIXME

/*********************************************************************
    Name: liblte_phy_pucch_format_1_1a_1b_channel_decode

    Description: Demodulates and decodes the Physical Uplink Control
                 Channel for formats 1, 1a, and 1b

    Document Reference: 3GPP TS 36.211 v10.1.0 section 5.4.1
                        3GPP TS 36.212 v10.1.0 section 5.2.3

    Notes: Only handling normal CP and N_ant=1
*********************************************************************/
// Defines
// Enums
typedef enum{
    LIBLTE_PHY_PUCCH_FORMAT_1 = 0,
    LIBLTE_PHY_PUCCH_FORMAT_1A,
    LIBLTE_PHY_PUCCH_FORMAT_1B,
    LIBLTE_PHY_PUCCH_FORMAT_2,
    LIBLTE_PHY_PUCCH_FORMAT_2A,
    LIBLTE_PHY_PUCCH_FORMAT_2B,
    LIBLTE_PHY_PUCCH_FORMAT_3,
    LIBLTE_PHY_PUCCH_FORMAT_N_ITEMS,
}LIBLTE_PHY_PUCCH_FORMAT_ENUM;
static const char liblte_phy_pucch_format_text[LIBLTE_PHY_PUCCH_FORMAT_N_ITEMS][20] = { "1", "1a", "1b",
                                                                                        "2", "2a", "2b",
                                                                                        "3"};
// Structs
// Functions
LIBLTE_ERROR_ENUM liblte_phy_pucch_format_1_1a_1b_channel_decode(LIBLTE_PHY_STRUCT            *phy_struct,
                                                                 LIBLTE_PHY_SUBFRAME_STRUCT   *subframe,
                                                                 LIBLTE_PHY_PUCCH_FORMAT_ENUM  format,
                                                                 uint32                        N_id_cell,
                                                                 uint8                         N_ant,
                                                                 uint32                        N_1_p_pucch,
                                                                 uint8                        *out_bits,
                                                                 uint32                       *N_out_bits);

/*********************************************************************
    Name: liblte_phy_pucch_format_2_2a_2b_channel_encode

    Description: Encodes and modulates the Physical Uplink Control
                 Channel for formats 2, 2a, and 2b

    Document Reference: 3GPP TS 36.211 v10.1.0 section 5.4.2
                        3GPP TS 36.212 v10.1.0 section 5.2.3

    Notes: Only handling normal CP and N_ant=1
*********************************************************************/
// Defines
// Enums
// Structs
// Functions
// FIXME

/*********************************************************************
    Name: liblte_phy_pucch_format_2_2a_2b_channel_decode

    Description: Demodulates and decodes the Physical Uplink Control
                 Channel for formats 2, 2a, and 2b

    Document Reference: 3GPP TS 36.211 v10.1.0 section 5.4.2
                        3GPP TS 36.212 v10.1.0 section 5.2.3

    Notes: Only handling normal CP and N_ant=1
*********************************************************************/
// Defines
// Enums
// Structs
// Functions
// FIXME

/*********************************************************************
    Name: liblte_phy_pucch_map_sr_config_idx

    Description: Maps SR configuration index to SR periodicity and
                 SR subframe offset

    Document Reference: 3GPP TS 36.213 v10.3.0 table 10.1.5-1
*********************************************************************/
// Defines
// Enums
// Structs
// Functions
void liblte_phy_pucch_map_sr_config_idx(uint32  i_sr,
                                        uint32 *sr_periodicity,
                                        uint32 *N_offset_sr);

/*********************************************************************
    Name: liblte_phy_generate_prach

    Description: Generates the baseband signal for a PRACH

    Document Reference: 3GPP TS 36.211 v10.1.0 section 5.7.3
*********************************************************************/
// Defines
// Enums
// Structs
// Functions
LIBLTE_ERROR_ENUM liblte_phy_generate_prach(LIBLTE_PHY_STRUCT *phy_struct,
                                            uint32             preamble_idx,
                                            uint32             freq_offset,
                                            float             *samps_re,
                                            float             *samps_im);

/*********************************************************************
    Name: liblte_phy_detect_prach

    Description: Detects PRACHs from baseband I/Q

    Document Reference: 3GPP TS 36.211 v10.1.0 section 5.7.2 and 5.7.3
*********************************************************************/
// Defines
// Enums
// Structs
// Functions
LIBLTE_ERROR_ENUM liblte_phy_detect_prach(LIBLTE_PHY_STRUCT *phy_struct,
                                          float             *samps_re,
                                          float             *samps_im,
                                          uint32             freq_offset,
                                          uint32            *N_det_pre,
                                          uint32            *det_pre,
                                          uint32            *det_ta);

/*********************************************************************
    Name: liblte_phy_pdsch_channel_encode

    Description: Encodes and modulates the Physical Downlink Shared
                 Channel

    Document Reference: 3GPP TS 36.211 v10.1.0 sections 6.3 and 6.4
*********************************************************************/
// Defines
#define LIBLTE_PHY_PDCCH_MAX_ALLOC 6
// Enums
// Structs
typedef struct{
    LIBLTE_PHY_ALLOCATION_STRUCT alloc[LIBLTE_PHY_PDCCH_MAX_ALLOC];
    uint32                       N_symbs;
    uint32                       N_alloc;
}LIBLTE_PHY_PDCCH_STRUCT;
// Functions
LIBLTE_ERROR_ENUM liblte_phy_pdsch_channel_encode(LIBLTE_PHY_STRUCT          *phy_struct,
                                                  LIBLTE_PHY_PDCCH_STRUCT    *pdcch,
                                                  uint32                      N_id_cell,
                                                  uint8                       N_ant,
                                                  LIBLTE_PHY_SUBFRAME_STRUCT *subframe);

/*********************************************************************
    Name: liblte_phy_pdsch_channel_decode

    Description: Demodulates and decodes the Physical Downlink Shared
                 Channel

    Document Reference: 3GPP TS 36.211 v10.1.0 sections 6.3 and 6.4
*********************************************************************/
// Defines
// Enums
// Structs
// Functions
LIBLTE_ERROR_ENUM liblte_phy_pdsch_channel_decode(LIBLTE_PHY_STRUCT            *phy_struct,
                                                  LIBLTE_PHY_SUBFRAME_STRUCT   *subframe,
                                                  LIBLTE_PHY_ALLOCATION_STRUCT *alloc,
                                                  uint32                        N_pdcch_symbs,
                                                  uint32                        N_id_cell,
                                                  uint8                         N_ant,
                                                  uint8                        *out_bits,
                                                  uint32                       *N_out_bits);

/*********************************************************************
    Name: liblte_phy_bch_channel_encode

    Description: Encodes and modulates the Physical Broadcast
                 Channel

    Document Reference: 3GPP TS 36.211 v10.1.0 section 6.6
*********************************************************************/
// Defines
// Enums
// Structs
// Functions
LIBLTE_ERROR_ENUM liblte_phy_bch_channel_encode(LIBLTE_PHY_STRUCT          *phy_struct,
                                                uint8                      *in_bits,
                                                uint32                      N_in_bits,
                                                uint32                      N_id_cell,
                                                uint8                       N_ant,
                                                LIBLTE_PHY_SUBFRAME_STRUCT *subframe,
                                                uint32                      sfn);

/*********************************************************************
    Name: liblte_phy_bch_channel_decode

    Description: Demodulates and decodes the Physical Broadcast
                 Channel

    Document Reference: 3GPP TS 36.211 v10.1.0 section 6.6
*********************************************************************/
// Defines
// Enums
// Structs
// Functions
LIBLTE_ERROR_ENUM liblte_phy_bch_channel_decode(LIBLTE_PHY_STRUCT          *phy_struct,
                                                LIBLTE_PHY_SUBFRAME_STRUCT *subframe,
                                                uint32                      N_id_cell,
                                                uint8                      *N_ant,
                                                uint8                      *out_bits,
                                                uint32                     *N_out_bits,
                                                uint8                      *offset);

/*********************************************************************
    Name: liblte_phy_pdcch_channel_encode

    Description: Encodes and modulates all of the Physical Downlink
                 Control Channels (PCFICH, PHICH, and PDCCH)

    Document Reference: 3GPP TS 36.211 v10.1.0 sections 6.7, 6.8, and
                        6.9
                        3GPP TS 36.212 v10.1.0 section 5.1.4.2.1
*********************************************************************/
// Defines
// Enums
typedef enum{
    LIBLTE_PHY_DCI_CA_NOT_PRESENT = 0,
    LIBLTE_PHY_DCI_CA_PRESENT,
}LIBLTE_PHY_DCI_CA_PRESENCE_ENUM;
// Structs
typedef struct{
    float  n[4];
    uint32 k[4];
    uint32 cfi;
    uint32 N_reg;
}LIBLTE_PHY_PCFICH_STRUCT;

typedef struct{
    float  z_re[3];
    float  z_im[3];
    uint32 k[75];
    uint32 N_reg;
    uint8  b[25][8];
    bool   present[25][8];
}LIBLTE_PHY_PHICH_STRUCT;
// Functions
LIBLTE_ERROR_ENUM liblte_phy_pdcch_channel_encode(LIBLTE_PHY_STRUCT              *phy_struct,
                                                  LIBLTE_PHY_PCFICH_STRUCT       *pcfich,
                                                  LIBLTE_PHY_PHICH_STRUCT        *phich,
                                                  LIBLTE_PHY_PDCCH_STRUCT        *pdcch,
                                                  uint32                          N_id_cell,
                                                  uint8                           N_ant,
                                                  float                           phich_res,
                                                  LIBLTE_RRC_PHICH_DURATION_ENUM  phich_dur,
                                                  LIBLTE_PHY_SUBFRAME_STRUCT     *subframe);

/*********************************************************************
    Name: liblte_phy_pdcch_channel_decode

    Description: Demodulates and decodes all of the Physical Downlink
                 Control Channels (PCFICH, PHICH, and PDCCH)

    Document Reference: 3GPP TS 36.211 v10.1.0 sections 6.7, 6.8, and
                        6.9
                        3GPP TS 36.212 v10.1.0 section 5.1.4.2.1
*********************************************************************/
// Defines
// Enums
// Structs
// Functions
LIBLTE_ERROR_ENUM liblte_phy_pdcch_channel_decode(LIBLTE_PHY_STRUCT              *phy_struct,
                                                  LIBLTE_PHY_SUBFRAME_STRUCT     *subframe,
                                                  uint32                          N_id_cell,
                                                  uint8                           N_ant,
                                                  float                           phich_res,
                                                  LIBLTE_RRC_PHICH_DURATION_ENUM  phich_dur,
                                                  LIBLTE_PHY_PCFICH_STRUCT       *pcfich,
                                                  LIBLTE_PHY_PHICH_STRUCT        *phich,
                                                  LIBLTE_PHY_PDCCH_STRUCT        *pdcch);

/*********************************************************************
    Name: liblte_phy_map_crs

    Description: Maps the Cell Specific Reference Signals to the
                 subframe

    Document Reference: 3GPP TS 36.211 v10.1.0 section 6.10.1
*********************************************************************/
// Defines
// Enums
// Structs
// Functions
LIBLTE_ERROR_ENUM liblte_phy_map_crs(LIBLTE_PHY_STRUCT          *phy_struct,
                                     LIBLTE_PHY_SUBFRAME_STRUCT *subframe,
                                     uint32                      N_id_cell,
                                     uint8                       N_ant);

/*********************************************************************
    Name: liblte_phy_map_pss

    Description: Maps the Primary Synchronization Signal to the
                 subframe.

    Document Reference: 3GPP TS 36.211 v10.1.0 section 6.11.1
*********************************************************************/
// Defines
// Enums
// Structs
// Functions
LIBLTE_ERROR_ENUM liblte_phy_map_pss(LIBLTE_PHY_STRUCT          *phy_struct,
                                     LIBLTE_PHY_SUBFRAME_STRUCT *subframe,
                                     uint32                      N_id_2,
                                     uint8                       N_ant);

/*********************************************************************
    Name: liblte_phy_find_pss_and_fine_timing

    Description: Searches for the Primary Synchronization Signal and
                 determines fine timing.

    Document Reference: 3GPP TS 36.211 v10.1.0 section 6.11.1
*********************************************************************/
// Defines
// Enums
// Structs
// Functions
LIBLTE_ERROR_ENUM liblte_phy_find_pss_and_fine_timing(LIBLTE_PHY_STRUCT *phy_struct,
                                                      float             *i_samps,
                                                      float             *q_samps,
                                                      uint32            *symb_starts,
                                                      uint32            *N_id_2,
                                                      uint32            *pss_symb,
                                                      float             *pss_thresh,
                                                      float             *freq_offset);

/*********************************************************************
    Name: liblte_phy_map_sss

    Description: Maps the Secondary Synchronization Signal to the
                 subframe.

    Document Reference: 3GPP TS 36.211 v10.1.0 section 6.11.2
*********************************************************************/
// Defines
// Enums
// Structs
// Functions
LIBLTE_ERROR_ENUM liblte_phy_map_sss(LIBLTE_PHY_STRUCT          *phy_struct,
                                     LIBLTE_PHY_SUBFRAME_STRUCT *subframe,
                                     uint32                      N_id_1,
                                     uint32                      N_id_2,
                                     uint8                       N_ant);

/*********************************************************************
    Name: liblte_phy_find_sss

    Description: Searches for the Secondary Synchronization Signal.

    Document Reference: 3GPP TS 36.211 v10.1.0 section 6.11.2
*********************************************************************/
// Defines
// Enums
// Structs
// Functions
LIBLTE_ERROR_ENUM liblte_phy_find_sss(LIBLTE_PHY_STRUCT *phy_struct,
                                      float             *i_samps,
                                      float             *q_samps,
                                      uint32             N_id_2,
                                      uint32            *symb_starts,
                                      float              pss_thresh,
                                      uint32            *N_id_1,
                                      uint32            *frame_start_idx);

/*********************************************************************
    Name: liblte_phy_dl_find_coarse_timing_and_freq_offset

    Description: Finds coarse time syncronization and frequency offset
                 by auto-correlating to find the cyclic prefix on
                 reference signal symbols of the downlink

    Document Reference: 3GPP TS 36.211 v10.1.0
*********************************************************************/
// Defines
#define LIBLTE_PHY_N_MAX_ROUGH_CORR_SEARCH_PEAKS 5
// Enums
// Structs
typedef struct{
    float  freq_offset[LIBLTE_PHY_N_MAX_ROUGH_CORR_SEARCH_PEAKS];
    uint32 symb_starts[LIBLTE_PHY_N_MAX_ROUGH_CORR_SEARCH_PEAKS][7];
    uint32 n_corr_peaks;
}LIBLTE_PHY_COARSE_TIMING_STRUCT;
// Functions
LIBLTE_ERROR_ENUM liblte_phy_dl_find_coarse_timing_and_freq_offset(LIBLTE_PHY_STRUCT               *phy_struct,
                                                                   float                           *i_samps,
                                                                   float                           *q_samps,
                                                                   uint32                           N_slots,
                                                                   LIBLTE_PHY_COARSE_TIMING_STRUCT *timing_struct);

/*********************************************************************
    Name: liblte_phy_create_dl_subframe

    Description: Creates the baseband signal for a particular
                 downlink subframe

    Document Reference: 3GPP TS 36.211 v10.1.0
*********************************************************************/
// Defines
// Enums
// Structs
// Functions
LIBLTE_ERROR_ENUM liblte_phy_create_dl_subframe(LIBLTE_PHY_STRUCT          *phy_struct,
                                                LIBLTE_PHY_SUBFRAME_STRUCT *subframe,
                                                uint8                       ant,
                                                float                      *i_samps,
                                                float                      *q_samps);

/*********************************************************************
    Name: liblte_phy_get_dl_subframe_and_ce

    Description: Resolves all symbols and channel estimates for a
                 particular downlink subframe

    Document Reference: 3GPP TS 36.211 v10.1.0
*********************************************************************/
// Defines
// Enums
// Structs
// Functions
LIBLTE_ERROR_ENUM liblte_phy_get_dl_subframe_and_ce(LIBLTE_PHY_STRUCT          *phy_struct,
                                                    float                      *i_samps,
                                                    float                      *q_samps,
                                                    uint32                      frame_start_idx,
                                                    uint8                       subfr_num,
                                                    uint32                      N_id_cell,
                                                    uint8                       N_ant,
                                                    LIBLTE_PHY_SUBFRAME_STRUCT *subframe);

/*********************************************************************
    Name: liblte_phy_get_ul_subframe

    Description: Resolves all symbols for a particular uplink subframe

    Document Reference: 3GPP TS 36.211 v10.1.0
*********************************************************************/
// Defines
// Enums
// Structs
// Functions
LIBLTE_ERROR_ENUM liblte_phy_get_ul_subframe(LIBLTE_PHY_STRUCT          *phy_struct,
                                             float                      *i_samps,
                                             float                      *q_samps,
                                             LIBLTE_PHY_SUBFRAME_STRUCT *subframe);

/*********************************************************************
    Name: liblte_phy_get_tbs_mcs_and_n_prb_for_dl

    Description: Determines the transport block size, modulation and
                 coding scheme, and the number of PRBs needed to send
                 the specified number of DL bits

    Document Reference: 3GPP TS 36.213 v10.3.0 section 7.1.7

    NOTES: Currently only supports DCI format 1A
*********************************************************************/
// Defines
// Enums
// Structs
// Functions
LIBLTE_ERROR_ENUM liblte_phy_get_tbs_mcs_and_n_prb_for_dl(uint32  N_bits,
                                                          uint32  N_subframe,
                                                          uint32  N_rb_dl,
                                                          uint16  rnti,
                                                          uint32 *tbs,
                                                          uint8  *mcs,
                                                          uint32 *N_prb);

/*********************************************************************
    Name: liblte_phy_get_tbs_and_n_prb_for_dl

    Description: Determines the transport block size and the number of
                 PRBs needed to send the specified number of DL bits
                 according to the specified modulation and coding
                 scheme

    Document Reference: 3GPP TS 36.213 v10.3.0 section 7.1.7

    NOTES: Currently only supports single layer transport blocks
*********************************************************************/
// Defines
// Enums
// Structs
// Functions
LIBLTE_ERROR_ENUM liblte_phy_get_tbs_and_n_prb_for_dl(uint32  N_bits,
                                                      uint32  N_rb_dl,
                                                      uint8   mcs,
                                                      uint32 *tbs,
                                                      uint32 *N_prb);

/*********************************************************************
    Name: liblte_phy_get_tbs_mcs_and_n_prb_for_ul

    Description: Determines the transport block size, modulation and
                 coding scheme, and the number of PRBs needed to send
                 the specified number of UL bits

    Document Reference: 3GPP TS 36.213 v10.3.0 section 7.1.7
*********************************************************************/
// Defines
// Enums
// Structs
// Functions
LIBLTE_ERROR_ENUM liblte_phy_get_tbs_mcs_and_n_prb_for_ul(uint32  N_bits,
                                                          uint32  N_rb_ul,
                                                          uint32 *tbs,
                                                          uint8  *mcs,
                                                          uint32 *N_prb);

/*********************************************************************
    Name: liblte_phy_get_n_cce

    Description: Determines the number of control channel elements
                 available in the specified subframe

    Document Reference: 3GPP TS 36.211 v10.1.0 section 6.8.1
*********************************************************************/
// Defines
// Enums
// Structs
// Functions
LIBLTE_ERROR_ENUM liblte_phy_get_n_cce(LIBLTE_PHY_STRUCT *phy_struct,
                                       float              phich_res,
                                       uint32             N_pdcch_symbs,
                                       uint8              N_ant,
                                       uint32            *N_cce);

/*********************************************************************
    Name: liblte_phy_rate_match_turbo

    Description: Rate matches turbo encoded data

    Document Reference: 3GPP TS 36.212 v10.1.0 section 5.1.4.1
*********************************************************************/
// Defines
// Enums
// Structs
// Functions
void liblte_phy_rate_match_turbo(LIBLTE_PHY_STRUCT         *phy_struct,
                                 uint8                     *d_bits,
                                 uint32                     N_d_bits,
                                 uint32                     N_codeblocks,
                                 uint32                     tx_mode,
                                 uint32                     N_soft,
                                 uint32                     M_dl_harq,
                                 LIBLTE_PHY_CHAN_TYPE_ENUM  chan_type,
                                 uint32                     rv_idx,
                                 uint32                     N_e_bits,
                                 uint8                     *e_bits);

/*********************************************************************
    Name: liblte_phy_rate_unmatch_turbo

    Description: Rate unmatches turbo encoded data

    Document Reference: 3GPP TS 36.212 v10.1.0 section 5.1.4.1
*********************************************************************/
// Defines
// Enums
// Structs
// Functions
void liblte_phy_rate_unmatch_turbo(LIBLTE_PHY_STRUCT         *phy_struct,
                                   float                     *e_bits,
                                   uint32                     N_e_bits,
                                   uint8                     *dummy_bits,
                                   uint32                     N_dummy_bits,
                                   uint32                     N_codeblocks,
                                   uint32                     tx_mode,
                                   uint32                     N_soft,
                                   uint32                     M_dl_harq,
                                   LIBLTE_PHY_CHAN_TYPE_ENUM  chan_type,
                                   uint32                     rv_idx,
                                   float                     *d_bits,
                                   uint32                    *N_d_bits);

/*********************************************************************
    Name: liblte_phy_code_block_segmentation

    Description: Performs code block segmentation for turbo coded
                 channels

    Document Reference: 3GPP TS 36.212 v10.1.0 section 5.1.2
*********************************************************************/
// Defines
// Enums
// Structs
// Functions
void liblte_phy_code_block_segmentation(uint8  *b_bits,
                                        uint32  N_b_bits,
                                        uint32 *N_codeblocks,
                                        uint32 *N_filler_bits,
                                        uint8  *c_bits,
                                        uint32  N_c_bits_max,
                                        uint32 *N_c_bits);

/*********************************************************************
    Name: liblte_phy_code_block_desegmentation

    Description: Performs code block desegmentation for turbo coded
                 channels

    Document Reference: 3GPP TS 36.212 v10.1.0 section 5.1.2
*********************************************************************/
// Defines
// Enums
// Structs
// Functions
void liblte_phy_code_block_desegmentation(uint8  *c_bits,
                                          uint32 *N_c_bits,
                                          uint32  N_c_bits_max,
                                          uint32  tbs,
                                          uint8  *b_bits,
                                          uint32  N_b_bits);

#endif /* __LIBLTE_PHY_H__ */
