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

    File: liblte_mac.h

    Description: Contains all the definitions for the LTE Medium Access Control
                 Layer library.

    Revision History
    ----------    -------------    --------------------------------------------
    07/21/2013    Ben Wojtowicz    Created file.
    03/26/2014    Ben Wojtowicz    Added DL-SCH/UL-SCH PDU handling.
    05/04/2014    Ben Wojtowicz    Added control element handling.
    06/15/2014    Ben Wojtowicz    Added support for padding LCIDs and breaking
                                   out max and min buffer sizes for BSRs.
    11/29/2014    Ben Wojtowicz    Using byte message struct for SDUs.
    02/15/2015    Ben Wojtowicz    Removed FIXME for transparent mode.
    03/11/2015    Ben Wojtowicz    Fixed long BSR CE and added extended power
                                   headroom CE support.
    07/03/2016    Ben Wojtowicz    Fixed extended power headroom CE.

*******************************************************************************/

#ifndef __LIBLTE_MAC_H__
#define __LIBLTE_MAC_H__

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "liblte_common.h"

/*******************************************************************************
                              DEFINES
*******************************************************************************/

// RNTIs 36.321 v10.2.0 Section 7.1
#define LIBLTE_MAC_INVALID_RNTI    0x0000
#define LIBLTE_MAC_RA_RNTI_START   0x0001
#define LIBLTE_MAC_RA_RNTI_END     0x003C
#define LIBLTE_MAC_C_RNTI_START    0x003D
#define LIBLTE_MAC_C_RNTI_END      0xFFF3
#define LIBLTE_MAC_RESV_RNTI_START 0xFFF4
#define LIBLTE_MAC_RESV_RNTI_END   0xFFFC
#define LIBLTE_MAC_M_RNTI          0xFFFD
#define LIBLTE_MAC_P_RNTI          0xFFFE
#define LIBLTE_MAC_SI_RNTI         0xFFFF

/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/


/*******************************************************************************
                              CONTROL ELEMENT DECLARATIONS
*******************************************************************************/

/*********************************************************************
    MAC CE Name: Truncated Buffer Status Report

    Description: CE containing one LCG ID field and one corresponding
                 Buffer Size field

    Document Reference: 36.321 v10.2.0 Section 6.1.3.1
*********************************************************************/
// Defines
// Enums
// Structs
typedef struct{
    uint32 max_buffer_size;
    uint32 min_buffer_size;
    uint8  lcg_id;
}LIBLTE_MAC_TRUNCATED_BSR_CE_STRUCT;
// Functions
LIBLTE_ERROR_ENUM liblte_mac_pack_truncated_bsr_ce(LIBLTE_MAC_TRUNCATED_BSR_CE_STRUCT  *truncated_bsr,
                                                   uint8                              **ce_ptr);
LIBLTE_ERROR_ENUM liblte_mac_unpack_truncated_bsr_ce(uint8                              **ce_ptr,
                                                     LIBLTE_MAC_TRUNCATED_BSR_CE_STRUCT  *truncated_bsr);

/*********************************************************************
    MAC CE Name: Short Buffer Status Report

    Description: CE containing one LCG ID field and one corresponding
                 Buffer Size field

    Document Reference: 36.321 v10.2.0 Section 6.1.3.1
*********************************************************************/
// Defines
// Enums
// Structs
typedef LIBLTE_MAC_TRUNCATED_BSR_CE_STRUCT LIBLTE_MAC_SHORT_BSR_CE_STRUCT;
// Functions
LIBLTE_ERROR_ENUM liblte_mac_pack_short_bsr_ce(LIBLTE_MAC_SHORT_BSR_CE_STRUCT  *short_bsr,
                                               uint8                          **ce_ptr);
LIBLTE_ERROR_ENUM liblte_mac_unpack_short_bsr_ce(uint8                          **ce_ptr,
                                                 LIBLTE_MAC_SHORT_BSR_CE_STRUCT  *short_bsr);

/*********************************************************************
    MAC CE Name: Long Buffer Status Report

    Description: CE containing four Buffer Size fields, corresponding
                 to LCG IDs #0 through #3

    Document Reference: 36.321 v10.2.0 Section 6.1.3.1
*********************************************************************/
// Defines
// Enums
// Structs
typedef struct{
    uint8 max_buffer_size_0;
    uint8 min_buffer_size_0;
    uint8 max_buffer_size_1;
    uint8 min_buffer_size_1;
    uint8 max_buffer_size_2;
    uint8 min_buffer_size_2;
    uint8 max_buffer_size_3;
    uint8 min_buffer_size_3;
}LIBLTE_MAC_LONG_BSR_CE_STRUCT;
// Functions
LIBLTE_ERROR_ENUM liblte_mac_pack_long_bsr_ce(LIBLTE_MAC_LONG_BSR_CE_STRUCT  *long_bsr,
                                              uint8                         **ce_ptr);
