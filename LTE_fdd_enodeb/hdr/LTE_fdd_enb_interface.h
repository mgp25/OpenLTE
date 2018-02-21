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

    File: LTE_fdd_enb_interface.h

    Description: Contains all the definitions for the LTE FDD eNodeB interface.

    Revision History
    ----------    -------------    --------------------------------------------
    11/09/2013    Ben Wojtowicz    Created file
    01/18/2014    Ben Wojtowicz    Added dynamic variables and added level to
                                   debug prints.
    03/26/2014    Ben Wojtowicz    Using the latest LTE library.
    04/12/2014    Ben Wojtowicz    Pulled in a patch from Max Suraev for more
                                   descriptive start failures.
    05/04/2014    Ben Wojtowicz    Added PCAP support and more error types.
    06/15/2014    Ben Wojtowicz    Added new error causes, ... support for info
                                   messages, and using the latest LTE library.
    07/22/2014    Ben Wojtowicz    Added clock source as a configurable
                                   parameter.
    08/03/2014    Ben Wojtowicz    Added HSS support.
    09/03/2014    Ben Wojtowicz    Added read only parameters for UL EARFCN,
                                   DL center frequency and UL center frequency.
    11/01/2014    Ben Wojtowicz    Added parameters for IP address assignment,
                                   DNS address, config file, and user file.
    11/29/2014    Ben Wojtowicz    Added support for the IP gateway.
    02/15/2015    Ben Wojtowicz    Moved to new message queue, added IP pcap
                                   support, and put error enum into common hdr.
    03/11/2015    Ben Wojtowicz    Made a common routine for formatting time.
    07/25/2015    Ben Wojtowicz    Made tx_gain and rx_gain into config file
                                   tracked parameters.
    12/06/2015    Ben Wojtowicz    Changed boost::mutex to sem_t.
    02/13/2016    Ben Wojtowicz    Added a command to print all registered
                                   users.
    07/29/2017    Ben Wojtowicz    Added input parameters for direct IPC to a UE
                                   and using the latest tools library.

*******************************************************************************/

#ifndef __LTE_FDD_ENB_INTERFACE_H__
#define __LTE_FDD_ENB_INTERFACE_H__

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "LTE_fdd_enb_common.h"
#include "LTE_fdd_enb_msgq.h"
#include "liblte_common.h"
#include "libtools_socket_wrap.h"
#include <string>

/*******************************************************************************
                              DEFINES
*******************************************************************************/

#define LTE_FDD_ENB_DEFAULT_CTRL_PORT 30000
#define LTE_FDD_ENB_DEBUG_PORT_OFFSET 1

/*******************************************************************************
                              FORWARD DECLARATIONS
*******************************************************************************/

class LTE_fdd_enb_pdcp;
class LTE_fdd_enb_mme;
class LTE_fdd_enb_gw;

/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/

typedef enum{
    LTE_FDD_ENB_DEBUG_TYPE_ERROR = 0,
    LTE_FDD_ENB_DEBUG_TYPE_WARNING,
    LTE_FDD_ENB_DEBUG_TYPE_INFO,
    LTE_FDD_ENB_DEBUG_TYPE_DEBUG,
    LTE_FDD_ENB_DEBUG_TYPE_N_ITEMS,
}LTE_FDD_ENB_DEBUG_TYPE_ENUM;
static const char LTE_fdd_enb_debug_type_text[LTE_FDD_ENB_DEBUG_TYPE_N_ITEMS][100] = {"error",
                                                                                      "warning",
                                                                                      "info",
                                                                                      "debug"};

typedef enum{
    LTE_FDD_ENB_DEBUG_LEVEL_RADIO = 0,
    LTE_FDD_ENB_DEBUG_LEVEL_PHY,
    LTE_FDD_ENB_DEBUG_LEVEL_MAC,
    LTE_FDD_ENB_DEBUG_LEVEL_RLC,
    LTE_FDD_ENB_DEBUG_LEVEL_PDCP,
    LTE_FDD_ENB_DEBUG_LEVEL_RRC,
    LTE_FDD_ENB_DEBUG_LEVEL_MME,
    LTE_FDD_ENB_DEBUG_LEVEL_GW,
    LTE_FDD_ENB_DEBUG_LEVEL_USER,
    LTE_FDD_ENB_DEBUG_LEVEL_RB,
    LTE_FDD_ENB_DEBUG_LEVEL_TIMER,
    LTE_FDD_ENB_DEBUG_LEVEL_IFACE,
    LTE_FDD_ENB_DEBUG_LEVEL_MSGQ,
    LTE_FDD_ENB_DEBUG_LEVEL_N_ITEMS,
}LTE_FDD_ENB_DEBUG_LEVEL_ENUM;
static const char LTE_fdd_enb_debug_level_text[LTE_FDD_ENB_DEBUG_LEVEL_N_ITEMS][100] = {"radio",
                                                                                        "phy",
                                                                                        "mac",
                                                                                        "rlc",
                                                                                        "pdcp",
                                                                                        "rrc",
                                                                                        "mme",
                                                                                        "gw",
                                                                                        "user",
                                                                                        "rb",
                                                                                        "timer",
                                                                                        "iface",
                                                                                        "msgq"};

