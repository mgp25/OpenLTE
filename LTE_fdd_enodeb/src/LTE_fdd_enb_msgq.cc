#line 2 "LTE_fdd_enb_msgq.cc" // Make __FILE__ omit the path
/*******************************************************************************

    Copyright 2013-2016 Ben Wojtowicz

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

    File: LTE_fdd_enb_msgq.cc

    Description: Contains all the implementations for the LTE FDD eNodeB
                 message queues.

    Revision History
    ----------    -------------    --------------------------------------------
    11/10/2013    Ben Wojtowicz    Created file
    11/23/2013    Ben Wojtowicz    Fixed a bug with receive size.
    01/18/2014    Ben Wojtowicz    Added ability to set priorities.
    06/15/2014    Ben Wojtowicz    Omitting path from __FILE__.
    02/15/2015    Ben Wojtowicz    Moving to new message queue with semaphores
                                   and circular buffers.
    03/15/2015    Ben Wojtowicz    Added a mutex to the circular buffer.
    07/25/2015    Ben Wojtowicz    Combined the DL and UL schedule messages into
                                   a single PHY schedule message.
    12/06/2015    Ben Wojtowicz    Changed boost::mutex and
                                   boost::interprocess::interprocess_semaphore
                                   to sem_t and properly initializing priority.
    02/13/2016    Ben Wojtowicz    Moved the buffer empty log from ERROR to
                                   WARNING.
    07/03/2016    Ben Wojtowicz    Setting processor affinity.

*******************************************************************************/

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_fdd_enb_interface.h"
#include "LTE_fdd_enb_msgq.h"

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
LTE_fdd_enb_msgq_cb::LTE_fdd_enb_msgq_cb()
{
}
LTE_fdd_enb_msgq_cb::LTE_fdd_enb_msgq_cb(FuncType f, void* o)
{
    func = f;
    obj  = o;
}
void LTE_fdd_enb_msgq_cb::operator()(LTE_FDD_ENB_MESSAGE_STRUCT &msg)
{
    return (*func)(obj, msg);
}

/********************************/
/*    Constructor/Destructor    */
/********************************/
LTE_fdd_enb_msgq::LTE_fdd_enb_msgq(std::string _msgq_name)
{
    sem_init(&sync_sem, 0, 1);
    sem_init(&msg_sem, 0, 1);
    circ_buf  = new boost::circular_buffer<LTE_FDD_ENB_MESSAGE_STRUCT>(100);
    msgq_name = _msgq_name;
    rx_setup  = false;
}
LTE_fdd_enb_msgq::~LTE_fdd_enb_msgq()
{
    if(rx_setup)
    {
        send(LTE_FDD_ENB_MESSAGE_TYPE_KILL,
             LTE_FDD_ENB_DEST_LAYER_ANY,
             NULL,
             0);

        // Cleanup thread
        pthread_cancel(rx_thread);
        pthread_join(rx_thread, NULL);
        rx_setup = false;
    }
    sem_destroy(&msg_sem);
    sem_destroy(&sync_sem);
    delete circ_buf;
}

/***************/
/*    Setup    */
/***************/
void LTE_fdd_enb_msgq::attach_rx(LTE_fdd_enb_msgq_cb cb)
{
    callback = cb;
    prio     = 0;
    pthread_create(&rx_thread, NULL, &receive_thread, this);
    rx_setup = true;
}
void LTE_fdd_enb_msgq::attach_rx(LTE_fdd_enb_msgq_cb cb,
                                 uint32              _prio)
{
    callback = cb;
    prio     = _prio;
    pthread_create(&rx_thread, NULL, &receive_thread, this);
    rx_setup = true;
}

