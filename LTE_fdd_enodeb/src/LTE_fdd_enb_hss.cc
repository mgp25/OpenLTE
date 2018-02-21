#line 2 "LTE_fdd_enb_hss.cc" // Make __FILE__ omit the path
/*******************************************************************************

    Copyright 2014-2017 Ben Wojtowicz

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

    File: LTE_fdd_enb_hss.cc

    Description: Contains all the implementations for the LTE FDD eNodeB
                 home subscriber server.

    Revision History
    ----------    -------------    --------------------------------------------
    06/15/2014    Ben Wojtowicz    Created file
    08/03/2014    Ben Wojtowicz    Added authentication vector support.
    09/03/2014    Ben Wojtowicz    Added sequence number resynch and key
                                   generation.
    11/01/2014    Ben Wojtowicz    Added user file support.
    11/29/2014    Ben Wojtowicz    Added support for regenerating eNodeB
                                   security data.
    07/25/2015    Ben Wojtowicz    Moved away from using boost::lexical_cast
                                   in del_user.
    12/06/2015    Ben Wojtowicz    Changed boost::mutex to pthread_mutex_t and
                                   sem_t.
    07/03/2016    Ben Wojtowicz    Fixed a bug in print_all_users.  Thanks to
                                   Sultan Qasim Khan for finding this.
    07/29/2017    Ben Wojtowicz    Using the latest tools library.

*******************************************************************************/

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_fdd_enb_hss.h"
#include "LTE_fdd_enb_cnfg_db.h"
#include "liblte_security.h"
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

LTE_fdd_enb_hss*       LTE_fdd_enb_hss::instance = NULL;
static pthread_mutex_t hss_instance_mutex        = PTHREAD_MUTEX_INITIALIZER;

/*******************************************************************************
                              CLASS IMPLEMENTATIONS
*******************************************************************************/

/*******************/
/*    Singleton    */
/*******************/
LTE_fdd_enb_hss* LTE_fdd_enb_hss::get_instance(void)
{
    libtools_scoped_lock lock(hss_instance_mutex);

    if(NULL == instance)
    {
        instance = new LTE_fdd_enb_hss();
    }

    return(instance);
}
void LTE_fdd_enb_hss::cleanup(void)
{
    libtools_scoped_lock lock(hss_instance_mutex);

    if(NULL != instance)
    {
        delete instance;
        instance = NULL;
    }
}

/********************************/
/*    Constructor/Destructor    */
/********************************/
LTE_fdd_enb_hss::LTE_fdd_enb_hss()
{
    sem_init(&user_sem, 0, 1);
    user_list.clear();
    use_user_file = false;
}
LTE_fdd_enb_hss::~LTE_fdd_enb_hss()
{
    std::list<LTE_FDD_ENB_HSS_USER_STRUCT *>::iterator iter;

    sem_wait(&user_sem);
    for(iter=user_list.begin(); iter!=user_list.end(); iter++)
    {
        delete (*iter);
    }
    sem_post(&user_sem);
    sem_destroy(&user_sem);
}