typedef enum{
    LTE_FDD_ENB_PCAP_DIRECTION_UL = 0,
    LTE_FDD_ENB_PCAP_DIRECTION_DL,
    LTE_FDD_ENB_PCAP_DIRECTION_N_ITEMS,
}LTE_FDD_ENB_PCAP_DIRECTION_ENUM;
static const char LTE_fdd_enb_pcap_direction_text[LTE_FDD_ENB_PCAP_DIRECTION_N_ITEMS][20] = {"UL",
                                                                                             "DL"};

typedef enum{
    LTE_FDD_ENB_VAR_TYPE_DOUBLE = 0,
    LTE_FDD_ENB_VAR_TYPE_INT64,
    LTE_FDD_ENB_VAR_TYPE_HEX,
    LTE_FDD_ENB_VAR_TYPE_UINT32,
}LTE_FDD_ENB_VAR_TYPE_ENUM;

typedef enum{
    // System parameters managed by LTE_fdd_enb_cnfg_db
    LTE_FDD_ENB_PARAM_BANDWIDTH = 0,
    LTE_FDD_ENB_PARAM_FREQ_BAND,
    LTE_FDD_ENB_PARAM_DL_EARFCN,
    LTE_FDD_ENB_PARAM_UL_EARFCN,
    LTE_FDD_ENB_PARAM_DL_CENTER_FREQ,
    LTE_FDD_ENB_PARAM_UL_CENTER_FREQ,
    LTE_FDD_ENB_PARAM_N_RB_DL,
    LTE_FDD_ENB_PARAM_N_RB_UL,
    LTE_FDD_ENB_PARAM_DL_BW,
    LTE_FDD_ENB_PARAM_N_SC_RB_DL,
    LTE_FDD_ENB_PARAM_N_SC_RB_UL,
    LTE_FDD_ENB_PARAM_N_ANT,
    LTE_FDD_ENB_PARAM_N_ID_CELL,
    LTE_FDD_ENB_PARAM_N_ID_2,
    LTE_FDD_ENB_PARAM_N_ID_1,
    LTE_FDD_ENB_PARAM_MCC,
    LTE_FDD_ENB_PARAM_MNC,
    LTE_FDD_ENB_PARAM_CELL_ID,
    LTE_FDD_ENB_PARAM_TRACKING_AREA_CODE,
    LTE_FDD_ENB_PARAM_Q_RX_LEV_MIN,
    LTE_FDD_ENB_PARAM_P0_NOMINAL_PUSCH,
    LTE_FDD_ENB_PARAM_P0_NOMINAL_PUCCH,
    LTE_FDD_ENB_PARAM_SIB3_PRESENT,
    LTE_FDD_ENB_PARAM_Q_HYST,
    LTE_FDD_ENB_PARAM_SIB4_PRESENT,
    LTE_FDD_ENB_PARAM_SIB5_PRESENT,
    LTE_FDD_ENB_PARAM_SIB6_PRESENT,
    LTE_FDD_ENB_PARAM_SIB7_PRESENT,
    LTE_FDD_ENB_PARAM_SIB8_PRESENT,
    LTE_FDD_ENB_PARAM_SEARCH_WIN_SIZE,
    LTE_FDD_ENB_PARAM_SYSTEM_INFO_VALUE_TAG,
    LTE_FDD_ENB_PARAM_SYSTEM_INFO_WINDOW_LENGTH,
    LTE_FDD_ENB_PARAM_PHICH_RESOURCE,
    LTE_FDD_ENB_PARAM_N_SCHED_INFO,
    LTE_FDD_ENB_PARAM_SYSTEM_INFO_PERIODICITY,
    LTE_FDD_ENB_PARAM_MAC_DIRECT_TO_UE,
    LTE_FDD_ENB_PARAM_PHY_DIRECT_TO_UE,
    LTE_FDD_ENB_PARAM_DEBUG_TYPE,
    LTE_FDD_ENB_PARAM_DEBUG_LEVEL,
    LTE_FDD_ENB_PARAM_ENABLE_PCAP,
    LTE_FDD_ENB_PARAM_IP_ADDR_START,
    LTE_FDD_ENB_PARAM_DNS_ADDR,
    LTE_FDD_ENB_PARAM_USE_CNFG_FILE,
    LTE_FDD_ENB_PARAM_USE_USER_FILE,
    LTE_FDD_ENB_PARAM_TX_GAIN,
    LTE_FDD_ENB_PARAM_RX_GAIN,

    // Radio parameters managed by LTE_fdd_enb_radio
    LTE_FDD_ENB_PARAM_AVAILABLE_RADIOS,
    LTE_FDD_ENB_PARAM_SELECTED_RADIO_NAME,
    LTE_FDD_ENB_PARAM_SELECTED_RADIO_IDX,
    LTE_FDD_ENB_PARAM_CLOCK_SOURCE,

    LTE_FDD_ENB_PARAM_N_ITEMS,
}LTE_FDD_ENB_PARAM_ENUM;
static const char LTE_fdd_enb_param_text[LTE_FDD_ENB_PARAM_N_ITEMS][100] = {"bandwidth",
                                                                            "band",
                                                                            "dl_earfcn",
                                                                            "ul_earfcn",
                                                                            "dl_center_freq",
                                                                            "ul_center_freq",
                                                                            "n_rb_dl",
                                                                            "n_rb_ul",
                                                                            "dl_bw",
                                                                            "n_sc_rb_dl",
                                                                            "n_sc_rb_ul",
                                                                            "n_ant",
                                                                            "n_id_cell",
                                                                            "n_id_2",
                                                                            "n_id_1",
                                                                            "mcc",
                                                                            "mnc",
                                                                            "cell_id",
                                                                            "tracking_area_code",
                                                                            "q_rx_lev_min",
                                                                            "p0_nominal_pusch",
                                                                            "p0_nominal_pucch",
                                                                            "sib3_present",
                                                                            "q_hyst",
                                                                            "sib4_present",
                                                                            "sib5_present",
                                                                            "sib6_present",
                                                                            "sib7_present",
                                                                            "sib8_present",
                                                                            "search_win_size",
                                                                            "system_info_value_tag",
                                                                            "system_info_window_length",
                                                                            "phich_resource",
                                                                            "n_sched_info",
                                                                            "system_info_periodicity",
                                                                            "mac_direct_to_ue",
                                                                            "phy_direct_to_ue",
                                                                            "debug_type",
                                                                            "debug_level",
                                                                            "enable_pcap",
                                                                            "ip_addr_start",
                                                                            "dns_addr",
                                                                            "use_cnfg_file",
                                                                            "use_user_file",
                                                                            "tx_gain",
                                                                            "rx_gain",
                                                                            "available_radios",
                                                                            "selected_radio_name",
                                                                            "selected_radio_idx",
                                                                            "clock_source"};

