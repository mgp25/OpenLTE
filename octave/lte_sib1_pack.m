%
% Copyright 2012 Ben Wojtowicz
%
%    This program is free software: you can redistribute it and/or modify
%    it under the terms of the GNU Affero General Public License as published by
%    the Free Software Foundation, either version 3 of the License, or
%    (at your option) any later version.
%
%    This program is distributed in the hope that it will be useful,
%    but WITHOUT ANY WARRANTY; without even the implied warranty of
%    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
%    GNU Affero General Public License for more details.
%
%    You should have received a copy of the GNU Affero General Public License
%    along with this program.  If not, see <http://www.gnu.org/licenses/>.
%
% Function:    lte_sib1_pack
% Description: Packs all of the fields into a System Information Type 1 message
% Inputs:      mcc          - Mobile Country Code
%              mnc          - Mobile Network Code
%              tac          - Tracking Area Code
%              cell_id      - Cell identity
%              q_rx_lev_min - Minimum RX level for access to the network
%              freq_band    - Frequency band used
% Outputs:     sib1         - Packed System Information Type 1 message
% Spec:        3GPP TS 36.331 section 6.2.2 v10.0.0
% Notes:       None
% Rev History: Ben Wojtowicz 1/22/2012 Created
%
function [sib1] = lte_sib1_pack(mcc, mnc, tac, cell_id, q_rx_lev_min, freq_band)
    % Check mcc
    if(!(mcc >= 0 && mcc <= 999))
        printf("ERROR: Invalid mcc %u (range is 0 to 999)\n", mcc);
        sib1 = 0;
        return;
    endif

    % Check mnc
    if(!(mnc >= 0 && mnc <= 999))
        printf("ERROR: Invalid mnc %u (range is 0 to 999)\n", mnc);
        sib1 = 0;
        return;
    endif

    % Check tac
    if(!(tac >= 0 && tac <= 65535))
        printf("ERROR: Invalid TAC %u (range is 0 to 65535)\n", tac);
        sib1 = 0;
        return;
    endif

    % Check cell_id
    if(!(cell_id >= 0 && cell_id <= 268435455))
        printf("ERROR: Invalid cell_id %u (range is 0 to 268435455)\n", cell_id);
        sib1 = 0;
        return;
    endif

    % Check q_rx_lev_min
    if(!(q_rx_lev_min >= -70 && q_rx_lev_min <= -22))
        printf("ERROR: Invalid q_rx_lev_min %u (range is -70 to -22)\n", q_rx_lev_min);
        sib1 = 0;
        return;
    endif

    % Check freq_band
    if(!(freq_band >= 1 && freq_band <= 43))
        printf("ERROR: Invalid freq_band %u (range is 1 to 43)\n", freq_band);
        sib1 = 0;
        return;
    endif
        
    % Convert MCC and MNC to digits
    tmp_mcc = mcc;
    tmp_mnc = mnc;
    for(n=0:2)
        mcc_dig(n+1) = floor(tmp_mcc/10^(2-n));
        mnc_dig(n+1) = floor(tmp_mnc/10^(2-n));
        tmp_mcc      = tmp_mcc - mcc_dig(n+1)*10^(2-n);
        tmp_mnc      = tmp_mnc - mnc_dig(n+1)*10^(2-n);
    endfor

    % Optional field indicators
    act_opt_field_inds = [0,0,0];

    % Cell Access Related Info
    act_num_plmn_ids = [0,0,0];
    act_mcc          = [1,cmn_dec2bin(mcc_dig(1),4),cmn_dec2bin(mcc_dig(2),4),cmn_dec2bin(mcc_dig(3),4)];
    act_mnc          = [1,cmn_dec2bin(mnc_dig(1),4),cmn_dec2bin(mnc_dig(2),4),cmn_dec2bin(mnc_dig(3),4)];
    act_crou         = [1];
    act_tac          = [cmn_dec2bin(tac, 16)];
    act_cell_id      = [cmn_dec2bin(cell_id, 28)];
    act_cb           = [1];
    act_ifr          = [1];
    act_csg_ind      = [0];
    act_car_info     = [0,act_num_plmn_ids,act_mcc,act_mnc,act_crou,act_tac,act_cell_id,act_cb,act_ifr,act_csg_ind];

    % Cell Selection Info
    act_q_rx_lev_min = cmn_dec2bin(q_rx_lev_min+70, 6);
    act_cs_info      = [0,act_q_rx_lev_min];

    % Frequency Band Indicator
    act_f_band_ind = cmn_dec2bin(freq_band-1, 6);

    % Scheduling Info List (size 1-32)
    act_sched_info_list_size  = [0,0,0,0,0];
    act_si_periodicity        = [0,0,0];
    act_sib_mapping_info_size = [0,0,0,0,0];
    act_sched_info_list       = [act_sched_info_list_size, act_si_periodicity, act_sib_mapping_info_size];

    % SI Window Length
    act_si_window_len = [0,0,0];

    % SI Value Tag
    act_si_value_tag = [0,0,0,0,0];

    % Pack SIB1
    sib1 = [act_opt_field_inds,act_car_info,act_cs_info,act_f_band_ind,act_sched_info_list,act_si_window_len,act_si_value_tag];
endfunction