LIBLTE_ERROR_ENUM liblte_mac_unpack_long_bsr_ce(uint8                         **ce_ptr,
                                                LIBLTE_MAC_LONG_BSR_CE_STRUCT  *long_bsr);

/*********************************************************************
    MAC CE Name: C-RNTI

    Description: CE containing a C-RNTI

    Document Reference: 36.321 v10.2.0 Section 6.1.3.2
*********************************************************************/
// Defines
// Enums
// Structs
typedef struct{
    uint16 c_rnti;
}LIBLTE_MAC_C_RNTI_CE_STRUCT;
// Functions
LIBLTE_ERROR_ENUM liblte_mac_pack_c_rnti_ce(LIBLTE_MAC_C_RNTI_CE_STRUCT  *c_rnti,
                                            uint8                       **ce_ptr);
LIBLTE_ERROR_ENUM liblte_mac_unpack_c_rnti_ce(uint8                       **ce_ptr,
                                              LIBLTE_MAC_C_RNTI_CE_STRUCT  *c_rnti);

/*********************************************************************
    MAC CE Name: DRX Command

    Description: CE containing nothing

    Document Reference: 36.321 v10.2.0 Section 6.1.3.3
*********************************************************************/
// Defines
// Enums
// Structs
// Functions

/*********************************************************************
    MAC CE Name: UE Contention Resolution Identity

    Description: CE containing the contention resolution identity for
                 a UE

    Document Reference: 36.321 v10.2.0 Section 6.1.3.4
*********************************************************************/
// Defines
// Enums
// Structs
typedef struct{
    uint64 id;
}LIBLTE_MAC_UE_CONTENTION_RESOLUTION_ID_CE_STRUCT;
// Functions
LIBLTE_ERROR_ENUM liblte_mac_pack_ue_contention_resolution_id_ce(LIBLTE_MAC_UE_CONTENTION_RESOLUTION_ID_CE_STRUCT  *ue_con_res_id,
                                                                 uint8                                            **ce_ptr);
LIBLTE_ERROR_ENUM liblte_mac_unpack_ue_contention_resolution_id_ce(uint8                                            **ce_ptr,
                                                                   LIBLTE_MAC_UE_CONTENTION_RESOLUTION_ID_CE_STRUCT  *ue_con_res_id);

/*********************************************************************
    MAC CE Name: Timing Advance Command

    Description: CE containing a timing advance command for a UE

    Document Reference: 36.321 v10.2.0 Section 6.1.3.5
*********************************************************************/
// Defines
// Enums
// Structs
typedef struct{
    uint8 ta;
}LIBLTE_MAC_TA_COMMAND_CE_STRUCT;
// Functions
LIBLTE_ERROR_ENUM liblte_mac_pack_ta_command_ce(LIBLTE_MAC_TA_COMMAND_CE_STRUCT  *ta_command,
                                                uint8                           **ce_ptr);
LIBLTE_ERROR_ENUM liblte_mac_unpack_ta_command_ce(uint8                           **ce_ptr,
                                                  LIBLTE_MAC_TA_COMMAND_CE_STRUCT  *ta_command);

/*********************************************************************
    MAC CE Name: Power Headroom

    Description: CE containing the power headroom of the UE

    Document Reference: 36.321 v10.2.0 Section 6.1.3.6
*********************************************************************/
// Defines
// Enums
// Structs
typedef struct{
    uint8 ph;
}LIBLTE_MAC_POWER_HEADROOM_CE_STRUCT;
// Functions
LIBLTE_ERROR_ENUM liblte_mac_pack_power_headroom_ce(LIBLTE_MAC_POWER_HEADROOM_CE_STRUCT  *power_headroom,
                                                    uint8                               **ce_ptr);
LIBLTE_ERROR_ENUM liblte_mac_unpack_power_headroom_ce(uint8                               **ce_ptr,
                                                      LIBLTE_MAC_POWER_HEADROOM_CE_STRUCT  *power_headroom);

