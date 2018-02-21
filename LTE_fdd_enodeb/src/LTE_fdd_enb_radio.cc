#line 2 "LTE_fdd_enb_radio.cc" // Make __FILE__ omit the path
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

    File: LTE_fdd_enb_radio.cc

    Description: Contains all the implementations for the LTE FDD eNodeB
                 radio device control.

    Revision History
    ----------    -------------    --------------------------------------------
    11/10/2013    Ben Wojtowicz    Created file
    12/30/2013    Ben Wojtowicz    Changed the setting of the thread priority
                                   to use the uhd method and fixed a bug with
                                   baseband saturation in transmit.
    01/18/2014    Ben Wojtowicz    Handling multiple antennas, added ability to
                                   update EARFCNs, and fixed sleep time for
                                   no_rf case.
    04/12/2014    Ben Wojtowicz    Pulled in a patch from Max Suraev for more
                                   descriptive start failures.
    06/15/2014    Ben Wojtowicz    Changed fn_combo to current_tti.
    07/22/2014    Ben Wojtowicz    Added clock source as a configurable
                                   parameter.
    08/03/2014    Ben Wojtowicz    Fixed clock_source bug.
    09/03/2014    Ben Wojtowicz    Fixed stop issue.
    12/16/2014    Ben Wojtowicz    Pulled in a patch from Ruben Merz to add
                                   USRP X300 support.
    12/24/2014    Ben Wojtowicz    Added more time spec information in debug.
    07/25/2015    Ben Wojtowicz    Added parameters to abstract PHY sample rate
                                   from radio sample rate.
    12/06/2015    Ben Wojtowicz    Changed boost::mutex to pthread_mutex_t and
                                   sem_t.
    07/03/2016    Ben Wojtowicz    Massive restructuring to support the addition
                                   of bladeRF as a radio choice and setting
                                   processor affinity.
    07/03/2016    Przemek Bereski  Addition of bladeRF as a radio choice.
    10/09/2016    Ben Wojtowicz    Added typecast for bladerf_get_timestamp().
    07/29/2017    Ben Wojtowicz    Using the latest tools library.

*******************************************************************************/

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_fdd_enb_radio.h"
#include "LTE_fdd_enb_phy.h"
#include "liblte_interface.h"
#include "libtools_scoped_lock.h"
#include <uhd/device.hpp>
#include <uhd/types/device_addr.hpp>
#include <uhd/property_tree.hpp>
#include <uhd/utils/thread_priority.hpp>

/*******************************************************************************
                              DEFINES
*******************************************************************************/

// Change this to 1 to turn on RADIO DEBUG
#define EXTRA_RADIO_DEBUG 0

// bladeRF defines
#define BLADERF_NUM_BUFFERS   256
#define BLADERF_NUM_TRANSFERS 32
#define BLADERF_TIMEOUT_MS    4000

/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/


/*******************************************************************************
                              GLOBAL VARIABLES
*******************************************************************************/

LTE_fdd_enb_radio*     LTE_fdd_enb_radio::instance = NULL;
static pthread_mutex_t radio_instance_mutex        = PTHREAD_MUTEX_INITIALIZER;

/*******************************************************************************
                              LOCAL FUNCTIONS
*******************************************************************************/

static uint32 find_usrps(LTE_FDD_ENB_AVAILABLE_RADIOS_STRUCT *radios)
{
    uhd::device_addr_t  hint;
    uhd::device_addrs_t devs = uhd::device::find(hint);
    size_t              i;

    if(NULL != radios)
    {
        for(i=0; i<devs.size(); i++)
        {
            radios->radio[radios->num_radios].name   = devs[i].to_string();
            radios->radio[radios->num_radios++].type = LTE_FDD_ENB_RADIO_TYPE_USRP_B2X0;
        }
    }

    return(devs.size());
}

static uint32 find_usrps(void)
{
    return(find_usrps(NULL));
}

static uint32 find_bladerfs(LTE_FDD_ENB_AVAILABLE_RADIOS_STRUCT *radios)
{
    struct bladerf_devinfo *devs;
    size_t                  i;
    int                     N_devs;

    N_devs = bladerf_get_device_list(&devs);
    if(N_devs < 0)
    {
        return(0);
    }
    if(NULL != radios)
    {
        for(i=0; i<N_devs; i++)
        {
            radios->radio[radios->num_radios].name    = "bladerf-";
            radios->radio[radios->num_radios].name   += devs[i].serial;
            radios->radio[radios->num_radios++].type  = LTE_FDD_ENB_RADIO_TYPE_BLADERF;
        }
    }
    bladerf_free_device_list(devs);

    return(N_devs);
}

static uint32 find_bladerfs(void)
{
    return(find_bladerfs(NULL));
}

/*******************************************************************************
                              CLASS IMPLEMENTATIONS
*******************************************************************************/

/**************************************/
/*    No-RF Constructor/Destructor    */
/**************************************/
LTE_fdd_enb_radio_no_rf::LTE_fdd_enb_radio_no_rf()
{
    // Sleep time between subframes
    sleep_time.tv_sec  = 0;
    sleep_time.tv_nsec = 1000000;
}
LTE_fdd_enb_radio_no_rf::~LTE_fdd_enb_radio_no_rf()
{
}

/*******************************/
/*    No-RF Radio Functions    */
/*******************************/
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_radio_no_rf::setup(void)
{
    return(LTE_FDD_ENB_ERROR_NONE);
}
void LTE_fdd_enb_radio_no_rf::teardown(void)
{
}
void LTE_fdd_enb_radio_no_rf::send(void)
{
}
void LTE_fdd_enb_radio_no_rf::receive(LTE_FDD_ENB_RADIO_PARAMS_STRUCT *radio_params)
{
    struct timespec time_rem;

    if(radio_params->init_needed)
    {
        // Signal PHY to generate first subframe
        radio_params->phy->radio_interface(&radio_params->tx_radio_buf[1]);
        radio_params->init_needed = false;
    }
    radio_params->rx_radio_buf[radio_params->buf_idx].current_tti = radio_params->rx_current_tti;
    radio_params->phy->radio_interface(&radio_params->tx_radio_buf[radio_params->buf_idx],
                                       &radio_params->rx_radio_buf[radio_params->buf_idx]);
    radio_params->buf_idx        = (radio_params->buf_idx + 1) % 2;
    radio_params->rx_current_tti = (radio_params->rx_current_tti + 1) % (LTE_FDD_ENB_CURRENT_TTI_MAX + 1);
    nanosleep(&sleep_time, &time_rem);
}

