#line 2 "LTE_fdd_enb_interface.cc" // Make __FILE__ omit the path
/*******************************************************************************

    Copyright 2013-2017 Ben Wojtowicz

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

    File: LTE_fdd_enb_interface.cc

    Description: Contains all the implementations for the LTE FDD eNodeB
                 interface.

    Revision History
    ----------    -------------    --------------------------------------------
    11/09/2013    Ben Wojtowicz    Created file
    01/18/2014    Ben Wojtowicz    Added dynamic variable support, added level
                                   to debug prints, fixed usec time in debug
                                   prints, and added uint32 variables.
    03/26/2014    Ben Wojtowicz    Added message printing.
    05/04/2014    Ben Wojtowicz    Added PCAP support.
    06/15/2014    Ben Wojtowicz    Added  ... support for info messages and
                                   using the latest LTE library.
    07/22/2014    Ben Wojtowicz    Added clock source as a configurable
                                   parameter.
    08/03/2014    Ben Wojtowicz    Added HSS support.
    09/03/2014    Ben Wojtowicz    Added read only parameters for UL EARFCN,
                                   DL center frequency and UL center frequency.
    11/01/2014    Ben Wojtowicz    Added parameters for IP address assignment,
                                   DNS address, config file, and user file.
    11/29/2014    Ben Wojtowicz    Added support for the IP gateway.
    12/16/2014    Ben Wojtowicz    Added ol extension to message queues.
    02/15/2015    Ben Wojtowicz    Moved to new message queue, added IP pcap
                                   support, and added UTC time to the log port.
    03/11/2015    Ben Wojtowicz    Made a common routine for formatting time.
    07/25/2015    Ben Wojtowicz    Added config file support for TX/RX gains.
    12/06/2015    Ben Wojtowicz    Changed boost::mutex to pthread_mutex_t and
                                   sem_t.
    02/13/2016    Ben Wojtowicz    Added a command to print all registered
                                   users.
    07/29/2017    Ben Wojtowicz    Added input parameters for direct IPC to a UE
                                   and using the latest tools library.

*******************************************************************************/

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_fdd_enb_interface.h"
#include "LTE_fdd_enb_cnfg_db.h"
#include "LTE_fdd_enb_user_mgr.h"
#include "LTE_fdd_enb_hss.h"
#include "LTE_fdd_enb_gw.h"
#include "LTE_fdd_enb_mme.h"
#include "LTE_fdd_enb_rrc.h"
#include "LTE_fdd_enb_pdcp.h"
#include "LTE_fdd_enb_rlc.h"
#include "LTE_fdd_enb_mac.h"
#include "LTE_fdd_enb_phy.h"
#include "LTE_fdd_enb_radio.h"
#include "LTE_fdd_enb_timer_mgr.h"
#include "liblte_interface.h"
#include "libtools_scoped_lock.h"
#include "libtools_helpers.h"
#include <boost/lexical_cast.hpp>
#include <arpa/inet.h>
#include <sys/time.h>
#include <stdarg.h>

/*******************************************************************************
                              DEFINES
*******************************************************************************/


/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/


/*******************************************************************************
                              GLOBAL VARIABLES
*******************************************************************************/

LTE_fdd_enb_interface* LTE_fdd_enb_interface::instance        = NULL;
static pthread_mutex_t interface_instance_mutex               = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t ctrl_connect_mutex                     = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t debug_connect_mutex                    = PTHREAD_MUTEX_INITIALIZER;
bool                   LTE_fdd_enb_interface::ctrl_connected  = false;
bool                   LTE_fdd_enb_interface::debug_connected = false;

/*******************************************************************************
                              CLASS IMPLEMENTATIONS
*******************************************************************************/

/*******************/
/*    Singleton    */
/*******************/
LTE_fdd_enb_interface* LTE_fdd_enb_interface::get_instance(void)
{
    libtools_scoped_lock lock(interface_instance_mutex);

    if(NULL == instance)
    {
        instance = new LTE_fdd_enb_interface();
    }

    return(instance);
}
void LTE_fdd_enb_interface::cleanup(void)
{
    libtools_scoped_lock lock(interface_instance_mutex);

    if(NULL != instance)
    {
        delete instance;
        instance = NULL;
    }
}

/********************************/
/*    Constructor/Destructor    */
/********************************/
LTE_fdd_enb_interface::LTE_fdd_enb_interface()
{
    uint32 i;

    // Communication
    sem_init(&ctrl_sem, 0, 1);
    sem_init(&debug_sem, 0, 1);
    ctrl_socket     = NULL;
    debug_socket    = NULL;
    ctrl_port       = LTE_FDD_ENB_DEFAULT_CTRL_PORT;
    debug_port      = ctrl_port + LTE_FDD_ENB_DEBUG_PORT_OFFSET;
    ctrl_connected  = false;
    debug_connected = false;

    // Variables
    pdcp = NULL;
    mme  = NULL;
    gw   = NULL;
    sem_init(&start_sem, 0, 1);
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_BANDWIDTH]]          = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_DOUBLE, LTE_FDD_ENB_PARAM_BANDWIDTH, 0, 0, 0, 0, true, false, false};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_FREQ_BAND]]          = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_INT64, LTE_FDD_ENB_PARAM_FREQ_BAND, 0, 0, 0, 0, true, true, false};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_DL_EARFCN]]          = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_INT64, LTE_FDD_ENB_PARAM_DL_EARFCN, 0, 0, 0, 0, true, true, false};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_UL_EARFCN]]          = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_INT64, LTE_FDD_ENB_PARAM_UL_EARFCN, 0, 0, 0, 0, false, false, true};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_DL_CENTER_FREQ]]     = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_INT64, LTE_FDD_ENB_PARAM_DL_CENTER_FREQ, 0, 0, 0, 0, false, false, true};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_UL_CENTER_FREQ]]     = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_INT64, LTE_FDD_ENB_PARAM_UL_CENTER_FREQ, 0, 0, 0, 0, false, false, true};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_N_ANT]]              = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_INT64, LTE_FDD_ENB_PARAM_N_ANT, 0, 0, 0, 0, true, false, false};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_N_ID_CELL]]          = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_INT64, LTE_FDD_ENB_PARAM_N_ID_CELL, 0, 0, 0, 503, false, false, false};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_MCC]]                = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_HEX, LTE_FDD_ENB_PARAM_MCC, 0, 0, 0, 0, true, true, false};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_MNC]]                = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_HEX, LTE_FDD_ENB_PARAM_MNC, 0, 0, 0, 0, true, true, false};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_CELL_ID]]            = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_INT64, LTE_FDD_ENB_PARAM_CELL_ID, 0, 0, 0, 268435455, false, true, false};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_TRACKING_AREA_CODE]] = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_INT64, LTE_FDD_ENB_PARAM_TRACKING_AREA_CODE, 0, 0, 0, 65535, false, true, false};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_Q_RX_LEV_MIN]]       = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_INT64, LTE_FDD_ENB_PARAM_Q_RX_LEV_MIN, 0, 0, -140, -44, false, true, false};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_P0_NOMINAL_PUSCH]]   = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_INT64, LTE_FDD_ENB_PARAM_P0_NOMINAL_PUSCH, 0, 0, -126, 24, false, true, false};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_P0_NOMINAL_PUCCH]]   = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_INT64, LTE_FDD_ENB_PARAM_P0_NOMINAL_PUCCH, 0, 0, -127, -96, false, true, false};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_SIB3_PRESENT]]       = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_INT64, LTE_FDD_ENB_PARAM_SIB3_PRESENT, 0, 0, 0, 1, false, true, false};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_Q_HYST]]             = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_INT64, LTE_FDD_ENB_PARAM_Q_HYST, 0, 0, 0, 0, true, true, false};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_SIB4_PRESENT]]       = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_INT64, LTE_FDD_ENB_PARAM_SIB4_PRESENT, 0, 0, 0, 1, false, true, false};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_SIB5_PRESENT]]       = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_INT64, LTE_FDD_ENB_PARAM_SIB5_PRESENT, 0, 0, 0, 1, false, true, false};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_SIB6_PRESENT]]       = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_INT64, LTE_FDD_ENB_PARAM_SIB6_PRESENT, 0, 0, 0, 1, false, true, false};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_SIB7_PRESENT]]       = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_INT64, LTE_FDD_ENB_PARAM_SIB7_PRESENT, 0, 0, 0, 1, false, true, false};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_SIB8_PRESENT]]       = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_INT64, LTE_FDD_ENB_PARAM_SIB8_PRESENT, 0, 0, 0, 1, false, true, false};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_SEARCH_WIN_SIZE]]    = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_INT64, LTE_FDD_ENB_PARAM_SEARCH_WIN_SIZE, 0, 0, 0, 15, false, true, false};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_MAC_DIRECT_TO_UE]]   = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_INT64, LTE_FDD_ENB_PARAM_MAC_DIRECT_TO_UE, 0, 0, 0, 1, false, false, false};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_PHY_DIRECT_TO_UE]]   = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_INT64, LTE_FDD_ENB_PARAM_PHY_DIRECT_TO_UE, 0, 0, 0, 1, false, false, false};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_DEBUG_TYPE]]         = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_UINT32, LTE_FDD_ENB_PARAM_DEBUG_TYPE, 0, 0, 0, 0, true, true, false};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_DEBUG_LEVEL]]        = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_UINT32, LTE_FDD_ENB_PARAM_DEBUG_LEVEL, 0, 0, 0, 0, true, true, false};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_ENABLE_PCAP]]        = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_INT64, LTE_FDD_ENB_PARAM_ENABLE_PCAP, 0, 0, 0, 1, false, true, false};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_IP_ADDR_START]]      = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_HEX, LTE_FDD_ENB_PARAM_IP_ADDR_START, 0, 0, 0, 0, true, false, false};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_DNS_ADDR]]           = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_HEX, LTE_FDD_ENB_PARAM_DNS_ADDR, 0, 0, 0, 0, true, false, false};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_USE_CNFG_FILE]]      = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_INT64, LTE_FDD_ENB_PARAM_USE_CNFG_FILE, 0, 0, 0, 1, false, true, false};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_USE_USER_FILE]]      = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_INT64, LTE_FDD_ENB_PARAM_USE_USER_FILE, 0, 0, 0, 1, false, true, false};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_TX_GAIN]]            = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_INT64, LTE_FDD_ENB_PARAM_TX_GAIN, 0, 0, 0, 100, false, true, false};
    var_map[LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_RX_GAIN]]            = (LTE_FDD_ENB_VAR_STRUCT){LTE_FDD_ENB_VAR_TYPE_INT64, LTE_FDD_ENB_PARAM_RX_GAIN, 0, 0, 0, 100, false, true, false};

    debug_type_mask = 0;
    for(i=0; i<LTE_FDD_ENB_DEBUG_TYPE_N_ITEMS; i++)
    {
        debug_type_mask |= 1 << i;
    }
    debug_level_mask = 0;
    for(i=0; i<LTE_FDD_ENB_DEBUG_LEVEL_N_ITEMS; i++)
    {
        debug_level_mask |= 1 << i;
    }
    open_lte_pcap_fd();
    open_ip_pcap_fd();
    shutdown = false;
    started  = false;
}
LTE_fdd_enb_interface::~LTE_fdd_enb_interface()
{
    stop_ports();

    fclose(lte_pcap_fd);
    fclose(ip_pcap_fd);

    sem_destroy(&start_sem);
    sem_destroy(&ctrl_sem);
    sem_destroy(&debug_sem);
}