/*********************************************************************
    MAC CE Name: Extended Power Headroom

    Description: CE containing the power headroom of the UE

    Document Reference: 36.321 v10.2.0 Section 6.1.3.6a
*********************************************************************/
// Defines
// Enums
// Structs
typedef struct{
    uint8 ph;
    uint8 p_cmax;
    bool  p;
    bool  v;
}LIBLTE_MAC_EPH_CELL_STRUCT;
typedef struct{
    LIBLTE_MAC_EPH_CELL_STRUCT pcell_type_1;
    LIBLTE_MAC_EPH_CELL_STRUCT pcell_type_2;
    LIBLTE_MAC_EPH_CELL_STRUCT scell[7];
    bool                       pcell_type_2_present;
    bool                       scell_present[7];
}LIBLTE_MAC_EXT_POWER_HEADROOM_CE_STRUCT;
// Functions
LIBLTE_ERROR_ENUM liblte_mac_pack_ext_power_headroom_ce(LIBLTE_MAC_EXT_POWER_HEADROOM_CE_STRUCT  *ext_power_headroom,
                                                        uint8                                   **ce_ptr);
LIBLTE_ERROR_ENUM liblte_mac_unpack_ext_power_headroom_ce(uint8                                   **ce_ptr,
                                                          bool                                      simultaneous_pucch_pusch,
                                                          LIBLTE_MAC_EXT_POWER_HEADROOM_CE_STRUCT  *ext_power_headroom);

/*********************************************************************
    MAC CE Name: MCH Scheduling Information

    Description: CE containing MTCH stops

    Document Reference: 36.321 v10.2.0 Section 6.1.3.7
*********************************************************************/
// Defines
#define LIBLTE_MAC_MCH_SCHEDULING_INFORMATION_MAX_N_ITEMS 10
// Enums
// Structs
typedef struct{
    uint16 stop_mch[LIBLTE_MAC_MCH_SCHEDULING_INFORMATION_MAX_N_ITEMS];
    uint8  lcid[LIBLTE_MAC_MCH_SCHEDULING_INFORMATION_MAX_N_ITEMS];
    uint8  N_items;
}LIBLTE_MAC_MCH_SCHEDULING_INFORMATION_CE_STRUCT;
// Functions
LIBLTE_ERROR_ENUM liblte_mac_pack_mch_scheduling_information_ce(LIBLTE_MAC_MCH_SCHEDULING_INFORMATION_CE_STRUCT  *mch_sched_info,
                                                                uint8                                           **ce_ptr);
LIBLTE_ERROR_ENUM liblte_mac_unpack_mch_scheduling_information_ce(uint8                                           **ce_ptr,
                                                                  LIBLTE_MAC_MCH_SCHEDULING_INFORMATION_CE_STRUCT  *mch_sched_info);

/*********************************************************************
    MAC CE Name: Activation Deactivation

    Description: CE containing activation/deactivation of SCells

    Document Reference: 36.321 v10.2.0 Section 6.1.3.8
*********************************************************************/
// Defines
// Enums
// Structs
typedef struct{
    bool c1;
    bool c2;
    bool c3;
    bool c4;
    bool c5;
    bool c6;
    bool c7;
}LIBLTE_MAC_ACTIVATION_DEACTIVATION_CE_STRUCT;
// Functions
LIBLTE_ERROR_ENUM liblte_mac_pack_activation_deactivation_ce(LIBLTE_MAC_ACTIVATION_DEACTIVATION_CE_STRUCT  *act_deact,
                                                             uint8                                        **ce_ptr);
LIBLTE_ERROR_ENUM liblte_mac_unpack_activation_deactivation_ce(uint8                                        **ce_ptr,
                                                               LIBLTE_MAC_ACTIVATION_DEACTIVATION_CE_STRUCT  *act_deact);

/*******************************************************************************
                              PDU DECLARATIONS
*******************************************************************************/