/******************************************/
/*    USRP-B2X0 Constructor/Destructor    */
/******************************************/
LTE_fdd_enb_radio_usrp_b2x0::LTE_fdd_enb_radio_usrp_b2x0()
{
}
LTE_fdd_enb_radio_usrp_b2x0::~LTE_fdd_enb_radio_usrp_b2x0()
{
}

/***********************************/
/*    USRP-B2X0 Radio Functions    */
/***********************************/
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_radio_usrp_b2x0::setup(uint32       idx,
                                                          double       bw,
                                                          int16        dl_earfcn,
                                                          int16        ul_earfcn,
                                                          std::string *clock_src,
                                                          uint32       samp_rate,
                                                          uint32       tx_gain,
                                                          uint32       rx_gain)
{
    uhd::device_addr_t     hint;
    uhd::device_addrs_t    devs = uhd::device::find(hint);
    uhd::stream_args_t     stream_args("fc32");
    LTE_FDD_ENB_ERROR_ENUM err              = LTE_FDD_ENB_ERROR_CANT_START;
    bool                   master_clock_set = false;

    try
    {
        // Setup the USRP
        if(devs[idx-1]["type"] == "x300")
        {
            devs[idx-1]["master_clock_rate"] = "184320000";
            master_clock_set                 = true;
        }
        usrp = uhd::usrp::multi_usrp::make(devs[idx-1]);
        usrp->set_clock_source(*clock_src);
        if(!master_clock_set)
        {
            usrp->set_master_clock_rate(30720000);
            if(2.0 >= fabs(usrp->get_master_clock_rate() - 30720000.0))
            {
                master_clock_set = true;
            }
        }
        if(master_clock_set)
        {
            usrp->set_tx_rate(samp_rate);
            usrp->set_rx_rate(samp_rate);
            usrp->set_tx_freq((double)liblte_interface_dl_earfcn_to_frequency(dl_earfcn));
            usrp->set_rx_freq((double)liblte_interface_ul_earfcn_to_frequency(ul_earfcn));
            usrp->set_tx_gain(tx_gain);
            usrp->set_rx_gain(rx_gain);

            // Setup the TX and RX streams
            tx_stream  = usrp->get_tx_stream(stream_args);
            rx_stream  = usrp->get_rx_stream(stream_args);
            N_tx_samps = tx_stream->get_max_num_samps();
            N_rx_samps = rx_stream->get_max_num_samps();
            if(N_rx_samps > LIBLTE_PHY_N_SAMPS_PER_SUBFR_1_92MHZ &&
               N_tx_samps > LIBLTE_PHY_N_SAMPS_PER_SUBFR_1_92MHZ)
            {
                N_rx_samps = LIBLTE_PHY_N_SAMPS_PER_SUBFR_1_92MHZ;
                N_tx_samps = LIBLTE_PHY_N_SAMPS_PER_SUBFR_1_92MHZ;

                err = LTE_FDD_ENB_ERROR_NONE;
            }
            recv_size = N_rx_samps;
        }else{
            err = LTE_FDD_ENB_ERROR_MASTER_CLOCK_FAIL;
        }
    }catch(...){
        // Nothing to do here
    }

    return(err);
}
void LTE_fdd_enb_radio_usrp_b2x0::teardown(void)
{
    uhd::stream_cmd_t cmd = uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS;

    usrp->issue_stream_cmd(cmd);
}
void LTE_fdd_enb_radio_usrp_b2x0::send(LTE_FDD_ENB_RADIO_TX_BUF_STRUCT *buf,
                                       LTE_FDD_ENB_RADIO_PARAMS_STRUCT *radio_params)
{
    uhd::tx_metadata_t metadata;
    uint32             samps_to_send = radio_params->N_samps_per_subfr;
    uint32             idx           = 0;
    uint32             i;
    uint32             p;
    uint16             N_skipped_subfrs;

    // Setup metadata
    metadata.has_time_spec  = true;
    metadata.start_of_burst = false;
    metadata.end_of_burst   = false;

    // Check current_tti
    if(buf->current_tti != next_tx_current_tti)
    {
        if(buf->current_tti > next_tx_current_tti)
        {
            N_skipped_subfrs = buf->current_tti - next_tx_current_tti;
        }else{
            N_skipped_subfrs = (buf->current_tti + LTE_FDD_ENB_CURRENT_TTI_MAX + 1) - next_tx_current_tti;
        }

        next_tx_ts          += uhd::time_spec_t::from_ticks(N_skipped_subfrs*radio_params->N_samps_per_subfr,
                                                            radio_params->fs);
        next_tx_current_tti  = (buf->current_tti + 1) % (LTE_FDD_ENB_CURRENT_TTI_MAX + 1);
    }else{
        next_tx_current_tti = (next_tx_current_tti + 1) % (LTE_FDD_ENB_CURRENT_TTI_MAX + 1);
    }

    while(samps_to_send > N_tx_samps)
    {
        metadata.time_spec = next_tx_ts;
        for(i=0; i<N_tx_samps; i++)
        {
            tx_buf[i] = gr_complex(buf->i_buf[0][idx+i]/50.0, buf->q_buf[0][idx+i]/50.0);
            for(p=1; p<radio_params->N_ant; p++)
            {
                tx_buf[i] += gr_complex(buf->i_buf[p][idx+i]/50.0, buf->q_buf[p][idx+i]/50.0);
            }
            tx_buf[i] /= radio_params->N_ant;
        }
#if EXTRA_RADIO_DEBUG
        radio_params->interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                                LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                                __FILE__,
                                                __LINE__,
                                                "Sending subfr %lld %u",
                                                metadata.time_spec.to_ticks(fs),
                                                buf->current_tti);
#endif
        tx_stream->send(tx_buf, N_tx_samps, metadata);
        idx           += N_tx_samps;
        samps_to_send -= N_tx_samps;
        next_tx_ts    += uhd::time_spec_t::from_ticks(N_tx_samps,
                                                      radio_params->fs);
    }
    if(0 != samps_to_send)
    {
        metadata.time_spec = next_tx_ts;
        for(i=0; i<samps_to_send; i++)
        {
            tx_buf[i] = gr_complex(buf->i_buf[0][idx+i]/50.0, buf->q_buf[0][idx+i]/50.0);
            for(p=1; p<radio_params->N_ant; p++)
            {
                tx_buf[i] += gr_complex(buf->i_buf[p][idx+i]/50.0, buf->q_buf[p][idx+i]/50.0);
            }
            tx_buf[i] /= radio_params->N_ant;
        }
#if EXTRA_RADIO_DEBUG
        radio_params->interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                                LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                                __FILE__,
                                                __LINE__,
                                                "Sending subfr %lld %u",
                                                metadata.time_spec.to_ticks(fs),
                                                buf->current_tti);
