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

    File: LTE_fdd_enb_rb.h

    Description: Contains all the definitions for the LTE FDD eNodeB
                 radio bearer class.

    Revision History
    ----------    -------------    --------------------------------------------
    05/04/2014    Ben Wojtowicz    Created file
    06/15/2014    Ben Wojtowicz    Added more states and procedures, QoS, MME,
                                   RLC, and uplink scheduling functionality.
    08/03/2014    Ben Wojtowicz    Added MME procedures/states, RRC NAS support,
                                   RRC transaction id, PDCP sequence numbers,
                                   and RLC transmit variables.
    09/03/2014    Ben Wojtowicz    Added more MME states and ability to store
                                   the contention resolution identity.
    11/01/2014    Ben Wojtowicz    Added more MME states and PDCP security.
    11/29/2014    Ben Wojtowicz    Added more DRB support, MME states, MME
                                   procedures, and PDCP configs, moved almost
                                   everything to byte messages structs, added
                                   IP gateway and RLC UMD support.
    12/16/2014    Ben Wojtowicz    Added QoS for default data services.
    12/24/2014    Ben Wojtowicz    Added asymmetric QoS support.
    02/15/2015    Ben Wojtowicz    Split UL/DL QoS TTI frequency, added reset
                                   user support, and added multiple UMD RLC data
                                   support.
    03/11/2015    Ben Wojtowicz    Added detach handling.
    07/25/2015    Ben Wojtowicz    Moved QoS structure to the user class and
                                   fixed RLC AM TX and RX buffers.
    12/06/2015    Ben Wojtowicz    Changed boost::mutex to sem_t.
    02/13/2016    Ben Wojtowicz    Added a wait for RRC connection
                                   reestablishment complete RRC state.
    12/18/2016    Ben Wojtowicz    Properly handling multiple RLC AMD PDUs.
    07/29/2017    Ben Wojtowicz    Remove last TTI storage.

*******************************************************************************/

#ifndef __LTE_FDD_ENB_RB_H__
#define __LTE_FDD_ENB_RB_H__

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_fdd_enb_common.h"
#include "liblte_rlc.h"
#include "liblte_rrc.h"
#include <list>
#include <map>
#include <semaphore.h>

/*******************************************************************************
                              DEFINES
*******************************************************************************/


/*******************************************************************************
                              FORWARD DECLARATIONS
*******************************************************************************/

class LTE_fdd_enb_user;

/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/

typedef enum{
    LTE_FDD_ENB_RB_SRB0 = 0,
    LTE_FDD_ENB_RB_SRB1,
    LTE_FDD_ENB_RB_SRB2,
    LTE_FDD_ENB_RB_DRB1,
    LTE_FDD_ENB_RB_DRB2,
    LTE_FDD_ENB_RB_N_ITEMS,
}LTE_FDD_ENB_RB_ENUM;
static const char LTE_fdd_enb_rb_text[LTE_FDD_ENB_RB_N_ITEMS][20] = {"SRB0",
                                                                     "SRB1",
                                                                     "SRB2",
                                                                     "DRB1",
                                                                     "DRB2"};

typedef enum{
    LTE_FDD_ENB_MME_PROC_IDLE = 0,
    LTE_FDD_ENB_MME_PROC_ATTACH,
    LTE_FDD_ENB_MME_PROC_SERVICE_REQUEST,
    LTE_FDD_ENB_MME_PROC_DETACH,
    LTE_FDD_ENB_MME_PROC_N_ITEMS,
}LTE_FDD_ENB_MME_PROC_ENUM;
static const char LTE_fdd_enb_mme_proc_text[LTE_FDD_ENB_MME_PROC_N_ITEMS][100] = {"IDLE",
                                                                                  "ATTACH",
                                                                                  "SERVICE REQUEST",
                                                                                  "DETACH"};