/*********************************************************************
    PDU Name: DL-SCH and UL-SCH MAC PDU

    Description: PDU containing a MAC header, zero or more MAC SDUs,
                 and zero or more MAC control elements

    Document Reference: 36.321 v10.2.0 Section 6.1.2
*********************************************************************/
// Defines
#define LIBLTE_MAC_MAX_MAC_PDU_N_SUBHEADERS               10
#define LIBLTE_MAC_DLSCH_CCCH_LCID                        0x00
#define LIBLTE_MAC_DLSCH_DCCH_LCID_BEGIN                  0x01
#define LIBLTE_MAC_DLSCH_DCCH_LCID_END                    0x0A
#define LIBLTE_MAC_DLSCH_ACTIVATION_DEACTIVATION_LCID     0x1B
#define LIBLTE_MAC_DLSCH_UE_CONTENTION_RESOLUTION_ID_LCID 0x1C
#define LIBLTE_MAC_DLSCH_TA_COMMAND_LCID                  0x1D
#define LIBLTE_MAC_DLSCH_DRX_COMMAND_LCID                 0x1E
#define LIBLTE_MAC_DLSCH_PADDING_LCID                     0x1F
#define LIBLTE_MAC_ULSCH_CCCH_LCID                        0x00
#define LIBLTE_MAC_ULSCH_DCCH_LCID_BEGIN                  0x01
#define LIBLTE_MAC_ULSCH_DCCH_LCID_END                    0x0A
#define LIBLTE_MAC_ULSCH_EXT_POWER_HEADROOM_REPORT_LCID   0x19
#define LIBLTE_MAC_ULSCH_POWER_HEADROOM_REPORT_LCID       0x1A
#define LIBLTE_MAC_ULSCH_C_RNTI_LCID                      0x1B
#define LIBLTE_MAC_ULSCH_TRUNCATED_BSR_LCID               0x1C
#define LIBLTE_MAC_ULSCH_SHORT_BSR_LCID                   0x1D
#define LIBLTE_MAC_ULSCH_LONG_BSR_LCID                    0x1E
#define LIBLTE_MAC_ULSCH_PADDING_LCID                     0x1F
#define LIBLTE_MAC_MCH_MCCH_LCID                          0x00
#define LIBLTE_MAC_MCH_MTCH_LCID_BEGIN                    0x01
#define LIBLTE_MAC_MCH_MTCH_LCID_END                      0x1C
#define LIBLTE_MAC_MCH_SCHEDULING_INFORMATION_LCID        0x1E
#define LIBLTE_MAC_MCH_PADDING_LCID                       0x1F
// Enums
typedef enum{
    LIBLTE_MAC_CHAN_TYPE_DLSCH = 0,
    LIBLTE_MAC_CHAN_TYPE_ULSCH,
    LIBLTE_MAC_CHAN_TYPE_MCH,
}LIBLTE_MAC_CHAN_TYPE_ENUM;
// Structs
typedef union{
    LIBLTE_MAC_TRUNCATED_BSR_CE_STRUCT               truncated_bsr;
    LIBLTE_MAC_SHORT_BSR_CE_STRUCT                   short_bsr;
    LIBLTE_MAC_LONG_BSR_CE_STRUCT                    long_bsr;
    LIBLTE_MAC_C_RNTI_CE_STRUCT                      c_rnti;
    LIBLTE_MAC_UE_CONTENTION_RESOLUTION_ID_CE_STRUCT ue_con_res_id;
    LIBLTE_MAC_TA_COMMAND_CE_STRUCT                  ta_command;
    LIBLTE_MAC_POWER_HEADROOM_CE_STRUCT              power_headroom;
    LIBLTE_MAC_EXT_POWER_HEADROOM_CE_STRUCT          ext_power_headroom;
    LIBLTE_MAC_MCH_SCHEDULING_INFORMATION_CE_STRUCT  mch_sched_info;
    LIBLTE_MAC_ACTIVATION_DEACTIVATION_CE_STRUCT     act_deact;
    LIBLTE_BYTE_MSG_STRUCT                           sdu;
}LIBLTE_MAC_SUBHEADER_PAYLOAD_UNION;
typedef struct{
    LIBLTE_MAC_SUBHEADER_PAYLOAD_UNION payload;
    uint32                             lcid;
}LIBLTE_MAC_PDU_SUBHEADER_STRUCT;
typedef struct{
    LIBLTE_MAC_PDU_SUBHEADER_STRUCT subheader[LIBLTE_MAC_MAX_MAC_PDU_N_SUBHEADERS];
    LIBLTE_MAC_CHAN_TYPE_ENUM       chan_type;
    uint32                          N_subheaders;
}LIBLTE_MAC_PDU_STRUCT;
// Functions
LIBLTE_ERROR_ENUM liblte_mac_pack_mac_pdu(LIBLTE_MAC_PDU_STRUCT *mac_pdu,
                                          LIBLTE_BIT_MSG_STRUCT *pdu);
LIBLTE_ERROR_ENUM liblte_mac_unpack_mac_pdu(LIBLTE_BIT_MSG_STRUCT *pdu,
                                            bool                   simultaneous_pucch_pusch,
                                            LIBLTE_MAC_PDU_STRUCT *mac_pdu);

/*********************************************************************
    PDU Name: Transparent

    Description: PDU containing a MAC SDU

    Document Reference: 36.321 v10.2.0 Section 6.1.4
*********************************************************************/
// Defines
// Enums
// Structs
// Functions