/***********************/
/*    Communication    */
/***********************/
void LTE_fdd_enb_interface::set_ctrl_port(int16 port)
{
    libtools_scoped_lock c_lock(ctrl_connect_mutex);
    libtools_scoped_lock d_lock(debug_connect_mutex);

    if(!ctrl_connected)
    {
        ctrl_port = port;
    }
    if(!debug_connected)
    {
        debug_port = ctrl_port + LTE_FDD_ENB_DEBUG_PORT_OFFSET;
    }
}
void LTE_fdd_enb_interface::start_ports(void)
{
    libtools_scoped_lock            c_lock(ctrl_sem);
    libtools_scoped_lock            d_lock(debug_sem);
    LIBTOOLS_SOCKET_WRAP_ERROR_ENUM error;

    if(NULL == debug_socket)
    {
        debug_socket = new libtools_socket_wrap(NULL,
                                                debug_port,
                                                LIBTOOLS_SOCKET_WRAP_TYPE_SERVER,
                                                &handle_debug_msg,
                                                &handle_debug_connect,
                                                &handle_debug_disconnect,
                                                &handle_debug_error,
                                                &error);
        if(LIBTOOLS_SOCKET_WRAP_SUCCESS != error)
        {
            printf("Couldn't open debug_socket %s\n", libtools_socket_wrap_error_text[error]);
            debug_socket = NULL;
        }
    }
    if(NULL == ctrl_socket)
    {
        ctrl_socket = new libtools_socket_wrap(NULL,
                                               ctrl_port,
                                               LIBTOOLS_SOCKET_WRAP_TYPE_SERVER,
                                               &handle_ctrl_msg,
                                               &handle_ctrl_connect,
                                               &handle_ctrl_disconnect,
                                               &handle_ctrl_error,
                                               &error);
        if(LIBTOOLS_SOCKET_WRAP_SUCCESS != error)
        {
            send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                           LTE_FDD_ENB_DEBUG_LEVEL_IFACE,
                           __FILE__,
                           __LINE__,
                           "Couldn't open ctrl_socket %s",
                           libtools_socket_wrap_error_text[error]);
            ctrl_socket = NULL;
        }
    }
}
void LTE_fdd_enb_interface::stop_ports(void)
{
    libtools_scoped_lock c_lock(ctrl_sem);
    libtools_scoped_lock d_lock(debug_sem);

    if(NULL != ctrl_socket)
    {
        delete ctrl_socket;
        ctrl_socket = NULL;
    }
    if(NULL != debug_socket)
    {
        delete debug_socket;
        debug_socket = NULL;
    }
}
void LTE_fdd_enb_interface::send_ctrl_msg(std::string msg)
{
    libtools_scoped_lock lock(ctrl_connect_mutex);
    std::string          tmp_msg;

    if(ctrl_connected)
    {
        tmp_msg  = msg;
        tmp_msg += "\n";
        ctrl_socket->send(tmp_msg);
    }
}
void LTE_fdd_enb_interface::send_ctrl_info_msg(std::string msg,
                                               ...)
{
    libtools_scoped_lock  lock(ctrl_connect_mutex);
    std::string           tmp_msg;
    va_list               args;
    char                 *args_msg;

    if(ctrl_connected)
    {
        tmp_msg = "info ";
        va_start(args, msg);
        if(-1 != vasprintf(&args_msg, msg.c_str(), args))
        {
            tmp_msg += args_msg;
        }
        tmp_msg += "\n";

        // Cleanup the variable argument string
        free(args_msg);

        ctrl_socket->send(tmp_msg);
    }
}
void LTE_fdd_enb_interface::send_ctrl_error_msg(LTE_FDD_ENB_ERROR_ENUM error,
                                                std::string            msg)
{
    libtools_scoped_lock lock(ctrl_connect_mutex);
    std::string          tmp_msg;

    if(ctrl_connected)
    {
        if(LTE_FDD_ENB_ERROR_NONE == error)
        {
            tmp_msg = "ok ";
        }else{
            tmp_msg  = "fail \"";
            tmp_msg += LTE_fdd_enb_error_text[error];
            tmp_msg += "\"";
        }
        tmp_msg += msg;
        tmp_msg += "\n";

        ctrl_socket->send(tmp_msg);
    }
}
void LTE_fdd_enb_interface::send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ENUM  type,
                                           LTE_FDD_ENB_DEBUG_LEVEL_ENUM level,
                                           std::string                  file_name,
                                           int32                        line,
                                           std::string                  msg,
                                           ...)
{
    libtools_scoped_lock  lock(debug_connect_mutex);
    std::string           tmp_msg;
    va_list               args;
    char                 *args_msg;

    if(debug_connected                 &&
       (debug_type_mask & (1 << type)) &&
       (debug_level_mask & (1 << level)))
    {
        // Format the output string
        get_formatted_time(tmp_msg);
        tmp_msg += " ";
        tmp_msg += LTE_fdd_enb_debug_type_text[type];
        tmp_msg += " ";
        tmp_msg += LTE_fdd_enb_debug_level_text[level];
        tmp_msg += " ";
        tmp_msg += file_name.c_str();
        tmp_msg += " ";
        tmp_msg += to_string(line);
        tmp_msg += " ";
        va_start(args, msg);
        if(-1 != vasprintf(&args_msg, msg.c_str(), args))
        {
            tmp_msg += args_msg;
        }
        tmp_msg += "\n";

        // Cleanup the variable argument string
        free(args_msg);

        debug_socket->send(tmp_msg);
    }
}
void LTE_fdd_enb_interface::send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ENUM   type,
                                           LTE_FDD_ENB_DEBUG_LEVEL_ENUM  level,
                                           std::string                   file_name,
                                           int32                         line,
                                           LIBLTE_BIT_MSG_STRUCT        *lte_msg,
                                           std::string                   msg,
                                           ...)
{
    libtools_scoped_lock  lock(debug_connect_mutex);
    std::string           tmp_msg;
    va_list               args;
    uint32                i;
    uint32                hex_val;
    char                 *args_msg;

    if(debug_connected                 &&
       (debug_type_mask & (1 << type)) &&
       (debug_level_mask & (1 << level)))
    {
        // Format the output string
        get_formatted_time(tmp_msg);
        tmp_msg += " ";
        tmp_msg += LTE_fdd_enb_debug_type_text[type];
        tmp_msg += " ";
        tmp_msg += LTE_fdd_enb_debug_level_text[level];
        tmp_msg += " ";
        tmp_msg += file_name.c_str();
        tmp_msg += " ";
        tmp_msg += to_string(line);
        tmp_msg += " ";
        va_start(args, msg);
        if(-1 != vasprintf(&args_msg, msg.c_str(), args))
        {
            tmp_msg += args_msg;
        }
        tmp_msg += " ";
        hex_val  = 0;
        for(i=0; i<lte_msg->N_bits; i++)
        {
            hex_val <<= 1;
            hex_val  |= lte_msg->msg[i];
            if((i % 4) == 3)
            {
                if(hex_val < 0xA)
                {
                    tmp_msg += (char)(hex_val + '0');
                }else{
                    tmp_msg += (char)((hex_val-0xA) + 'A');
                }
                hex_val = 0;
            }
        }
        if((lte_msg->N_bits % 4) != 0)
        {
            for(i=0; i<4-(lte_msg->N_bits % 4); i++)
            {
                hex_val <<= 1;
            }
            if(hex_val < 0xA)
            {
                tmp_msg += (char)(hex_val + '0');
            }else{
                tmp_msg += (char)((hex_val-0xA) + 'A');
            }
        }
        tmp_msg += "\n";

        // Cleanup the variable argument string
        free(args_msg);

        debug_socket->send(tmp_msg);
    }
}
void LTE_fdd_enb_interface::send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ENUM   type,
                                           LTE_FDD_ENB_DEBUG_LEVEL_ENUM  level,
                                           std::string                   file_name,
                                           int32                         line,
                                           LIBLTE_BYTE_MSG_STRUCT       *lte_msg,
                                           std::string                   msg,
                                           ...)
{
    libtools_scoped_lock  lock(debug_connect_mutex);
    std::string           tmp_msg;
    va_list               args;
    char                 *args_msg;

    if(debug_connected                 &&
       (debug_type_mask & (1 << type)) &&
       (debug_level_mask & (1 << level)))
    {
        // Format the output string
        get_formatted_time(tmp_msg);
        tmp_msg += " ";
        tmp_msg += LTE_fdd_enb_debug_type_text[type];
        tmp_msg += " ";
        tmp_msg += LTE_fdd_enb_debug_level_text[level];
        tmp_msg += " ";
        tmp_msg += file_name.c_str();
        tmp_msg += " ";
        tmp_msg += to_string(line);
        tmp_msg += " ";
        va_start(args, msg);
        if(-1 != vasprintf(&args_msg, msg.c_str(), args))
        {
            tmp_msg += args_msg;
        }
        tmp_msg += " " + to_string(lte_msg->msg, lte_msg->N_bytes) + "\n";

        // Cleanup the variable argument string
        free(args_msg);

        debug_socket->send(tmp_msg);
    }
}
void LTE_fdd_enb_interface::open_lte_pcap_fd(void)
{
    uint32 magic_number = 0xa1b2c3d4;
    uint32 timezone     = 0;
    uint32 sigfigs      = 0;
    uint32 snap_len     = 0xFFFF;
    uint32 dlt          = 147;
    uint32 tmp_u32;
    uint16 major_version = 2;
    uint16 minor_version = 4;
    uint16 tmp_u16;

    lte_pcap_fd = fopen("/tmp/LTE_fdd_enodeb.pcap", "w");

    tmp_u32 = htonl(magic_number);
    fwrite(&tmp_u32, sizeof(tmp_u32), 1, lte_pcap_fd);
    tmp_u16 = htons(major_version);
    fwrite(&tmp_u16, sizeof(tmp_u16), 1, lte_pcap_fd);
    tmp_u16 = htons(minor_version);
    fwrite(&tmp_u16, sizeof(tmp_u16), 1, lte_pcap_fd);
    tmp_u32 = htonl(timezone);
    fwrite(&tmp_u32, sizeof(tmp_u32), 1, lte_pcap_fd);
    tmp_u32 = htonl(sigfigs);
    fwrite(&tmp_u32, sizeof(tmp_u32), 1, lte_pcap_fd);
    tmp_u32 = htonl(snap_len);
    fwrite(&tmp_u32, sizeof(tmp_u32), 1, lte_pcap_fd);
    tmp_u32 = htonl(dlt);
    fwrite(&tmp_u32, sizeof(tmp_u32), 1, lte_pcap_fd);
}
void LTE_fdd_enb_interface::open_ip_pcap_fd(void)
{
    uint32 magic_number = 0xa1b2c3d4;
    uint32 timezone     = 0;
    uint32 sigfigs      = 0;
    uint32 snap_len     = 0xFFFF;
    uint32 dlt          = 228;
    uint32 tmp_u32;
    uint16 major_version = 2;
    uint16 minor_version = 4;
    uint16 tmp_u16;

    ip_pcap_fd = fopen("/tmp/LTE_fdd_enodeb_ip.pcap", "w");

    tmp_u32 = htonl(magic_number);
    fwrite(&tmp_u32, sizeof(tmp_u32), 1, ip_pcap_fd);
    tmp_u16 = htons(major_version);
    fwrite(&tmp_u16, sizeof(tmp_u16), 1, ip_pcap_fd);
    tmp_u16 = htons(minor_version);
    fwrite(&tmp_u16, sizeof(tmp_u16), 1, ip_pcap_fd);
    tmp_u32 = htonl(timezone);
    fwrite(&tmp_u32, sizeof(tmp_u32), 1, ip_pcap_fd);
    tmp_u32 = htonl(sigfigs);
    fwrite(&tmp_u32, sizeof(tmp_u32), 1, ip_pcap_fd);
    tmp_u32 = htonl(snap_len);
    fwrite(&tmp_u32, sizeof(tmp_u32), 1, ip_pcap_fd);
    tmp_u32 = htonl(dlt);
    fwrite(&tmp_u32, sizeof(tmp_u32), 1, ip_pcap_fd);
}
void LTE_fdd_enb_interface::send_lte_pcap_msg(LTE_FDD_ENB_PCAP_DIRECTION_ENUM  dir,
                                              uint32                           rnti,
                                              uint32                           current_tti,
                                              uint8                           *msg,
                                              uint32                           N_bits)
{
    LTE_fdd_enb_cnfg_db *cnfg_db = LTE_fdd_enb_cnfg_db::get_instance();
    struct timeval       time;
    struct timezone      time_zone;
    int64                enable_pcap;
    uint32               i;
    uint32               idx;
    uint32               length;
    uint32               tmp_u32;
    uint16               tmp_u16;
    uint8                pcap_c_hdr[15];
    uint8                pcap_msg[LIBLTE_MAX_MSG_SIZE*2];

    cnfg_db->get_param(LTE_FDD_ENB_PARAM_ENABLE_PCAP, enable_pcap);

    if(enable_pcap)
    {
        // Get approximate time stamp
        gettimeofday(&time, &time_zone);

        // Radio Type
        pcap_c_hdr[0] = 1;

        // Direction
        pcap_c_hdr[1] = dir;

        // RNTI Type
        if(0xFFFFFFFF == rnti)
        {
            pcap_c_hdr[2] = 0;
        }else if(LIBLTE_MAC_P_RNTI == rnti){
            pcap_c_hdr[2] = 1;
        }else if(LIBLTE_MAC_RA_RNTI_START <= rnti &&
                 LIBLTE_MAC_RA_RNTI_END   >= rnti){
            pcap_c_hdr[2] = 2;
        }else if(LIBLTE_MAC_SI_RNTI == rnti){
            pcap_c_hdr[2] = 4;
        }else if(LIBLTE_MAC_M_RNTI == rnti){
            pcap_c_hdr[2] = 6;
        }else{
            pcap_c_hdr[2] = 3;
        }

        // RNTI Tag and RNTI
        pcap_c_hdr[3] = 2;
        tmp_u16       = htons((uint16)rnti);
        memcpy(&pcap_c_hdr[4], &tmp_u16, sizeof(uint16));

        // UEID Tag and UEID
        pcap_c_hdr[6] = 3;
        tmp_u16       = htons((uint16)rnti);
        memcpy(&pcap_c_hdr[7], &tmp_u16, sizeof(uint16));

        // SUBFN Tag and SUBFN
        pcap_c_hdr[9] = 4;
        tmp_u16       = htons((uint16)(current_tti%10));
        memcpy(&pcap_c_hdr[10], &tmp_u16, sizeof(uint16));

        // CRC Status Tag and CRC Status
        pcap_c_hdr[12] = 7;
        pcap_c_hdr[13] = 1;

        // Payload Tag
        pcap_c_hdr[14] = 1;

        // Payload
        idx           = 0;
        pcap_msg[idx] = 0;
        for(i=0; i<N_bits; i++)
        {
            pcap_msg[idx] <<= 1;
            pcap_msg[idx]  |= msg[i];
            if((i % 8) == 7)
            {
                idx++;
                pcap_msg[idx] = 0;
            }
        }

        // Total Length
        length = 15 + idx;

        // Write Data
        tmp_u32 = htonl(time.tv_sec);
        fwrite(&tmp_u32, sizeof(uint32), 1, lte_pcap_fd);
        tmp_u32 = htonl(time.tv_usec);
        fwrite(&tmp_u32, sizeof(uint32), 1, lte_pcap_fd);
        tmp_u32 = htonl(length);
        fwrite(&tmp_u32,   sizeof(uint32), 1,   lte_pcap_fd);
        fwrite(&tmp_u32,   sizeof(uint32), 1,   lte_pcap_fd);
        fwrite(pcap_c_hdr, sizeof(uint8),  15,  lte_pcap_fd);
        fwrite(pcap_msg,   sizeof(uint8),  idx, lte_pcap_fd);
    }
}
void LTE_fdd_enb_interface::send_ip_pcap_msg(uint8  *msg,
                                             uint32  N_bytes)
{
    LTE_fdd_enb_cnfg_db *cnfg_db = LTE_fdd_enb_cnfg_db::get_instance();
    struct timeval       time;
    struct timezone      time_zone;
    int64                enable_pcap;
    uint32               tmp;

    cnfg_db->get_param(LTE_FDD_ENB_PARAM_ENABLE_PCAP, enable_pcap);

    if(enable_pcap)
    {
        // Get approximate time stamp
        gettimeofday(&time, &time_zone);

        // Write Data
        tmp = htonl(time.tv_sec);
        fwrite(&tmp, sizeof(uint32), 1, ip_pcap_fd);
        tmp = htonl(time.tv_usec);
        fwrite(&tmp, sizeof(uint32), 1, ip_pcap_fd);
        tmp = htonl(N_bytes);
        fwrite(&tmp, sizeof(uint32), 1,       ip_pcap_fd);
        fwrite(&tmp, sizeof(uint32), 1,       ip_pcap_fd);
        fwrite(msg,  sizeof(uint8),  N_bytes, ip_pcap_fd);
    }
}
void LTE_fdd_enb_interface::handle_ctrl_msg(std::string msg)
{
    LTE_fdd_enb_interface *interface = LTE_fdd_enb_interface::get_instance();
    LTE_fdd_enb_cnfg_db   *cnfg_db   = LTE_fdd_enb_cnfg_db::get_instance();
    LTE_fdd_enb_radio     *radio     = LTE_fdd_enb_radio::get_instance();

    if(std::string::npos != msg.find("read"))
    {
        interface->handle_read(msg.substr(msg.find("read")+sizeof("read"), std::string::npos));
    }else if(std::string::npos != msg.find("write")){
        interface->send_ctrl_error_msg(interface->handle_write(msg.substr(msg.find("write")+sizeof("write"), std::string::npos)), "");
    }else if(std::string::npos != msg.find("start")){
        interface->handle_start();
    }else if(std::string::npos != msg.find("stop")){
        interface->handle_stop();
    }else if(std::string::npos != msg.find("shutdown")){
        interface->shutdown = true;
        if(radio->is_started())
        {
            interface->handle_stop();
        }else{
            interface->send_ctrl_error_msg(LTE_FDD_ENB_ERROR_NONE, "");
        }
    }else if(std::string::npos != msg.find("construct_si")){
        if(interface->app_is_started())
        {
            cnfg_db->construct_sys_info(interface->pdcp, interface->mme);
            interface->send_ctrl_error_msg(LTE_FDD_ENB_ERROR_NONE, "");
        }else{
            interface->send_ctrl_error_msg(LTE_FDD_ENB_ERROR_ALREADY_STOPPED, "");
        }
    }else if(std::string::npos != msg.find("help")){
        interface->handle_help();
    }else if(std::string::npos != msg.find("add_user")){
        interface->handle_add_user(msg.substr(msg.find("add_user")+sizeof("add_user"), std::string::npos));
    }else if(std::string::npos != msg.find("del_user")){
        interface->handle_del_user(msg.substr(msg.find("del_user")+sizeof("del_user"), std::string::npos));
    }else if(std::string::npos != msg.find("print_users")){
        interface->handle_print_users();
    }else if(std::string::npos != msg.find("print_registered_users")){
        interface->handle_print_registered_users();
    }else{
        interface->send_ctrl_error_msg(LTE_FDD_ENB_ERROR_INVALID_COMMAND, "");
    }
}
void LTE_fdd_enb_interface::handle_ctrl_connect(void)
{
    pthread_mutex_lock(&ctrl_connect_mutex);
    LTE_fdd_enb_interface::ctrl_connected = true;
    pthread_mutex_unlock(&ctrl_connect_mutex);

    LTE_fdd_enb_interface *interface = LTE_fdd_enb_interface::get_instance();

    interface->send_ctrl_msg("*** LTE FDD ENB ***");
    interface->send_ctrl_msg("Type help to see a list of commands");
}
void LTE_fdd_enb_interface::handle_ctrl_disconnect(void)
{
    libtools_scoped_lock lock(ctrl_connect_mutex);

    LTE_fdd_enb_interface::ctrl_connected = false;
}
void LTE_fdd_enb_interface::handle_ctrl_error(LIBTOOLS_SOCKET_WRAP_ERROR_ENUM err)
{
    LTE_fdd_enb_interface *interface = LTE_fdd_enb_interface::get_instance();

    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ERROR,
                              LTE_FDD_ENB_DEBUG_LEVEL_IFACE,
                              __FILE__,
                              __LINE__,
                              "ctrl_socket error %s",
                              libtools_socket_wrap_error_text[err]);
    assert(0);
}
void LTE_fdd_enb_interface::handle_debug_msg(std::string msg)
{
    // No messages to handle
}
void LTE_fdd_enb_interface::handle_debug_connect(void)
{
    pthread_mutex_lock(&debug_connect_mutex);
    LTE_fdd_enb_interface::debug_connected = true;
    pthread_mutex_unlock(&debug_connect_mutex);

    LTE_fdd_enb_interface *interface = LTE_fdd_enb_interface::get_instance();

    interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                              LTE_FDD_ENB_DEBUG_LEVEL_IFACE,
                              __FILE__,
                              __LINE__,
                              "*** LTE FDD ENB DEBUG INTERFACE ***");
}
void LTE_fdd_enb_interface::handle_debug_disconnect(void)
{
    libtools_scoped_lock lock(debug_connect_mutex);

    LTE_fdd_enb_interface::debug_connected = false;
}
void LTE_fdd_enb_interface::handle_debug_error(LIBTOOLS_SOCKET_WRAP_ERROR_ENUM err)
{
    printf("debug_socket error %s\n", libtools_socket_wrap_error_text[err]);
    assert(0);
}

