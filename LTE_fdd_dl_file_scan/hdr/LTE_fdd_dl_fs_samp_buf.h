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

    File: LTE_fdd_dl_fs_samp_buf.h

    Description: Contains all the definitions for the LTE FDD DL File Scanner
                 sample buffer block.

    Revision History
    ----------    -------------    --------------------------------------------
    03/23/2012    Ben Wojtowicz    Created file.
    08/19/2012    Ben Wojtowicz    Added states and state memory and added
                                   decoding of all SIBs.
    11/10/2012    Ben Wojtowicz    Using the latest libraries to decode more
                                   than 1 eNB
    01/07/2013    Ben Wojtowicz    Moved from automake to cmake
    03/03/2013    Ben Wojtowicz    Added support for paging channel, SIB5, SIB6,
                                   and SIB7 decoding, using the latest
                                   libraries, and fixed a bug that allowed
                                   multiple decodes of the same channel.
    03/17/2013    Ben Wojtowicz    Added paging message printing.
    07/21/2013    Ben Wojtowicz    Using the latest LTE library.
    08/26/2013    Ben Wojtowicz    Updates to support GnuRadio 3.7.
    09/28/2013    Ben Wojtowicz    Added support for setting the sample rate
                                   and input data type.
    06/15/2014    Ben Wojtowicz    Using the latest LTE library.
    11/09/2014    Ben Wojtowicz    Added SIB13 printing.

*******************************************************************************/

#ifndef __LTE_FDD_DL_FS_SAMP_BUF_H__
#define __LTE_FDD_DL_FS_SAMP_BUF_H__

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_fdd_dl_fs_api.h"
#include "liblte_phy.h"
#include "liblte_rrc.h"
#include <gnuradio/sync_block.h>

/*******************************************************************************
                              DEFINES
*******************************************************************************/

#define LTE_FDD_DL_FS_SAMP_BUF_SIZE       (LIBLTE_PHY_N_SAMPS_PER_FRAME_30_72MHZ*10)
#define LTE_FDD_DL_FS_SAMP_BUF_NUM_FRAMES (10)

#define LTE_FDD_DL_FS_SAMP_BUF_N_DECODED_CHANS_MAX 10

// Configurable Parameters
#define FS_PARAM "fs"

/*******************************************************************************
                              FORWARD DECLARATIONS
*******************************************************************************/

class LTE_fdd_dl_fs_samp_buf;

/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/

typedef boost::shared_ptr<LTE_fdd_dl_fs_samp_buf> LTE_fdd_dl_fs_samp_buf_sptr;

typedef enum{
    LTE_FDD_DL_FS_IN_SIZE_INT8 = 0,
    LTE_FDD_DL_FS_IN_SIZE_GR_COMPLEX,
}LTE_FDD_DL_FS_IN_SIZE_ENUM;

typedef enum{
    LTE_FDD_DL_FS_SAMP_BUF_STATE_COARSE_TIMING_SEARCH = 0,
    LTE_FDD_DL_FS_SAMP_BUF_STATE_PSS_AND_FINE_TIMING_SEARCH,
    LTE_FDD_DL_FS_SAMP_BUF_STATE_SSS_SEARCH,
    LTE_FDD_DL_FS_SAMP_BUF_STATE_BCH_DECODE,
    LTE_FDD_DL_FS_SAMP_BUF_STATE_PDSCH_DECODE_SIB1,
    LTE_FDD_DL_FS_SAMP_BUF_STATE_PDSCH_DECODE_SI_GENERIC,
}LTE_FDD_DL_FS_SAMP_BUF_STATE_ENUM;

/*******************************************************************************
                              CLASS DECLARATIONS
*******************************************************************************/

LTE_FDD_DL_FS_API LTE_fdd_dl_fs_samp_buf_sptr LTE_fdd_dl_fs_make_samp_buf(size_t in_size_val);
class LTE_FDD_DL_FS_API LTE_fdd_dl_fs_samp_buf : public gr::sync_block
{
public:
    ~LTE_fdd_dl_fs_samp_buf();

    int32 work(int32                      ninput_items,
               gr_vector_const_void_star &input_items,
               gr_vector_void_star       &output_items);

private:
    friend LTE_FDD_DL_FS_API LTE_fdd_dl_fs_samp_buf_sptr LTE_fdd_dl_fs_make_samp_buf(size_t in_size_val);

    LTE_fdd_dl_fs_samp_buf(size_t in_size_val);

    // Input parameters
    LTE_FDD_DL_FS_IN_SIZE_ENUM in_size;

    // LTE library
    LIBLTE_PHY_STRUCT                *phy_struct;
    LIBLTE_PHY_COARSE_TIMING_STRUCT   timing_struct;
    LIBLTE_BIT_MSG_STRUCT             rrc_msg;
    LIBLTE_RRC_MIB_STRUCT             mib;
    LIBLTE_RRC_BCCH_DLSCH_MSG_STRUCT  bcch_dlsch_msg;
    LIBLTE_RRC_PCCH_MSG_STRUCT        pcch_msg;
    LIBLTE_PHY_FS_ENUM                fs;

    // Sample buffer
    float  *i_buf;
    float  *q_buf;
    uint32  samp_buf_w_idx;
    uint32  samp_buf_r_idx;
    bool    last_samp_was_i;

    // Variables
    LTE_FDD_DL_FS_SAMP_BUF_STATE_ENUM state;
    float                             phich_res;
    uint32                            sfn;
    uint32                            N_sfr;
    uint32                            N_id_cell;
    uint32                            N_id_1;
    uint32                            N_id_2;
    uint32                            corr_peak_idx;
    uint32                            decoded_chans[LTE_FDD_DL_FS_SAMP_BUF_N_DECODED_CHANS_MAX];
    uint32                            N_decoded_chans;
    uint8                             N_ant;
    uint8                             prev_si_value_tag;
    bool                              prev_si_value_tag_valid;
    bool                              mib_printed;
    bool                              sib1_printed;
    bool                              sib2_printed;
    bool                              sib3_printed;
    bool                              sib3_expected;
    bool                              sib4_printed;
    bool                              sib4_expected;
    bool                              sib5_printed;
    bool                              sib5_expected;
    bool                              sib6_printed;
    bool                              sib6_expected;
    bool                              sib7_printed;
    bool                              sib7_expected;
    bool                              sib8_printed;
    bool                              sib8_expected;
    bool                              sib13_printed;
    bool                              sib13_expected;

    // Helpers
    void init(void);
    void copy_input_to_samp_buf(gr_vector_const_void_star &input_items, int32 ninput_items);
    void freq_shift(uint32 start_idx, uint32 num_samps, float freq_offset);
    void print_mib(LIBLTE_RRC_MIB_STRUCT *mib);
    void print_sib1(LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_1_STRUCT *sib1);
    void print_sib2(LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_2_STRUCT *sib2);
    void print_sib3(LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_3_STRUCT *sib3);
    void print_sib4(LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_4_STRUCT *sib4);
    void print_sib5(LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_5_STRUCT *sib5);
    void print_sib6(LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_6_STRUCT *sib6);
    void print_sib7(LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_7_STRUCT *sib7);
    void print_sib8(LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_8_STRUCT *sib8);
    void print_sib13(LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_13_STRUCT *sib13);
    void print_page(LIBLTE_RRC_PAGING_STRUCT *page);

    // Configuration
    void print_config(void);
    void change_config(char *line);
    bool set_fs(char *char_value);
    bool need_config;
};

#endif /* __LTE_FDD_DL_FS_SAMP_BUF_H__ */
