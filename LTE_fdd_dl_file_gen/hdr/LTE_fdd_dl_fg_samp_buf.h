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

    File: LTE_fdd_dl_fg_samp_buf.h

    Description: Contains all the definitions for the LTE FDD DL File Generator
                 sample buffer block.

    Revision History
    ----------    -------------    --------------------------------------------
    06/15/2012    Ben Wojtowicz    Created file.
    08/19/2012    Ben Wojtowicz    Using the latest liblte library.
    11/10/2012    Ben Wojtowicz    Added SIB2 support and changed the parameter
                                   input method to be "interactive"
    12/26/2012    Ben Wojtowicz    Added SIB3, SIB4, and SIB8 support and fixed
                                   a file size bug
    01/07/2013    Ben Wojtowicz    Moved from automake to cmake
    03/03/2013    Ben Wojtowicz    Added support for a test load.
    07/21/2013    Ben Wojtowicz    Using the latest LTE library.
    08/26/2013    Ben Wojtowicz    Updates to support GnuRadio 3.7.
    09/16/2013    Ben Wojtowicz    Added support for changing the sample rate.
    09/28/2013    Ben Wojtowicz    Added support for setting the sample rate
                                   and output data type.
    06/15/2014    Ben Wojtowicz    Using the latest LTE library.

*******************************************************************************/

#ifndef __LTE_FDD_DL_FG_SAMP_BUF_H__
#define __LTE_FDD_DL_FG_SAMP_BUF_H__

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_fdd_dl_fg_api.h"
#include "liblte_phy.h"
#include "liblte_rrc.h"
#include <gnuradio/sync_block.h>

/*******************************************************************************
                              DEFINES
*******************************************************************************/

#define LTE_FDD_DL_FG_SAMP_BUF_SIZE (LIBLTE_PHY_N_SAMPS_PER_FRAME_30_72MHZ)

// Configurable Parameters
#define BANDWIDTH_PARAM          "bandwidth"
#define FS_PARAM                 "fs"
#define FREQ_BAND_PARAM          "freq_band"
#define N_FRAMES_PARAM           "n_frames"
#define N_ANT_PARAM              "n_ant"
#define N_ID_CELL_PARAM          "n_id_cell"
#define MCC_PARAM                "mcc"
#define MNC_PARAM                "mnc"
#define CELL_ID_PARAM            "cell_id"
#define TRACKING_AREA_CODE_PARAM "tracking_area_code"
#define Q_RX_LEV_MIN_PARAM       "q_rx_lev_min"
#define P0_NOMINAL_PUSCH_PARAM   "p0_nominal_pusch"
#define P0_NOMINAL_PUCCH_PARAM   "p0_nominal_pucch"
#define SIB3_PRESENT_PARAM       "sib3_present"
#define Q_HYST_PARAM             "q_hyst"
#define SIB4_PRESENT_PARAM       "sib4_present"
#define NEIGH_CELL_LIST_PARAM    "neigh_cell_list"
#define SIB8_PRESENT_PARAM       "sib8_present"
#define SEARCH_WIN_SIZE_PARAM    "search_win_size"
#define PERCENT_LOAD_PARAM       "percent_load"

/*******************************************************************************
                              FORWARD DECLARATIONS
*******************************************************************************/

class LTE_fdd_dl_fg_samp_buf;

/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/

typedef boost::shared_ptr<LTE_fdd_dl_fg_samp_buf> LTE_fdd_dl_fg_samp_buf_sptr;

typedef enum{
    LTE_FDD_DL_FG_OUT_SIZE_INT8 = 0,
    LTE_FDD_DL_FG_OUT_SIZE_GR_COMPLEX,
}LTE_FDD_DL_FG_OUT_SIZE_ENUM;

/*******************************************************************************
                              CLASS DECLARATIONS
*******************************************************************************/

