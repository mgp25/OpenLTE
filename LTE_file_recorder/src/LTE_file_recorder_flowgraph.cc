/*******************************************************************************

    Copyright 2013,2015 Ben Wojtowicz

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

    File: LTE_file_recorder_flowgraph.cc

    Description: Contains all the implementations for the LTE file recorder
                 gnuradio flowgraph.

    Revision History
    ----------    -------------    --------------------------------------------
    08/26/2013    Ben Wojtowicz    Created file
    11/13/2013    Ben Wojtowicz    Added support for USRP B2X0.
    11/30/2013    Ben Wojtowicz    Added support for bladeRF.
    12/06/2015    Ben Wojtowicz    Changed boost::mutex to pthread_mutex_t.

*******************************************************************************/

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_file_recorder_flowgraph.h"
#include "libtools_scoped_lock.h"
#include "uhd/usrp/multi_usrp.hpp"

/*******************************************************************************
                              DEFINES
*******************************************************************************/


/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/


/*******************************************************************************
                              GLOBAL VARIABLES
*******************************************************************************/

LTE_file_recorder_flowgraph* LTE_file_recorder_flowgraph::instance = NULL;
static pthread_mutex_t       flowgraph_instance_mutex              = PTHREAD_MUTEX_INITIALIZER;

/*******************************************************************************
                              CLASS IMPLEMENTATIONS
*******************************************************************************/

// Singleton
LTE_file_recorder_flowgraph* LTE_file_recorder_flowgraph::get_instance(void)
{
    libtools_scoped_lock lock(flowgraph_instance_mutex);

    if(NULL == instance)
    {
        instance = new LTE_file_recorder_flowgraph();
    }

    return(instance);
}
void LTE_file_recorder_flowgraph::cleanup(void)
{
    libtools_scoped_lock lock(flowgraph_instance_mutex);

    if(NULL != instance)
    {
        delete instance;
        instance = NULL;
    }
}

// Constructor/Destructor
LTE_file_recorder_flowgraph::LTE_file_recorder_flowgraph()
{
    pthread_mutex_init(&start_mutex, NULL);
    started = false;
}
LTE_file_recorder_flowgraph::~LTE_file_recorder_flowgraph()
{
    libtools_scoped_lock lock(start_mutex);

    if(started)
    {
        pthread_mutex_unlock(&start_mutex);
        stop();
    }
    pthread_mutex_destroy(&start_mutex);
}

