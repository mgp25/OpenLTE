#line 2 "LTE_fdd_enb_timer_mgr.cc" // Make __FILE__ omit the path
/*******************************************************************************

    Copyright 2014-2015 Ben Wojtowicz

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

    File: LTE_fdd_enb_timer_mgr.cc

    Description: Contains all the implementations for the LTE FDD eNodeB
                 timer manager.

    Revision History
    ----------    -------------    --------------------------------------------
    05/04/2014    Ben Wojtowicz    Created file
    06/15/2014    Ben Wojtowicz    Added millisecond resolution.
    08/03/2014    Ben Wojtowicz    Added an invalid timer id.
    11/29/2014    Ben Wojtowicz    Added timer reset support.
    12/16/2014    Ben Wojtowicz    Passing timer tick to user_mgr.
    02/15/2015    Ben Wojtowicz    Moved to new message queue for timer ticks.
    12/06/2015    Ben Wojtowicz    Changed boost::mutex to pthread_mutex_t and
                                   sem_t.

*******************************************************************************/

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_fdd_enb_timer_mgr.h"
#include "LTE_fdd_enb_user_mgr.h"
#include "libtools_scoped_lock.h"
#include <list>

/*******************************************************************************
                              DEFINES
*******************************************************************************/


/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/


/*******************************************************************************
                              GLOBAL VARIABLES
*******************************************************************************/

LTE_fdd_enb_timer_mgr* LTE_fdd_enb_timer_mgr::instance = NULL;
static pthread_mutex_t timer_mgr_instance_mutex        = PTHREAD_MUTEX_INITIALIZER;

/*******************************************************************************
                              CLASS IMPLEMENTATIONS
*******************************************************************************/

/*******************/
/*    Singleton    */
/*******************/
LTE_fdd_enb_timer_mgr* LTE_fdd_enb_timer_mgr::get_instance(void)
{
    libtools_scoped_lock lock(timer_mgr_instance_mutex);

    if(NULL == instance)
    {
        instance = new LTE_fdd_enb_timer_mgr();
    }

    return(instance);
}
void LTE_fdd_enb_timer_mgr::cleanup(void)
{
    libtools_scoped_lock lock(timer_mgr_instance_mutex);

    if(NULL != instance)
    {
        delete instance;
        instance = NULL;
    }
}

/********************************/
/*    Constructor/Destructor    */
/********************************/
LTE_fdd_enb_timer_mgr::LTE_fdd_enb_timer_mgr()
{
    sem_init(&start_sem, 0, 1);
    sem_init(&timer_sem, 0, 1);
    interface = NULL;
    started   = false;
}
LTE_fdd_enb_timer_mgr::~LTE_fdd_enb_timer_mgr()
{
    std::map<uint32, LTE_fdd_enb_timer *>::iterator iter;

    sem_wait(&timer_sem);
    for(iter=timer_map.begin(); iter!=timer_map.end(); iter++)
    {
        delete (*iter).second;
    }
    sem_post(&timer_sem);
    sem_destroy(&timer_sem);
    sem_destroy(&start_sem);
}

/********************/
/*    Start/Stop    */
/********************/
void LTE_fdd_enb_timer_mgr::start(LTE_fdd_enb_msgq      *from_mac,
                                  LTE_fdd_enb_interface *iface)
{
    libtools_scoped_lock lock(start_sem);
    LTE_fdd_enb_msgq_cb  timer_cb(&LTE_fdd_enb_msgq_cb_wrapper<LTE_fdd_enb_timer_mgr, &LTE_fdd_enb_timer_mgr::handle_msg>, this);

    if(!started)
    {
        interface     = iface;
        started       = true;
        next_timer_id = 0;
        msgq_from_mac = from_mac;
        msgq_from_mac->attach_rx(timer_cb);
    }
}
void LTE_fdd_enb_timer_mgr::stop(void)
{
    libtools_scoped_lock lock(start_sem);

    if(started)
    {
        started = false;
    }
}