typedef struct{
    LTE_FDD_ENB_VAR_TYPE_ENUM var_type;
    LTE_FDD_ENB_PARAM_ENUM    param;
    double                    double_l_bound;
    double                    double_u_bound;
    int64                     int64_l_bound;
    int64                     int64_u_bound;
    bool                      special_bounds;
    bool                      dynamic;
    bool                      read_only;
}LTE_FDD_ENB_VAR_STRUCT;

/*******************************************************************************
                              CLASS DECLARATIONS
*******************************************************************************/

class LTE_fdd_enb_interface
{
public:
    // Singleton
    static LTE_fdd_enb_interface* get_instance(void);
    static void cleanup(void);

    // Communication
    void set_ctrl_port(int16 port);
    void start_ports(void);
    void stop_ports(void);
    void send_ctrl_msg(std::string msg);
    void send_ctrl_info_msg(std::string msg, ...);
    void send_ctrl_error_msg(LTE_FDD_ENB_ERROR_ENUM error, std::string msg);
    void send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ENUM type, LTE_FDD_ENB_DEBUG_LEVEL_ENUM level, std::string file_name, int32 line, std::string msg, ...);
    void send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ENUM type, LTE_FDD_ENB_DEBUG_LEVEL_ENUM level, std::string file_name, int32 line, LIBLTE_BIT_MSG_STRUCT *lte_msg, std::string msg, ...);
    void send_debug_msg(LTE_FDD_ENB_DEBUG_TYPE_ENUM type, LTE_FDD_ENB_DEBUG_LEVEL_ENUM level, std::string file_name, int32 line, LIBLTE_BYTE_MSG_STRUCT *lte_msg, std::string msg, ...);
    void open_lte_pcap_fd(void);
    void open_ip_pcap_fd(void);
    void send_lte_pcap_msg(LTE_FDD_ENB_PCAP_DIRECTION_ENUM dir, uint32 rnti, uint32 current_tti, uint8 *msg, uint32 N_bits);
    void send_ip_pcap_msg(uint8 *msg, uint32 N_bytes);
    static void handle_ctrl_msg(std::string msg);
    static void handle_ctrl_connect(void);
    static void handle_ctrl_disconnect(void);
    static void handle_ctrl_error(LIBTOOLS_SOCKET_WRAP_ERROR_ENUM err);
    static void handle_debug_msg(std::string msg);
    static void handle_debug_connect(void);
    static void handle_debug_disconnect(void);
    static void handle_debug_error(LIBTOOLS_SOCKET_WRAP_ERROR_ENUM err);
    sem_t                 ctrl_sem;
    sem_t                 debug_sem;
    FILE                 *lte_pcap_fd;
    FILE                 *ip_pcap_fd;
    libtools_socket_wrap *ctrl_socket;
    libtools_socket_wrap *debug_socket;
    int16                 ctrl_port;
    int16                 debug_port;
    static bool           ctrl_connected;
    static bool           debug_connected;

    // Handlers
    LTE_FDD_ENB_ERROR_ENUM handle_write(std::string msg);
    void handle_add_user(std::string msg);

    // Get/Set
    bool get_shutdown(void);
    bool app_is_started(void);

