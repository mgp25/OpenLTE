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

    File: LTE_file_recorder_flowgraph.h

    Description: Contains all the definitions for the LTE file recorder
                 gnuradio flowgraph.

    Revision History
    ----------    -------------    --------------------------------------------
    08/26/2013    Ben Wojtowicz    Created file
    11/13/2013    Ben Wojtowicz    Added support for USRP B2X0.
    11/30/2013    Ben Wojtowicz    Added support for bladeRF.
    12/06/2015    Ben Wojtowicz    Changed boost::mutex to pthread_mutex_t.

*******************************************************************************/

#ifndef __LTE_FILE_RECORDER_FLOWGRAPH_H__
#define __LTE_FILE_RECORDER_FLOWGRAPH_H__

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_file_recorder_interface.h"
#include <gnuradio/top_block.h>
#include <osmosdr/source.h>
#include <gnuradio/blocks/file_sink.h>

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
    LTE_FILE_RECORDER_HW_TYPE_RTL_SDR = 0,
    LTE_FILE_RECORDER_HW_TYPE_HACKRF,
    LTE_FILE_RECORDER_HW_TYPE_USRP,
    LTE_FILE_RECORDER_HW_TYPE_BLADERF,
    LTE_FILE_RECORDER_HW_TYPE_UNKNOWN,
}LTE_FILE_RECORDER_HW_TYPE_ENUM;

/*******************************************************************************
                              CLASS DECLARATIONS
*******************************************************************************/

class LTE_file_recorder_flowgraph
{
public:
    // Singleton
    static LTE_file_recorder_flowgraph* get_instance(void);
    static void cleanup(void);

    // Flowgraph
    bool is_started(void);
    LTE_FILE_RECORDER_STATUS_ENUM start(uint16 earfcn, std::string file_name);
    LTE_FILE_RECORDER_STATUS_ENUM stop(void);

private:
    // Singleton
    static LTE_file_recorder_flowgraph *instance;
    LTE_file_recorder_flowgraph();
    ~LTE_file_recorder_flowgraph();

    // Run
    static void* run_thread(void *inputs);

    // Variables
    gr::top_block_sptr          top_block;
    osmosdr::source::sptr       samp_src;
    gr::blocks::file_sink::sptr file_sink;

    pthread_t       start_thread;
    pthread_mutex_t start_mutex;
    bool            started;
};

#endif /* __LTE_FILE_RECORDER_FLOWGRAPH_H__ */