typedef enum{
    LTE_FDD_ENB_MME_STATE_IDLE = 0,
    LTE_FDD_ENB_MME_STATE_ID_REQUEST_IMSI,
    LTE_FDD_ENB_MME_STATE_REJECT,
    LTE_FDD_ENB_MME_STATE_AUTHENTICATE,
    LTE_FDD_ENB_MME_STATE_AUTH_REJECTED,
    LTE_FDD_ENB_MME_STATE_ENABLE_SECURITY,
    LTE_FDD_ENB_MME_STATE_RELEASE,
    LTE_FDD_ENB_MME_STATE_RRC_SECURITY,
    LTE_FDD_ENB_MME_STATE_ESM_INFO_TRANSFER,
    LTE_FDD_ENB_MME_STATE_ATTACH_ACCEPT,
    LTE_FDD_ENB_MME_STATE_ATTACHED,
    LTE_FDD_ENB_MME_STATE_SETUP_DRB,
    LTE_FDD_ENB_MME_STATE_SEND_DETACH_ACCEPT,
    LTE_FDD_ENB_MME_STATE_N_ITEMS,
}LTE_FDD_ENB_MME_STATE_ENUM;
static const char LTE_fdd_enb_mme_state_text[LTE_FDD_ENB_MME_STATE_N_ITEMS][100] = {"IDLE",
                                                                                    "ID REQUEST IMSI",
                                                                                    "REJECT",
                                                                                    "AUTHENTICATE",
                                                                                    "AUTH REJECTED",
                                                                                    "ENABLE SECURITY",
                                                                                    "RELEASE",
                                                                                    "RRC SECURITY",
                                                                                    "ESM INFO TRANSFER",
                                                                                    "ATTACH ACCEPT",
                                                                                    "ATTACHED",
                                                                                    "SETUP DRB",
                                                                                    "SEND DETACH ACCEPT"};

typedef enum{
    LTE_FDD_ENB_RRC_PROC_IDLE = 0,
    LTE_FDD_ENB_RRC_PROC_RRC_CON_REQ,
    LTE_FDD_ENB_RRC_PROC_RRC_CON_REEST_REQ,
    LTE_FDD_ENB_RRC_PROC_N_ITEMS,
}LTE_FDD_ENB_RRC_PROC_ENUM;
static const char LTE_fdd_enb_rrc_proc_text[LTE_FDD_ENB_RRC_PROC_N_ITEMS][100] = {"IDLE",
                                                                                  "RRC CON REQ",
                                                                                  "RRC CON REEST REQ"};

typedef enum{
    LTE_FDD_ENB_RRC_STATE_IDLE = 0,
    LTE_FDD_ENB_RRC_STATE_SRB1_SETUP,
    LTE_FDD_ENB_RRC_STATE_WAIT_FOR_CON_SETUP_COMPLETE,
    LTE_FDD_ENB_RRC_STATE_RRC_CONNECTED,
    LTE_FDD_ENB_RRC_STATE_WAIT_FOR_CON_REEST_COMPLETE,
    LTE_FDD_ENB_RRC_STATE_N_ITEMS,
}LTE_FDD_ENB_RRC_STATE_ENUM;
static const char LTE_fdd_enb_rrc_state_text[LTE_FDD_ENB_RRC_STATE_N_ITEMS][100] = {"IDLE",
                                                                                    "SRB1 SETUP",
                                                                                    "WAIT FOR CON SETUP COMPLETE",
                                                                                    "RRC CONNECTED",
                                                                                    "WAIT FOR CON REEST COMPLETE"};

typedef enum{
    LTE_FDD_ENB_PDCP_CONFIG_N_A = 0,
    LTE_FDD_ENB_PDCP_CONFIG_SECURITY,
    LTE_FDD_ENB_PDCP_CONFIG_LONG_SN,
    LTE_FDD_ENB_PDCP_CONFIG_N_ITEMS,
}LTE_FDD_ENB_PDCP_CONFIG_ENUM;
static const char LTE_fdd_enb_pdcp_config_text[LTE_FDD_ENB_PDCP_CONFIG_N_ITEMS][20] = {"N/A",
                                                                                       "SECURITY",
                                                                                       "LONG SN"};

typedef enum{
    LTE_FDD_ENB_RLC_CONFIG_TM = 0,
    LTE_FDD_ENB_RLC_CONFIG_UM,
    LTE_FDD_ENB_RLC_CONFIG_AM,
    LTE_FDD_ENB_RLC_CONFIG_N_ITEMS,
}LTE_FDD_ENB_RLC_CONFIG_ENUM;
static const char LTE_fdd_enb_rlc_config_text[LTE_FDD_ENB_RLC_CONFIG_N_ITEMS][20] = {"TM",
                                                                                     "UM",
                                                                                     "AM"};

typedef enum{
    LTE_FDD_ENB_MAC_CONFIG_TM = 0,
    LTE_FDD_ENB_MAC_CONFIG_N_ITEMS,
}LTE_FDD_ENB_MAC_CONFIG_ENUM;
static const char LTE_fdd_enb_mac_config_text[LTE_FDD_ENB_MAC_CONFIG_N_ITEMS][20] = {"TM"};

/*******************************************************************************
                              CLASS DECLARATIONS
*******************************************************************************/