LTE_FDD_DL_FG_API LTE_fdd_dl_fg_samp_buf_sptr LTE_fdd_dl_fg_make_samp_buf(size_t out_size_val);
class LTE_FDD_DL_FG_API LTE_fdd_dl_fg_samp_buf : public gr::sync_block
{
public:
    ~LTE_fdd_dl_fg_samp_buf();

    int32 work(int32                      noutput_items,
               gr_vector_const_void_star &input_items,
               gr_vector_void_star       &output_items);

private:
    friend LTE_FDD_DL_FG_API LTE_fdd_dl_fg_samp_buf_sptr LTE_fdd_dl_fg_make_samp_buf(size_t out_size_val);

    LTE_fdd_dl_fg_samp_buf(size_t out_size_val);

    // Input parameters
    LTE_FDD_DL_FG_OUT_SIZE_ENUM out_size;

    // Sample buffer
    float  *i_buf;
    float  *q_buf;
    uint32  samp_buf_idx;
    bool    samples_ready;
    bool    last_samp_was_i;

    // LTE parameters
    void recreate_sched_info(void);
    LIBLTE_BIT_MSG_STRUCT                    rrc_msg;
    LIBLTE_RRC_MIB_STRUCT                    mib;
    LIBLTE_RRC_BCCH_DLSCH_MSG_STRUCT         bcch_dlsch_msg;
    LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_1_STRUCT  sib1;
    LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_2_STRUCT  sib2;
    LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_3_STRUCT  sib3;
    LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_4_STRUCT  sib4;
    LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_8_STRUCT  sib8;
    LIBLTE_PHY_STRUCT                       *phy_struct;
    LIBLTE_PHY_PCFICH_STRUCT                 pcfich;
    LIBLTE_PHY_PHICH_STRUCT                  phich;
    LIBLTE_PHY_PDCCH_STRUCT                  pdcch;
    LIBLTE_PHY_SUBFRAME_STRUCT               subframe;
    LIBLTE_PHY_FS_ENUM                       fs;
    float                                    phich_res;
    float                                    bandwidth;
    uint32                                   sfn;
    uint32                                   N_frames;
    uint32                                   N_id_cell;
    uint32                                   N_id_1;
    uint32                                   N_id_2;
    uint32                                   N_rb_dl;
    uint32                                   si_periodicity_T;
    uint32                                   si_win_len;
    uint32                                   sib_tx_mode;
    uint32                                   percent_load;
    uint8                                    N_ant;
    uint8                                    sib3_present;
    uint8                                    sib4_present;
    uint8                                    sib8_present;

    // Configuration
    void print_config(void);
    void change_config(char *line);
    bool set_bandwidth(char *char_value);
    bool set_fs(char *char_value);
    bool set_n_ant(char *char_value);
    bool set_n_id_cell(char *char_value);
    bool set_mcc(char *char_value);
    bool set_mnc(char *char_value);
    bool set_q_hyst(char *char_value);
    bool set_neigh_cell_list(char *char_value);
    bool set_q_offset_range(LIBLTE_RRC_Q_OFFSET_RANGE_ENUM *q_offset_range, char *char_value);
    bool set_param(uint32 *param, char *char_value, uint32 llimit, uint32 ulimit);
    bool set_param(uint16 *param, char *char_value, uint16 llimit, uint16 ulimit);
    bool set_param(uint8 *param, char *char_value, uint8 llimit, uint8 ulimit);
    bool set_param(int32 *param, char *char_value, int32 llimit, int32 ulimit);
    bool set_param(int16 *param, char *char_value, int16 llimit, int16 ulimit);
    bool set_param(int8 *param, char *char_value, int8 llimit, int8 ulimit);
    bool char_to_uint32(char *char_value, uint32 *uint32_value);
    bool char_to_int32(char *char_value, int32 *int32_value);
    bool need_config;
};

#endif /* __LTE_FDD_DL_FG_SAMP_BUF_H__ */
