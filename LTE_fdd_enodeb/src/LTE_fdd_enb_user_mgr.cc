#line 2 "LTE_fdd_enb_user_mgr.cc" // Make __FILE__ omit the path
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

    File: LTE_fdd_enb_user_mgr.cc

    Description: Contains all the implementations for the LTE FDD eNodeB
                 user manager.

    Revision History
    ----------    -------------    --------------------------------------------
    11/10/2013    Ben Wojtowicz    Created file
    01/18/2014    Ben Wojtowicz    Added level to debug prints.
    05/04/2014    Ben Wojtowicz    Added C-RNTI timeout timers.
    06/15/2014    Ben Wojtowicz    Deleting user on C-RNTI expiration.
    08/03/2014    Ben Wojtowicz    Refactored add_user.
    11/01/2014    Ben Wojtowicz    Added M-TMSI assignment.
    11/29/2014    Ben Wojtowicz    Refactored C-RNTI assign/release, added
                                   C-RNTI transfer, added more ways to add,
                                   delete, and find users.
    12/16/2014    Ben Wojtowicz    Added delayed user delete functionality.
    12/24/2014    Ben Wojtowicz    Hack to get around a crash when releasing a
                                   C-RNTI.
    02/15/2015    Ben Wojtowicz    Fixed C-RNTI assign/release list management.
    12/06/2015    Ben Wojtowicz    Changed boost::mutex to pthread_mutex_t and
                                   sem_t and changed the user deletion
                                   procedure.
    02/13/2016    Ben Wojtowicz    Added ability to retrieve a string of all
                                   registered users, added a user inactivity
                                   timer, and properly updating the iterator
                                   after erasing a user from the user list
                                   (thanks to Damian Jarek for reporting this).
    07/03/2016    Ben Wojtowicz    Added logic to use the C-RNTI instead of
                                   IMSI/IMEI when printing a user that doesn't
                                   have IMSI/IMEI set.
    07/29/2017    Ben Wojtowicz    Using the latest tools library.

*******************************************************************************/

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_fdd_enb_user_mgr.h"
#include "LTE_fdd_enb_timer_mgr.h"
#include "liblte_mac.h"
#include "libtools_scoped_lock.h"
#include "libtools_helpers.h"

/*******************************************************************************
                              DEFINES
*******************************************************************************/


/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/


/*******************************************************************************
                              GLOBAL VARIABLES
*******************************************************************************/

LTE_fdd_enb_user_mgr*  LTE_fdd_enb_user_mgr::instance = NULL;
static pthread_mutex_t user_mgr_instance_mutex        = PTHREAD_MUTEX_INITIALIZER;

/*******************************************************************************
                              CLASS IMPLEMENTATIONS
*******************************************************************************/

/*******************/
/*    Singleton    */
/*******************/
LTE_fdd_enb_user_mgr* LTE_fdd_enb_user_mgr::get_instance(void)
{
    libtools_scoped_lock lock(user_mgr_instance_mutex);

    if(NULL == instance)
    {
        instance = new LTE_fdd_enb_user_mgr();
    }

    return(instance);
}
void LTE_fdd_enb_user_mgr::cleanup(void)
{
    libtools_scoped_lock lock(user_mgr_instance_mutex);

    if(NULL != instance)
    {
        delete instance;
        instance = NULL;
    }
}

/********************************/
/*    Constructor/Destructor    */
/********************************/
LTE_fdd_enb_user_mgr::LTE_fdd_enb_user_mgr()
{
    sem_init(&user_sem, 0, 1);
    sem_init(&c_rnti_sem, 0, 1);
    sem_init(&timer_id_sem, 0, 1);
    next_m_tmsi = 1;
    next_c_rnti = LIBLTE_MAC_C_RNTI_START;
}
LTE_fdd_enb_user_mgr::~LTE_fdd_enb_user_mgr()
{
    sem_destroy(&user_sem);
    sem_destroy(&c_rnti_sem);
    sem_destroy(&timer_id_sem);
}

