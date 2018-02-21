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

    File: LTE_file_recorder_interface.h

    Description: Contains all the definitions for the LTE file recorder
                 interface.

    Revision History
    ----------    -------------    --------------------------------------------
    08/26/2013    Ben Wojtowicz    Created file
    12/06/2015    Ben Wojtowicz    Changed boost::mutex to pthread_mutex_t.

*******************************************************************************/

#ifndef __LTE_FILE_RECORDER_INTERFACE_H__
#define __LTE_FILE_RECORDER_INTERFACE_H__

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "liblte_interface.h"
#include "libtools_socket_wrap.h"
#include <string>

/*******************************************************************************
                              DEFINES
*******************************************************************************/

#define LTE_FILE_RECORDER_DEFAULT_CTRL_PORT 25000

/*******************************************************************************
                              FORWARD DECLARATIONS
*******************************************************************************/


/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/

typedef enum{
    LTE_FILE_RECORDER_STATUS_OK = 0,
    LTE_FILE_RECORDER_STATUS_FAIL,
}LTE_FILE_RECORDER_STATUS_ENUM;

/*******************************************************************************
                              CLASS DECLARATIONS
*******************************************************************************/

class LTE_file_recorder_interface
{
public:
    // Singleton
    static LTE_file_recorder_interface* get_instance(void);
    static void cleanup(void);

    // Communication
    void set_ctrl_port(int16 port);
    void start_ctrl_port(void);
    void stop_ctrl_port(void);
    void send_ctrl_msg(std::string msg);
    void send_ctrl_info_msg(std::string msg);
    void send_ctrl_status_msg(LTE_FILE_RECORDER_STATUS_ENUM status, std::string msg);
    static void handle_ctrl_msg(std::string msg);
    static void handle_ctrl_connect(void);
    static void handle_ctrl_disconnect(void);
    static void handle_ctrl_error(LIBTOOLS_SOCKET_WRAP_ERROR_ENUM err);
    pthread_mutex_t       ctrl_mutex;
    libtools_socket_wrap *ctrl_socket;
    int16                 ctrl_port;
    static bool           ctrl_connected;

    // Get/Set
    bool get_shutdown(void);

private:
    // Singleton
    static LTE_file_recorder_interface *instance;
    LTE_file_recorder_interface();
    ~LTE_file_recorder_interface();

    // Handlers
    void handle_read(std::string msg);
    void handle_write(std::string msg);
    void handle_start(void);
    void handle_stop(void);
    void handle_help(void);

    // Reads/Writes
    void read_earfcn(void);
    void write_earfcn(std::string earfcn_str);
    void read_file_name(void);
    void write_file_name(std::string file_name_str);

    // Variables
    std::string file_name;
    uint16      earfcn;
    bool        shutdown;
};

#endif /* __LTE_FILE_RECORDER_INTERFACE_H__ */
