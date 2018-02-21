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

    File: liblte_pdcp.h

    Description: Contains all the definitions for the LTE Packet Data
                 Convergence Protocol Layer library.

    Revision History
    ----------    -------------    --------------------------------------------
    08/03/2014    Ben Wojtowicz    Created file.
    11/01/2014    Ben Wojtowicz    Added integrity protection of messages.
    11/29/2014    Ben Wojtowicz    Using the byte message struct for everything
                                   except RRC SDUs and added user plan data
                                   processing.
    03/11/2015    Ben Wojtowicz    Added data PDU with short SN support.
    12/06/2015    Ben Wojtowicz    Added control PDU for interspersed ROHC
                                   feedback and RN user plane data PDU with
                                   integrity protection support.

*******************************************************************************/

#ifndef __LIBLTE_PDCP_H__
#define __LIBLTE_PDCP_H__

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "liblte_common.h"

/*******************************************************************************
                              DEFINES
*******************************************************************************/


/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/


/*******************************************************************************
                              PARAMETER DECLARATIONS
*******************************************************************************/

/*********************************************************************
    Parameter: MAC-I

    Description: Carries the calculated message authentication code.

    Document Reference: 36.323 v10.1.0 Section 6.3.4
*********************************************************************/
// Defines
#define LIBLTE_PDCP_CONTROL_MAC_I 0x00000000
// Enums
// Structs
// Functions

/*********************************************************************
    Parameter: D/C

    Description: Specifies whether the PDU is data or control.

    Document Reference: 36.323 v10.1.0 Section 6.3.7
*********************************************************************/
// Defines
// Enums
typedef enum{
    LIBLTE_PDCP_D_C_CONTROL_PDU = 0,
    LIBLTE_PDCP_D_C_DATA_PDU,
    LIBLTE_PDCP_D_C_N_ITEMS,
}LIBLTE_PDCP_D_C_ENUM;
static const char liblte_pdcp_d_c_text[LIBLTE_PDCP_D_C_N_ITEMS][20] = {"Control PDU",
                                                                       "Data PDU"};
// Structs
// Functions

/*********************************************************************
    Parameter: PDU Type

    Description: Specifies what type of PDU is present.

    Document Reference: 36.323 v10.1.0 Section 6.3.8
*********************************************************************/
// Defines
#define LIBLTE_PDCP_PDU_TYPE_PDCP_STATUS_REPORT                0x0
#define LIBLTE_PDCP_PDU_TYPE_INTERSPERSED_ROHC_FEEDBACK_PACKET 0x1
// Enums
// Structs
// Functions

/*********************************************************************
    Parameter: Bitmap

    Description: Specifies whether SDUs have or have not been
                 received.

    Document Reference: 36.323 v10.1.0 Section 6.3.10
*********************************************************************/
// Defines
// Enums
// Structs
// Functions

/*******************************************************************************
                              PDU DECLARATIONS
*******************************************************************************/

/*********************************************************************
    PDU Type: Control Plane PDCP Data PDU

    Document Reference: 36.323 v10.1.0 Section 6.2.2
*********************************************************************/
// Defines
// Enums
// Structs
typedef struct{
    LIBLTE_BIT_MSG_STRUCT data;
    uint32                count;
}LIBLTE_PDCP_CONTROL_PDU_STRUCT;
// Functions
LIBLTE_ERROR_ENUM liblte_pdcp_pack_control_pdu(LIBLTE_PDCP_CONTROL_PDU_STRUCT *contents,
                                               LIBLTE_BYTE_MSG_STRUCT         *pdu);
LIBLTE_ERROR_ENUM liblte_pdcp_pack_control_pdu(LIBLTE_PDCP_CONTROL_PDU_STRUCT *contents,
                                               LIBLTE_BIT_MSG_STRUCT          *data,
                                               LIBLTE_BYTE_MSG_STRUCT         *pdu);
LIBLTE_ERROR_ENUM liblte_pdcp_pack_control_pdu(LIBLTE_PDCP_CONTROL_PDU_STRUCT *contents,
                                               uint8                          *key_256,
                                               uint8                           direction,
                                               uint8                           rb_id,
                                               LIBLTE_BYTE_MSG_STRUCT         *pdu);