class LTE_fdd_enb_rb
{
public:
    // Constructor/Destructor
    LTE_fdd_enb_rb(LTE_FDD_ENB_RB_ENUM _rb, LTE_fdd_enb_user *_user);
    ~LTE_fdd_enb_rb();

    // Identity
    LTE_FDD_ENB_RB_ENUM get_rb_id(void);
    LTE_fdd_enb_user* get_user(void);
    void reset_user(LTE_fdd_enb_user *_user);

    // GW
    void queue_gw_data_msg(LIBLTE_BYTE_MSG_STRUCT *gw_data);
    LTE_FDD_ENB_ERROR_ENUM get_next_gw_data_msg(LIBLTE_BYTE_MSG_STRUCT **gw_data);
    LTE_FDD_ENB_ERROR_ENUM delete_next_gw_data_msg(void);

    // MME
    void queue_mme_nas_msg(LIBLTE_BYTE_MSG_STRUCT *nas_msg);
    LTE_FDD_ENB_ERROR_ENUM get_next_mme_nas_msg(LIBLTE_BYTE_MSG_STRUCT **nas_msg);
    LTE_FDD_ENB_ERROR_ENUM delete_next_mme_nas_msg(void);
    void set_mme_procedure(LTE_FDD_ENB_MME_PROC_ENUM procedure);
    LTE_FDD_ENB_MME_PROC_ENUM get_mme_procedure(void);
    void set_mme_state(LTE_FDD_ENB_MME_STATE_ENUM state);
    LTE_FDD_ENB_MME_STATE_ENUM get_mme_state(void);

    // RRC
    LIBLTE_RRC_UL_CCCH_MSG_STRUCT ul_ccch_msg;
    LIBLTE_RRC_UL_DCCH_MSG_STRUCT ul_dcch_msg;
    LIBLTE_RRC_DL_CCCH_MSG_STRUCT dl_ccch_msg;
    LIBLTE_RRC_DL_DCCH_MSG_STRUCT dl_dcch_msg;
    void queue_rrc_pdu(LIBLTE_BIT_MSG_STRUCT *pdu);
    LTE_FDD_ENB_ERROR_ENUM get_next_rrc_pdu(LIBLTE_BIT_MSG_STRUCT **pdu);
    LTE_FDD_ENB_ERROR_ENUM delete_next_rrc_pdu(void);
    void queue_rrc_nas_msg(LIBLTE_BYTE_MSG_STRUCT *nas_msg);
    LTE_FDD_ENB_ERROR_ENUM get_next_rrc_nas_msg(LIBLTE_BYTE_MSG_STRUCT **nas_msg);
    LTE_FDD_ENB_ERROR_ENUM delete_next_rrc_nas_msg(void);
    void set_rrc_procedure(LTE_FDD_ENB_RRC_PROC_ENUM procedure);
    LTE_FDD_ENB_RRC_PROC_ENUM get_rrc_procedure(void);
    void set_rrc_state(LTE_FDD_ENB_RRC_STATE_ENUM state);
    LTE_FDD_ENB_RRC_STATE_ENUM get_rrc_state(void);
    uint8 get_rrc_transaction_id(void);
    void set_rrc_transaction_id(uint8 transaction_id);

    // PDCP
    void queue_pdcp_pdu(LIBLTE_BYTE_MSG_STRUCT *pdu);
    LTE_FDD_ENB_ERROR_ENUM get_next_pdcp_pdu(LIBLTE_BYTE_MSG_STRUCT **pdu);
    LTE_FDD_ENB_ERROR_ENUM delete_next_pdcp_pdu(void);
    void queue_pdcp_sdu(LIBLTE_BIT_MSG_STRUCT *sdu);
    LTE_FDD_ENB_ERROR_ENUM get_next_pdcp_sdu(LIBLTE_BIT_MSG_STRUCT **sdu);
    LTE_FDD_ENB_ERROR_ENUM delete_next_pdcp_sdu(void);
    void queue_pdcp_data_sdu(LIBLTE_BYTE_MSG_STRUCT *sdu);
    LTE_FDD_ENB_ERROR_ENUM get_next_pdcp_data_sdu(LIBLTE_BYTE_MSG_STRUCT **sdu);
    LTE_FDD_ENB_ERROR_ENUM delete_next_pdcp_data_sdu(void);
    void set_pdcp_config(LTE_FDD_ENB_PDCP_CONFIG_ENUM config);
    LTE_FDD_ENB_PDCP_CONFIG_ENUM get_pdcp_config(void);
    uint32 get_pdcp_rx_count(void);
    void set_pdcp_rx_count(uint32 rx_count);
    uint32 get_pdcp_tx_count(void);
    void set_pdcp_tx_count(uint32 tx_count);