/******************/
/*    Handlers    */
/******************/
void LTE_fdd_enb_interface::handle_read(std::string msg)
{
    LTE_fdd_enb_cnfg_db                                     *cnfg_db = LTE_fdd_enb_cnfg_db::get_instance();
    LTE_fdd_enb_radio                                       *radio   = LTE_fdd_enb_radio::get_instance();
    std::map<std::string, LTE_FDD_ENB_VAR_STRUCT>::iterator  iter    = var_map.find(msg);
    std::string                                              tmp_str;
    std::string                                              s_value;
    LTE_FDD_ENB_AVAILABLE_RADIOS_STRUCT                      avail_radios   = radio->get_available_radios();
    LTE_FDD_ENB_RADIO_STRUCT                                 selected_radio = radio->get_selected_radio();
    double                                                   d_value;
    int64                                                    i_value;
    uint32                                                   u_value;
    uint32                                                   i;

    if(var_map.end() != iter)
    {
        // Handle all system parameters
        switch((*iter).second.var_type)
        {
        case LTE_FDD_ENB_VAR_TYPE_DOUBLE:
            cnfg_db->get_param((*iter).second.param, d_value);
            send_ctrl_error_msg(LTE_FDD_ENB_ERROR_NONE, to_string(d_value));
            break;
        case LTE_FDD_ENB_VAR_TYPE_INT64:
            cnfg_db->get_param((*iter).second.param, i_value);
            if(LTE_FDD_ENB_PARAM_FREQ_BAND == (*iter).second.param)
            {
                send_ctrl_error_msg(LTE_FDD_ENB_ERROR_NONE, liblte_interface_band_text[i_value]);
            }else{
                send_ctrl_error_msg(LTE_FDD_ENB_ERROR_NONE, to_string(i_value));
            }
            break;
        case LTE_FDD_ENB_VAR_TYPE_HEX:
            cnfg_db->get_param((*iter).second.param, s_value);
            send_ctrl_error_msg(LTE_FDD_ENB_ERROR_NONE, s_value);
            break;
        case LTE_FDD_ENB_VAR_TYPE_UINT32:
            cnfg_db->get_param((*iter).second.param, u_value);
            if(LTE_FDD_ENB_PARAM_DEBUG_TYPE == (*iter).second.param)
            {
                for(i=0; i<LTE_FDD_ENB_DEBUG_TYPE_N_ITEMS; i++)
                {
                    if((u_value & (1 << i)))
                    {
                        tmp_str += LTE_fdd_enb_debug_type_text[i];
                        tmp_str += " ";
                    }
                }
                send_ctrl_error_msg(LTE_FDD_ENB_ERROR_NONE, tmp_str);
            }else if(LTE_FDD_ENB_PARAM_DEBUG_LEVEL == (*iter).second.param){
                for(i=0; i<LTE_FDD_ENB_DEBUG_LEVEL_N_ITEMS; i++)
                {
                    if((u_value & (1 << i)))
                    {
                        tmp_str += LTE_fdd_enb_debug_level_text[i];
                        tmp_str += " ";
                    }
                }
                send_ctrl_error_msg(LTE_FDD_ENB_ERROR_NONE, tmp_str);
            }else{
                send_ctrl_error_msg(LTE_FDD_ENB_ERROR_NONE, to_string(u_value));
            }
            break;
        default:
            send_ctrl_error_msg(LTE_FDD_ENB_ERROR_INVALID_PARAM, "");
            break;
        }
    }else{
        // Handle all radio parameters
        if(std::string::npos != msg.find(LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_AVAILABLE_RADIOS]))
        {
            send_ctrl_error_msg(LTE_FDD_ENB_ERROR_NONE, to_string(avail_radios.num_radios));
            for(i=0; i<avail_radios.num_radios; i++)
            {
                tmp_str  = to_string(i);
                tmp_str += ":";
                tmp_str += avail_radios.radio[i].name;
                send_ctrl_msg(tmp_str);
            }
        }else if(std::string::npos != msg.find(LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_SELECTED_RADIO_NAME])){
            send_ctrl_error_msg(LTE_FDD_ENB_ERROR_NONE, selected_radio.name);
        }else if(std::string::npos != msg.find(LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_SELECTED_RADIO_IDX])){
            send_ctrl_error_msg(LTE_FDD_ENB_ERROR_NONE, to_string(radio->get_selected_radio_idx()));
        }else if(std::string::npos != msg.find(LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_CLOCK_SOURCE])){
            send_ctrl_error_msg(LTE_FDD_ENB_ERROR_NONE, radio->get_clock_source());
        }else{
            send_ctrl_error_msg(LTE_FDD_ENB_ERROR_INVALID_PARAM, "");
        }
    }
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_interface::handle_write(std::string msg)
{
    LTE_fdd_enb_radio                                       *radio   = LTE_fdd_enb_radio::get_instance();
    std::map<std::string, LTE_FDD_ENB_VAR_STRUCT>::iterator  iter    = var_map.find(msg.substr(0, msg.find(" ")));
    LTE_FDD_ENB_ERROR_ENUM                                   err;
    double                                                   d_value;
    int64                                                    i_value;
    uint32                                                   u_value;
    uint32                                                   i;

    try
    {
        if(var_map.end() != iter)
        {
            if(!(*iter).second.read_only)
            {
                // Handle all system parameters
                switch((*iter).second.var_type)
                {
                case LTE_FDD_ENB_VAR_TYPE_DOUBLE:
                    d_value = boost::lexical_cast<double>(msg.substr(msg.find(" ")+1, std::string::npos));
                    err     = write_value(&(*iter).second, d_value);
                    break;
                case LTE_FDD_ENB_VAR_TYPE_INT64:
                    i_value = boost::lexical_cast<int64>(msg.substr(msg.find(" ")+1, std::string::npos));
                    err     = write_value(&(*iter).second, i_value);
                    break;
                case LTE_FDD_ENB_VAR_TYPE_HEX:
                    err = write_value(&(*iter).second, msg.substr(msg.find(" ")+1, std::string::npos));
                    break;
                case LTE_FDD_ENB_VAR_TYPE_UINT32:
                    if(LTE_FDD_ENB_PARAM_DEBUG_TYPE == (*iter).second.param)
                    {
                        u_value = 0;
                        for(i=0; i<LTE_FDD_ENB_DEBUG_TYPE_N_ITEMS; i++)
                        {
                            if(std::string::npos != msg.substr(msg.find(" ")+1, std::string::npos).find(LTE_fdd_enb_debug_type_text[i]))
                            {
                                u_value |= 1 << i;
                            }
                        }
                        debug_type_mask = u_value;
                        err             = write_value(&(*iter).second, u_value);
                    }else if(LTE_FDD_ENB_PARAM_DEBUG_LEVEL == (*iter).second.param){
                        u_value = 0;
                        for(i=0; i<LTE_FDD_ENB_DEBUG_LEVEL_N_ITEMS; i++)
                        {
                            if(std::string::npos != msg.substr(msg.find(" ")+1, std::string::npos).find(LTE_fdd_enb_debug_level_text[i]))
                            {
                                u_value |= 1 << i;
                            }
                        }
                        debug_level_mask = u_value;
                        err              = write_value(&(*iter).second, u_value);
                    }else{
                        u_value = boost::lexical_cast<uint32>(msg.substr(msg.find(" ")+1, std::string::npos));
                        err     = write_value(&(*iter).second, u_value);
                    }
                    break;
                default:
                    err = LTE_FDD_ENB_ERROR_INVALID_PARAM;
                    break;
                }
            }else{
                err = LTE_FDD_ENB_ERROR_READ_ONLY;
            }
        }else{
            // Handle all radio parameters
            if(std::string::npos != msg.find(LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_SELECTED_RADIO_IDX]))
            {
                u_value = boost::lexical_cast<uint32>(msg.substr(msg.find(" ")+1, std::string::npos));
                err = radio->set_selected_radio_idx(u_value);
            }else if(std::string::npos != msg.find(LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_CLOCK_SOURCE])){
                err = radio->set_clock_source(msg.substr(msg.find(" ")+1, std::string::npos));
            }else{
                err = LTE_FDD_ENB_ERROR_INVALID_PARAM;
            }
        }
    }catch(...){
        err = LTE_FDD_ENB_ERROR_EXCEPTION;
    }

    return(err);
}
void LTE_fdd_enb_interface::handle_start(void)
{
    LTE_fdd_enb_cnfg_db    *cnfg_db   = LTE_fdd_enb_cnfg_db::get_instance();
    LTE_fdd_enb_mac        *mac       = LTE_fdd_enb_mac::get_instance();
    LTE_fdd_enb_rlc        *rlc       = LTE_fdd_enb_rlc::get_instance();
    LTE_fdd_enb_rrc        *rrc       = LTE_fdd_enb_rrc::get_instance();
    LTE_fdd_enb_phy        *phy       = LTE_fdd_enb_phy::get_instance();
    LTE_fdd_enb_radio      *radio     = LTE_fdd_enb_radio::get_instance();
    LTE_fdd_enb_timer_mgr  *timer_mgr = LTE_fdd_enb_timer_mgr::get_instance();
    LTE_FDD_ENB_ERROR_ENUM  err;
    int64                   mac_direct_to_ue;
    int64                   phy_direct_to_ue;
    char                    err_str[LTE_FDD_ENB_MAX_LINE_SIZE];

    sem_wait(&start_sem);
    if(!started)
    {
        started = true;
        sem_post(&start_sem);

        // Initialize inter-stack communication
        phy_to_mac_comm   = new LTE_fdd_enb_msgq("phy_to_mac");
        mac_to_phy_comm   = new LTE_fdd_enb_msgq("mac_to_phy");
        mac_to_rlc_comm   = new LTE_fdd_enb_msgq("mac_to_rlc");
        mac_to_timer_comm = new LTE_fdd_enb_msgq("mac_to_timer");
        rlc_to_mac_comm   = new LTE_fdd_enb_msgq("rlc_to_mac");
        rlc_to_pdcp_comm  = new LTE_fdd_enb_msgq("rlc_to_pdcp");
        pdcp_to_rlc_comm  = new LTE_fdd_enb_msgq("pdcp_to_rlc");
        pdcp_to_rrc_comm  = new LTE_fdd_enb_msgq("pdcp_to_rrc");
        rrc_to_pdcp_comm  = new LTE_fdd_enb_msgq("rrc_to_pdcp");
        rrc_to_mme_comm   = new LTE_fdd_enb_msgq("rrc_to_mme");
        mme_to_rrc_comm   = new LTE_fdd_enb_msgq("mme_to_rrc");
        pdcp_to_gw_comm   = new LTE_fdd_enb_msgq("pdcp_to_gw");
        gw_to_pdcp_comm   = new LTE_fdd_enb_msgq("gw_to_pdcp");

        // Construct layers
        pdcp = new LTE_fdd_enb_pdcp();
        mme  = new LTE_fdd_enb_mme();
        gw   = new LTE_fdd_enb_gw();

        // Construct the system information
        cnfg_db->construct_sys_info(pdcp, mme);

        // Start layers
        err = gw->start(pdcp_to_gw_comm, gw_to_pdcp_comm, err_str, this);
        if(LTE_FDD_ENB_ERROR_NONE == err)
        {
            cnfg_db->get_param(LTE_FDD_ENB_PARAM_MAC_DIRECT_TO_UE, mac_direct_to_ue);
            cnfg_db->get_param(LTE_FDD_ENB_PARAM_PHY_DIRECT_TO_UE, phy_direct_to_ue);

            phy->start(mac_to_phy_comm, phy_to_mac_comm, phy_direct_to_ue, this);
            mac->start(phy_to_mac_comm, rlc_to_mac_comm, mac_to_phy_comm, mac_to_rlc_comm, mac_to_timer_comm, mac_direct_to_ue, this);
            timer_mgr->start(mac_to_timer_comm, this);
            rlc->start(mac_to_rlc_comm, pdcp_to_rlc_comm, rlc_to_mac_comm, rlc_to_pdcp_comm, this);
            pdcp->start(rlc_to_pdcp_comm, rrc_to_pdcp_comm, gw_to_pdcp_comm, pdcp_to_rlc_comm, pdcp_to_rrc_comm, pdcp_to_gw_comm, this);
            rrc->start(pdcp_to_rrc_comm, mme_to_rrc_comm, rrc_to_pdcp_comm, rrc_to_mme_comm, this);
            mme->start(rrc_to_mme_comm, mme_to_rrc_comm, this);
            err = radio->start();
            if(LTE_FDD_ENB_ERROR_NONE == err)
            {
                send_ctrl_error_msg(err, "");
            }else{
                sem_wait(&start_sem);
                started = false;
                sem_post(&start_sem);

                phy->stop();
                mac->stop();
                timer_mgr->stop();
                rlc->stop();
                pdcp->stop();
                rrc->stop();
                mme->stop();

                send_ctrl_error_msg(err, "");
            }
        }else{
            send_ctrl_error_msg(err, err_str);
        }
    }else{
        sem_post(&start_sem);
        send_ctrl_error_msg(LTE_FDD_ENB_ERROR_ALREADY_STARTED, "");
    }
}
void LTE_fdd_enb_interface::handle_stop(void)
{
    LTE_fdd_enb_timer_mgr  *timer_mgr = LTE_fdd_enb_timer_mgr::get_instance();
    LTE_fdd_enb_radio      *radio     = LTE_fdd_enb_radio::get_instance();
    LTE_fdd_enb_phy        *phy       = LTE_fdd_enb_phy::get_instance();
    LTE_fdd_enb_mac        *mac       = LTE_fdd_enb_mac::get_instance();
    LTE_fdd_enb_rlc        *rlc       = LTE_fdd_enb_rlc::get_instance();
    LTE_fdd_enb_rrc        *rrc       = LTE_fdd_enb_rrc::get_instance();
    LTE_FDD_ENB_ERROR_ENUM  err;

    sem_wait(&start_sem);
    if(started)
    {
        started = false;
        sem_post(&start_sem);

        // Stop all layers
        err = radio->stop();
        if(LTE_FDD_ENB_ERROR_NONE == err)
        {
            phy->stop();
            mac->stop();
            timer_mgr->stop();
            rlc->stop();
            pdcp->stop();
            rrc->stop();
            mme->stop();
            gw->stop();

            // Send a message to all inter-stack communication message queues and cleanup
            phy_to_mac_comm->send(LTE_FDD_ENB_MESSAGE_TYPE_KILL,
                                  LTE_FDD_ENB_DEST_LAYER_ANY,
                                  NULL,
                                  0);
            mac_to_phy_comm->send(LTE_FDD_ENB_MESSAGE_TYPE_KILL,
                                  LTE_FDD_ENB_DEST_LAYER_ANY,
                                  NULL,
                                  0);
            mac_to_rlc_comm->send(LTE_FDD_ENB_MESSAGE_TYPE_KILL,
                                  LTE_FDD_ENB_DEST_LAYER_ANY,
                                  NULL,
                                  0);
            mac_to_timer_comm->send(LTE_FDD_ENB_MESSAGE_TYPE_KILL,
                                    LTE_FDD_ENB_DEST_LAYER_ANY,
                                    NULL,
                                    0);
            rlc_to_mac_comm->send(LTE_FDD_ENB_MESSAGE_TYPE_KILL,
                                  LTE_FDD_ENB_DEST_LAYER_ANY,
                                  NULL,
                                  0);
            rlc_to_pdcp_comm->send(LTE_FDD_ENB_MESSAGE_TYPE_KILL,
                                   LTE_FDD_ENB_DEST_LAYER_ANY,
                                   NULL,
                                   0);
            pdcp_to_rlc_comm->send(LTE_FDD_ENB_MESSAGE_TYPE_KILL,
                                   LTE_FDD_ENB_DEST_LAYER_ANY,
                                   NULL,
                                   0);
            pdcp_to_rrc_comm->send(LTE_FDD_ENB_MESSAGE_TYPE_KILL,
                                   LTE_FDD_ENB_DEST_LAYER_ANY,
                                   NULL,
                                   0);
            rrc_to_pdcp_comm->send(LTE_FDD_ENB_MESSAGE_TYPE_KILL,
                                   LTE_FDD_ENB_DEST_LAYER_ANY,
                                   NULL,
                                   0);
            rrc_to_mme_comm->send(LTE_FDD_ENB_MESSAGE_TYPE_KILL,
                                  LTE_FDD_ENB_DEST_LAYER_ANY,
                                  NULL,
                                  0);
            mme_to_rrc_comm->send(LTE_FDD_ENB_MESSAGE_TYPE_KILL,
                                  LTE_FDD_ENB_DEST_LAYER_ANY,
                                  NULL,
                                  0);
            pdcp_to_gw_comm->send(LTE_FDD_ENB_MESSAGE_TYPE_KILL,
                                  LTE_FDD_ENB_DEST_LAYER_ANY,
                                  NULL,
                                  0);
            gw_to_pdcp_comm->send(LTE_FDD_ENB_MESSAGE_TYPE_KILL,
                                  LTE_FDD_ENB_DEST_LAYER_ANY,
                                  NULL,
                                  0);
            delete phy_to_mac_comm;
            delete mac_to_phy_comm;
            delete mac_to_rlc_comm;
            delete mac_to_timer_comm;
            delete rlc_to_mac_comm;
            delete rlc_to_pdcp_comm;
            delete pdcp_to_rlc_comm;
            delete pdcp_to_rrc_comm;
            delete rrc_to_pdcp_comm;
            delete rrc_to_mme_comm;
            delete mme_to_rrc_comm;
            delete pdcp_to_gw_comm;
            delete gw_to_pdcp_comm;

            // Cleanup all layers
            LTE_fdd_enb_radio::cleanup();
            LTE_fdd_enb_phy::cleanup();
            LTE_fdd_enb_mac::cleanup();
            LTE_fdd_enb_timer_mgr::cleanup();
            LTE_fdd_enb_rlc::cleanup();
            LTE_fdd_enb_rrc::cleanup();

            // Delete layers
            delete pdcp;
            delete mme;
            delete gw;

            send_ctrl_error_msg(LTE_FDD_ENB_ERROR_NONE, "");
        }else{
            send_ctrl_error_msg(err, "");
        }
    }else{
        sem_post(&start_sem);
        send_ctrl_error_msg(LTE_FDD_ENB_ERROR_ALREADY_STOPPED, "");
    }
}
void LTE_fdd_enb_interface::handle_help(void)
{
    LTE_fdd_enb_cnfg_db                                     *cnfg_db = LTE_fdd_enb_cnfg_db::get_instance();
    LTE_fdd_enb_radio                                       *radio   = LTE_fdd_enb_radio::get_instance();
    std::map<std::string, LTE_FDD_ENB_VAR_STRUCT>::iterator  iter;
    std::string                                              tmp_str;
    std::string                                              s_value;
    LTE_FDD_ENB_AVAILABLE_RADIOS_STRUCT                      avail_radios   = radio->get_available_radios();
    LTE_FDD_ENB_RADIO_STRUCT                                 selected_radio = radio->get_selected_radio();
    double                                                   d_value;
    int64                                                    i_value;
    uint32                                                   u_value;
    uint32                                                   i;

    send_ctrl_msg("***System Configuration Parameters***");
    send_ctrl_msg("\tRead parameters using read <param> format");
    send_ctrl_msg("\tSet parameters using write <param> <value> format");
    // Commands
    send_ctrl_msg("\tCommands:");
    send_ctrl_msg("\t\tstart                                  - Constructs the system information and starts the eNB");
    send_ctrl_msg("\t\tstop                                   - Stops the eNB");
    send_ctrl_msg("\t\tshutdown                               - Stops the eNB and exits");
    send_ctrl_msg("\t\tconstruct_si                           - Constructs the new system information");
    send_ctrl_msg("\t\thelp                                   - Prints this screen");
    send_ctrl_msg("\t\tadd_user imsi=<imsi> imei=<imei> k=<k> - Adds a user to the HSS (<imsi> and <imei> are 15 decimal digits, and <k> is 32 hex digits)");
    send_ctrl_msg("\t\tdel_user imsi=<imsi>                   - Deletes a user from the HSS");
    send_ctrl_msg("\t\tprint_users                            - Prints all the users in the HSS");
    send_ctrl_msg("\t\tprint_registered_users                 - Prints all the users currently registered");

    // Radio Parameters
    send_ctrl_msg("\tRadio Parameters:");
    tmp_str  = "\t\t";
    tmp_str += LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_AVAILABLE_RADIOS];
    tmp_str += ": (read-only)";
    send_ctrl_msg(tmp_str);
    for(i=0; i<avail_radios.num_radios; i++)
    {
        tmp_str  = "\t\t\t";
        tmp_str += to_string(i);
        tmp_str += ": ";
        tmp_str += avail_radios.radio[i].name;
        send_ctrl_msg(tmp_str);
    }
    tmp_str  = "\t\t";
    tmp_str += LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_SELECTED_RADIO_NAME];
    tmp_str += " (read-only) = ";
    tmp_str += selected_radio.name;
    send_ctrl_msg(tmp_str);
    tmp_str  = "\t\t";
    tmp_str += LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_SELECTED_RADIO_IDX];
    tmp_str += " = ";
    tmp_str += to_string(radio->get_selected_radio_idx());
    send_ctrl_msg(tmp_str);
    tmp_str  = "\t\t";
    tmp_str += LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_CLOCK_SOURCE];
    tmp_str += " = ";
    tmp_str += radio->get_clock_source();
    send_ctrl_msg(tmp_str);

    // System Parameters
    send_ctrl_msg("\tSystem Parameters:");
    for(iter=var_map.begin(); iter!=var_map.end(); iter++)
    {
        tmp_str  = "\t\t";
        tmp_str += LTE_fdd_enb_param_text[(*iter).second.param];
        tmp_str += " = ";
        switch((*iter).second.var_type)
        {
        case LTE_FDD_ENB_VAR_TYPE_DOUBLE:
            cnfg_db->get_param((*iter).second.param, d_value);
            tmp_str += to_string(d_value);
            break;
        case LTE_FDD_ENB_VAR_TYPE_INT64:
            cnfg_db->get_param((*iter).second.param, i_value);
            if(LTE_FDD_ENB_PARAM_FREQ_BAND == (*iter).second.param)
            {
                tmp_str += liblte_interface_band_text[i_value];
            }else{
                tmp_str += to_string(i_value);
            }
            break;
        case LTE_FDD_ENB_VAR_TYPE_HEX:
            s_value.clear();
            cnfg_db->get_param((*iter).second.param, s_value);
            tmp_str += s_value;
            break;
        case LTE_FDD_ENB_VAR_TYPE_UINT32:
            cnfg_db->get_param((*iter).second.param, u_value);
            if(LTE_FDD_ENB_PARAM_DEBUG_TYPE == (*iter).second.param)
            {
                for(i=0; i<LTE_FDD_ENB_DEBUG_TYPE_N_ITEMS; i++)
                {
                    if((u_value & (1 << i)))
                    {
                        tmp_str += LTE_fdd_enb_debug_type_text[i];
                        tmp_str += " ";
                    }
                }
            }else if(LTE_FDD_ENB_PARAM_DEBUG_LEVEL == (*iter).second.param){
                for(i=0; i<LTE_FDD_ENB_DEBUG_LEVEL_N_ITEMS; i++)
                {
                    if((u_value & (1 << i)))
                    {
                        tmp_str += LTE_fdd_enb_debug_level_text[i];
                        tmp_str += " ";
                    }
                }
            }else{
                tmp_str += to_string(u_value);
            }
            break;
        }
        send_ctrl_msg(tmp_str);
    }
}
void LTE_fdd_enb_interface::handle_add_user(std::string msg)
{
    LTE_fdd_enb_hss *hss = LTE_fdd_enb_hss::get_instance();
    std::string      imsi_str;
    std::string      imei_str;
    std::string      k_str;
    bool             imsi_valid = true;
    bool             imei_valid = true;
    bool             k_valid    = true;

    // Extract IMSI and check
    imsi_str   = msg.substr(msg.find("imsi")+sizeof("imsi"), std::string::npos);
    imsi_str   = imsi_str.substr(0, imsi_str.find(" "));
    imsi_valid = is_string_valid_as_number(imsi_str, 15, 10);

    // Extract IMEI and check
    imei_str   = msg.substr(msg.find("imei")+sizeof("imei"), std::string::npos);
    imei_str   = imei_str.substr(0, imei_str.find(" "));
    imei_valid = is_string_valid_as_number(imei_str, 15, 10);

    // Extract K and check
    k_str   = msg.substr(msg.find("k")+sizeof("k"), std::string::npos);
    k_str   = k_str.substr(0, k_str.find(" "));
    k_valid = is_string_valid_as_number(k_str, 32, 16);

    if(imsi_valid && imei_valid && k_valid)
    {
        send_ctrl_error_msg(hss->add_user(imsi_str, imei_str, k_str), "");
    }else{
        send_ctrl_error_msg(LTE_FDD_ENB_ERROR_INVALID_PARAM, "");
    }
}
void LTE_fdd_enb_interface::handle_del_user(std::string msg)
{
    LTE_fdd_enb_hss *hss = LTE_fdd_enb_hss::get_instance();
    std::string      imsi_str;

    // Extract IMSI
    imsi_str = msg.substr(msg.find("imsi")+sizeof("imsi"), std::string::npos);
    imsi_str = imsi_str.substr(0, imsi_str.find(" "));

    if(is_string_valid_as_number(imsi_str, 15, 10))
    {
        send_ctrl_error_msg(hss->del_user(imsi_str), "");
    }else{
        send_ctrl_error_msg(LTE_FDD_ENB_ERROR_INVALID_PARAM, "");
    }
}
void LTE_fdd_enb_interface::handle_print_users(void)
{
    LTE_fdd_enb_hss *hss = LTE_fdd_enb_hss::get_instance();

    send_ctrl_error_msg(LTE_FDD_ENB_ERROR_NONE, hss->print_all_users());
}
void LTE_fdd_enb_interface::handle_print_registered_users(void)
{
    LTE_fdd_enb_user_mgr *user_mgr = LTE_fdd_enb_user_mgr::get_instance();

    send_ctrl_error_msg(LTE_FDD_ENB_ERROR_NONE, user_mgr->print_all_users());
}