/****************************/
/*    External Interface    */
/****************************/
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_user_mgr::assign_c_rnti(LTE_fdd_enb_user *user,
                                                           uint16           *c_rnti)
{
    libtools_scoped_lock                           lock(c_rnti_sem);
    LTE_fdd_enb_interface                         *interface    = LTE_fdd_enb_interface::get_instance();
    std::map<uint16, LTE_fdd_enb_user*>::iterator  iter         = c_rnti_map.find(next_c_rnti);
    LTE_FDD_ENB_ERROR_ENUM                         err          = LTE_FDD_ENB_ERROR_NONE;
    uint16                                         start_c_rnti = next_c_rnti;

    while(c_rnti_map.end() != iter)
    {
        next_c_rnti++;
        if(LIBLTE_MAC_C_RNTI_END < next_c_rnti)
        {
            next_c_rnti = LIBLTE_MAC_C_RNTI_START;
        }
        if(next_c_rnti == start_c_rnti)
        {
            err = LTE_FDD_ENB_ERROR_NO_FREE_C_RNTI;
            break;
        }

        iter = c_rnti_map.find(next_c_rnti);
    }

    if(LTE_FDD_ENB_ERROR_NONE == err)
    {
        c_rnti_map[next_c_rnti] = user;
        *c_rnti                 = next_c_rnti++;

        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                  LTE_FDD_ENB_DEBUG_LEVEL_USER,
                                  __FILE__,
                                  __LINE__,
                                  "C-RNTI=%u assigned",
                                  *c_rnti);

        if(LIBLTE_MAC_C_RNTI_END < next_c_rnti)
        {
            next_c_rnti = LIBLTE_MAC_C_RNTI_START;
        }
    }

    return(err);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_user_mgr::release_c_rnti(uint16 c_rnti)
{
    libtools_scoped_lock                           c_lock(c_rnti_sem);
    libtools_scoped_lock                           t_lock(timer_id_sem);
    LTE_fdd_enb_interface                         *interface = LTE_fdd_enb_interface::get_instance();
    LTE_fdd_enb_timer_mgr                         *timer_mgr = LTE_fdd_enb_timer_mgr::get_instance();
    std::map<uint16, LTE_fdd_enb_user*>::iterator  iter      = c_rnti_map.find(c_rnti);
    std::map<uint16, uint32>::iterator             tr_iter;
    std::map<uint32, uint16>::iterator             tf_iter;
    LTE_FDD_ENB_ERROR_ENUM                         err = LTE_FDD_ENB_ERROR_C_RNTI_NOT_FOUND;

    if(c_rnti_map.end() != iter)
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                  LTE_FDD_ENB_DEBUG_LEVEL_USER,
                                  __FILE__,
                                  __LINE__,
                                  "C-RNTI=%u released",
                                  c_rnti);

        // Initialize or delete the user
        if((*iter).second->is_id_set())
        {
            (*iter).second->init();
        }else{
            del_user((*iter).second);
        }

        // Release the C-RNTI
        c_rnti_map.erase(iter);

        // Stop the C-RNTI timer
        tr_iter = timer_id_map_reverse.find(c_rnti);
        if(timer_id_map_reverse.end() != tr_iter)
        {
            timer_mgr->stop_timer((*tr_iter).second);
            tf_iter = timer_id_map_forward.find((*tr_iter).second);
            if(timer_id_map_forward.end() != tf_iter)
            {
                timer_id_map_forward.erase(tf_iter);
            }
            timer_id_map_reverse.erase(tr_iter);
        }

        err = LTE_FDD_ENB_ERROR_NONE;
    }

    return(err);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_user_mgr::transfer_c_rnti(LTE_fdd_enb_user *old_user,
                                                             LTE_fdd_enb_user *new_user)
{
    libtools_scoped_lock                          lock(c_rnti_sem);
    std::map<uint16, LTE_fdd_enb_user*>::iterator iter = c_rnti_map.find(old_user->get_c_rnti());
    LTE_FDD_ENB_ERROR_ENUM                        err  = LTE_FDD_ENB_ERROR_C_RNTI_NOT_FOUND;
    uint16                                        c_rnti;

    if(c_rnti_map.end() != iter)
    {
        c_rnti = old_user->get_c_rnti();

        // Cleanup the old user
        if(old_user->is_id_set())
        {
            old_user->init();
        }else{
            old_user->prepare_for_deletion();
        }

        // Update the C-RNTI map
        c_rnti_map.erase(iter);
        c_rnti_map[c_rnti] = new_user;
        iter               = c_rnti_map.find(new_user->get_c_rnti());
        if(c_rnti_map.end() != iter)
        {
            c_rnti_map.erase(iter);
        }
        new_user->set_c_rnti(c_rnti);

        err = LTE_FDD_ENB_ERROR_NONE;
    }

    return(err);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_user_mgr::reset_c_rnti_timer(uint16 c_rnti)
{
    libtools_scoped_lock                lock(timer_id_sem);
    std::map<uint16, uint32>::iterator  iter      = timer_id_map_reverse.find(c_rnti);
    LTE_fdd_enb_timer_mgr              *timer_mgr = LTE_fdd_enb_timer_mgr::get_instance();
    LTE_FDD_ENB_ERROR_ENUM              err       = LTE_FDD_ENB_ERROR_C_RNTI_NOT_FOUND;

    if(timer_id_map_reverse.end() != iter)
    {
        timer_mgr->reset_timer((*iter).second);

        err = LTE_FDD_ENB_ERROR_NONE;
    }

    return(err);
}
uint32 LTE_fdd_enb_user_mgr::get_next_m_tmsi(void)
{
    return(next_m_tmsi++);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_user_mgr::add_user(LTE_fdd_enb_user **user)
{
    LTE_fdd_enb_interface  *interface = LTE_fdd_enb_interface::get_instance();
    LTE_fdd_enb_timer_mgr  *timer_mgr = LTE_fdd_enb_timer_mgr::get_instance();
    LTE_fdd_enb_user       *new_user  = NULL;
    LTE_fdd_enb_timer_cb    timer_expiry_cb(&LTE_fdd_enb_timer_cb_wrapper<LTE_fdd_enb_user_mgr, &LTE_fdd_enb_user_mgr::handle_c_rnti_timer_expiry>, this);
    LTE_FDD_ENB_ERROR_ENUM  err       = LTE_FDD_ENB_ERROR_NONE;
    uint32                  timer_id;
    uint16                  c_rnti;

    new_user = new LTE_fdd_enb_user();

    if(NULL != new_user)
    {
        // Assign C-RNTI
        if(LTE_FDD_ENB_ERROR_NONE == assign_c_rnti(new_user, &c_rnti))
        {
            // Start C-RNTI reservation timer
            timer_mgr->start_timer(5000, timer_expiry_cb, &timer_id);
            sem_wait(&timer_id_sem);
            timer_id_map_forward[timer_id] = c_rnti;
            timer_id_map_reverse[c_rnti]   = timer_id;
            sem_post(&timer_id_sem);

            // Setup user
            new_user->set_c_rnti(c_rnti);
            new_user->start_inactivity_timer(LTE_FDD_ENB_USER_INACTIVITY_TIMER_VALUE_MS);

            // Store user
            sem_wait(&user_sem);
            user_list.push_back(new_user);
            sem_post(&user_sem);

            // Return user
            *user = new_user;
        }else{
            err = LTE_FDD_ENB_ERROR_NO_FREE_C_RNTI;
            delete new_user;
        }
    }else{
        err = LTE_FDD_ENB_ERROR_BAD_ALLOC;
    }

    return(err);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_user_mgr::find_user(std::string        imsi,
                                                       LTE_fdd_enb_user **user)
{
    libtools_scoped_lock                    lock(user_sem);
    std::list<LTE_fdd_enb_user*>::iterator  iter;
    LTE_FDD_ENB_ERROR_ENUM                  err      = LTE_FDD_ENB_ERROR_USER_NOT_FOUND;
    uint64                                  imsi_num = 0;

    if(imsi.length() == 15)
    {
        to_number(imsi, 15, &imsi_num);

        for(iter=user_list.begin(); iter!=user_list.end(); iter++)
        {
            if((*iter)->is_id_set() &&
               (*iter)->get_id()->imsi == imsi_num)
            {
                *user = (*iter);
                err   = LTE_FDD_ENB_ERROR_NONE;
                break;
            }
        }
    }

    return(err);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_user_mgr::find_user(uint16             c_rnti,
                                                       LTE_fdd_enb_user **user)
{
    libtools_scoped_lock                   lock(user_sem);
    std::list<LTE_fdd_enb_user*>::iterator iter;
    LTE_FDD_ENB_ERROR_ENUM                 err = LTE_FDD_ENB_ERROR_USER_NOT_FOUND;

    for(iter=user_list.begin(); iter!=user_list.end(); iter++)
    {
        if((*iter)->is_c_rnti_set() &&
           (*iter)->get_c_rnti() == c_rnti)
        {
            *user = (*iter);
            err   = LTE_FDD_ENB_ERROR_NONE;
            break;
        }
    }

    return(err);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_user_mgr::find_user(LIBLTE_MME_EPS_MOBILE_ID_GUTI_STRUCT  *guti,
                                                       LTE_fdd_enb_user                     **user)
{
    libtools_scoped_lock                   lock(user_sem);
    std::list<LTE_fdd_enb_user*>::iterator iter;
    LTE_FDD_ENB_ERROR_ENUM                 err = LTE_FDD_ENB_ERROR_USER_NOT_FOUND;

    for(iter=user_list.begin(); iter!=user_list.end(); iter++)
    {
        if((*iter)->is_guti_set()                                  &&
           (*iter)->get_guti()->m_tmsi       == guti->m_tmsi       &&
           (*iter)->get_guti()->mcc          == guti->mcc          &&
           (*iter)->get_guti()->mnc          == guti->mnc          &&
           (*iter)->get_guti()->mme_group_id == guti->mme_group_id &&
           (*iter)->get_guti()->mme_code     == guti->mme_code)
        {
            *user = (*iter);
            err   = LTE_FDD_ENB_ERROR_NONE;
            break;
        }
    }

    return(err);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_user_mgr::find_user(LIBLTE_RRC_S_TMSI_STRUCT  *s_tmsi,
                                                       LTE_fdd_enb_user         **user)
{
    libtools_scoped_lock                   lock(user_sem);
    std::list<LTE_fdd_enb_user*>::iterator iter;
    LTE_FDD_ENB_ERROR_ENUM                 err = LTE_FDD_ENB_ERROR_USER_NOT_FOUND;

    for(iter=user_list.begin(); iter!=user_list.end(); iter++)
    {
        if((*iter)->is_guti_set()                          &&
           (*iter)->get_guti()->m_tmsi   == s_tmsi->m_tmsi &&
           (*iter)->get_guti()->mme_code == s_tmsi->mmec)
        {
            *user = (*iter);
            err   = LTE_FDD_ENB_ERROR_NONE;
            break;
        }
    }

    return(err);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_user_mgr::find_user(uint32             ip_addr,
                                                       LTE_fdd_enb_user **user)
{
    libtools_scoped_lock                   lock(user_sem);
    std::list<LTE_fdd_enb_user*>::iterator iter;
    LTE_FDD_ENB_ERROR_ENUM                 err = LTE_FDD_ENB_ERROR_USER_NOT_FOUND;

    for(iter=user_list.begin(); iter!=user_list.end(); iter++)
    {
        if((*iter)->is_ip_addr_set() &&
           (*iter)->get_ip_addr() == ip_addr)
        {
            *user = (*iter);
            err   = LTE_FDD_ENB_ERROR_NONE;
            break;
        }
    }

    return(err);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_user_mgr::del_user(LTE_fdd_enb_user *user)
{
    libtools_scoped_lock                    lock(user_sem);
    std::list<LTE_fdd_enb_user*>::iterator  iter;
    LTE_fdd_enb_user                       *tmp_user = NULL;
    LTE_FDD_ENB_ERROR_ENUM                  err      = LTE_FDD_ENB_ERROR_USER_NOT_FOUND;

    if(user->is_id_set())
    {
        for(iter=user_list.begin(); iter!=user_list.end(); iter++)
        {
            if(user->get_id()->imsi == (*iter)->get_id()->imsi &&
               user->get_id()->imei == (*iter)->get_id()->imei)
            {
                tmp_user = (*iter);
                iter     = user_list.erase(iter);
                delete tmp_user;
                err = LTE_FDD_ENB_ERROR_NONE;
                break;
            }
        }
    }else if(user->is_guti_set()){
        for(iter=user_list.begin(); iter!=user_list.end(); iter++)
        {
            if(user->get_guti()->m_tmsi       == (*iter)->get_guti()->m_tmsi       &&
               user->get_guti()->mcc          == (*iter)->get_guti()->mcc          &&
               user->get_guti()->mnc          == (*iter)->get_guti()->mnc          &&
               user->get_guti()->mme_group_id == (*iter)->get_guti()->mme_group_id &&
               user->get_guti()->mme_code     == (*iter)->get_guti()->mme_code)
            {
                tmp_user = (*iter);
                iter     = user_list.erase(iter);
                delete tmp_user;
                err = LTE_FDD_ENB_ERROR_NONE;
                break;
            }
        }
    }else if(user->is_c_rnti_set()){
        for(iter=user_list.begin(); iter!=user_list.end(); iter++)
        {
            if(user->get_c_rnti() == (*iter)->get_c_rnti())
            {
                tmp_user = (*iter);
                iter     = user_list.erase(iter);
                delete tmp_user;
                err = LTE_FDD_ENB_ERROR_NONE;
                break;
            }
        }
    }

    return(err);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_user_mgr::del_user(std::string imsi)
{
    libtools_scoped_lock                    lock(user_sem);
    std::list<LTE_fdd_enb_user*>::iterator  iter;
    LTE_fdd_enb_user                       *tmp_user = NULL;
    LTE_FDD_ENB_ERROR_ENUM                  err      = LTE_FDD_ENB_ERROR_USER_NOT_FOUND;
    uint64                                  imsi_num = 0;

    if(imsi.length() == 15)
    {
        to_number(imsi, 15, &imsi_num);

        for(iter=user_list.begin(); iter!=user_list.end(); iter++)
        {
            if((*iter)->is_id_set() &&
               (*iter)->get_id()->imsi == imsi_num)
            {
                tmp_user = (*iter);
                iter     = user_list.erase(iter);
                delete tmp_user;
                err = LTE_FDD_ENB_ERROR_NONE;
                break;
            }
        }
    }

    return(err);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_user_mgr::del_user(uint16 c_rnti)
{
    libtools_scoped_lock                    lock(user_sem);
    std::list<LTE_fdd_enb_user*>::iterator  iter;
    LTE_fdd_enb_user                       *tmp_user = NULL;
    LTE_FDD_ENB_ERROR_ENUM                  err      = LTE_FDD_ENB_ERROR_USER_NOT_FOUND;

    for(iter=user_list.begin(); iter!=user_list.end(); iter++)
    {
        if((*iter)->is_c_rnti_set() &&
           (*iter)->get_c_rnti() == c_rnti)
        {
            tmp_user = (*iter);
            iter     = user_list.erase(iter);
            delete tmp_user;
            err = LTE_FDD_ENB_ERROR_NONE;
            break;
        }
    }

    return(err);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_user_mgr::del_user(LIBLTE_MME_EPS_MOBILE_ID_GUTI_STRUCT *guti)
{
    libtools_scoped_lock                    lock(user_sem);
    std::list<LTE_fdd_enb_user*>::iterator  iter;
    LTE_fdd_enb_user                       *tmp_user = NULL;
    LTE_FDD_ENB_ERROR_ENUM                  err      = LTE_FDD_ENB_ERROR_USER_NOT_FOUND;

    for(iter=user_list.begin(); iter!=user_list.end(); iter++)
    {
        if((*iter)->is_guti_set()                                  &&
           (*iter)->get_guti()->m_tmsi       == guti->m_tmsi       &&
           (*iter)->get_guti()->mcc          == guti->mcc          &&
           (*iter)->get_guti()->mnc          == guti->mnc          &&
           (*iter)->get_guti()->mme_group_id == guti->mme_group_id &&
           (*iter)->get_guti()->mme_code     == guti->mme_code)
        {
            tmp_user = (*iter);
            iter     = user_list.erase(iter);
            delete tmp_user;
            err = LTE_FDD_ENB_ERROR_NONE;
            break;
        }
    }

    return(err);
}
std::string LTE_fdd_enb_user_mgr::print_all_users(void)
{
    libtools_scoped_lock                    lock(user_sem);
    std::list<LTE_fdd_enb_user*>::iterator  iter;
    std::string                             output;
    LIBLTE_MME_EPS_MOBILE_ID_GUTI_STRUCT   *guti;
    uint32                                  i;
    uint32                                  hex_val;

    output = to_string((uint32)user_list.size());
    for(iter=user_list.begin(); iter!=user_list.end(); iter++)
    {
        output += "\n";
        if((*iter)->is_id_set())
        {
            output += "imsi=" + to_string((*iter)->get_imsi_num(), 15) + " ";
            output += "imei=" + to_string((*iter)->get_imei_num(), 15) + " ";
        }else{
            output += "c-rnti=" + to_string((*iter)->get_c_rnti()) + " ";
        }
        if((*iter)->is_guti_set())
        {
            guti    = (*iter)->get_guti();
            output += " m-tmsi=";
            for(i=0; i<8; i++)
            {
                hex_val = (guti->m_tmsi >> (28 - (i*4))) & 0xF;
                if(hex_val < 0xA)
                {
                    output += (char)(hex_val + '0');
                }else{
                    output += (char)((hex_val-0xA) + 'A');
                }
            }
        }
    }

    return(output);
}

/**********************/
/*    C-RNTI Timer    */
/**********************/
void LTE_fdd_enb_user_mgr::handle_c_rnti_timer_expiry(uint32 timer_id)
{
    LTE_fdd_enb_interface              *interface = LTE_fdd_enb_interface::get_instance();
    std::map<uint32, uint16>::iterator  forward_iter = timer_id_map_forward.find(timer_id);
    std::map<uint16, uint32>::iterator  reverse_iter;
    uint16                              c_rnti;

    sem_wait(&timer_id_sem);
    if(timer_id_map_forward.end() != forward_iter)
    {
        interface->send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_INFO,
                                  LTE_FDD_ENB_DEBUG_LEVEL_USER,
                                  __FILE__,
                                  __LINE__,
                                  "C-RNTI allocation timer expiry C-RNTI=%u",
                                  (*forward_iter).second);
        c_rnti = (*forward_iter).second;
        reverse_iter = timer_id_map_reverse.find((*forward_iter).second);
        if(timer_id_map_reverse.end() != reverse_iter)
        {
            timer_id_map_reverse.erase(reverse_iter);
        }
        timer_id_map_forward.erase(forward_iter);

        sem_post(&timer_id_sem);
        release_c_rnti(c_rnti);
    }else{
        sem_post(&timer_id_sem);
    }
}