/****************************/
/*    External Interface    */
/****************************/
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_hss::add_user(std::string imsi,
                                                 std::string imei,
                                                 std::string k)
{
    std::list<LTE_FDD_ENB_HSS_USER_STRUCT *>::iterator  iter;
    LTE_FDD_ENB_HSS_USER_STRUCT                        *new_user = new LTE_FDD_ENB_HSS_USER_STRUCT;
    LTE_FDD_ENB_ERROR_ENUM                              err      = LTE_FDD_ENB_ERROR_BAD_ALLOC;

    if(NULL != new_user      &&
       15   == imsi.length() &&
       15   == imei.length() &&
       32   == k.length())
    {
        to_number(imsi, 15, &new_user->id.imsi);
        to_number(imei, 15, &new_user->id.imei);
        to_number(k, 16, new_user->stored_data.k);

        new_user->generated_data.sqn_he = 0;
        new_user->generated_data.seq_he = 0;
        new_user->generated_data.ind_he = 0;

        sem_wait(&user_sem);
        err = LTE_FDD_ENB_ERROR_NONE;
        for(iter=user_list.begin(); iter!=user_list.end(); iter++)
        {
            if((*iter)->id.imsi == new_user->id.imsi &&
               (*iter)->id.imei == new_user->id.imei)
            {
                err = LTE_FDD_ENB_ERROR_DUPLICATE_ENTRY;
            }
        }
        if(LTE_FDD_ENB_ERROR_NONE == err)
        {
            user_list.push_back(new_user);
        }else{
            delete new_user;
        }
        sem_post(&user_sem);

        if(use_user_file)
        {
            write_user_file();
        }
    }

    return(err);
}
LTE_FDD_ENB_ERROR_ENUM LTE_fdd_enb_hss::del_user(std::string imsi)
{
    std::list<LTE_FDD_ENB_HSS_USER_STRUCT *>::iterator  iter;
    LTE_FDD_ENB_HSS_USER_STRUCT                        *user = NULL;
    LTE_FDD_ENB_ERROR_ENUM                              err  = LTE_FDD_ENB_ERROR_USER_NOT_FOUND;
    uint64                                              imsi_num;
    bool                                                update_user_file = false;

    to_number(imsi, 15, &imsi_num);

    sem_wait(&user_sem);
    for(iter=user_list.begin(); iter!=user_list.end(); iter++)
    {
        if(imsi_num == (*iter)->id.imsi)
        {
            user = (*iter);
            user_list.erase(iter);
            delete user;
            err = LTE_FDD_ENB_ERROR_NONE;
            break;
        }

        update_user_file = true;
    }
    sem_post(&user_sem);

    if(update_user_file &&
       use_user_file)
    {
        write_user_file();
    }

    return(err);
}
std::string LTE_fdd_enb_hss::print_all_users(void)
{
    libtools_scoped_lock                               lock(user_sem);
    std::list<LTE_FDD_ENB_HSS_USER_STRUCT *>::iterator iter;
    std::string                                        output;

    output = to_string((uint32)user_list.size());
    for(iter=user_list.begin(); iter!=user_list.end(); iter++)
    {
        output += "\n";
        output += "imsi=" + to_string((*iter)->id.imsi, 15) + " ";
        output += "imei=" + to_string((*iter)->id.imei, 15) + " ";
        output += "k=" + to_string((*iter)->stored_data.k, 16);
    }

    return(output);
}
bool LTE_fdd_enb_hss::is_imsi_allowed(uint64 imsi)
{
    libtools_scoped_lock                               lock(user_sem);
    std::list<LTE_FDD_ENB_HSS_USER_STRUCT *>::iterator iter;
    bool                                               ret = false;

    for(iter=user_list.begin(); iter!=user_list.end(); iter++)
    {
        if(imsi == (*iter)->id.imsi)
        {
            ret = true;
            break;
        }
    }

    return(ret);
}
bool LTE_fdd_enb_hss::is_imei_allowed(uint64 imei)
{
    libtools_scoped_lock                               lock(user_sem);
    std::list<LTE_FDD_ENB_HSS_USER_STRUCT *>::iterator iter;
    bool                                               ret = false;

    for(iter=user_list.begin(); iter!=user_list.end(); iter++)
    {
        if(imei == (*iter)->id.imei)
        {
            ret = true;
            break;
        }
    }

    return(ret);
}
LTE_FDD_ENB_USER_ID_STRUCT* LTE_fdd_enb_hss::get_user_id_from_imsi(uint64 imsi)
{
    libtools_scoped_lock                                lock(user_sem);
    std::list<LTE_FDD_ENB_HSS_USER_STRUCT *>::iterator  iter;
    LTE_FDD_ENB_USER_ID_STRUCT                         *id = NULL;

    for(iter=user_list.begin(); iter!=user_list.end(); iter++)
    {
        if(imsi == (*iter)->id.imsi)
        {
            id = &(*iter)->id;
        }
    }

    return(id);
}
LTE_FDD_ENB_USER_ID_STRUCT* LTE_fdd_enb_hss::get_user_id_from_imei(uint64 imei)
{
    libtools_scoped_lock                                lock(user_sem);
    std::list<LTE_FDD_ENB_HSS_USER_STRUCT *>::iterator  iter;
    LTE_FDD_ENB_USER_ID_STRUCT                         *id = NULL;

    for(iter=user_list.begin(); iter!=user_list.end(); iter++)
    {
        if(imei == (*iter)->id.imei)
        {
            id = &(*iter)->id;
        }
    }

    return(id);
}
void LTE_fdd_enb_hss::generate_security_data(LTE_FDD_ENB_USER_ID_STRUCT *id,
                                             uint16                      mcc,
                                             uint16                      mnc)
{
    libtools_scoped_lock                               lock(user_sem);
    std::list<LTE_FDD_ENB_HSS_USER_STRUCT *>::iterator iter;
    uint32                                             i;
    uint32                                             rand_val;
    uint8                                              sqn[6];
    uint8                                              amf[2] = {0x80, 0x00}; // 3GPP 33.102 v10.0.0 Annex H

    for(iter=user_list.begin(); iter!=user_list.end(); iter++)
    {
        if(id->imei == (*iter)->id.imei &&
           id->imsi == (*iter)->id.imsi)
        {
            // Generate sqn
            // From 33.102 v10.0.0 section C.3.2
            (*iter)->generated_data.seq_he = ((*iter)->generated_data.seq_he + 1) % LTE_FDD_ENB_SEQ_HE_MAX_VALUE;
            (*iter)->generated_data.ind_he = ((*iter)->generated_data.ind_he + 1) % LTE_FDD_ENB_IND_HE_MAX_VALUE;
            (*iter)->generated_data.sqn_he = ((*iter)->generated_data.seq_he << LTE_FDD_ENB_IND_HE_N_BITS) | (*iter)->generated_data.ind_he;
            for(i=0; i<6; i++)
            {
                sqn[i] = ((*iter)->generated_data.sqn_he >> (5-i)*8) & 0xFF;
            }

            // Generate RAND
            for(i=0; i<4; i++)
            {
                rand_val                                     = rand();
                (*iter)->generated_data.auth_vec.rand[i*4+0] = rand_val & 0xFF;
                (*iter)->generated_data.auth_vec.rand[i*4+1] = (rand_val >> 8) & 0xFF;
                (*iter)->generated_data.auth_vec.rand[i*4+2] = (rand_val >> 16) & 0xFF;
                (*iter)->generated_data.auth_vec.rand[i*4+3] = (rand_val >> 24) & 0xFF;
            }

            // Generate MAC, RES, CK, IK, and AK
            liblte_security_milenage_f1((*iter)->stored_data.k,
                                        (*iter)->generated_data.auth_vec.rand,
                                        sqn,
                                        amf,
                                        (*iter)->generated_data.mac);
            liblte_security_milenage_f2345((*iter)->stored_data.k,
                                           (*iter)->generated_data.auth_vec.rand,
                                           (*iter)->generated_data.auth_vec.res,
                                           (*iter)->generated_data.auth_vec.ck,
                                           (*iter)->generated_data.auth_vec.ik,
                                           (*iter)->generated_data.ak);

            // Construct AUTN
            for(i=0; i<6; i++)
            {
                (*iter)->generated_data.auth_vec.autn[i] = sqn[i] ^ (*iter)->generated_data.ak[i];
            }
            for(i=0; i<2; i++)
            {
                (*iter)->generated_data.auth_vec.autn[6+i] = amf[i];
            }
            for(i=0; i<8; i++)
            {
                (*iter)->generated_data.auth_vec.autn[8+i] = (*iter)->generated_data.mac[i];
            }

            // Reset NAS counts
            // 3GPP 33.401 v10.0.0 section 6.5
            (*iter)->generated_data.auth_vec.nas_count_ul = 0;
            (*iter)->generated_data.auth_vec.nas_count_dl = 0;

            // Generate Kasme
            liblte_security_generate_k_asme((*iter)->generated_data.auth_vec.ck,
                                            (*iter)->generated_data.auth_vec.ik,
                                            (*iter)->generated_data.ak,
                                            sqn,
                                            mcc,
                                            mnc,
                                            (*iter)->generated_data.k_asme);

            // Generate K_nas_enc and K_nas_int
            liblte_security_generate_k_nas((*iter)->generated_data.k_asme,
                                           LIBLTE_SECURITY_CIPHERING_ALGORITHM_ID_EEA0,
                                           LIBLTE_SECURITY_INTEGRITY_ALGORITHM_ID_128_EIA2,
                                           (*iter)->generated_data.auth_vec.k_nas_enc,
                                           (*iter)->generated_data.auth_vec.k_nas_int);

            // Generate K_enb
            liblte_security_generate_k_enb((*iter)->generated_data.k_asme,
                                           (*iter)->generated_data.auth_vec.nas_count_ul,
                                           (*iter)->generated_data.k_enb);

            // Generate K_rrc_enc and K_rrc_int
            liblte_security_generate_k_rrc((*iter)->generated_data.k_enb,
                                           LIBLTE_SECURITY_CIPHERING_ALGORITHM_ID_EEA0,
                                           LIBLTE_SECURITY_INTEGRITY_ALGORITHM_ID_128_EIA2,
                                           (*iter)->generated_data.auth_vec.k_rrc_enc,
                                           (*iter)->generated_data.auth_vec.k_rrc_int);

            // Generate K_up_enc and K_up_int
            liblte_security_generate_k_up((*iter)->generated_data.k_enb,
                                          LIBLTE_SECURITY_CIPHERING_ALGORITHM_ID_EEA0,
                                          LIBLTE_SECURITY_INTEGRITY_ALGORITHM_ID_128_EIA2,
                                          (*iter)->generated_data.k_up_enc,
                                          (*iter)->generated_data.k_up_int);

            break;
        }
    }
}
void LTE_fdd_enb_hss::security_resynch(LTE_FDD_ENB_USER_ID_STRUCT *id,
                                       uint16                      mcc,
                                       uint16                      mnc,
                                       uint8                      *auts)
{
    libtools_scoped_lock                               lock(user_sem);
    std::list<LTE_FDD_ENB_HSS_USER_STRUCT *>::iterator iter;
    uint32                                             i;
    uint8                                              sqn[6];

    for(iter=user_list.begin(); iter!=user_list.end(); iter++)
    {
        if(id->imei == (*iter)->id.imei &&
           id->imsi == (*iter)->id.imsi)
        {
            // Decode returned SQN and break into SEQ and IND
            liblte_security_milenage_f5_star((*iter)->stored_data.k,
                                             (*iter)->generated_data.auth_vec.rand,
                                             (*iter)->generated_data.ak);
            (*iter)->generated_data.sqn_he = 0;
            for(i=0; i<6; i++)
            {
                sqn[i]                          = auts[i] ^ (*iter)->generated_data.ak[i];
                (*iter)->generated_data.sqn_he |= (uint64)sqn[i] << (5-i)*8;
            }
            (*iter)->generated_data.seq_he = (*iter)->generated_data.sqn_he >> LTE_FDD_ENB_IND_HE_N_BITS;
            (*iter)->generated_data.ind_he = (*iter)->generated_data.sqn_he & LTE_FDD_ENB_IND_HE_MASK;
            if((*iter)->generated_data.ind_he > 0)
            {
                (*iter)->generated_data.ind_he--;
            }

            break;
        }
    }
}
LTE_FDD_ENB_AUTHENTICATION_VECTOR_STRUCT* LTE_fdd_enb_hss::regenerate_enb_security_data(LTE_FDD_ENB_USER_ID_STRUCT *id,
                                                                                        uint32                      nas_count_ul)
{
    libtools_scoped_lock                                lock(user_sem);
    std::list<LTE_FDD_ENB_HSS_USER_STRUCT *>::iterator  iter;
    LTE_FDD_ENB_AUTHENTICATION_VECTOR_STRUCT           *auth_vec = NULL;

    for(iter=user_list.begin(); iter!=user_list.end(); iter++)
    {
        if(id->imei == (*iter)->id.imei &&
           id->imsi == (*iter)->id.imsi)
        {
            // Generate K_enb
            liblte_security_generate_k_enb((*iter)->generated_data.k_asme,
                                           nas_count_ul,
                                           (*iter)->generated_data.k_enb);

            // Generate K_rrc_enc and K_rrc_int
            liblte_security_generate_k_rrc((*iter)->generated_data.k_enb,
                                           LIBLTE_SECURITY_CIPHERING_ALGORITHM_ID_EEA0,
                                           LIBLTE_SECURITY_INTEGRITY_ALGORITHM_ID_128_EIA2,
                                           (*iter)->generated_data.auth_vec.k_rrc_enc,
                                           (*iter)->generated_data.auth_vec.k_rrc_int);

            // Generate K_up_enc and K_up_int
            liblte_security_generate_k_up((*iter)->generated_data.k_enb,
                                          LIBLTE_SECURITY_CIPHERING_ALGORITHM_ID_EEA0,
                                          LIBLTE_SECURITY_INTEGRITY_ALGORITHM_ID_128_EIA2,
                                          (*iter)->generated_data.k_up_enc,
                                          (*iter)->generated_data.k_up_int);

            auth_vec = &(*iter)->generated_data.auth_vec;

            break;
        }
    }

    return(auth_vec);
}
LTE_FDD_ENB_AUTHENTICATION_VECTOR_STRUCT* LTE_fdd_enb_hss::get_auth_vec(LTE_FDD_ENB_USER_ID_STRUCT *id)
{
    libtools_scoped_lock                                lock(user_sem);
    std::list<LTE_FDD_ENB_HSS_USER_STRUCT *>::iterator  iter;
    LTE_FDD_ENB_AUTHENTICATION_VECTOR_STRUCT           *auth_vec = NULL;

    for(iter=user_list.begin(); iter!=user_list.end(); iter++)
    {
        if(id->imei == (*iter)->id.imei &&
           id->imsi == (*iter)->id.imsi)
        {
            auth_vec = &(*iter)->generated_data.auth_vec;

            break;
        }
    }

    return(auth_vec);
}