/****************************/
/*    External Interface    */
/****************************/
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_timer_mgr::start_timer(uint32                m_seconds,
                                                          LTE_fdd_enb_timer_cb  cb,
                                                          uint32               *timer_id)
{
    libtools_scoped_lock                             lock(timer_sem);
    std::map<uint32, LTE_fdd_enb_timer *>::iterator  iter      = timer_map.find(next_timer_id);
    LTE_fdd_enb_timer                               *new_timer = NULL;
    LTE_FDD_ENB_ERROR_ENUM                           err       = LTE_FDD_ENB_ERROR_BAD_ALLOC;

    while(timer_map.end()              != iter &&
          LTE_FDD_ENB_INVALID_TIMER_ID != next_timer_id)
    {
        next_timer_id++;
        iter = timer_map.find(next_timer_id);
    }
    new_timer = new LTE_fdd_enb_timer(m_seconds, next_timer_id, cb);

    if(NULL != new_timer)
    {
        *timer_id                  = next_timer_id;
        timer_map[next_timer_id++] = new_timer;
        err                        = LTE_FDD_ENB_ERROR_NONE;
    }

    return(err);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_timer_mgr::stop_timer(uint32 timer_id)
{
    libtools_scoped_lock                            lock(timer_sem);
    std::map<uint32, LTE_fdd_enb_timer *>::iterator iter = timer_map.find(timer_id);
    LTE_FDD_ENB_ERROR_ENUM                          err  = LTE_FDD_ENB_ERROR_TIMER_NOT_FOUND;

    if(timer_map.end() != iter)
    {
        delete (*iter).second;
        timer_map.erase(iter);
        err = LTE_FDD_ENB_ERROR_NONE;
    }

    return(err);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_timer_mgr::reset_timer(uint32 timer_id)
{
    libtools_scoped_lock                            lock(timer_sem);
    std::map<uint32, LTE_fdd_enb_timer *>::iterator iter = timer_map.find(timer_id);
    LTE_FDD_ENB_ERROR_ENUM                          err  = LTE_FDD_ENB_ERROR_TIMER_NOT_FOUND;

    if(timer_map.end() != iter)
    {
        (*iter).second->reset();
        err = LTE_FDD_ENB_ERROR_NONE;
    }

    return(err);
}

/***********************/
/*    Communication    */
/***********************/
void LTE_fdd_enb_timer_mgr::handle_msg(LTE_FDD_ENB_MESSAGE_STRUCT &msg)
{
    if(LTE_FDD_ENB_DEST_LAYER_TIMER_MGR == msg.dest_layer ||
       LTE_FDD_ENB_DEST_LAYER_ANY       == msg.dest_layer)
    {
        switch(msg.type)
        {
        case LTE_FDD_ENB_MESSAGE_TYPE_TIMER_TICK:
            handle_tick();
            break;
        default:
            interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_WARNING,
                                      LTE_FDD_ENB_DEBUG_LEVEL_TIMER,
                                      __FILE__,
                                      __LINE__,
                                      "Received invalid TIMER message %s",
                                      LTE_fdd_enb_message_type_text[msg.type]);
            break;
        }
    }else{
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_WARNING,
                                  LTE_FDD_ENB_DEBUG_LEVEL_TIMER,
                                  __FILE__,
                                  __LINE__,
                                  "Received message for invalid layer (%s)",
                                  LTE_fdd_enb_dest_layer_text[msg.dest_layer]);
    }
}
void LTE_fdd_enb_timer_mgr::handle_tick(void)
{
    LTE_fdd_enb_user_mgr                            *user_mgr = LTE_fdd_enb_user_mgr::get_instance();
    std::map<uint32, LTE_fdd_enb_timer *>::iterator  iter;
    std::list<uint32>                                expired_list;

    sem_wait(&timer_sem);
    for(iter=timer_map.begin(); iter!=timer_map.end(); iter++)
    {
        (*iter).second->increment();

        if((*iter).second->expired())
        {
            expired_list.push_back((*iter).first);
        }
    }
    sem_post(&timer_sem);

    // Delete expired timers
    while(0 != expired_list.size())
    {
        iter = timer_map.find(expired_list.front());
        if(timer_map.end() != iter)
        {
            (*iter).second->call_callback();
            delete (*iter).second;
            timer_map.erase(iter);
        }
        expired_list.pop_front();
    }
}