    // RLC
    void queue_rlc_pdu(LIBLTE_BYTE_MSG_STRUCT *pdu);
    LTE_FDD_ENB_ERROR_ENUM get_next_rlc_pdu(LIBLTE_BYTE_MSG_STRUCT **pdu);
    LTE_FDD_ENB_ERROR_ENUM delete_next_rlc_pdu(void);
    void queue_rlc_sdu(LIBLTE_BYTE_MSG_STRUCT *sdu);
    LTE_FDD_ENB_ERROR_ENUM get_next_rlc_sdu(LIBLTE_BYTE_MSG_STRUCT **sdu);
    LTE_FDD_ENB_ERROR_ENUM delete_next_rlc_sdu(void);
    LTE_FDD_ENB_RLC_CONFIG_ENUM get_rlc_config(void);
    uint16 get_rlc_vrr(void);
    void set_rlc_vrr(uint16 vrr);
    void update_rlc_vrr(void);
    uint16 get_rlc_vrmr(void);
    uint16 get_rlc_vrh(void);
    void set_rlc_vrh(uint16 vrh);
    void rlc_add_to_am_reception_buffer(LIBLTE_RLC_SINGLE_AMD_PDU_STRUCT *amd_pdu);
    void rlc_get_am_reception_buffer_status(LIBLTE_RLC_STATUS_PDU_STRUCT *status);
    LTE_FDD_ENB_ERROR_ENUM rlc_am_reassemble(LIBLTE_BYTE_MSG_STRUCT *sdu);
    uint16 get_rlc_vta(void);
    void set_rlc_vta(uint16 vta);
    uint16 get_rlc_vtms(void);
    uint16 get_rlc_vts(void);
    void set_rlc_vts(uint16 vts);
    void rlc_add_to_transmission_buffer(LIBLTE_RLC_SINGLE_AMD_PDU_STRUCT *amd_pdu);
    void rlc_update_transmission_buffer(LIBLTE_RLC_STATUS_PDU_STRUCT *status);
    void rlc_start_t_poll_retransmit(void);
    void rlc_stop_t_poll_retransmit(void);
    void handle_t_poll_retransmit_timer_expiry(uint32 timer_id);
    void set_rlc_vruh(uint16 vruh);
    uint16 get_rlc_vruh(void);
    void set_rlc_vrur(uint16 vrur);
    uint16 get_rlc_vrur(void);
    uint16 get_rlc_um_window_size(void);
    void rlc_add_to_um_reception_buffer(LIBLTE_RLC_UMD_PDU_STRUCT *umd_pdu, uint32 idx);
    LTE_FDD_ENB_ERROR_ENUM rlc_um_reassemble(LIBLTE_BYTE_MSG_STRUCT *sdu);
    void set_rlc_vtus(uint16 vtus);
    uint16 get_rlc_vtus(void);

    // MAC
    void queue_mac_sdu(LIBLTE_BYTE_MSG_STRUCT *sdu);
    LTE_FDD_ENB_ERROR_ENUM get_next_mac_sdu(LIBLTE_BYTE_MSG_STRUCT **sdu);
    LTE_FDD_ENB_ERROR_ENUM delete_next_mac_sdu(void);
    LTE_FDD_ENB_MAC_CONFIG_ENUM get_mac_config(void);
    void set_con_res_id(uint64 con_res_id);
    uint64 get_con_res_id(void);
    void set_send_con_res_id(bool send_con_res_id);
    bool get_send_con_res_id(void);

    // DRB
    void set_eps_bearer_id(uint32 ebi);
    uint32 get_eps_bearer_id(void);
    void set_lc_id(uint32 _lc_id);
    uint32 get_lc_id(void);
    void set_drb_id(uint8 _drb_id);
    uint8 get_drb_id(void);
    void set_log_chan_group(uint8 lcg);
    uint8 get_log_chan_group(void);

private:
    // Identity
    LTE_FDD_ENB_RB_ENUM  rb;
    LTE_fdd_enb_user    *user;

    // GW
    sem_t                               gw_data_msg_queue_sem;
    std::list<LIBLTE_BYTE_MSG_STRUCT *> gw_data_msg_queue;

    // MME
    sem_t                               mme_nas_msg_queue_sem;
    std::list<LIBLTE_BYTE_MSG_STRUCT *> mme_nas_msg_queue;
    LTE_FDD_ENB_MME_PROC_ENUM           mme_procedure;
    LTE_FDD_ENB_MME_STATE_ENUM          mme_state;

