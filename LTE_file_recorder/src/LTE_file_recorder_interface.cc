/*******************************************************************************

    Copyright 2013,2015,2017 Ben Wojtowicz

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

    File: LTE_file_recorder_interface.cc

    Description: Contains all the implementations for the LTE file recorder
                 interface.

    Revision History
    ----------    -------------    --------------------------------------------
    08/26/2013    Ben Wojtowicz    Created file
    12/06/2015    Ben Wojtowicz    Changed boost::mutex to pthread_mutex_t.
    07/29/2017    Ben Wojtowicz    Using the latest tools library.

*******************************************************************************/

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_file_recorder_interface.h"
#include "LTE_file_recorder_flowgraph.h"
#include "libtools_scoped_lock.h"
#include "libtools_helpers.h"

/*******************************************************************************
                              DEFINES
*******************************************************************************/

#define EARFCN_PARAM    "earfcn"
#define FILE_NAME_PARAM "file_name"

/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/


/*******************************************************************************
                              GLOBAL VARIABLES
*******************************************************************************/

LTE_file_recorder_interface* LTE_file_recorder_interface::instance       = NULL;
static pthread_mutex_t       interface_instance_mutex                    = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t       connect_mutex                               = PTHREAD_MUTEX_INITIALIZER;
bool                         LTE_file_recorder_interface::ctrl_connected = false;

/*******************************************************************************
                              CLASS IMPLEMENTATIONS
*******************************************************************************/

// Singleton
LTE_file_recorder_interface* LTE_file_recorder_interface::get_instance(void)
{
    libtools_scoped_lock lock(interface_instance_mutex);

    if(NULL == instance)
    {
        instance = new LTE_file_recorder_interface();
    }

    return(instance);
}
void LTE_file_recorder_interface::cleanup(void)
{
    libtools_scoped_lock lock(interface_instance_mutex);

    if(NULL != instance)
    {
        delete instance;
        instance = NULL;
    }
}

// Constructor/Destructor
LTE_file_recorder_interface::LTE_file_recorder_interface()
{
    uint32 i;

    // Communication
    pthread_mutex_init(&ctrl_mutex, NULL);
    ctrl_socket    = NULL;
    ctrl_port      = LTE_FILE_RECORDER_DEFAULT_CTRL_PORT;
    ctrl_connected = false;

    // Variables
    earfcn    = liblte_interface_first_dl_earfcn[0];
    file_name = "/tmp/lte_iq_file.bin";
    shutdown  = false;
}
LTE_file_recorder_interface::~LTE_file_recorder_interface()
{
    stop_ctrl_port();
    pthread_mutex_destroy(&ctrl_mutex);
}

