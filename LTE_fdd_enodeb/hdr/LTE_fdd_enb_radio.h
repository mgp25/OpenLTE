/*******************************************************************************

    Copyright 2013-2017 Ben Wojtowicz
    Copyright 2016 Przemek Bereski (bladeRF support)

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

    File: LTE_fdd_enb_radio.h

    Description: Contains all the definitions for the LTE FDD eNodeB
                 radio device control.

    Revision History
    ----------    -------------    --------------------------------------------
    11/09/2013    Ben Wojtowicz    Created file
    01/18/2014    Ben Wojtowicz    Handling EARFCN updates and multiple
                                   antennas.
    06/15/2014    Ben Wojtowicz    Changed fn_combo to current_tti.
    07/22/2014    Ben Wojtowicz    Added clock source as a configurable
                                   parameter.
    07/25/2015    Ben Wojtowicz    Added parameters to abstract PHY sample rate
                                   from radio sample rate.
    12/06/2015    Ben Wojtowicz    Changed boost::mutex to sem_t.
    07/03/2016    Ben Wojtowicz    Massive restructuring to support the addition
                                   of bladeRF as a radio choice.
    07/03/2016    Przemek Bereski  Addition of bladeRF as a radio choice.
    07/29/2017    Ben Wojtowicz    Using the latest tools library.

*******************************************************************************/

#ifndef __LTE_FDD_ENB_RADIO_H__
#define __LTE_FDD_ENB_RADIO_H__

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_fdd_enb_interface.h"
#include "liblte_phy.h"
#include "libtools_ipc_msgq.h"
#include <gnuradio/gr_complex.h>
#include <uhd/usrp/multi_usrp.hpp>
#include <libbladeRF.h>

/*******************************************************************************
                              DEFINES
*******************************************************************************/


/*******************************************************************************
                              FORWARD DECLARATIONS
*******************************************************************************/


/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/

typedef enum{
    LTE_FDD_ENB_RADIO_TYPE_NO_RF = 0,
    LTE_FDD_ENB_RADIO_TYPE_USRP_B2X0,
    LTE_FDD_ENB_RADIO_TYPE_BLADERF,
}LTE_FDD_ENB_RADIO_TYPE_ENUM;

typedef struct{
    std::string                 name;
    LTE_FDD_ENB_RADIO_TYPE_ENUM type;
}LTE_FDD_ENB_RADIO_STRUCT;

typedef struct{
    LTE_FDD_ENB_RADIO_STRUCT radio[100];
    uint32                   num_radios;
}LTE_FDD_ENB_AVAILABLE_RADIOS_STRUCT;

typedef LIBTOOLS_IPC_MSGQ_PHY_SAMPS_MSG_STRUCT LTE_FDD_ENB_RADIO_TX_BUF_STRUCT;
typedef LIBTOOLS_IPC_MSGQ_PHY_SAMPS_MSG_STRUCT LTE_FDD_ENB_RADIO_RX_BUF_STRUCT;

class LTE_fdd_enb_phy;
typedef struct{
    LTE_fdd_enb_interface           *interface;
    LTE_fdd_enb_phy                 *phy;
    LTE_FDD_ENB_RADIO_TX_BUF_STRUCT  tx_radio_buf[2];
    LTE_FDD_ENB_RADIO_RX_BUF_STRUCT  rx_radio_buf[2];
    uint32                           N_subfrs_dropped;
    uint32                           samp_rate;
    uint32                           buf_idx;
    uint32                           recv_idx;
    uint32                           samp_idx;
    uint32                           num_samps;
    uint32                           N_samps_per_subfr;
    uint32                           fs;
    uint16                           rx_current_tti;
    uint8                            N_ant;
    bool                             init_needed;
    bool                             rx_synced;
}LTE_FDD_ENB_RADIO_PARAMS_STRUCT;

/*******************************************************************************
                              CLASS DECLARATIONS
*******************************************************************************/

class LTE_fdd_enb_radio_no_rf
{
public:
    // Constructor/Destructor
    LTE_fdd_enb_radio_no_rf();
    ~LTE_fdd_enb_radio_no_rf();

    // Radio functions
    LTE_FDD_ENB_ERROR_ENUM setup(void);
    void teardown(void);
    void send(void);
    void receive(LTE_FDD_ENB_RADIO_PARAMS_STRUCT *radio_params);

private:
    // Parameters
    struct timespec sleep_time;
};

class LTE_fdd_enb_radio_usrp_b2x0
{
public:
    // Constructor/Destructor
    LTE_fdd_enb_radio_usrp_b2x0();
    ~LTE_fdd_enb_radio_usrp_b2x0();