    // RRC
    sem_t                               rrc_pdu_queue_sem;
    sem_t                               rrc_nas_msg_queue_sem;
    std::list<LIBLTE_BIT_MSG_STRUCT *>  rrc_pdu_queue;
    std::list<LIBLTE_BYTE_MSG_STRUCT *> rrc_nas_msg_queue;
    LTE_FDD_ENB_RRC_PROC_ENUM           rrc_procedure;
    LTE_FDD_ENB_RRC_STATE_ENUM          rrc_state;
    uint8                               rrc_transaction_id;

    // PDCP
    sem_t                               pdcp_pdu_queue_sem;
    sem_t                               pdcp_sdu_queue_sem;
    sem_t                               pdcp_data_sdu_queue_sem;
    std::list<LIBLTE_BYTE_MSG_STRUCT *> pdcp_pdu_queue;
    std::list<LIBLTE_BIT_MSG_STRUCT *>  pdcp_sdu_queue;
    std::list<LIBLTE_BYTE_MSG_STRUCT *> pdcp_data_sdu_queue;
    LTE_FDD_ENB_PDCP_CONFIG_ENUM        pdcp_config;
    uint32                              pdcp_rx_count;
    uint32                              pdcp_tx_count;

    // RLC
    sem_t                                                rlc_pdu_queue_sem;
    sem_t                                                rlc_sdu_queue_sem;
    std::list<LIBLTE_BYTE_MSG_STRUCT *>                  rlc_pdu_queue;
    std::list<LIBLTE_BYTE_MSG_STRUCT *>                  rlc_sdu_queue;
    std::map<uint16, LIBLTE_RLC_SINGLE_AMD_PDU_STRUCT *> rlc_am_reception_buffer;
    std::map<uint16, LIBLTE_RLC_SINGLE_AMD_PDU_STRUCT *> rlc_am_transmission_buffer;
    std::map<uint16, LIBLTE_BYTE_MSG_STRUCT *>           rlc_um_reception_buffer;
    LTE_FDD_ENB_RLC_CONFIG_ENUM                          rlc_config;
    uint32                                               t_poll_retransmit_timer_id;
    uint16                                               rlc_vrr;
    uint16                                               rlc_vrmr;
    uint16                                               rlc_vrh;
    uint16                                               rlc_vta;
    uint16                                               rlc_vtms;
    uint16                                               rlc_vts;
    uint16                                               rlc_vruh;
    uint16                                               rlc_vrur;
    uint16                                               rlc_um_window_size;
    uint16                                               rlc_first_um_segment_sn;
    uint16                                               rlc_last_um_segment_sn;
    uint16                                               rlc_vtus;

    // MAC
    sem_t                               mac_sdu_queue_sem;
    std::list<LIBLTE_BYTE_MSG_STRUCT *> mac_sdu_queue;
    LTE_FDD_ENB_MAC_CONFIG_ENUM         mac_config;
    uint64                              mac_con_res_id;
    bool                                mac_send_con_res_id;

    // DRB
    uint32 eps_bearer_id;
    uint32 lc_id;
    uint8  drb_id;
    uint8  log_chan_group;

    // Generic
    void queue_msg(LIBLTE_BIT_MSG_STRUCT *msg, sem_t *sem, std::list<LIBLTE_BIT_MSG_STRUCT *> *queue);
    void queue_msg(LIBLTE_BYTE_MSG_STRUCT *msg, sem_t *sem, std::list<LIBLTE_BYTE_MSG_STRUCT *> *queue);
    LTE_FDD_ENB_ERROR_ENUM get_next_msg(sem_t *sem, std::list<LIBLTE_BIT_MSG_STRUCT *> *queue, LIBLTE_BIT_MSG_STRUCT **msg);
    LTE_FDD_ENB_ERROR_ENUM get_next_msg(sem_t *sem, std::list<LIBLTE_BYTE_MSG_STRUCT *> *queue, LIBLTE_BYTE_MSG_STRUCT **msg);
    LTE_FDD_ENB_ERROR_ENUM delete_next_msg(sem_t *sem, std::list<LIBLTE_BIT_MSG_STRUCT *> *queue);
    LTE_FDD_ENB_ERROR_ENUM delete_next_msg(sem_t *sem, std::list<LIBLTE_BYTE_MSG_STRUCT *> *queue);
};

#endif /* __LTE_FDD_ENB_RB_H__ */