#endif
        tx_stream->send(tx_buf, samps_to_send, metadata);
        next_tx_ts += uhd::time_spec_t::from_ticks(samps_to_send,
                                                   radio_params->fs);
    }
}
void LTE_fdd_enb_radio_usrp_b2x0::receive(LTE_FDD_ENB_RADIO_PARAMS_STRUCT *radio_params)
{
    uhd::stream_cmd_t cmd = uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS;
    uint32            i;

    if(radio_params->init_needed)
    {
        // Setup time specs
        next_tx_ts        = uhd::time_spec_t::from_ticks(radio_params->samp_rate,
                                                         radio_params->samp_rate); // 1 second to make sure everything is setup
        next_rx_ts        = next_tx_ts;
        next_rx_ts       -= uhd::time_spec_t::from_ticks(radio_params->N_samps_per_subfr*2,
                                                         radio_params->samp_rate); // Retard RX by 2 subframes
        next_rx_subfr_ts  = next_rx_ts;

        // Reset USRP time
        usrp->set_time_now(uhd::time_spec_t::from_ticks(0, radio_params->samp_rate));

        // Signal PHY to generate first subframe
        radio_params->phy->radio_interface(&radio_params->tx_radio_buf[1]);

        // Start streaming
        cmd.stream_now = true;
        usrp->issue_stream_cmd(cmd);

        radio_params->init_needed = false;
    }

    if(!radio_params->rx_synced)
    {
        radio_params->num_samps  = rx_stream->recv(rx_buf, recv_size, metadata);
        check_ts                 = metadata.time_spec;
        check_ts                += uhd::time_spec_t::from_ticks(radio_params->num_samps,
                                                                radio_params->samp_rate);

        if(check_ts.to_ticks(radio_params->samp_rate) == next_rx_ts.to_ticks(radio_params->samp_rate))
        {
            radio_params->interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                                    LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                                    __FILE__,
                                                    __LINE__,
                                                    "RX synced %lld %lld",
                                                    check_ts.to_ticks(radio_params->samp_rate),
                                                    next_rx_ts.to_ticks(radio_params->samp_rate));
            radio_params->samp_idx  = 0;
            radio_params->rx_synced = true;
        }else{
            check_ts += uhd::time_spec_t::from_ticks(N_rx_samps,
                                                     radio_params->samp_rate);
            if(check_ts.to_ticks(radio_params->samp_rate) > next_rx_ts.to_ticks(radio_params->samp_rate))
            {
                radio_params->interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                                        LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                                        __FILE__,
                                                        __LINE__,
                                                        "RX modifying recv_size to sync %lld %lld",
                                                        check_ts.to_ticks(radio_params->samp_rate),
                                                        next_rx_ts.to_ticks(radio_params->samp_rate));
                check_ts  -= uhd::time_spec_t::from_ticks(N_rx_samps, radio_params->samp_rate);
                recv_size  = (uint32)(next_rx_ts.to_ticks(radio_params->samp_rate) - check_ts.to_ticks(radio_params->samp_rate));
            }
        }
    }else{
        radio_params->num_samps = rx_stream->recv(rx_buf, N_rx_samps, metadata);
        if(0 != radio_params->num_samps)
        {
            next_rx_ts_ticks  = next_rx_ts.to_ticks(radio_params->samp_rate);
            metadata_ts_ticks = metadata.time_spec.to_ticks(radio_params->samp_rate);
            if(radio_params->num_samps != N_rx_samps)
            {
                radio_params->interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                                        LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                                        __FILE__,
                                                        __LINE__,
                                                        "RX packet size issue %u %u %lld %lld",
                                                        radio_params->num_samps,
                                                        N_rx_samps,
                                                        metadata_ts_ticks,
                                                        next_rx_ts_ticks);
            }
            if((next_rx_ts_ticks - metadata_ts_ticks) > 1)
            {
                // FIXME: Not sure this will ever happen
                radio_params->interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                                        LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                                        __FILE__,
                                                        __LINE__,
                                                        "RX old time spec %lld %lld",
                                                        metadata_ts_ticks,
                                                        next_rx_ts_ticks);
            }else if((metadata_ts_ticks - next_rx_ts_ticks) > 1){
                radio_params->interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                                        LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                                        __FILE__,
                                                        __LINE__,
                                                        "RX overrun %lld %lld",
                                                        metadata_ts_ticks,
                                                        next_rx_ts_ticks);

                // Determine how many subframes we are going to drop
                radio_params->N_subfrs_dropped = ((metadata_ts_ticks - next_rx_ts_ticks)/radio_params->N_samps_per_subfr) + 2;

                // Jump the rx_current_tti
                radio_params->rx_current_tti = (radio_params->rx_current_tti + radio_params->N_subfrs_dropped) % (LTE_FDD_ENB_CURRENT_TTI_MAX + 1);

                // Align the samples coming from the radio
                radio_params->rx_synced  = false;
                next_rx_subfr_ts        += uhd::time_spec_t::from_ticks(radio_params->N_subfrs_dropped*radio_params->N_samps_per_subfr,
                                                                        radio_params->samp_rate);
                next_rx_ts               = next_rx_subfr_ts;
                check_ts                 = metadata.time_spec;
                check_ts                += uhd::time_spec_t::from_ticks(radio_params->num_samps + N_rx_samps,
                                                                        radio_params->samp_rate);

                if(check_ts.to_ticks(radio_params->samp_rate) > next_rx_ts.to_ticks(radio_params->samp_rate))
                {
                    check_ts  -= uhd::time_spec_t::from_ticks(N_rx_samps, radio_params->samp_rate);
                    recv_size  = (uint32)(next_rx_ts.to_ticks(radio_params->samp_rate) - check_ts.to_ticks(radio_params->samp_rate));
                }else{
                    recv_size = N_rx_samps;
                }
            }else{
                // FIXME: May need to realign to 1920 sample boundries
                radio_params->recv_idx  = 0;
                next_rx_ts             += uhd::time_spec_t::from_ticks(radio_params->num_samps,
                                                                       radio_params->samp_rate);
                while(radio_params->num_samps > 0)
                {
                    if((radio_params->samp_idx + radio_params->num_samps) <= radio_params->N_samps_per_subfr)
                    {
                        for(i=0; i<radio_params->num_samps; i++)
                        {
                            radio_params->rx_radio_buf[radio_params->buf_idx].i_buf[0][radio_params->samp_idx+i] = rx_buf[radio_params->recv_idx+i].real();
                            radio_params->rx_radio_buf[radio_params->buf_idx].q_buf[0][radio_params->samp_idx+i] = rx_buf[radio_params->recv_idx+i].imag();
                        }
                        radio_params->samp_idx += radio_params->num_samps;

                        if(radio_params->samp_idx == radio_params->N_samps_per_subfr)
                        {
#if EXTRA_RADIO_DEBUG
                            radio_params->interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                                                    LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                                                    __FILE__,
                                                                    __LINE__,
                                                                    "Receiving subfr %lld %u",
                                                                    next_rx_subfr_ts.to_ticks(radio_params->samp_rate),
                                                                    radio_params->rx_current_tti);
#endif
                            radio_params->rx_radio_buf[radio_params->buf_idx].current_tti = radio_params->rx_current_tti;
                            radio_params->phy->radio_interface(&radio_params->tx_radio_buf[radio_params->buf_idx],
                                                               &radio_params->rx_radio_buf[radio_params->buf_idx]);
                            radio_params->buf_idx         = (radio_params->buf_idx + 1) % 2;
                            radio_params->rx_current_tti  = (radio_params->rx_current_tti + 1) % (LTE_FDD_ENB_CURRENT_TTI_MAX + 1);
                            radio_params->samp_idx        = 0;
                            next_rx_subfr_ts             += uhd::time_spec_t::from_ticks(radio_params->N_samps_per_subfr,
                                                                                         radio_params->samp_rate);
                        }
                        radio_params->num_samps = 0;
                    }else{
                        for(i=0; i<(radio_params->N_samps_per_subfr - radio_params->samp_idx); i++)
                        {
                            radio_params->rx_radio_buf[radio_params->buf_idx].i_buf[0][radio_params->samp_idx+i] = rx_buf[radio_params->recv_idx+i].real();
                            radio_params->rx_radio_buf[radio_params->buf_idx].q_buf[0][radio_params->samp_idx+i] = rx_buf[radio_params->recv_idx+i].imag();
                        }
#if EXTRA_RADIO_DEBUG
                        radio_params->interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                                                LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                                                __FILE__,
                                                                __LINE__,
                                                                "Receiving subfr %lld %u",
                                                                next_rx_subfr_ts.to_ticks(radio_params->samp_rate),
                                                                radio_params->rx_current_tti);
#endif
                        radio_params->rx_radio_buf[radio_params->buf_idx].current_tti = radio_params->rx_current_tti;
                        radio_params->phy->radio_interface(&radio_params->tx_radio_buf[radio_params->buf_idx],
                                                           &radio_params->rx_radio_buf[radio_params->buf_idx]);
                        radio_params->buf_idx         = (radio_params->buf_idx + 1) % 2;
                        radio_params->rx_current_tti  = (radio_params->rx_current_tti + 1) % (LTE_FDD_ENB_CURRENT_TTI_MAX + 1);
                        radio_params->num_samps      -= (radio_params->N_samps_per_subfr - radio_params->samp_idx);
                        radio_params->recv_idx        = (radio_params->N_samps_per_subfr - radio_params->samp_idx);
                        radio_params->samp_idx        = 0;
                        next_rx_subfr_ts             += uhd::time_spec_t::from_ticks(radio_params->N_samps_per_subfr,
                                                                                     radio_params->samp_rate);
                    }
                }
            }
        }else{
            radio_params->interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                                    LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                                    __FILE__,
                                                    __LINE__,
                                                    "RX error %u",
                                                    (uint32)metadata.error_code);
        }
    }
}

