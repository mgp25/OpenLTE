/*******************************************************************************

    Copyright 2017 Ben Wojtowicz

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

    File: libtools_ipc_msgq.h

    Description: Contains all the definitions for the interprocess message
                 queue abstraction tool.

    Revision History
    ----------    -------------    --------------------------------------------
    07/29/2017    Ben Wojtowicz    Created file

*******************************************************************************/

#ifndef __LIBTOOLS_IPC_MSGQ_H__
#define __LIBTOOLS_IPC_MSGQ_H__

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "typedefs.h"
#include "liblte_phy.h"
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


typedef enum{
    LIBTOOLS_IPC_MSGQ_MESSAGE_TYPE_KILL = 0,
    LIBTOOLS_IPC_MSGQ_MESSAGE_TYPE_RACH,
    LIBTOOLS_IPC_MSGQ_MESSAGE_TYPE_MAC_PDU,
    LIBTOOLS_IPC_MSGQ_MESSAGE_TYPE_RAR_PDU,
    LIBTOOLS_IPC_MSGQ_MESSAGE_TYPE_UL_ALLOC,
    LIBTOOLS_IPC_MSGQ_MESSAGE_TYPE_PHY_SAMPS,
    LIBTOOLS_IPC_MSGQ_MESSAGE_TYPE_N_ITEMS,
}LIBTOOLS_IPC_MSGQ_MESSAGE_TYPE_ENUM;
static const char libtools_ipc_msgq_message_type_text[LIBTOOLS_IPC_MSGQ_MESSAGE_TYPE_N_ITEMS][100] = {"Kill",
                                                                                                      "RACH",
                                                                                                      "MAC PDU",
                                                                                                      "RAR PDU",
                                                                                                      "UL ALLOC",
                                                                                                      "PHY SAMPS"};

typedef struct{
    uint32 preamble;
}LIBTOOLS_IPC_MSGQ_RACH_MSG_STRUCT;

typedef struct{
    LIBLTE_BIT_MSG_STRUCT msg;
    uint16                rnti;
}LIBTOOLS_IPC_MSGQ_MAC_PDU_MSG_STRUCT;

typedef struct{
    LIBLTE_BIT_MSG_STRUCT msg;
    uint32                tti;
}LIBTOOLS_IPC_MSGQ_RAR_PDU_MSG_STRUCT;

typedef struct{
    uint32 size;
    uint32 tti;
}LIBTOOLS_IPC_MSGQ_UL_ALLOC_MSG_STRUCT;

typedef struct{
    float  i_buf[4][LIBLTE_PHY_N_SAMPS_PER_SUBFR_30_72MHZ];
    float  q_buf[4][LIBLTE_PHY_N_SAMPS_PER_SUBFR_30_72MHZ];
    uint32 N_samps_per_ant;
    uint16 current_tti;
    uint8  N_ant;
}LIBTOOLS_IPC_MSGQ_PHY_SAMPS_MSG_STRUCT;

typedef union{
    LIBTOOLS_IPC_MSGQ_RACH_MSG_STRUCT      rach;
    LIBTOOLS_IPC_MSGQ_MAC_PDU_MSG_STRUCT   mac_pdu_msg;
    LIBTOOLS_IPC_MSGQ_RAR_PDU_MSG_STRUCT   rar_pdu_msg;
    LIBTOOLS_IPC_MSGQ_UL_ALLOC_MSG_STRUCT  ul_alloc_msg;
    LIBTOOLS_IPC_MSGQ_PHY_SAMPS_MSG_STRUCT phy_samps_msg;
}LIBTOOLS_IPC_MSGQ_MESSAGE_UNION;

typedef struct{
    LIBTOOLS_IPC_MSGQ_MESSAGE_TYPE_ENUM type;
    LIBTOOLS_IPC_MSGQ_MESSAGE_UNION     msg;
}LIBTOOLS_IPC_MSGQ_MESSAGE_STRUCT;

/*******************************************************************************
                              CLASS DECLARATIONS
*******************************************************************************/

// Message queue callback
class libtools_ipc_msgq_cb
{
public:
    typedef void (*FuncType)(void*, LIBTOOLS_IPC_MSGQ_MESSAGE_STRUCT*);
    libtools_ipc_msgq_cb();
    libtools_ipc_msgq_cb(FuncType f, void* o);
    void operator()(LIBTOOLS_IPC_MSGQ_MESSAGE_STRUCT *msg);
private:
    FuncType  func;
    void     *obj;
};
template<class class_type, void (class_type::*Func)(LIBTOOLS_IPC_MSGQ_MESSAGE_STRUCT*)>
    void libtools_ipc_msgq_cb_wrapper(void *o, LIBTOOLS_IPC_MSGQ_MESSAGE_STRUCT *msg)
{
    return (static_cast<class_type*>(o)->*Func)(msg);
}

class libtools_ipc_msgq
{
public:
    libtools_ipc_msgq(std::string          _msgq_name,
                      libtools_ipc_msgq_cb cb);
    ~libtools_ipc_msgq();

    // Send/Receive
    void send(LIBTOOLS_IPC_MSGQ_MESSAGE_TYPE_ENUM type, LIBTOOLS_IPC_MSGQ_MESSAGE_UNION *msg_content, uint32 msg_content_size);
private:
    // Send/Receive
    static void* receive_thread(void *inputs);

    // Variables
    libtools_ipc_msgq_cb callback;
    std::string          msgq_name;
    pthread_t            rx_thread;
};

#endif /* __LIBTOOLS_IPC_MSGQ_H__ */