LIBLTE_ERROR_ENUM liblte_pdcp_pack_control_pdu(LIBLTE_PDCP_CONTROL_PDU_STRUCT *contents,
                                               LIBLTE_BIT_MSG_STRUCT          *data,
                                               uint8                          *key_256,
                                               uint8                           direction,
                                               uint8                           rb_id,
                                               LIBLTE_BYTE_MSG_STRUCT         *pdu);
LIBLTE_ERROR_ENUM liblte_pdcp_unpack_control_pdu(LIBLTE_BYTE_MSG_STRUCT         *pdu,
                                                 LIBLTE_PDCP_CONTROL_PDU_STRUCT *contents);

/*********************************************************************
    PDU Type: User Plane PDCP Data PDU with long PDCP SN

    Document Reference: 36.323 v10.1.0 Section 6.2.3
*********************************************************************/
// Defines
// Enums
// Structs
typedef struct{
    LIBLTE_BYTE_MSG_STRUCT data;
    uint32                 count;
}LIBLTE_PDCP_DATA_PDU_WITH_LONG_SN_STRUCT;
// Functions
LIBLTE_ERROR_ENUM liblte_pdcp_pack_data_pdu_with_long_sn(LIBLTE_PDCP_DATA_PDU_WITH_LONG_SN_STRUCT *contents,
                                                         LIBLTE_BYTE_MSG_STRUCT                   *pdu);
LIBLTE_ERROR_ENUM liblte_pdcp_pack_data_pdu_with_long_sn(LIBLTE_PDCP_DATA_PDU_WITH_LONG_SN_STRUCT *contents,
                                                         LIBLTE_BYTE_MSG_STRUCT                   *data,
                                                         LIBLTE_BYTE_MSG_STRUCT                   *pdu);
LIBLTE_ERROR_ENUM liblte_pdcp_unpack_data_pdu_with_long_sn(LIBLTE_BYTE_MSG_STRUCT                   *pdu,
                                                           LIBLTE_PDCP_DATA_PDU_WITH_LONG_SN_STRUCT *contents);

/*********************************************************************
    PDU Type: User Plane PDCP Data PDU with short PDCP SN

    Document Reference: 36.323 v10.1.0 Section 6.2.4
*********************************************************************/
// Defines
// Enums
// Structs
typedef LIBLTE_PDCP_DATA_PDU_WITH_LONG_SN_STRUCT LIBLTE_PDCP_DATA_PDU_WITH_SHORT_SN_STRUCT;
// Functions
LIBLTE_ERROR_ENUM liblte_pdcp_pack_data_pdu_with_short_sn(LIBLTE_PDCP_DATA_PDU_WITH_SHORT_SN_STRUCT *contents,
                                                          LIBLTE_BYTE_MSG_STRUCT                    *pdu);
LIBLTE_ERROR_ENUM liblte_pdcp_pack_data_pdu_with_short_sn(LIBLTE_PDCP_DATA_PDU_WITH_SHORT_SN_STRUCT *contents,
                                                          LIBLTE_BYTE_MSG_STRUCT                    *data,
                                                          LIBLTE_BYTE_MSG_STRUCT                    *pdu);
LIBLTE_ERROR_ENUM liblte_pdcp_unpack_data_pdu_with_short_sn(LIBLTE_BYTE_MSG_STRUCT                    *pdu,
                                                            LIBLTE_PDCP_DATA_PDU_WITH_SHORT_SN_STRUCT *contents);