/****************************************/
/*    BladeRF Constructor/Destructor    */
/****************************************/
LTE_fdd_enb_radio_bladerf::LTE_fdd_enb_radio_bladerf()
{
    bladerf         = NULL;
    first_tx_sample = true;
    memset(&metadata_tx, 0, sizeof(metadata_tx));
}
LTE_fdd_enb_radio_bladerf::~LTE_fdd_enb_radio_bladerf()
{
}

/*********************************/
/*    BladeRF Radio Functions    */
/*********************************/
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_radio_bladerf::setup(uint32 idx,
                                                        double bandwidth,
                                                        int16  dl_earfcn,
                                                        int16  ul_earfcn,
                                                        uint8  N_ant,
                                                        uint32 samp_rate,
                                                        uint32 N_samps_per_subfr)
{
    LTE_fdd_enb_interface  *interface = LTE_fdd_enb_interface::get_instance();
    struct bladerf_devinfo *devs;
    LTE_FDD_ENB_ERROR_ENUM  err = LTE_FDD_ENB_ERROR_CANT_START;
    uint32                  bladerf_idx;
    uint32                  buffer_size;
    int                     status;

    // Only supporting N_ant=1
    if(1 != N_ant)
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                  __FILE__,
                                  __LINE__,
                                  "Not supported N_ant=%u",
                                  N_ant);
        return(err);
    }

    // Determine the bladerf index
    bladerf_idx = idx - find_usrps() - 1;
    if(bladerf_idx >= find_bladerfs())
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                  __FILE__,
                                  __LINE__,
                                  "Incorrect bladerf index %u",
                                  bladerf_idx);
        return(err);
    }

    // Open bladerf device
    bladerf_get_device_list(&devs);
    status = bladerf_open_with_devinfo(&bladerf, &devs[bladerf_idx]);
    if(0 != status)
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                  __FILE__,
                                  __LINE__,
                                  "Failed to open device: %s",
                                  bladerf_strerror(status));
        return(err);
    }

    // Set RX frequency
    status = bladerf_set_frequency(bladerf,
                                   BLADERF_MODULE_RX,
                                   liblte_interface_ul_earfcn_to_frequency(ul_earfcn));
    if(0 != status)
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                  __FILE__,
                                  __LINE__,
                                  "Failed to set RX frequency: %s",
                                  bladerf_strerror(status));
        bladerf_close(bladerf);
        return(err);
    }

    // Set RX sample rate
    status = bladerf_set_sample_rate(bladerf,
                                     BLADERF_MODULE_RX,
                                     samp_rate,
                                     NULL);
    if(0 != status)
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                  __FILE__,
                                  __LINE__,
                                  "Failed to set RX sample rate: %s",
                                  bladerf_strerror(status));
        bladerf_close(bladerf);
        return(err);
    }

    // Set RX bandwidth
    status = bladerf_set_bandwidth(bladerf,
                                   BLADERF_MODULE_RX,
                                   (uint32)(bandwidth * 1000000),
                                   NULL);
    if(0 != status)
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                  __FILE__,
                                  __LINE__,
                                  "Failed to set RX bandwidth: %s",
                                  bladerf_strerror(status));
        bladerf_close(bladerf);
        return(err);
    }

    // Set LNA gain
    // FIXME: rx_gain
    status = bladerf_set_lna_gain(bladerf,
                                  BLADERF_LNA_GAIN_MID);
    if(0 != status)
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                  __FILE__,
                                  __LINE__,
                                  "Failed to set LNA gain: %s",
                                  bladerf_strerror(status));
        bladerf_close(bladerf);
        return(err);
    }

    // Set RX VGA1 gain
    // FIXME: rx_gain
    status = bladerf_set_rxvga1(bladerf,
                                BLADERF_RXVGA1_GAIN_MIN);
    if(0 != status)
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                  __FILE__,
                                  __LINE__,
                                  "Failed to set RX VGA1 gain: %s",
                                  bladerf_strerror(status));
        bladerf_close(bladerf);
        return(err);
    }

    // Set RX VGA2 gain
    // FIXME: rx_gain
    status = bladerf_set_rxvga2(bladerf,
                                BLADERF_RXVGA2_GAIN_MIN);
    if(0 != status)
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                  __FILE__,
                                  __LINE__,
                                  "Failed to set RX VGA2 gain: %s",
                                  bladerf_strerror(status));
        bladerf_close(bladerf);
        return(err);
    }

    // Set TX frequency
    status = bladerf_set_frequency(bladerf,
                                   BLADERF_MODULE_TX,
                                   liblte_interface_dl_earfcn_to_frequency(dl_earfcn));
    if(0 != status)
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                  __FILE__,
                                  __LINE__,
                                  "Failed to set TX frequency: %s",
                                  bladerf_strerror(status));
        bladerf_close(bladerf);
        return(err);
    }

    // Set TX sample rate
    status = bladerf_set_sample_rate(bladerf,
                                     BLADERF_MODULE_TX,
                                     samp_rate,
                                     NULL);
    if(0 != status)
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                  __FILE__,
                                  __LINE__,
                                  "Failed to set TX sample rate: %s",
                                  bladerf_strerror(status));
        bladerf_close(bladerf);
        return(err);
    }

    // Set TX bandwidth
    status = bladerf_set_bandwidth(bladerf,
                                   BLADERF_MODULE_TX,
                                   (uint32)(bandwidth * 1000000),
                                   NULL);
    if(0 != status)
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                  __FILE__,
                                  __LINE__,
                                  "Failed to set TX bandwidth: %s",
                                  bladerf_strerror(status));
        bladerf_close(bladerf);
        return(err);
    }

    // Set TX VGA1 gain
    // FIXME: tx_gain
    status = bladerf_set_txvga1(bladerf,
                                BLADERF_TXVGA1_GAIN_MAX);
    if(0 != status)
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                  __FILE__,
                                  __LINE__,
                                  "Failed to set TX VGA1 gain: %s",
                                  bladerf_strerror(status));
        bladerf_close(bladerf);
        return(err);
    }

    // Set TX VGA2 gain
    // FIXME: tx_gain
    status = bladerf_set_txvga2(bladerf,
                                BLADERF_TXVGA2_GAIN_MAX - 1);
    if(0 != status)
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                  __FILE__,
                                  __LINE__,
                                  "Failed to set TX VGA2 gain: %s",
                                  bladerf_strerror(status));
        bladerf_close(bladerf);
        return(err);
    }

    // Setup sync TX
    if(0 == (N_samps_per_subfr % 1024))
    {
        buffer_size = N_samps_per_subfr;
    }else{
        buffer_size = 1024;
    }
    status = bladerf_sync_config(bladerf,
                                 BLADERF_MODULE_TX,
                                 BLADERF_FORMAT_SC16_Q11_META,
                                 BLADERF_NUM_BUFFERS,
                                 buffer_size,
                                 BLADERF_NUM_TRANSFERS,
                                 BLADERF_TIMEOUT_MS);
    if(0 != status)
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                  __FILE__,
                                  __LINE__,
                                  "Failed to set TX sync config: %s",
                                  bladerf_strerror(status));
        bladerf_close(bladerf);
        return(err);
    }

    // Setup sync RX
    status = bladerf_sync_config(bladerf,
                                 BLADERF_MODULE_RX,
                                 BLADERF_FORMAT_SC16_Q11_META,
                                 BLADERF_NUM_BUFFERS,
                                 buffer_size,
                                 BLADERF_NUM_TRANSFERS,
                                 BLADERF_TIMEOUT_MS);
    if(0 != status)
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                  __FILE__,
                                  __LINE__,
                                  "Failed to set RX sync config: %s",
                                  bladerf_strerror(status));
        bladerf_close(bladerf);
        return(err);
    }

    // Enable TX
    status = bladerf_enable_module(bladerf, BLADERF_MODULE_TX, true);
    if(0 != status)
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                  __FILE__,
                                  __LINE__,
                                  "Failed to enable TX module: %s",
                                  bladerf_strerror(status));
        bladerf_close(bladerf);
        return(err);
    }

    // Enable RX
    status = bladerf_enable_module(bladerf, BLADERF_MODULE_RX, true);
    if(0 != status)
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                  __FILE__,
                                  __LINE__,
                                  "Failed to enable RX module: %s",
                                  bladerf_strerror(status));
        bladerf_close(bladerf);
        return(err);
    }

    return(LTE_FDD_ENB_ERROR_NONE);
}
void LTE_fdd_enb_radio_bladerf::teardown(void)
{
    if(NULL != bladerf)
    {
        bladerf_close(bladerf);
    }
}
void LTE_fdd_enb_radio_bladerf::send(LTE_FDD_ENB_RADIO_TX_BUF_STRUCT *buf,
                                     LTE_FDD_ENB_RADIO_PARAMS_STRUCT *radio_params)
{
    uint32 i;
    uint32 samps_to_send = radio_params->N_samps_per_subfr;
    int    status;
    uint16 N_skipped_subfrs;

    // Check current_tti
    if(buf->current_tti != next_tx_current_tti)
    {
        if(buf->current_tti > next_tx_current_tti)
        {
            N_skipped_subfrs = buf->current_tti - next_tx_current_tti;
        }else{
            N_skipped_subfrs = (buf->current_tti + LTE_FDD_ENB_CURRENT_TTI_MAX + 1) - next_tx_current_tti;
        }

        next_tx_ts            += N_skipped_subfrs * radio_params->N_samps_per_subfr;
        next_tx_current_tti    = (buf->current_tti + 1) % (LTE_FDD_ENB_CURRENT_TTI_MAX + 1);
        metadata_tx.flags      = BLADERF_META_FLAG_TX_UPDATE_TIMESTAMP;
        metadata_tx.timestamp  = next_tx_ts;
    }else{
        next_tx_current_tti = (next_tx_current_tti + 1) % (LTE_FDD_ENB_CURRENT_TTI_MAX + 1);
        metadata_tx.flags   = 0;
    }

    for(i=0; i<samps_to_send; i++)
    {
        tx_buf[(i*2)  ] = (int16_t)(buf->i_buf[0][i] * 40.0);
        tx_buf[(i*2)+1] = (int16_t)(buf->q_buf[0][i] * 40.0);
    }

    if(first_tx_sample)
    {
        metadata_tx.flags     = BLADERF_META_FLAG_TX_BURST_START;
        metadata_tx.timestamp = next_tx_ts;
        first_tx_sample       = false;
    }

    // TX samples
    status = bladerf_sync_tx(bladerf, tx_buf, samps_to_send, &metadata_tx, 1000);
    if(BLADERF_ERR_TIME_PAST == status)
    {
        LTE_fdd_enb_interface *interface = LTE_fdd_enb_interface::get_instance();
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                  __FILE__,
                                  __LINE__,
                                  "TX failed: BLADERF_ERR_TIME_PAST");
    }else if(0 != status){
        LTE_fdd_enb_interface *interface = LTE_fdd_enb_interface::get_instance();
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                  __FILE__,
                                  __LINE__,
                                  "TX failed: %s",
                                  bladerf_strerror(status));
    }else if(BLADERF_META_STATUS_UNDERRUN & metadata_tx.status){
        LTE_fdd_enb_interface *interface = LTE_fdd_enb_interface::get_instance();
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                  LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                  __FILE__,
                                  __LINE__,
                                  "TX failed: BLADERF_META_STATUS_UNDERRUN");
    }

    next_tx_ts += samps_to_send;
}
void LTE_fdd_enb_radio_bladerf::receive(LTE_FDD_ENB_RADIO_PARAMS_STRUCT *radio_params)
{
    uint32 i;
    int    status;

    if(radio_params->init_needed)
    {
        // Assume RX_timestamp and TX_timestamp difference is 0
        bladerf_get_timestamp(bladerf, BLADERF_MODULE_RX, (uint64_t*)&rx_ts);
        next_tx_ts            = rx_ts + radio_params->samp_rate; // 1 second to make sure everything is setup
        metadata_rx.flags     = 0;
        metadata_rx.timestamp = next_tx_ts - (radio_params->N_samps_per_subfr*2); // Retard RX by 2 subframes

        // Signal PHY to generate first subframe
        radio_params->phy->radio_interface(&radio_params->tx_radio_buf[1]);

        radio_params->init_needed = false;
    }else{
        metadata_rx.flags = 0; // Use timestamps
    }

    // RX
    status = bladerf_sync_rx(bladerf, rx_buf, radio_params->N_samps_per_subfr, &metadata_rx, 1000);
    if(0 != status)
    {
        radio_params->interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                                LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                                __FILE__,
                                                __LINE__,
                                                "RX failed: %s",
                                                bladerf_strerror(status));
    }else if(BLADERF_META_STATUS_OVERRUN & metadata_rx.status){
        radio_params->interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                                LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                                __FILE__,
                                                __LINE__,
                                                "RX failed: BLADERF_META_STATUS_OVERRUN");

        // Determine how many subframes we are going to drop
        radio_params->N_subfrs_dropped = 10;

        // Jump the rx_current_tti and timestamp
        radio_params->rx_current_tti  = (radio_params->rx_current_tti + radio_params->N_subfrs_dropped) % (LTE_FDD_ENB_CURRENT_TTI_MAX + 1);
        metadata_rx.timestamp        += radio_params->N_subfrs_dropped * radio_params->N_samps_per_subfr;

        // FIXME: This doesn't recover from the overrun
    }else{
        for(i=0; i<radio_params->N_samps_per_subfr; i++)
        {
            radio_params->rx_radio_buf[radio_params->buf_idx].i_buf[0][i] = rx_buf[(i*2)  ] / 40.0;
            radio_params->rx_radio_buf[radio_params->buf_idx].q_buf[0][i] = rx_buf[(i*2)+1] / 40.0;
        }
        metadata_rx.timestamp                                         += radio_params->N_samps_per_subfr;
        radio_params->rx_radio_buf[radio_params->buf_idx].current_tti  = radio_params->rx_current_tti;
        radio_params->phy->radio_interface(&radio_params->tx_radio_buf[radio_params->buf_idx],
                                           &radio_params->rx_radio_buf[radio_params->buf_idx]);
        radio_params->buf_idx        = (radio_params->buf_idx + 1) % 2;
        radio_params->rx_current_tti = (radio_params->rx_current_tti + 1) % (LTE_FDD_ENB_CURRENT_TTI_MAX + 1);
    }
}