/**********************/
/*    Send/Receive    */
/**********************/
void LTE_fdd_enb_msgq::send(LTE_FDD_ENB_MESSAGE_TYPE_ENUM  type,
                            LTE_FDD_ENB_DEST_LAYER_ENUM    dest_layer,
                            LTE_FDD_ENB_MESSAGE_UNION     *msg_content,
                            uint32                         msg_content_size)
{
    LTE_FDD_ENB_MESSAGE_STRUCT msg;

    msg.type       = type;
    msg.dest_layer = dest_layer;
    if(msg_content != NULL)
    {
        memcpy(&msg.msg, msg_content, msg_content_size);
    }

    sem_wait(&sync_sem);
    circ_buf->push_back(msg);
    sem_post(&sync_sem);
    sem_post(&msg_sem);
}
void LTE_fdd_enb_msgq::send(LTE_FDD_ENB_MESSAGE_TYPE_ENUM       type,
                            LTE_FDD_ENB_DL_SCHEDULE_MSG_STRUCT *dl_sched,
                            LTE_FDD_ENB_UL_SCHEDULE_MSG_STRUCT *ul_sched)
{
    LTE_FDD_ENB_MESSAGE_STRUCT msg;

    msg.type       = type;
    msg.dest_layer = LTE_FDD_ENB_DEST_LAYER_PHY;
    memcpy(&msg.msg.phy_schedule.dl_sched, dl_sched, sizeof(LTE_FDD_ENB_DL_SCHEDULE_MSG_STRUCT));
    memcpy(&msg.msg.phy_schedule.ul_sched, ul_sched, sizeof(LTE_FDD_ENB_UL_SCHEDULE_MSG_STRUCT));

    sem_wait(&sync_sem);
    circ_buf->push_back(msg);
    sem_post(&sync_sem);
    sem_post(&msg_sem);
}
void LTE_fdd_enb_msgq::send(LTE_FDD_ENB_MESSAGE_STRUCT &msg)
{
    sem_wait(&sync_sem);
    circ_buf->push_back(msg);
    sem_post(&sync_sem);
    sem_post(&msg_sem);
}
void* LTE_fdd_enb_msgq::receive_thread(void *inputs)
{
    LTE_fdd_enb_interface      *interface = LTE_fdd_enb_interface::get_instance();
    LTE_fdd_enb_msgq           *msgq      = (LTE_fdd_enb_msgq *)inputs;
    LTE_FDD_ENB_MESSAGE_STRUCT  msg;
    struct sched_param          priority;
    cpu_set_t                   af_mask;
    bool                        not_done = true;

    // Set priority
    if(msgq->prio != 0)
    {
        // FIXME: verify
        priority.sched_priority = msgq->prio;
        pthread_setschedparam(msgq->rx_thread, SCHED_FIFO, &priority);
    }

    // Set affinity to not the last core (last core is for PHY/Radio)
    pthread_getaffinity_np(msgq->rx_thread, sizeof(af_mask), &af_mask);
    CPU_CLR(sysconf(_SC_NPROCESSORS_ONLN)-1, &af_mask);
    pthread_setaffinity_np(msgq->rx_thread, sizeof(af_mask), &af_mask);

    while(not_done)
    {
        // Wait for a message
        sem_wait(&msgq->msg_sem);
        sem_wait(&msgq->sync_sem);
        if(msgq->circ_buf->size() != 0)
        {
            while(msgq->circ_buf->size() != 0)
            {
                msg = msgq->circ_buf->front();
                msgq->circ_buf->pop_front();
                sem_post(&msgq->sync_sem);

                // Process message
                switch(msg.type)
                {
                case LTE_FDD_ENB_MESSAGE_TYPE_KILL:
                    not_done = false;
                    break;
                default:
                    msgq->callback(msg);
                    break;
                }

                sem_wait(&msgq->sync_sem);
            }
        }else{
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_WARNING,
                                      LTE_FDD_ENB_DEBUG_LEVEL_MSGQ,
                                      __FILE__,
                                      __LINE__,
                                      "%s circular buffer empty on receive",
                                      msgq->msgq_name.c_str());
        }
        sem_post(&msgq->sync_sem);
    }

    return(NULL);
}
