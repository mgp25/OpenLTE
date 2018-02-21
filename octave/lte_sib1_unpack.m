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
% Function:    lte_sib1_unpack
% Description: Unpacks all of the fields from a System Information Block
%              Type 1 message.
% Inputs:      sib1_msg - Packed System Information Type 1 message
% Outputs:     sib_s    - Structure containing all of the SIB1 fields
%                         (plmn_ids, tac, cell_id, cell_barred, q_rx_lev_min,
%                         band, sched_info_list)
% Spec:        3GPP TS 36.331 section 6.2.2 v10.0.0
% Notes:       None
% Rev History: Ben Wojtowicz 02/11/2012 Created
%              Ben Wojtowicz 04/21/2012 Fixed conversions of q_rx_lev_min
%                                       and q_rx_lev_min_offset.  Split
%                                       unpacking into functions.
%
function [sib1_s] = lte_sib1_unpack(sib1_msg)
    % Unpack the message
    idx = 0;

    % Optional field indicators
    p_max_opt        = sib1_msg(idx+1);
    idx              = idx + 1;
    tdd_config_opt   = sib1_msg(idx+1);
    idx              = idx + 1;
    non_crit_ext_opt = sib1_msg(idx+1);
    idx              = idx + 1;

    % Cell Access Related Info
    [idx, cari_s] = cell_access_related_info_unpack(sib1_msg, idx);

    % Cell Selection Info
    [idx, csi_s] = cell_selection_info_unpack(sib1_msg, idx);

    % Power Max
    [idx, pm_s] = power_max_unpack(sib1_msg, idx, p_max_opt);

    % Frequency band indicator
    [idx, fbi_s] = frequency_band_indicator_unpack(sib1_msg, idx);

    % Scheduling Info List
    [idx, sil_s] = scheduling_info_list_unpack(sib1_msg, idx);

    % TDD Config
    [idx, tddc_s] = tdd_config_unpack(sib1_msg, idx, tdd_config_opt);

    % SI Window Length
    [idx, siwl_s] = si_window_length_unpack(sib1_msg, idx);

    % System Info Value Tag
    [idx, sivt_s] = system_info_value_tag_unpack(sib1_msg, idx);

    % Non Critical Extension
    [idx, nce_s] = non_critical_extension_unpack(sib1_msg, idx, non_crit_ext_opt);

    % Fill the output structure
    sib1_s.plmn_ids        = cari_s.plmn_id_list;
    sib1_s.tac             = cari_s.tac;
    sib1_s.cell_id         = cari_s.cell_id;
    sib1_s.cell_barred     = cari_s.cell_barred;
    sib1_s.q_rx_lev_min    = csi_s.q_rx_lev_min;
    sib1_s.band            = fbi_s.f_band_ind;
    sib1_s.sched_info_list = sil_s.sched_info_list;
    sib1_s.si_window_len   = siwl_s.si_window_len;
endfunction

function [out_idx, cari_s] = cell_access_related_info_unpack(msg, in_idx)
    out_idx = in_idx;

    % Optional flag
    csg_id_opt = msg(out_idx+1);
    out_idx    = out_idx + 1;

    % PLMN ID List
    plmn_id_list.num_ids = cmn_bin2dec(msg(out_idx+1:out_idx+3), 3)+1;
    out_idx              = out_idx + 3;
    for(n=0:plmn_id_list.num_ids-1)
        mcc_opt = msg(out_idx+1);
        out_idx = out_idx + 1;
        if(mcc_opt == 1)
            id.mcc = 0;
            for(m=0:2)
                tmp     = cmn_bin2dec(msg(out_idx+1:out_idx+4), 4);
                id.mcc  = id.mcc + tmp*10^(2-m);
                out_idx = out_idx + 4;
            endfor
        else
            id.mcc = plmn_id_list.id(n).mcc;
        endif
        id.mnc_size = msg(out_idx+1)+2;
        out_idx     = out_idx + 1;
        id.mnc      = 0;
        for(m=0:id.mnc_size-1)
            tmp     = cmn_bin2dec(msg(out_idx+1:out_idx+4), 4);
            id.mnc  = id.mnc + tmp*10^(id.mnc_size-1-m);
            out_idx = out_idx + 4;
        endfor
        id.crou              = msg(out_idx+1);
        out_idx              = out_idx + 1;
        plmn_id_list.id(n+1) = id;
    endfor
    tac              = cmn_bin2dec(msg(out_idx+1:out_idx+16), 16);
    out_idx          = out_idx + 16;
    cell_id          = cmn_bin2dec(msg(out_idx+1:out_idx+28), 28);
    out_idx          = out_idx + 28;
    cell_barred      = msg(out_idx+1);
    out_idx          = out_idx + 1;
    intra_freq_resel = msg(out_idx+1);
    out_idx          = out_idx + 1;
    csg_indication   = msg(out_idx+1);
    out_idx          = out_idx + 1;
    if(csg_id_opt == 1)
        csg_id  = cmn_bin2dec(msg(out_idx+1:out_idx+27), 27);
        out_idx = out_idx + 27;
    endif

    cari_s.plmn_id_list = plmn_id_list;
    cari_s.tac          = tac;
    cari_s.cell_id      = cell_id;
    cari_s.cell_barred  = cell_barred;