private:
    // Singleton
    static LTE_fdd_enb_interface *instance;
    LTE_fdd_enb_interface();
    ~LTE_fdd_enb_interface();

    // Handlers
    void handle_read(std::string msg);
    void handle_start(void);
    void handle_stop(void);
    void handle_help(void);
    void handle_del_user(std::string msg);
    void handle_print_users(void);
    void handle_print_registered_users(void);

    // Variables
    std::map<std::string, LTE_FDD_ENB_VAR_STRUCT>  var_map;
    LTE_fdd_enb_pdcp                              *pdcp;
    LTE_fdd_enb_mme                               *mme;
    LTE_fdd_enb_gw                                *gw;
    sem_t                                          start_sem;
    uint32                                         debug_type_mask;
    uint32                                         debug_level_mask;
    bool                                           shutdown;
    bool                                           started;

    // Helpers
    LTE_FDD_ENB_ERROR_ENUM write_value(LTE_FDD_ENB_VAR_STRUCT *var, double value);
    LTE_FDD_ENB_ERROR_ENUM write_value(LTE_FDD_ENB_VAR_STRUCT *var, int64 value);
    LTE_FDD_ENB_ERROR_ENUM write_value(LTE_FDD_ENB_VAR_STRUCT *var, std::string value);
    LTE_FDD_ENB_ERROR_ENUM write_value(LTE_FDD_ENB_VAR_STRUCT *var, uint32 value);

    // Inter-stack communication
    LTE_fdd_enb_msgq *phy_to_mac_comm;
    LTE_fdd_enb_msgq *mac_to_phy_comm;
    LTE_fdd_enb_msgq *mac_to_rlc_comm;
    LTE_fdd_enb_msgq *mac_to_timer_comm;
    LTE_fdd_enb_msgq *rlc_to_mac_comm;
    LTE_fdd_enb_msgq *rlc_to_pdcp_comm;
    LTE_fdd_enb_msgq *pdcp_to_rlc_comm;
    LTE_fdd_enb_msgq *pdcp_to_rrc_comm;
    LTE_fdd_enb_msgq *rrc_to_pdcp_comm;
    LTE_fdd_enb_msgq *rrc_to_mme_comm;
    LTE_fdd_enb_msgq *mme_to_rrc_comm;
    LTE_fdd_enb_msgq *pdcp_to_gw_comm;
    LTE_fdd_enb_msgq *gw_to_pdcp_comm;
};

#endif /* __LTE_FDD_ENB_INTERFACE_H__ */