/*******************/
/*    Singleton    */
/*******************/
LTE_fdd_enb_radio* LTE_fdd_enb_radio::get_instance(void)
{
    libtools_scoped_lock lock(radio_instance_mutex);

    if(NULL == instance)
    {
        instance = new LTE_fdd_enb_radio();
    }

    return(instance);
}
void LTE_fdd_enb_radio::cleanup(void)
{
    libtools_scoped_lock lock(radio_instance_mutex);

    if(NULL != instance)
    {
        delete instance;
        instance = NULL;
    }
}

/********************************/
/*    Constructor/Destructor    */
/********************************/
LTE_fdd_enb_radio::LTE_fdd_enb_radio()
{
    // Start/Stop
    sem_init(&start_sem, 0, 1);
    started = false;

    // Setup generic radios
    available_radios.num_radios = 0;
    get_available_radios();
    tx_gain      = 0;
    rx_gain      = 0;
    clock_source = "internal";

    // Setup radio thread
    get_radio_sample_rate();
}
LTE_fdd_enb_radio::~LTE_fdd_enb_radio()
{
    stop();
    sem_destroy(&start_sem);
}

/********************/
/*    Start/Stop    */
/********************/
bool LTE_fdd_enb_radio::is_started(void)
{
    libtools_scoped_lock lock(start_sem);

    return(started);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_radio::start(void)
{
    LTE_fdd_enb_cnfg_db    *cnfg_db = LTE_fdd_enb_cnfg_db::get_instance();
    LTE_FDD_ENB_ERROR_ENUM  err = LTE_FDD_ENB_ERROR_CANT_START;
    double                  bandwidth;
    int64                   dl_earfcn;
    int64                   ul_earfcn;
    int64                   N_ant;

    sem_wait(&start_sem);
    if(false == started)
    {
        // Get the DL and UL EARFCNs
        cnfg_db->get_param(LTE_FDD_ENB_PARAM_DL_EARFCN, dl_earfcn);
        cnfg_db->get_param(LTE_FDD_ENB_PARAM_UL_EARFCN, ul_earfcn);

        // Get the number of TX antennas
        cnfg_db->get_param(LTE_FDD_ENB_PARAM_N_ANT, N_ant);
        radio_params.N_ant = N_ant;

        // Get the bandwidth
        cnfg_db->get_param(LTE_FDD_ENB_PARAM_BANDWIDTH, bandwidth);

        // Setup the appropriate radio
        started = true;
        sem_post(&start_sem);
        switch(get_selected_radio_type())
        {
        case LTE_FDD_ENB_RADIO_TYPE_NO_RF:
            err = no_rf.setup();
            if(LTE_FDD_ENB_ERROR_NONE != err)
            {
                started = false;
                return(err);
            }
            break;
        case LTE_FDD_ENB_RADIO_TYPE_USRP_B2X0:
            err = usrp_b2x0.setup(selected_radio_idx,
                                  bandwidth,
                                  dl_earfcn,
                                  ul_earfcn,
                                  &clock_source,
                                  get_radio_sample_rate(),
                                  tx_gain,
                                  rx_gain);
            if(LTE_FDD_ENB_ERROR_NONE != err)
            {
                started = false;
                return(err);
            }
            break;
        case LTE_FDD_ENB_RADIO_TYPE_BLADERF:
            err = bladerf.setup(selected_radio_idx,
                                bandwidth,
                                dl_earfcn,
                                ul_earfcn,
                                N_ant,
                                get_radio_sample_rate(),
                                radio_params.N_samps_per_subfr);
            if(LTE_FDD_ENB_ERROR_NONE != err)
            {
                started = false;
                return(err);
            }
            break;
        default:
            started = false;
            return(LTE_FDD_ENB_ERROR_OUT_OF_BOUNDS);
            break;
        }

        // Kick off the receiving thread
        pthread_create(&radio_thread, NULL, &radio_thread_func, NULL);
    }else{
        sem_post(&start_sem);
    }

    return(err);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_radio::stop(void)
{
    uhd::stream_cmd_t      cmd = uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS;
    LTE_FDD_ENB_ERROR_ENUM err = LTE_FDD_ENB_ERROR_CANT_STOP;

    sem_wait(&start_sem);
    if(started)
    {
        started = false;
        sem_post(&start_sem);
        switch(get_selected_radio_type())
        {
        case LTE_FDD_ENB_RADIO_TYPE_NO_RF:
            no_rf.teardown();
            break;
        case LTE_FDD_ENB_RADIO_TYPE_USRP_B2X0:
            usrp_b2x0.teardown();
            break;
        case LTE_FDD_ENB_RADIO_TYPE_BLADERF:
            bladerf.teardown();
            break;
        default:
            err = LTE_FDD_ENB_ERROR_OUT_OF_BOUNDS;
            break;
        }
        sleep(1);
        pthread_cancel(radio_thread);
        pthread_join(radio_thread, NULL);
        err = LTE_FDD_ENB_ERROR_NONE;
    }else{
        sem_post(&start_sem);
    }

    return(err);
}

/****************************/
/*    External Interface    */
/****************************/
LTE_FDD_ENB_AVAILABLE_RADIOS_STRUCT LTE_fdd_enb_radio::get_available_radios(void)
{
    uint32 orig_num_radios = available_radios.num_radios;

    available_radios.num_radios = 0;

    // No RF
    available_radios.radio[available_radios.num_radios].name   = "no_rf";
    available_radios.radio[available_radios.num_radios++].type = LTE_FDD_ENB_RADIO_TYPE_NO_RF;

    // USRP devices
    find_usrps(&available_radios);

    // bladerf devices
    find_bladerfs(&available_radios);

    // Reset to sane default
    if(orig_num_radios != available_radios.num_radios)
    {
        if(1 == available_radios.num_radios)
        {
            selected_radio_idx = 0;
        }else{
            selected_radio_idx = 1;
        }
        selected_radio_type = available_radios.radio[selected_radio_idx].type;
    }

    return(available_radios);
}
LTE_FDD_ENB_RADIO_STRUCT LTE_fdd_enb_radio::get_selected_radio(void)
{
    uint32 num_usrps    = find_usrps();
    uint32 num_bladerfs = find_bladerfs();

    if(available_radios.num_radios != (num_usrps + num_bladerfs + 1))
    {
        get_available_radios();
    }

    return(available_radios.radio[selected_radio_idx]);
}
uint32 LTE_fdd_enb_radio::get_selected_radio_idx(void)
{
    uint32 num_usrps    = find_usrps();
    uint32 num_bladerfs = find_bladerfs();

    if(available_radios.num_radios != (num_usrps + num_bladerfs + 1))
    {
        get_available_radios();
    }

    return(selected_radio_idx);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_radio::set_selected_radio_idx(uint32 idx)
{
    uint32                 num_usrps    = find_usrps();
    uint32                 num_bladerfs = find_bladerfs();
    LTE_FDD_ENB_ERROR_ENUM err          = LTE_FDD_ENB_ERROR_OUT_OF_BOUNDS;

    if(available_radios.num_radios != (num_usrps + num_bladerfs + 1))
    {
        get_available_radios();
    }

    if(idx < available_radios.num_radios)
    {
        selected_radio_idx  = idx;
        selected_radio_type = available_radios.radio[selected_radio_idx].type;
        err                 = LTE_FDD_ENB_ERROR_NONE;
    }

    return(err);
}
LTE_FDD_ENB_RADIO_TYPE_ENUM LTE_fdd_enb_radio::get_selected_radio_type(void)
{
    uint32 num_usrps    = find_usrps();
    uint32 num_bladerfs = find_bladerfs();

    if(available_radios.num_radios != (num_usrps + num_bladerfs + 1))
    {
        get_available_radios();
    }

    return(available_radios.radio[selected_radio_idx].type);
}
uint32 LTE_fdd_enb_radio::get_tx_gain(void)
{
    return(tx_gain);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_radio::set_tx_gain(uint32 gain)
{
    libtools_scoped_lock   lock(start_sem);
    LTE_FDD_ENB_ERROR_ENUM err = LTE_FDD_ENB_ERROR_OUT_OF_BOUNDS;

    if(!started)
    {
        tx_gain = gain;
        err     = LTE_FDD_ENB_ERROR_NONE;
    }

    return(err);
}
uint32 LTE_fdd_enb_radio::get_rx_gain(void)
{
    return(rx_gain);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_radio::set_rx_gain(uint32 gain)
{
    libtools_scoped_lock   lock(start_sem);
    LTE_FDD_ENB_ERROR_ENUM err = LTE_FDD_ENB_ERROR_OUT_OF_BOUNDS;

    if(!started)
    {
        rx_gain = gain;
        err     = LTE_FDD_ENB_ERROR_NONE;
    }

    return(err);
}
std::string LTE_fdd_enb_radio::get_clock_source(void)
{
    return(clock_source);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_radio::set_clock_source(std::string source)
{
    libtools_scoped_lock   lock(start_sem);
    LTE_FDD_ENB_ERROR_ENUM err = LTE_FDD_ENB_ERROR_OUT_OF_BOUNDS;

    if("internal" == source ||
       "external" == source)
    {
        if(!started)
        {
            clock_source = source;
            err          = LTE_FDD_ENB_ERROR_NONE;
        }
    }

    return(err);
}
uint32 LTE_fdd_enb_radio::get_phy_sample_rate(void)
{
    LTE_fdd_enb_cnfg_db *cnfg_db = LTE_fdd_enb_cnfg_db::get_instance();
    int64                dl_bw;

    if(!started)
    {
        cnfg_db->get_param(LTE_FDD_ENB_PARAM_DL_BW, dl_bw);
        switch(dl_bw)
        {
        case LIBLTE_RRC_DL_BANDWIDTH_100:
            // Intentional fall-thru
        case LIBLTE_RRC_DL_BANDWIDTH_75:
            radio_params.fs                = 30720000;
            radio_params.N_samps_per_subfr = LIBLTE_PHY_N_SAMPS_PER_SUBFR_30_72MHZ;
            break;
        case LIBLTE_RRC_DL_BANDWIDTH_50:
            radio_params.fs                = 15360000;
            radio_params.N_samps_per_subfr = LIBLTE_PHY_N_SAMPS_PER_SUBFR_15_36MHZ;
            break;
        case LIBLTE_RRC_DL_BANDWIDTH_25:
            radio_params.fs                = 7680000;
            radio_params.N_samps_per_subfr = LIBLTE_PHY_N_SAMPS_PER_SUBFR_7_68MHZ;
            break;
        case LIBLTE_RRC_DL_BANDWIDTH_15:
            radio_params.fs                = 3840000;
            radio_params.N_samps_per_subfr = LIBLTE_PHY_N_SAMPS_PER_SUBFR_3_84MHZ;
            break;
        case LIBLTE_RRC_DL_BANDWIDTH_6:
            // Intentional fall-thru
        default:
            radio_params.fs                = 1920000;
            radio_params.N_samps_per_subfr = LIBLTE_PHY_N_SAMPS_PER_SUBFR_1_92MHZ;
            break;
        }
    }

    return(radio_params.fs);
}
uint32 LTE_fdd_enb_radio::get_radio_sample_rate(void)
{
    libtools_scoped_lock lock(start_sem);

    if(!started)
    {
        radio_params.fs = get_phy_sample_rate();
    }

    return(radio_params.fs);
}
void LTE_fdd_enb_radio::send(LTE_FDD_ENB_RADIO_TX_BUF_STRUCT *buf)
{
    switch(selected_radio_type)
    {
    case LTE_FDD_ENB_RADIO_TYPE_NO_RF:
        no_rf.send();
        break;
    case LTE_FDD_ENB_RADIO_TYPE_USRP_B2X0:
        usrp_b2x0.send(buf, &radio_params);
        break;
    case LTE_FDD_ENB_RADIO_TYPE_BLADERF:
        bladerf.send(buf, &radio_params);
        break;
    default:
        radio_params.interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                               LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                               __FILE__,
                                               __LINE__,
                                               "Invalid radio type %u",
                                               selected_radio_type);
        break;
    }
}
pthread_t LTE_fdd_enb_radio::get_radio_thread(void)
{
    return(radio_thread);
}

/************************/
/*    Generic Radios    */
/************************/
void* LTE_fdd_enb_radio::radio_thread_func(void *inputs)
{
    LTE_fdd_enb_radio           *radio = LTE_fdd_enb_radio::get_instance();
    struct sched_param           priority;
    cpu_set_t                    af_mask;
    LTE_FDD_ENB_RADIO_TYPE_ENUM  radio_type = radio->get_selected_radio_type();

    // Setup radio params
    radio->radio_params.interface         = LTE_fdd_enb_interface::get_instance();
    radio->radio_params.phy               = LTE_fdd_enb_phy::get_instance();
    radio->radio_params.samp_rate         = radio->radio_params.fs;
    radio->radio_params.buf_idx           = 0;
    radio->radio_params.recv_idx          = 0;
    radio->radio_params.samp_idx          = 0;
    radio->radio_params.num_samps         = 0;
    radio->radio_params.rx_current_tti    = (LTE_FDD_ENB_CURRENT_TTI_MAX + 1) - 2;
    radio->radio_params.init_needed       = true;
    radio->radio_params.rx_synced         = false;

    // Set highest priority
    priority.sched_priority = 99;
    pthread_setschedparam(radio->radio_thread, SCHED_FIFO, &priority);

    // Set affinity to the last core for PHY/Radio
    CPU_ZERO(&af_mask);
    CPU_SET(sysconf(_SC_NPROCESSORS_ONLN)-1, &af_mask);
    pthread_setaffinity_np(radio->get_radio_thread(), sizeof(af_mask), &af_mask);

    while(radio->is_started())
    {
        switch(radio_type)
        {
        case LTE_FDD_ENB_RADIO_TYPE_NO_RF:
            radio->no_rf.receive(&radio->radio_params);
            break;
        case LTE_FDD_ENB_RADIO_TYPE_USRP_B2X0:
            radio->usrp_b2x0.receive(&radio->radio_params);
            break;
        case LTE_FDD_ENB_RADIO_TYPE_BLADERF:
            radio->bladerf.receive(&radio->radio_params);
            break;
        default:
            radio->radio_params.interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                                                          LTE_FDD_ENB_DEBUG_LEVEL_RADIO,
                                                          __FILE__,
                                                          __LINE__,
                                                          "Invalid radio type %u",
                                                          radio_type);
            return(NULL);
        }
    }

    return(NULL);
}