// Communication
void LTE_file_recorder_interface::set_ctrl_port(int16 port)
{
    libtools_scoped_lock lock(connect_mutex);

    if(!ctrl_connected)
    {
        ctrl_port = port;
    }
}
void LTE_file_recorder_interface::start_ctrl_port(void)
{
    libtools_scoped_lock            lock(ctrl_mutex);
    LIBTOOLS_SOCKET_WRAP_ERROR_ENUM error;

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
            printf("ERROR: Couldn't open ctrl_socket %u\n", error);
            ctrl_socket = NULL;
        }
    }
}
void LTE_file_recorder_interface::stop_ctrl_port(void)
{
    libtools_scoped_lock lock(ctrl_mutex);

    if(NULL != ctrl_socket)
    {
        delete ctrl_socket;
        ctrl_socket = NULL;
    }
}
void LTE_file_recorder_interface::send_ctrl_msg(std::string msg)
{
    libtools_scoped_lock lock(connect_mutex);
    std::string          tmp_msg;

    if(ctrl_connected)
    {
        tmp_msg  = msg;
        tmp_msg += "\n";
        ctrl_socket->send(tmp_msg);
    }
}
void LTE_file_recorder_interface::send_ctrl_info_msg(std::string msg)
{
    libtools_scoped_lock lock(connect_mutex);
    std::string          tmp_msg;

    if(ctrl_connected)
    {
        tmp_msg  = "info ";
        tmp_msg += msg;
        tmp_msg += "\n";
        ctrl_socket->send(tmp_msg);
    }
}
void LTE_file_recorder_interface::send_ctrl_status_msg(LTE_FILE_RECORDER_STATUS_ENUM status,
                                                       std::string                   msg)
{
    libtools_scoped_lock lock(connect_mutex);
    std::string          tmp_msg;

    if(ctrl_connected)
    {
        if(LTE_FILE_RECORDER_STATUS_OK == status)
        {
            tmp_msg = "ok ";
        }else{
            tmp_msg = "fail ";
        }
        tmp_msg += msg;
        tmp_msg += "\n";

        ctrl_socket->send(tmp_msg);
    }
}
void LTE_file_recorder_interface::handle_ctrl_msg(std::string msg)
{
    LTE_file_recorder_interface *interface = LTE_file_recorder_interface::get_instance();

    if(std::string::npos != msg.find("read"))
    {
        interface->handle_read(msg);
    }else if(std::string::npos != msg.find("write")){
        interface->handle_write(msg);
    }else if(std::string::npos != msg.find("start")){
        interface->handle_start();
    }else if(std::string::npos != msg.find("stop")){
        interface->handle_stop();
    }else if(std::string::npos != msg.find("shutdown")){
        interface->shutdown = true;
        interface->send_ctrl_status_msg(LTE_FILE_RECORDER_STATUS_OK, "");
    }else if(std::string::npos != msg.find("help")){
        interface->handle_help();
    }else{
        interface->send_ctrl_status_msg(LTE_FILE_RECORDER_STATUS_FAIL, "Invalid param");
    }
}
void LTE_file_recorder_interface::handle_ctrl_connect(void)
{
    LTE_file_recorder_interface *interface = LTE_file_recorder_interface::get_instance();

    pthread_mutex_lock(&connect_mutex);
    LTE_file_recorder_interface::ctrl_connected = true;
    pthread_mutex_unlock(&connect_mutex);

    interface->send_ctrl_msg("*** LTE File Recorder ***");
    interface->send_ctrl_msg("Type help to see a list of commands");
}
void LTE_file_recorder_interface::handle_ctrl_disconnect(void)
{
    libtools_scoped_lock lock(connect_mutex);

    LTE_file_recorder_interface::ctrl_connected = false;
}
void LTE_file_recorder_interface::handle_ctrl_error(LIBTOOLS_SOCKET_WRAP_ERROR_ENUM err)
{
    printf("ERROR: ctrl_socket error %u\n", err);
    assert(0);
}

