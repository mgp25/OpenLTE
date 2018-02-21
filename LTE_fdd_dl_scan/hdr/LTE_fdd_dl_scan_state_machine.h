/*******************************************************************************

    Copyright 2013-2014 Ben Wojtowicz

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

    File: LTE_fdd_dl_scan_state_machine.h

    Description: Contains all the definitions for the LTE FDD DL Scanner
                 state machine block.

    Revision History
    ----------    -------------    --------------------------------------------
    02/26/2013    Ben Wojtowicz    Created file
    07/21/2013    Ben Wojtowicz    Added support for multiple sample rates
    08/26/2013    Ben Wojtowicz    Updates to support GnuRadio 3.7.
    06/15/2014    Ben Wojtowicz    Using the latest LTE library.

*******************************************************************************/

#ifndef __LTE_FDD_DL_SCAN_STATE_MACHINE_H__
#define __LTE_FDD_DL_SCAN_STATE_MACHINE_H__

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_fdd_dl_scan_state_machine_api.h"
#include "LTE_fdd_dl_scan_interface.h"
#include "liblte_phy.h"
#include "liblte_rrc.h"
#include <gnuradio/sync_block.h>

/*******************************************************************************
                              DEFINES
*******************************************************************************/

#define LTE_FDD_DL_SCAN_STATE_MACHINE_N_DECODED_CHANS_MAX 10

/*******************************************************************************
                              FORWARD DECLARATIONS
*******************************************************************************/

class LTE_fdd_dl_scan_state_machine;

/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/

typedef boost::shared_ptr<LTE_fdd_dl_scan_state_machine> LTE_fdd_dl_scan_state_machine_sptr;

typedef enum{
    LTE_FDD_DL_SCAN_STATE_MACHINE_STATE_COARSE_TIMING_SEARCH = 0,
    LTE_FDD_DL_SCAN_STATE_MACHINE_STATE_PSS_AND_FINE_TIMING_SEARCH,
    LTE_FDD_DL_SCAN_STATE_MACHINE_STATE_SSS_SEARCH,
    LTE_FDD_DL_SCAN_STATE_MACHINE_STATE_BCH_DECODE,
    LTE_FDD_DL_SCAN_STATE_MACHINE_STATE_PDSCH_DECODE_SIB1,
    LTE_FDD_DL_SCAN_STATE_MACHINE_STATE_PDSCH_DECODE_SI_GENERIC,
}LTE_FDD_DL_SCAN_STATE_MACHINE_STATE_ENUM;

/*******************************************************************************
                              CLASS DECLARATIONS
*******************************************************************************/

LTE_FDD_DL_SCAN_STATE_MACHINE_API LTE_fdd_dl_scan_state_machine_sptr LTE_fdd_dl_scan_make_state_machine (uint32 samp_rate);
class LTE_FDD_DL_SCAN_STATE_MACHINE_API LTE_fdd_dl_scan_state_machine : public gr::sync_block
{
public:
    ~LTE_fdd_dl_scan_state_machine();

    int32 work(int32                      ninput_items,
               gr_vector_const_void_star &input_items,
               gr_vector_void_star       &output_items);

private:
    friend LTE_FDD_DL_SCAN_STATE_MACHINE_API LTE_fdd_dl_scan_state_machine_sptr LTE_fdd_dl_scan_make_state_machine(uint32 samp_rate);

    LTE_fdd_dl_scan_state_machine(uint32 samp_rate);

    // LTE library
    LIBLTE_PHY_STRUCT                *phy_struct;
    LIBLTE_PHY_COARSE_TIMING_STRUCT   timing_struct;
    LIBLTE_BIT_MSG_STRUCT             rrc_msg;
    LIBLTE_RRC_MIB_STRUCT             mib;
    LIBLTE_RRC_BCCH_DLSCH_MSG_STRUCT  bcch_dlsch_msg;

    // Sample buffer
    float  *i_buf;
    float  *q_buf;
    uint32  samp_buf_w_idx;
    uint32  samp_buf_r_idx;
    uint32  one_subframe_num_samps;
    uint32  one_frame_num_samps;
    uint32  freq_change_wait_num_samps;
    uint32  coarse_timing_search_num_samps;
    uint32  pss_and_fine_timing_search_num_samps;
    uint32  sss_search_num_samps;
    uint32  bch_decode_num_samps;
    uint32  pdsch_decode_sib1_num_samps;
    uint32  pdsch_decode_si_generic_num_samps;

    // Variables
    LTE_FDD_DL_SCAN_CHAN_DATA_STRUCT         chan_data;
    LTE_FDD_DL_SCAN_STATE_MACHINE_STATE_ENUM state;
    float                                    phich_res;
    uint32                                   N_sfr;
    uint32                                   N_id_1;
    uint32                                   N_id_2;
    uint32                                   corr_peak_idx;
    uint32                                   decoded_chans[LTE_FDD_DL_SCAN_STATE_MACHINE_N_DECODED_CHANS_MAX];
    uint32                                   N_decoded_chans;
    uint32                                   N_attempts;
    uint32                                   N_bch_attempts;
    uint32                                   N_pdsch_attempts;
    uint32                                   N_samps_needed;
    uint32                                   freq_change_wait_cnt;
    uint32                                   sfn;
    uint8                                    N_ant;
    bool                                     freq_change_wait_done;
    bool                                     sib1_sent;
    bool                                     sib2_sent;
    bool                                     sib3_sent;
    bool                                     sib4_sent;
    bool                                     sib5_sent;
    bool                                     sib6_sent;
    bool                                     sib7_sent;
    bool                                     sib8_sent;
    bool                                     send_cnf;

    // Helpers
    void init(void);
    void copy_input_to_samp_buf(const gr_complex *in, int32 ninput_items);
    void freq_shift(uint32 start_idx, uint32 num_samps, float freq_offset);
    void channel_found(bool &switch_freq, int32 &done_flag);
    void channel_not_found(bool &switch_freq, int32 &done_flag);
};

#endif /* __LTE_FDD_DL_SCAN_STATE_MACHINE_H__ */