/*******************/
/*    Gets/Sets    */
/*******************/
bool LTE_fdd_enb_interface::get_shutdown(void)
{
    return(shutdown);
}
bool LTE_fdd_enb_interface::app_is_started(void)
{
    libtools_scoped_lock lock(start_sem);
    return(started);
}

/*****************/
/*    Helpers    */
/*****************/
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_interface::write_value(LTE_FDD_ENB_VAR_STRUCT *var,
                                                          double                  value)
{
    LTE_fdd_enb_cnfg_db    *cnfg_db = LTE_fdd_enb_cnfg_db::get_instance();
    LTE_FDD_ENB_ERROR_ENUM  err     = LTE_FDD_ENB_ERROR_OUT_OF_BOUNDS;

    if(started && !var->dynamic)
    {
        err = LTE_FDD_ENB_ERROR_VARIABLE_NOT_DYNAMIC;
    }else{
        if(!var->special_bounds)
        {
            if(value >= var->double_l_bound &&
               value <= var->double_u_bound)
            {
                err = cnfg_db->set_param(var->param, value);
            }
        }else{
            if(LTE_FDD_ENB_PARAM_BANDWIDTH == var->param &&
               (value                      == 1.4        ||
                value                      == 3          ||
                value                      == 5          ||
                value                      == 10         ||
                value                      == 15         ||
                value                      == 20))
            {
                err = cnfg_db->set_param(var->param, value);
            }else{
                err = LTE_FDD_ENB_ERROR_INVALID_PARAM;
            }
        }
    }

    return(err);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_interface::write_value(LTE_FDD_ENB_VAR_STRUCT *var,
                                                          int64                   value)
{
    LTE_fdd_enb_cnfg_db    *cnfg_db = LTE_fdd_enb_cnfg_db::get_instance();
    LTE_FDD_ENB_ERROR_ENUM  err     = LTE_FDD_ENB_ERROR_OUT_OF_BOUNDS;
    int64                   band    = LIBLTE_INTERFACE_BAND_N_ITEMS;
    uint32                  i;

    if(started && !var->dynamic)
    {
        err = LTE_FDD_ENB_ERROR_VARIABLE_NOT_DYNAMIC;
    }else{
        if(!var->special_bounds)
        {
            if(value >= var->int64_l_bound &&
               value <= var->int64_u_bound)
            {
                err = cnfg_db->set_param(var->param, value);
            }
        }else{
            if(LTE_FDD_ENB_PARAM_N_ANT == var->param &&
               (value                  == 1          ||
                value                  == 2          ||
                value                  == 4))
            {
                err = cnfg_db->set_param(var->param, value);
            }else if(LTE_FDD_ENB_PARAM_Q_HYST == var->param){
                for(i=0; i<LIBLTE_RRC_Q_HYST_N_ITEMS; i++)
                {
                    if(value == liblte_rrc_q_hyst_num[i])
                    {
                        err = cnfg_db->set_param(var->param, value);
                        break;
                    }
                }
            }else if(LTE_FDD_ENB_PARAM_FREQ_BAND == var->param){
                for(i=0; i<LIBLTE_INTERFACE_BAND_N_ITEMS; i++)
                {
                    if(value == liblte_interface_band_num[i])
                    {
                        err = cnfg_db->set_param(var->param, (int64)i);
                        err = cnfg_db->set_param(LTE_FDD_ENB_PARAM_DL_EARFCN, (int64)liblte_interface_first_dl_earfcn[i]);
                        if(app_is_started())
                        {
                            cnfg_db->construct_sys_info(pdcp, mme);
                        }
                        break;
                    }
                }
            }else if(LTE_FDD_ENB_PARAM_DL_EARFCN == var->param){
                cnfg_db->get_param(LTE_FDD_ENB_PARAM_FREQ_BAND, band);
                if(value >= liblte_interface_first_dl_earfcn[band] &&
                   value <= liblte_interface_last_dl_earfcn[band])
                {
                    err = cnfg_db->set_param(var->param, value);
                }
            }else{
                err = LTE_FDD_ENB_ERROR_INVALID_PARAM;
            }
        }
    }

    return(err);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_interface::write_value(LTE_FDD_ENB_VAR_STRUCT *var,
                                                          std::string             value)
{
    LTE_fdd_enb_cnfg_db    *cnfg_db = LTE_fdd_enb_cnfg_db::get_instance();
    LTE_FDD_ENB_ERROR_ENUM  err     = LTE_FDD_ENB_ERROR_OUT_OF_BOUNDS;
    uint32                  i;
    const char             *v_str = value.c_str();

    if(started && !var->dynamic)
    {
        err = LTE_FDD_ENB_ERROR_VARIABLE_NOT_DYNAMIC;
    }else{
        if((LTE_FDD_ENB_PARAM_MCC == var->param &&
            value.length()        == 3)         ||
           (LTE_FDD_ENB_PARAM_MNC == var->param &&
            (value.length()       == 2          ||
             value.length()       == 3)))
        {
            for(i=0; i<value.length(); i++)
            {
                if((v_str[i] & 0x0F) >= 0x0A)
                {
                    break;
                }
            }
            if(i == value.length())
            {
                err = cnfg_db->set_param(var->param, value);
            }
        }else if((LTE_FDD_ENB_PARAM_IP_ADDR_START == var->param  ||
                  LTE_FDD_ENB_PARAM_DNS_ADDR      == var->param) &&
                 value.length()                   == 8){
            err = cnfg_db->set_param(var->param, value);
        }else{
            err = LTE_FDD_ENB_ERROR_INVALID_PARAM;
        }
    }

    return(err);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_interface::write_value(LTE_FDD_ENB_VAR_STRUCT *var,
                                                          uint32                  value)
{
    LTE_fdd_enb_cnfg_db    *cnfg_db = LTE_fdd_enb_cnfg_db::get_instance();
    LTE_FDD_ENB_ERROR_ENUM  err     = LTE_FDD_ENB_ERROR_OUT_OF_BOUNDS;

    if(started && !var->dynamic)
    {
        err = LTE_FDD_ENB_ERROR_VARIABLE_NOT_DYNAMIC;
    }else{
        if(!var->special_bounds)
        {
            if(value >= var->int64_l_bound &&
               value <= var->int64_u_bound)
            {
                err = cnfg_db->set_param(var->param, value);
            }
        }else{
            err = cnfg_db->set_param(var->param, value);
        }
    }

    return(err);
}
