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

    File: LTE_fdd_enb_timer_mgr.h

    Description: Contains all the definitions for the LTE FDD eNodeB
                 timer manager.

    Revision History
    ----------    -------------    --------------------------------------------
    05/04/2014    Ben Wojtowicz    Created file
    06/15/2014    Ben Wojtowicz    Added millisecond resolution.
    08/03/2014    Ben Wojtowicz    Added an invalid timer id.
    11/29/2014    Ben Wojtowicz    Added timer reset support.
    02/15/2015    Ben Wojtowicz    Moved to new message queue for timer ticks.
    12/06/2015    Ben Wojtowicz    Changed boost::mutex to sem_t.

*******************************************************************************/

#ifndef __LTE_FDD_ENB_TIMER_MGR_H__
#define __LTE_FDD_ENB_TIMER_MGR_H__

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_fdd_enb_interface.h"
#include "LTE_fdd_enb_timer.h"
#include "LTE_fdd_enb_msgq.h"

/*******************************************************************************
                              DEFINES
*******************************************************************************/

#define LTE_FDD_ENB_INVALID_TIMER_ID 0xFFFFFFFF

/*******************************************************************************
                              FORWARD DECLARATIONS
*******************************************************************************/


/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/


/*******************************************************************************
                              CLASS DECLARATIONS
*******************************************************************************/

class LTE_fdd_enb_timer_mgr
{
public:
    // Singleton
    static LTE_fdd_enb_timer_mgr* get_instance(void);
    static void cleanup(void);

    // Start/Stop
    void start(LTE_fdd_enb_msgq *from_mac, LTE_fdd_enb_interface *iface);
    void stop(void);

    // External Interface
    LTE_FDD_ENB_ERROR_ENUM start_timer(uint32 m_seconds, LTE_fdd_enb_timer_cb cb, uint32 *timer_id);
    LTE_FDD_ENB_ERROR_ENUM stop_timer(uint32 timer_id);
    LTE_FDD_ENB_ERROR_ENUM reset_timer(uint32 timer_id);

private:
    // Singleton
    static LTE_fdd_enb_timer_mgr *instance;
    LTE_fdd_enb_timer_mgr();
    ~LTE_fdd_enb_timer_mgr();

    // Start/Stop
    LTE_fdd_enb_interface *interface;
    sem_t                  start_sem;
    bool                   started;

    // Communication
    void handle_msg(LTE_FDD_ENB_MESSAGE_STRUCT &msg);
    void handle_tick(void);
    LTE_fdd_enb_msgq *msgq_from_mac;

    // Timer Storage
    sem_t                                timer_sem;
    std::map<uint32, LTE_fdd_enb_timer*> timer_map;
    uint32                               next_timer_id;
};

#endif /* __LTE_FDD_ENB_TIMER_MGR_H__ */
