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

    File: LTE_fdd_enb_user_mgr.h

    Description: Contains all the definitions for the LTE FDD eNodeB
                 user manager.

    Revision History
    ----------    -------------    --------------------------------------------
    11/09/2013    Ben Wojtowicz    Created file
    05/04/2014    Ben Wojtowicz    Added C-RNTI timeout timers.
    08/03/2014    Ben Wojtowicz    Refactored add_user.
    11/01/2014    Ben Wojtowicz    Added M-TMSI assignment.
    11/29/2014    Ben Wojtowicz    Refactored C-RNTI assign/release, added
                                   C-RNTI transfer, added more ways to add,
                                   delete, and find users.
    12/16/2014    Ben Wojtowicz    Added delayed user delete functionality.
    12/06/2015    Ben Wojtowicz    Changed boost::mutex to sem_t and changed the
                                   user deletion procedure.
    02/13/2016    Ben Wojtowicz    Added ability to retrieve a string of all
                                   registered users.

*******************************************************************************/

#ifndef __LTE_FDD_ENB_USER_MGR_H__
#define __LTE_FDD_ENB_USER_MGR_H__

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_fdd_enb_interface.h"
#include "LTE_fdd_enb_user.h"
#include <string>

/*******************************************************************************
                              DEFINES
*******************************************************************************/


/*******************************************************************************
                              FORWARD DECLARATIONS
*******************************************************************************/


/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/


/*******************************************************************************
                              CLASS DECLARATIONS
*******************************************************************************/

class LTE_fdd_enb_user_mgr
{
public:
    // Singleton
    static LTE_fdd_enb_user_mgr* get_instance(void);
    static void cleanup(void);

    // External interface
    LTE_FDD_ENB_ERROR_ENUM assign_c_rnti(LTE_fdd_enb_user *user, uint16 *c_rnti);
    LTE_FDD_ENB_ERROR_ENUM release_c_rnti(uint16 c_rnti);
    LTE_FDD_ENB_ERROR_ENUM transfer_c_rnti(LTE_fdd_enb_user *old_user, LTE_fdd_enb_user *new_user);
    LTE_FDD_ENB_ERROR_ENUM reset_c_rnti_timer(uint16 c_rnti);
    uint32 get_next_m_tmsi(void);
    LTE_FDD_ENB_ERROR_ENUM add_user(LTE_fdd_enb_user **user);
    LTE_FDD_ENB_ERROR_ENUM find_user(std::string imsi, LTE_fdd_enb_user **user);
    LTE_FDD_ENB_ERROR_ENUM find_user(uint16 c_rnti, LTE_fdd_enb_user **user);
    LTE_FDD_ENB_ERROR_ENUM find_user(LIBLTE_MME_EPS_MOBILE_ID_GUTI_STRUCT *guti, LTE_fdd_enb_user **user);
    LTE_FDD_ENB_ERROR_ENUM find_user(LIBLTE_RRC_S_TMSI_STRUCT *s_tmsi, LTE_fdd_enb_user **user);
    LTE_FDD_ENB_ERROR_ENUM find_user(uint32 ip_addr, LTE_fdd_enb_user **user);
    LTE_FDD_ENB_ERROR_ENUM del_user(LTE_fdd_enb_user *user);
    LTE_FDD_ENB_ERROR_ENUM del_user(std::string imsi);
    LTE_FDD_ENB_ERROR_ENUM del_user(uint16 c_rnti);
    LTE_FDD_ENB_ERROR_ENUM del_user(LIBLTE_MME_EPS_MOBILE_ID_GUTI_STRUCT *guti);
    std::string print_all_users(void);

private:
    // Singleton
    static LTE_fdd_enb_user_mgr *instance;
    LTE_fdd_enb_user_mgr();
    ~LTE_fdd_enb_user_mgr();

    // C-RNTI Timer
    void handle_c_rnti_timer_expiry(uint32 timer_id);

    // User storage
    std::list<LTE_fdd_enb_user*>        user_list;
    std::list<LTE_fdd_enb_user*>        delayed_del_user_list;
    std::map<uint16, LTE_fdd_enb_user*> c_rnti_map;
    std::map<uint32, uint16>            timer_id_map_forward;
    std::map<uint16, uint32>            timer_id_map_reverse;
    sem_t                               user_sem;
    sem_t                               c_rnti_sem;
    sem_t                               timer_id_sem;
    uint32                              next_m_tmsi;
    uint16                              next_c_rnti;
};

#endif /* __LTE_FDD_ENB_USER_MGR_H__ */