endfunction

function [out_idx, csi_s] = cell_selection_info_unpack(msg, in_idx)
    out_idx = in_idx;

    q_rx_lev_min_offset_opt = msg(out_idx+1);
    out_idx                 = out_idx + 1;
    q_rx_lev_min            = (cmn_bin2dec(msg(out_idx+1:out_idx+6), 6) - 70)*2;
    out_idx                 = out_idx + 6;
    if(q_rx_lev_min_offset_opt == 1)
        q_rx_lev_min_offset = (cmn_bin2dec(msg(out_idx+1:out_idx+3), 3) + 1)*2;
        out_idx             = out_idx + 3;
    else
        q_rx_lev_min_offset = 0;
    endif

    csi_s.q_rx_lev_min = q_rx_lev_min;
endfunction

function [out_idx, pm_s] = power_max_unpack(msg, in_idx, opt)
    out_idx = in_idx;

    pm_s.opt = opt;
    if(opt == 1)
        pm_s.p_max = cmn_bin2dec(msg(out_idx+1:out_idx+6), 6) - 30;
        out_idx    = out_idx + 6;
    endif
endfunction

function [out_idx, fbi_s] = frequency_band_indicator_unpack(msg, in_idx)
    out_idx = in_idx;

    fbi_s.f_band_ind = cmn_bin2dec(msg(out_idx+1:out_idx+6), 6) + 1;
    out_idx          = out_idx + 6;
endfunction

function [out_idx, sil_s] = scheduling_info_list_unpack(msg, in_idx)
    out_idx = in_idx;

    sched_info_list.size = cmn_bin2dec(msg(out_idx+1:out_idx+5), 5) + 1;
    out_idx              = out_idx + 5;
    for(n=0:sched_info_list.size-1)
        tmp.si_periodicity = 2^(cmn_bin2dec(msg(out_idx+1:out_idx+3), 3)+3);
        out_idx            = out_idx + 3;
        tmp.sib_map_size   = cmn_bin2dec(msg(out_idx+1:out_idx+5), 5);
        out_idx            = out_idx + 5;
        for(m=0:tmp.sib_map_size-1)
            tmp.sib_map(m+1).sib_type_ext = msg(out_idx+1);
            out_idx                       = out_idx + 1;
            tmp.sib_map(m+1).sib_type     = cmn_bin2dec(msg(out_idx+1:out_idx+4), 4);
            out_idx                       = out_idx + 4;
        endfor
        sched_info_list.sched_info(n+1) = tmp;
    endfor
    sil_s.sched_info_list = sched_info_list;
endfunction

function [out_idx, tddc_s] = tdd_config_unpack(msg, in_idx, opt)
    out_idx = in_idx;

    if(opt == 1)
        tddc_s.sf_assign           = cmn_bin2dec(msg(out_idx+1:out_idx+3), 3);
        out_idx                    = out_idx + 3;
        tddc_s.special_sf_patterns = cmn_bin2dec(msg(out_idx+1:out_idx+4), 4);
        out_idx                    = out_idx + 4;
    endif
    tddc_s.opt = opt;
endfunction

function [out_idx, siwl_s] = si_window_length_unpack(msg, in_idx)
    out_idx = in_idx;

    si_win_len = cmn_bin2dec(msg(out_idx+1:out_idx+3), 3);
    out_idx    = out_idx + 3;
    if(si_win_len == 0)
        si_window_len = 1;
    elseif(si_win_len == 1)
        si_window_len = 2;
    elseif(si_win_len == 2)
        si_window_len = 5;
    elseif(si_win_len == 3)
        si_window_len = 10;
    elseif(si_win_len == 4)
        si_window_len = 20;
    else
        si_window_len = 40;
    endif
    siwl_s.si_window_len = si_window_len;
endfunction

function [out_idx, sivt_s] = system_info_value_tag_unpack(msg, in_idx)
    out_idx = in_idx;

    si_value_tag = cmn_bin2dec(msg(out_idx+1:out_idx+5), 5);
    out_idx      = out_idx + 5;

    sivt_s.si_value_tag = si_value_tag;
endfunction

function [out_idx, nce_s] = non_critical_extension_unpack(msg, in_idx, opt)
    out_idx = in_idx;

    nce_s.opt = opt;
    if(opt == 1)
        nce_s.non_crit_ext = msg(out_idx+1:end);
        out_idx            = out_idx + length(msg(out_idx+1:end));
    endif
endfunction