// Flowgraph
bool LTE_file_recorder_flowgraph::is_started(void)
{
    libtools_scoped_lock lock(start_mutex);

    return(started);
}
LTE_FILE_RECORDER_STATUS_ENUM LTE_file_recorder_flowgraph::start(uint16      earfcn,
                                                                 std::string file_name)
{
    libtools_scoped_lock            lock(start_mutex);
    LTE_file_recorder_interface    *interface = LTE_file_recorder_interface::get_instance();
    uhd::device_addr_t              hint;
    LTE_FILE_RECORDER_STATUS_ENUM   err           = LTE_FILE_RECORDER_STATUS_FAIL;
    LTE_FILE_RECORDER_HW_TYPE_ENUM  hardware_type = LTE_FILE_RECORDER_HW_TYPE_UNKNOWN;
    double                          mcr;
    uint32                          freq;

    if(!started)
    {
        if(NULL == top_block.get())
        {
            top_block = gr::make_top_block("top");
        }
        if(NULL == samp_src.get())
        {
            osmosdr::source::sptr tmp_src0 = osmosdr::source::make("uhd");
            BOOST_FOREACH(const uhd::device_addr_t &dev, uhd::device::find(hint))
            {
                uhd::usrp::multi_usrp::make(dev)->set_master_clock_rate(30720000);
                mcr = uhd::usrp::multi_usrp::make(dev)->get_master_clock_rate();
            }
            if(0 != tmp_src0->get_sample_rates().size() &&
               1 >= fabs(mcr - 30720000))
            {
                hardware_type = LTE_FILE_RECORDER_HW_TYPE_USRP;
                samp_src      = tmp_src0;
            }else{
                osmosdr::source::sptr tmp_src1 = osmosdr::source::make("hackrf");
                if(0 != tmp_src1->get_sample_rates().size())
                {
                    hardware_type = LTE_FILE_RECORDER_HW_TYPE_HACKRF;
                    samp_src      = tmp_src1;
                }else{
                    osmosdr::source::sptr tmp_src2 = osmosdr::source::make("bladerf");
                    if(0 != tmp_src2->get_sample_rates().size())
                    {
                        hardware_type = LTE_FILE_RECORDER_HW_TYPE_BLADERF;
                        samp_src      = tmp_src2;
                    }else{
                        osmosdr::source::sptr tmp_src3 = osmosdr::source::make("rtl=0");
                        if(0 != tmp_src3->get_sample_rates().size())
                        {
                            hardware_type = LTE_FILE_RECORDER_HW_TYPE_RTL_SDR;
                            samp_src      = tmp_src3;
                        }else{
                            samp_src = osmosdr::source::make();
                        }
                    }
                }
            }
        }
        if(NULL == file_sink.get())
        {
            file_sink = gr::blocks::file_sink::make(sizeof(gr_complex), file_name.c_str());
        }

        if(NULL != top_block.get() &&
           NULL != samp_src.get()  &&
           NULL != file_sink.get())
        {
            if(0 != samp_src->get_num_channels())
            {
                switch(hardware_type)
                {
                case LTE_FILE_RECORDER_HW_TYPE_USRP:
                    samp_src->set_sample_rate(15360000);
                    samp_src->set_gain_mode(false);
                    samp_src->set_gain(35);
                    samp_src->set_bandwidth(10000000);
                    break;
                case LTE_FILE_RECORDER_HW_TYPE_HACKRF:
                    samp_src->set_sample_rate(15360000);
                    samp_src->set_gain_mode(false);
                    samp_src->set_gain(14);
                    samp_src->set_dc_offset_mode(osmosdr::source::DCOffsetAutomatic);
                    break;
                case LTE_FILE_RECORDER_HW_TYPE_BLADERF:
                    samp_src->set_sample_rate(15360000);
                    samp_src->set_gain_mode(false);
                    samp_src->set_gain(6, "LNA");
                    samp_src->set_gain(33, "VGA1");
                    samp_src->set_gain(3, "VGA2");
                    samp_src->set_bandwidth(10000000);
                    break;
                case LTE_FILE_RECORDER_HW_TYPE_UNKNOWN:
                default:
                    printf("Unknown hardware, treating like RTL-SDR\n");
                case LTE_FILE_RECORDER_HW_TYPE_RTL_SDR:
                    samp_src->set_sample_rate(1920000);
                    samp_src->set_gain_mode(true);
                    break;
                }

                freq = liblte_interface_dl_earfcn_to_frequency(earfcn);
                if(freq == 0)
                {
                    freq = liblte_interface_ul_earfcn_to_frequency(earfcn);
                }
                samp_src->set_center_freq(freq);
                top_block->connect(samp_src, 0, file_sink, 0);
                if(0 == pthread_create(&start_thread, NULL, &run_thread, this))
                {
                    err     = LTE_FILE_RECORDER_STATUS_OK;
                    started = true;
                }else{
                    top_block->disconnect_all();
                }
            }else{
                samp_src.reset();
            }
        }
    }

    return(err);
}
LTE_FILE_RECORDER_STATUS_ENUM LTE_file_recorder_flowgraph::stop(void)
{
    libtools_scoped_lock          lock(start_mutex);
    LTE_FILE_RECORDER_STATUS_ENUM err = LTE_FILE_RECORDER_STATUS_FAIL;

    if(started)
    {
        started = false;
        pthread_mutex_unlock(&start_mutex);
        top_block->stop();
        sleep(1); // Wait for state_machine to exit
        pthread_cancel(start_thread);
        pthread_join(start_thread, NULL);
        top_block.reset();
        err = LTE_FILE_RECORDER_STATUS_OK;
    }

    return(err);
}

// Run
void* LTE_file_recorder_flowgraph::run_thread(void *inputs)
{
    LTE_file_recorder_interface *interface = LTE_file_recorder_interface::get_instance();
    LTE_file_recorder_flowgraph *flowgraph = (LTE_file_recorder_flowgraph *)inputs;

    // Disable cancellation while running, state machine block will respond to the stop
    if(0 == pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL))
    {
        flowgraph->top_block->run(10000);

        if(0 != pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL))
        {
            printf("flowgraph: issue with enable cancel state\n");
        }
    }else{
        printf("flowgraph: issue with disable cancel state\n");
    }

    // Tear down flowgraph
    flowgraph->top_block->stop();
    flowgraph->top_block->disconnect_all();
    flowgraph->samp_src.reset();
    flowgraph->file_sink.reset();

    // Wait for flowgraph to be stopped
    if(flowgraph->is_started())
    {
        while(flowgraph->is_started())
        {
            sleep(1);
        }
    }

    return(NULL);
}