    // Radio functions
    LTE_FDD_ENB_ERROR_ENUM setup(uint32 idx, double bw, int16 dl_earfcn, int16 ul_earfcn, std::string *clock_src, uint32 samp_rate, uint32 tx_gain, uint32 rx_gain);
    void teardown(void);
    void send(LTE_FDD_ENB_RADIO_TX_BUF_STRUCT *buf, LTE_FDD_ENB_RADIO_PARAMS_STRUCT *radio_params);
    void receive(LTE_FDD_ENB_RADIO_PARAMS_STRUCT *radio_params);

private:
    // Parameters
    uhd::usrp::multi_usrp::sptr usrp;
    uhd::tx_streamer::sptr      tx_stream;
    uhd::rx_streamer::sptr      rx_stream;
    uhd::rx_metadata_t          metadata;
    uhd::time_spec_t            next_tx_ts;
    uhd::time_spec_t            next_rx_ts;
    uhd::time_spec_t            next_rx_subfr_ts;
    uhd::time_spec_t            check_ts;
    gr_complex                  tx_buf[LIBLTE_PHY_N_SAMPS_PER_SUBFR_30_72MHZ];
    gr_complex                  rx_buf[LIBLTE_PHY_N_SAMPS_PER_SUBFR_30_72MHZ];
    int64                       next_rx_ts_ticks;
    int64                       metadata_ts_ticks;
    uint32                      N_tx_samps;
    uint32                      N_rx_samps;
    uint32                      recv_size;
    uint16                      next_tx_current_tti;
};

class LTE_fdd_enb_radio_bladerf
{
public:
    // Constructor/Destructor
    LTE_fdd_enb_radio_bladerf();
    ~LTE_fdd_enb_radio_bladerf();

    // Radio functions
    LTE_FDD_ENB_ERROR_ENUM setup(uint32 idx, double bandwidth, int16 dl_earfcn, int16 ul_earfcn, uint8 N_ant, uint32 samp_rate, uint32 N_samps_per_subfr);
    void teardown(void);
    void send(LTE_FDD_ENB_RADIO_TX_BUF_STRUCT *buf, LTE_FDD_ENB_RADIO_PARAMS_STRUCT *radio_params);
    void receive(LTE_FDD_ENB_RADIO_PARAMS_STRUCT *radio_params);

private:
    // Parameters
    struct bladerf   *bladerf;
    bladerf_metadata  metadata_tx;
    bladerf_metadata  metadata_rx;
    uint64            next_tx_ts;
    uint64            rx_ts;
    uint16            next_tx_current_tti;
    int16             tx_buf[LIBLTE_PHY_N_SAMPS_PER_SUBFR_30_72MHZ*2];
    int16             rx_buf[LIBLTE_PHY_N_SAMPS_PER_SUBFR_30_72MHZ*2];
    bool              first_tx_sample;
};

class LTE_fdd_enb_radio
{
public:
    // Singleton
    static LTE_fdd_enb_radio* get_instance(void);
    static void cleanup(void);

    // Start/Stop
    bool is_started(void);
    LTE_FDD_ENB_ERROR_ENUM start(void);
    LTE_FDD_ENB_ERROR_ENUM stop(void);

    // External interface
    LTE_FDD_ENB_AVAILABLE_RADIOS_STRUCT get_available_radios(void);
    LTE_FDD_ENB_RADIO_STRUCT get_selected_radio(void);
    uint32 get_selected_radio_idx(void);
    LTE_FDD_ENB_ERROR_ENUM set_selected_radio_idx(uint32 idx);
    LTE_FDD_ENB_RADIO_TYPE_ENUM get_selected_radio_type(void);
    uint32 get_tx_gain(void);
    LTE_FDD_ENB_ERROR_ENUM set_tx_gain(uint32 gain);
    uint32 get_rx_gain(void);
    LTE_FDD_ENB_ERROR_ENUM set_rx_gain(uint32 gain);
    std::string get_clock_source(void);
    LTE_FDD_ENB_ERROR_ENUM set_clock_source(std::string source);
    uint32 get_phy_sample_rate(void);
    uint32 get_radio_sample_rate(void);
    void send(LTE_FDD_ENB_RADIO_TX_BUF_STRUCT *buf);
    pthread_t get_radio_thread(void);

private:
    // Singleton
    static LTE_fdd_enb_radio *instance;
    LTE_fdd_enb_radio();
    ~LTE_fdd_enb_radio();

    // Start/Stop
    sem_t start_sem;
    bool  started;

    // Generic radios
    static void* radio_thread_func(void *inputs);
    pthread_t                           radio_thread;
    std::string                         clock_source;
    LTE_FDD_ENB_RADIO_PARAMS_STRUCT     radio_params;
    LTE_FDD_ENB_AVAILABLE_RADIOS_STRUCT available_radios;
    LTE_FDD_ENB_RADIO_TYPE_ENUM         selected_radio_type;
    uint32                              selected_radio_idx;
    uint32                              tx_gain;
    uint32                              rx_gain;

    // Specific radios
    LTE_fdd_enb_radio_no_rf     no_rf;
    LTE_fdd_enb_radio_usrp_b2x0 usrp_b2x0;
    LTE_fdd_enb_radio_bladerf   bladerf;
};

#endif /* __LTE_FDD_ENB_RADIO_H__ */