/*********************************************************************
    PDU Name: Random Access Response

    Description: PDU containing a MAC header and zero or more MAC
                 Random Access Responses

    Document Reference: 36.321 v10.2.0 Section 6.1.5
*********************************************************************/
// Defines
// Enums
typedef enum{
    LIBLTE_MAC_RAR_HEADER_TYPE_BI = 0,
    LIBLTE_MAC_RAR_HEADER_TYPE_RAPID,
    LIBLTE_MAC_RAR_HEADER_TYPE_N_ITEMS,
}LIBLTE_MAC_RAR_HEADER_TYPE_ENUM;
static const char liblte_mac_rar_header_type_text[LIBLTE_MAC_RAR_HEADER_TYPE_N_ITEMS][20] = {"BI", "RAPID"};
typedef enum{
    LIBLTE_MAC_RAR_HOPPING_DISABLED = 0,
    LIBLTE_MAC_RAR_HOPPING_ENABLED,
    LIBLTE_MAC_RAR_HOPPING_N_ITEMS,
}LIBLTE_MAC_RAR_HOPPING_ENUM;
static const char liblte_mac_rar_hopping_text[LIBLTE_MAC_RAR_HOPPING_N_ITEMS][20] = {"Disabled", "Enabled"};
typedef enum{
    LIBLTE_MAC_RAR_TPC_COMMAND_N6dB = 0,
    LIBLTE_MAC_RAR_TPC_COMMAND_N4dB,
    LIBLTE_MAC_RAR_TPC_COMMAND_N2dB,
    LIBLTE_MAC_RAR_TPC_COMMAND_0dB,
    LIBLTE_MAC_RAR_TPC_COMMAND_2dB,
    LIBLTE_MAC_RAR_TPC_COMMAND_4dB,
    LIBLTE_MAC_RAR_TPC_COMMAND_6dB,
    LIBLTE_MAC_RAR_TPC_COMMAND_8dB,
    LIBLTE_MAC_RAR_TPC_COMMAND_N_ITEMS,
}LIBLTE_MAC_RAR_TPC_COMMAND_ENUM;
static const char liblte_mac_rar_tpc_command_text[LIBLTE_MAC_RAR_TPC_COMMAND_N_ITEMS][20] = {"-6dB", "-4dB", "-2dB",  "0dB",
                                                                                              "2dB",  "4dB",  "6dB",  "8dB"};
static const int8 liblte_mac_rar_tpc_command_num[LIBLTE_MAC_RAR_TPC_COMMAND_N_ITEMS] = {-6, -4, -2, 0, 2, 4, 6, 8};
typedef enum{
    LIBLTE_MAC_RAR_UL_DELAY_DISABLED = 0,
    LIBLTE_MAC_RAR_UL_DELAY_ENABLED,
    LIBLTE_MAC_RAR_UL_DELAY_N_ITEMS,
}LIBLTE_MAC_RAR_UL_DELAY_ENUM;
static const char liblte_mac_rar_ul_delay_text[LIBLTE_MAC_RAR_UL_DELAY_N_ITEMS][20] = {"Disabled", "Enabled"};
typedef enum{
    LIBLTE_MAC_RAR_CSI_REQ_DISABLED = 0,
    LIBLTE_MAC_RAR_CSI_REQ_ENABLED,
    LIBLTE_MAC_RAR_CSI_REQ_N_ITEMS,
}LIBLTE_MAC_RAR_CSI_REQ_ENUM;
static const char liblte_mac_rar_csi_req_text[LIBLTE_MAC_RAR_CSI_REQ_N_ITEMS][20] = {"Disabled", "Enabled"};
// Structs
typedef struct{
    LIBLTE_MAC_RAR_HEADER_TYPE_ENUM hdr_type;
    LIBLTE_MAC_RAR_HOPPING_ENUM     hopping_flag;
    LIBLTE_MAC_RAR_TPC_COMMAND_ENUM tpc_command;
    LIBLTE_MAC_RAR_UL_DELAY_ENUM    ul_delay;
    LIBLTE_MAC_RAR_CSI_REQ_ENUM     csi_req;
    uint16                          rba;
    uint16                          timing_adv_cmd;
    uint16                          temp_c_rnti;
    uint8                           mcs;
    uint8                           RAPID;
    uint8                           BI;
}LIBLTE_MAC_RAR_STRUCT;
// Functions
LIBLTE_ERROR_ENUM liblte_mac_pack_random_access_response_pdu(LIBLTE_MAC_RAR_STRUCT *rar,
                                                             LIBLTE_BIT_MSG_STRUCT *pdu);
LIBLTE_ERROR_ENUM liblte_mac_unpack_random_access_response_pdu(LIBLTE_BIT_MSG_STRUCT *pdu,
                                                               LIBLTE_MAC_RAR_STRUCT *rar);

#endif /* __LIBLTE_MAC_H__ */