// Handlers
void LTE_file_recorder_interface::handle_read(std::string msg)
{
    if(std::string::npos != msg.find(EARFCN_PARAM))
    {
        read_earfcn();
    }else if(std::string::npos != msg.find(FILE_NAME_PARAM)){
        read_file_name();
    }else{
        send_ctrl_status_msg(LTE_FILE_RECORDER_STATUS_FAIL, "Invalid read");
    }
}
void LTE_file_recorder_interface::handle_write(std::string msg)
{
    if(std::string::npos != msg.find(EARFCN_PARAM))
    {
        write_earfcn(msg.substr(msg.find(EARFCN_PARAM)+sizeof(EARFCN_PARAM), std::string::npos).c_str());
    }else if(std::string::npos != msg.find(FILE_NAME_PARAM)){
        write_file_name(msg.substr(msg.find(FILE_NAME_PARAM)+sizeof(FILE_NAME_PARAM), std::string::npos).c_str());
    }else{
        send_ctrl_status_msg(LTE_FILE_RECORDER_STATUS_FAIL, "Invalid write");
    }
}
void LTE_file_recorder_interface::handle_start(void)
{
    LTE_file_recorder_flowgraph *flowgraph = LTE_file_recorder_flowgraph::get_instance();

    if(!flowgraph->is_started())
    {
        if(LTE_FILE_RECORDER_STATUS_OK == flowgraph->start(earfcn, file_name))
        {
            send_ctrl_status_msg(LTE_FILE_RECORDER_STATUS_OK, "");
        }else{
            send_ctrl_status_msg(LTE_FILE_RECORDER_STATUS_FAIL, "Start fail, likely there is no hardware connected");
        }
    }else{
        send_ctrl_status_msg(LTE_FILE_RECORDER_STATUS_FAIL, "Flowgraph already started");
    }
}
void LTE_file_recorder_interface::handle_stop(void)
{
    LTE_file_recorder_flowgraph *flowgraph = LTE_file_recorder_flowgraph::get_instance();

    if(flowgraph->is_started())
    {
        if(LTE_FILE_RECORDER_STATUS_OK == flowgraph->stop())
        {
            send_ctrl_status_msg(LTE_FILE_RECORDER_STATUS_OK, "");
        }else{
            send_ctrl_status_msg(LTE_FILE_RECORDER_STATUS_FAIL, "Stop fail");
        }
    }else{
        send_ctrl_status_msg(LTE_FILE_RECORDER_STATUS_FAIL, "Flowgraph not started");
    }
}
void LTE_file_recorder_interface::handle_help(void)
{
    std::string tmp_str;

    send_ctrl_msg("***System Configuration Parameters***");
    send_ctrl_msg("\tRead parameters using read <param> format");
    send_ctrl_msg("\tSet parameters using write <param> <value> format");
    send_ctrl_msg("\tCommands:");
    send_ctrl_msg("\t\tstart    - Starts recording the specified earfcn");
    send_ctrl_msg("\t\tstop     - Stops the recording");
    send_ctrl_msg("\t\tshutdown - Stops the recording and exits");
    send_ctrl_msg("\t\thelp     - Prints this screen");
    send_ctrl_msg("\tParameters:");

    // EARFCN
    tmp_str  = "\t\t";
    tmp_str += EARFCN_PARAM;
    tmp_str += " = ";
    tmp_str += to_string(earfcn);
    send_ctrl_msg(tmp_str);

    // FILE_NAME
    tmp_str  = "\t\t";
    tmp_str += FILE_NAME_PARAM;
    tmp_str += " = " + file_name;
    send_ctrl_msg(tmp_str);
}

// Gets/Sets
bool LTE_file_recorder_interface::get_shutdown(void)
{
    return(shutdown);
}

// Reads/Writes
void LTE_file_recorder_interface::read_earfcn(void)
{
    send_ctrl_status_msg(LTE_FILE_RECORDER_STATUS_OK, to_string(earfcn).c_str());
}
void LTE_file_recorder_interface::write_earfcn(std::string earfcn_str)
{
    uint16 tmp_earfcn = LIBLTE_INTERFACE_DL_EARFCN_INVALID;

    to_number(earfcn_str, &tmp_earfcn);

    if(0 != liblte_interface_dl_earfcn_to_frequency(tmp_earfcn) ||
       0 != liblte_interface_ul_earfcn_to_frequency(tmp_earfcn))
    {
        earfcn = tmp_earfcn;
        send_ctrl_status_msg(LTE_FILE_RECORDER_STATUS_OK, "");
    }else{
        send_ctrl_status_msg(LTE_FILE_RECORDER_STATUS_FAIL, "Invalid earfcn");
    }
}
void LTE_file_recorder_interface::read_file_name(void)
{
    send_ctrl_status_msg(LTE_FILE_RECORDER_STATUS_OK, file_name.c_str());
}
void LTE_file_recorder_interface::write_file_name(std::string file_name_str)
{
    file_name = file_name_str;
    send_ctrl_status_msg(LTE_FILE_RECORDER_STATUS_OK, "\"File name not checked\"");
}
