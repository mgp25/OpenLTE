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

    File: LTE_fdd_enb_hss.h

    Description: Contains all the definitions for the LTE FDD eNodeB
                 home subscriber server.

    Revision History
    ----------    -------------    --------------------------------------------
    06/15/2014    Ben Wojtowicz    Created file
    08/03/2014    Ben Wojtowicz    Added authentication vector support.
    09/03/2014    Ben Wojtowicz    Added sequence number resynch.
    11/01/2014    Ben Wojtowicz    Added user file support.
    11/29/2014    Ben Wojtowicz    Added support for regenerating eNodeB
                                   security data.
    12/06/2015    Ben Wojtowicz    Changed boost::mutex to sem_t.

*******************************************************************************/

#ifndef __LTE_FDD_ENB_HSS_H__
#define __LTE_FDD_ENB_HSS_H__

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_fdd_enb_interface.h"
#include "LTE_fdd_enb_user.h"
#include <list>

/*******************************************************************************
                              DEFINES
*******************************************************************************/

#define LTE_FDD_ENB_IND_HE_N_BITS    5
#define LTE_FDD_ENB_IND_HE_MASK      0x1FUL
#define LTE_FDD_ENB_IND_HE_MAX_VALUE 31
#define LTE_FDD_ENB_SEQ_HE_MAX_VALUE 0x7FFFFFFFFFFFUL

/*******************************************************************************
                              FORWARD DECLARATIONS
*******************************************************************************/


/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/

typedef struct{
    uint8 k[16];
}LTE_FDD_ENB_STORED_DATA_STRUCT;

typedef struct{
    LTE_FDD_ENB_AUTHENTICATION_VECTOR_STRUCT auth_vec;
    uint64                                   sqn_he;
    uint64                                   seq_he;
    uint8                                    ak[6];
    uint8                                    mac[8];
    uint8                                    k_asme[32];
    uint8                                    k_enb[32];
    uint8                                    k_up_enc[32];
    uint8                                    k_up_int[32];
    uint8                                    ind_he;
}LTE_FDD_ENB_GENERATED_DATA_STRUCT;

typedef struct{
    LTE_FDD_ENB_USER_ID_STRUCT        id;
    LTE_FDD_ENB_STORED_DATA_STRUCT    stored_data;
    LTE_FDD_ENB_GENERATED_DATA_STRUCT generated_data;
}LTE_FDD_ENB_HSS_USER_STRUCT;

/*******************************************************************************
                              CLASS DECLARATIONS
*******************************************************************************/

class LTE_fdd_enb_hss
{
public:
    // Singleton
    static LTE_fdd_enb_hss* get_instance(void);
    static void cleanup(void);

    // External interface
    LTE_FDD_ENB_ERROR_ENUM add_user(std::string imsi, std::string imei, std::string k);
    LTE_FDD_ENB_ERROR_ENUM del_user(std::string imsi);
    std::string print_all_users(void);
    bool is_imsi_allowed(uint64 imsi);
    bool is_imei_allowed(uint64 imei);
    LTE_FDD_ENB_USER_ID_STRUCT* get_user_id_from_imsi(uint64 imsi);
    LTE_FDD_ENB_USER_ID_STRUCT* get_user_id_from_imei(uint64 imei);
    void generate_security_data(LTE_FDD_ENB_USER_ID_STRUCT *id, uint16 mcc, uint16 mnc);
    void security_resynch(LTE_FDD_ENB_USER_ID_STRUCT *id, uint16 mcc, uint16 mnc, uint8 *auts);
    LTE_FDD_ENB_AUTHENTICATION_VECTOR_STRUCT* regenerate_enb_security_data(LTE_FDD_ENB_USER_ID_STRUCT *id, uint32 nas_count_ul);
    LTE_FDD_ENB_AUTHENTICATION_VECTOR_STRUCT* get_auth_vec(LTE_FDD_ENB_USER_ID_STRUCT *id);

    // User File
    void set_use_user_file(bool uuf);
    void read_user_file(void);

private:
    // Singleton
    static LTE_fdd_enb_hss *instance;
    LTE_fdd_enb_hss();
    ~LTE_fdd_enb_hss();

    // Allowed users
    sem_t                                    user_sem;
    std::list<LTE_FDD_ENB_HSS_USER_STRUCT *> user_list;

    // User File
    void write_user_file(void);
    void delete_user_file(void);
    bool use_user_file;
};

#endif /* __LTE_FDD_ENB_HSS_H__ */