/*********************************************************************
    PDU Type: PDCP Control PDU for interspersed ROHC feedback packet

    Document Reference: 36.323 v10.1.0 Section 6.2.5
*********************************************************************/
// Defines
#define LIBLTE_PDCP_PDU_TYPE_STATUS_REPORT                     0x0
#define LIBLTE_PDCP_PDU_TYPE_INTERSPERSED_ROHC_FEEDBACK_PACKET 0x1
// Enums
// Structs
typedef struct{
    LIBLTE_BYTE_MSG_STRUCT rohc_feedback_packet;
    uint8                  pdu_type;
}LIBLTE_PDCP_CONTROL_PDU_FOR_INTERSPERSED_ROHC_FEEDBACK_STRUCT;
// Functions
LIBLTE_ERROR_ENUM liblte_pdcp_pack_control_pdu_for_interspersed_rohc_feedback(LIBLTE_PDCP_CONTROL_PDU_FOR_INTERSPERSED_ROHC_FEEDBACK_STRUCT *contents,
                                                                              LIBLTE_BYTE_MSG_STRUCT                                        *pdu);
LIBLTE_ERROR_ENUM liblte_pdcp_pack_control_pdu_for_interspersed_rohc_feedback(LIBLTE_PDCP_CONTROL_PDU_FOR_INTERSPERSED_ROHC_FEEDBACK_STRUCT *contents,
                                                                              LIBLTE_BYTE_MSG_STRUCT                                        *rohc_feedback_packet,
                                                                              LIBLTE_BYTE_MSG_STRUCT                                        *pdu);
LIBLTE_ERROR_ENUM liblte_pdcp_unpack_control_pdu_for_interspersed_rohc_feedback(LIBLTE_BYTE_MSG_STRUCT                                        *pdu,
                                                                                LIBLTE_PDCP_CONTROL_PDU_FOR_INTERSPERSED_ROHC_FEEDBACK_STRUCT *contents);

/*********************************************************************
    PDU Type: PDCP Control PDU for PDCP status report

    Document Reference: 36.323 v10.1.0 Section 6.2.6
*********************************************************************/
// Defines
// Enums
// Structs
// Functions
// FIXME

/*********************************************************************
    PDU Type: RN User Plane PDCP Data PDU with integrity protection

    Document Reference: 36.323 v10.1.0 Section 6.2.8
*********************************************************************/
// Defines
// Enums
// Structs
typedef struct{
    LIBLTE_BYTE_MSG_STRUCT data;
    uint32                 count;
}LIBLTE_PDCP_RN_USER_PLANE_DATA_PDU_STRUCT;
// Functions
LIBLTE_ERROR_ENUM liblte_pdcp_pack_rn_user_plane_data_pdu(LIBLTE_PDCP_RN_USER_PLANE_DATA_PDU_STRUCT *contents,
                                                          LIBLTE_BYTE_MSG_STRUCT                    *pdu);
LIBLTE_ERROR_ENUM liblte_pdcp_pack_rn_user_plane_data_pdu(LIBLTE_PDCP_RN_USER_PLANE_DATA_PDU_STRUCT *contents,
                                                          LIBLTE_BYTE_MSG_STRUCT                    *data,
                                                          LIBLTE_BYTE_MSG_STRUCT                    *pdu);
LIBLTE_ERROR_ENUM liblte_pdcp_pack_rn_user_plane_data_pdu(LIBLTE_PDCP_RN_USER_PLANE_DATA_PDU_STRUCT *contents,
                                                          uint8                                     *key_256,
                                                          uint8                                      direction,
                                                          uint8                                      rb_id,
                                                          LIBLTE_BYTE_MSG_STRUCT                    *pdu);
LIBLTE_ERROR_ENUM liblte_pdcp_pack_rn_user_plane_data_pdu(LIBLTE_PDCP_RN_USER_PLANE_DATA_PDU_STRUCT *contents,
                                                          LIBLTE_BYTE_MSG_STRUCT                    *data,
                                                          uint8                                     *key_256,
                                                          uint8                                      direction,
                                                          uint8                                      rb_id,
                                                          LIBLTE_BYTE_MSG_STRUCT                    *pdu);
LIBLTE_ERROR_ENUM liblte_pdcp_unpack_rn_user_plane_data_pdu(LIBLTE_BYTE_MSG_STRUCT                    *pdu,
                                                            LIBLTE_PDCP_RN_USER_PLANE_DATA_PDU_STRUCT *contents);

#endif /* __LIBLTE_PDCP_H__ */
