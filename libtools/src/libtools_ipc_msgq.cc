/*******************************************************************************

    Copyright 2017 Ben Wojtowicz

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

    File: libtools_ipc_msgq.cc

    Description: Contains all the implementations for the interprocess message
                 queue abstraction tool.

    Revision History
    ----------    -------------    --------------------------------------------
    07/29/2017    Ben Wojtowicz    Created file

*******************************************************************************/

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "libtools_ipc_msgq.h"
#include <boost/interprocess/ipc/message_queue.hpp>

/*******************************************************************************
                              DEFINES
*******************************************************************************/


/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/


/*******************************************************************************
                              GLOBAL VARIABLES
*******************************************************************************/


/*******************************************************************************
                              CLASS IMPLEMENTATIONS
*******************************************************************************/

/******************/
/*    Callback    */
/******************/
libtools_ipc_msgq_cb::libtools_ipc_msgq_cb()
{
}
libtools_ipc_msgq_cb::libtools_ipc_msgq_cb(FuncType f, void* o)
{
    func = f;
    obj  = o;
}
void libtools_ipc_msgq_cb::operator()(LIBTOOLS_IPC_MSGQ_MESSAGE_STRUCT *msg)
{
    return (*func)(obj, msg);
}

/********************************/
/*    Constructor/Destructor    */
/********************************/
libtools_ipc_msgq::libtools_ipc_msgq(std::string          _msgq_name,
                                     libtools_ipc_msgq_cb cb)
{
    msgq_name = _msgq_name;
    callback  = cb;
    pthread_create(&rx_thread, NULL, &receive_thread, this);
}
libtools_ipc_msgq::~libtools_ipc_msgq()
{
    send(LIBTOOLS_IPC_MSGQ_MESSAGE_TYPE_KILL, NULL, 0);

    // Cleanup thread
    pthread_cancel(rx_thread);
    pthread_join(rx_thread, NULL);
}

/**********************/
/*    Send/Receive    */
/**********************/
void libtools_ipc_msgq::send(LIBTOOLS_IPC_MSGQ_MESSAGE_TYPE_ENUM  type,
                             LIBTOOLS_IPC_MSGQ_MESSAGE_UNION     *msg_content,
                             uint32                               msg_content_size)
{
    LIBTOOLS_IPC_MSGQ_MESSAGE_STRUCT   msg;
    boost::interprocess::message_queue mq(boost::interprocess::open_only,
                                          msgq_name.c_str());

    msg.type = type;
    if(msg_content != NULL)
    {
        memcpy(&msg.msg, msg_content, msg_content_size);
    }

    mq.try_send(&msg, sizeof(LIBTOOLS_IPC_MSGQ_MESSAGE_STRUCT), 0);
}
void* libtools_ipc_msgq::receive_thread(void *inputs)
{
    libtools_ipc_msgq                *msgq = (libtools_ipc_msgq *)inputs;
    LIBTOOLS_IPC_MSGQ_MESSAGE_STRUCT  msg;
    std::size_t                       rx_size;
    uint32                            prio;
    bool                              not_done = true;

    // Open the message_queue
    boost::interprocess::message_queue mq(boost::interprocess::open_or_create,
                                          msgq->msgq_name.c_str(),
                                          100,
                                          sizeof(LIBTOOLS_IPC_MSGQ_MESSAGE_STRUCT));

    while(not_done)
    {
        // FIXME: SET A TIMEOUT

        // Wait for a message
        mq.receive(&msg, sizeof(msg), rx_size, prio);

        // Process message
        if(sizeof(msg) == rx_size)
        {
            msgq->callback(&msg);

            if(LIBTOOLS_IPC_MSGQ_MESSAGE_TYPE_KILL == msg.type)
            {
                not_done = false;
            }
        }else{
            // FIXME
            printf("ERROR %s Invalid message size received: %u\n",
                   msgq->msgq_name.c_str(),
                   (uint32)rx_size);
        }
    }

    return(NULL);
}
