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

    File: libtools_socket_wrap.h

    Description: Contains all the definitions for the text mode communication
                 socket abstraction tool.

    Revision History
    ----------    -------------    --------------------------------------------
    02/26/2013    Ben Wojtowicz    Created file
    07/21/2013    Ben Wojtowicz    Added enum to text conversion
    12/06/2015    Ben Wojtowicz    Change from boost::mutex to sem_t.

*******************************************************************************/

#ifndef __LIBTOOLS_SOCKET_WRAP_H__
#define __LIBTOOLS_SOCKET_WRAP_H__

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "typedefs.h"
#include <pthread.h>
#include <string>
#include <semaphore.h>

/*******************************************************************************
                              DEFINES
*******************************************************************************/


/*******************************************************************************
                              FORWARD DECLARATIONS
*******************************************************************************/

class libtools_socket_wrap;

/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/

typedef enum{
    LIBTOOLS_SOCKET_WRAP_SUCCESS = 0,
    LIBTOOLS_SOCKET_WRAP_ERROR_INVALID_INPUTS,
    LIBTOOLS_SOCKET_WRAP_ERROR_SOCKET,
    LIBTOOLS_SOCKET_WRAP_ERROR_PTHREAD,
    LIBTOOLS_SOCKET_WRAP_ERROR_WRITE_FAIL,
    LIBTOOLS_SOCKET_WRAP_ERROR_N_ITEMS,
}LIBTOOLS_SOCKET_WRAP_ERROR_ENUM;
static const char libtools_socket_wrap_error_text[LIBTOOLS_SOCKET_WRAP_ERROR_N_ITEMS][20] = {"Success",
                                                                                             "Invalid Inputs",
                                                                                             "Socket",
                                                                                             "PThread",
                                                                                             "Write Fail"};

typedef enum{
    LIBTOOLS_SOCKET_WRAP_TYPE_SERVER = 0,
    LIBTOOLS_SOCKET_WRAP_TYPE_CLIENT,
}LIBTOOLS_SOCKET_WRAP_TYPE_ENUM;

typedef enum{
    LIBTOOLS_SOCKET_WRAP_STATE_DISCONNECTED = 0,
    LIBTOOLS_SOCKET_WRAP_STATE_THREADED,
    LIBTOOLS_SOCKET_WRAP_STATE_CONNECTED,
    LIBTOOLS_SOCKET_WRAP_STATE_CLOSED,
}LIBTOOLS_SOCKET_WRAP_STATE_ENUM;

typedef struct{
    libtools_socket_wrap *socket_wrap;
    void                  (*handle_msg)(std::string msg);
    void                  (*handle_connect)(void);
    void                  (*handle_disconnect)(void);
    void                  (*handle_error)(LIBTOOLS_SOCKET_WRAP_ERROR_ENUM err);
    int32                 sock;
}LIBTOOLS_SOCKET_WRAP_THREAD_INPUTS_STRUCT;

/*******************************************************************************
                              CLASS DECLARATIONS
*******************************************************************************/

class libtools_socket_wrap
{
public:
    libtools_socket_wrap(uint32                          *ip_addr,
                         uint16                           port,
                         LIBTOOLS_SOCKET_WRAP_TYPE_ENUM   type,
                         void                             (*handle_msg)(std::string msg),
                         void                             (*handle_connect)(void),
                         void                             (*handle_disconnect)(void),
                         void                             (*handle_error)(LIBTOOLS_SOCKET_WRAP_ERROR_ENUM err),
                         LIBTOOLS_SOCKET_WRAP_ERROR_ENUM *error);
    ~libtools_socket_wrap();

    // Send
    LIBTOOLS_SOCKET_WRAP_ERROR_ENUM send(std::string msg);
private:
    // Receive
    static void* server_thread(void *inputs);
    static void* client_thread(void *inputs);

    // Variables
    LIBTOOLS_SOCKET_WRAP_THREAD_INPUTS_STRUCT thread_inputs;
    LIBTOOLS_SOCKET_WRAP_STATE_ENUM           socket_state;
    pthread_t                                 socket_thread;
    sem_t                                     socket_sem;
    uint32                                    socket_ip_addr;
    int32                                     socket_fd;
    int32                                     sock;
    uint16                                    socket_port;
    void                                      (*socket_handle_msg)(std::string msg);
    void                                      (*socket_handle_connect)(void);
    void                                      (*socket_handle_disconnect)(void);
    void                                      (*socket_handle_error)(LIBTOOLS_SOCKET_WRAP_ERROR_ENUM err);
};

#endif /* __LIBTOOLS_SOCKET_WRAP_H__ */