/*******************/
/*    User File    */
/*******************/
void LTE_fdd_enb_hss::set_use_user_file(bool uuf)
{
    use_user_file = uuf;

    if(use_user_file)
    {
        write_user_file();
    }else{
        delete_user_file();
    }
}
void LTE_fdd_enb_hss::read_user_file(void)
{
    LTE_fdd_enb_interface *interface = LTE_fdd_enb_interface::get_instance();
    LTE_fdd_enb_cnfg_db   *cnfg_db   = LTE_fdd_enb_cnfg_db::get_instance();
    std::string            line_str;
    FILE                  *user_file = NULL;
    int64                  uuf       = 1;
    char                   str[LTE_FDD_ENB_MAX_LINE_SIZE];

    user_file = fopen("/tmp/LTE_fdd_enodeb.user_db", "r");

    if(NULL != user_file)
    {
        while(NULL != fgets(str, LTE_FDD_ENB_MAX_LINE_SIZE, user_file))
        {
            line_str = str;
            interface->handle_add_user(line_str.substr(0, line_str.length()-1));
        }
        fclose(user_file);
        use_user_file = true;
        cnfg_db->set_param(LTE_FDD_ENB_PARAM_USE_USER_FILE, uuf);
    }
}
void LTE_fdd_enb_hss::write_user_file(void)
{
    libtools_scoped_lock                                lock(user_sem);
    std::list<LTE_FDD_ENB_HSS_USER_STRUCT *>::iterator  iter;
    FILE                                               *user_file = NULL;
    uint32                                              i;

    user_file = fopen("/tmp/LTE_fdd_enodeb.user_db", "w");

    if(NULL != user_file)
    {
        for(iter=user_list.begin(); iter!=user_list.end(); iter++)
        {
            fprintf(user_file, "imsi=%015llu imei=%015llu k=", (*iter)->id.imsi, (*iter)->id.imei);
            for(i=0; i<16; i++)
            {
                fprintf(user_file, "%02X", (*iter)->stored_data.k[i]);
            }
            fprintf(user_file, "\n");
        }
        fclose(user_file);
    }
}
void LTE_fdd_enb_hss::delete_user_file(void)
{
    remove("/tmp/LTE_fdd_enodeb.user_db");
}
