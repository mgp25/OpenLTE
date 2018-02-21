%
% Copyright 2012, 2014 Ben Wojtowicz
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
% Function:    lte_rate_unmatch_turbo
% Description: Rate unmatches turbo encoded data
% Inputs:      in_bits   - Input bits to rate unmatcher
%              C         - Number of code blocks
%              dummy_out - Dummy encoded bits (all zeros, plus dummy)
%              tx_mode   - Transmission mode used
%              N_soft    - Number of soft bits
%              M_dl_harq - Maximum number of DL HARQ processes
%              chan_type - "dlsch", "ulsch", "_pch_", or "_mch_"
%              rv_idx    - Redundancy version number
% Outputs:     out_bits - Output bits from the rate unmatcher
% Spec:        3GPP TS 36.212 section 5.1.4.1 v10.1.0
% Notes:       None
% Rev History: Ben Wojtowicz 01/10/2012 Created
%              Ben Wojtowicz 03/26/2014 Made all the chan_type values the same
%                                       length and moved all of the N_cb calculations
%                                       to dlsch and _pch_ chan_type values.
%
function [out_bits] = lte_rate_unmatch_turbo(in_bits, C, dummy_out, tx_mode, N_soft, M_dl_harq, chan_type, rv_idx)
    % Check dummy_out
    [this_is_three, dummy_out_len] = size(dummy_out);
    if(this_is_three ~= 3)
        printf("ERROR: Invalid dimension for dummy_out (%u should be 3)\n", this_is_three);
        out_bits = 0;
        return;
    endif

    % In order to undo bit collection, selection, and transmission,
    % a dummy block must be sub block interleaved to determine
    % where NULL bits are to be inserted
    % Sub block interleaving
    % Step 1: Assign C_tc_sb to 32
    C_tc_sb = 32;

    % Step 2: Determine the number of rows
    R_tc_sb = 0;
    while(dummy_out_len > (C_tc_sb*R_tc_sb))
        R_tc_sb = R_tc_sb + 1;
    endwhile

    % Inter-column permutation values
    ic_perm = [0,16,8,24,4,20,12,28,2,18,10,26,6,22,14,30,1,17,9,25,5,21,13,29,3,19,11,27,7,23,15,31];

    % Steps 3, 4, and 5
    for(x=0:this_is_three-1)
        % Step 3: Pack data into matrix and pad with dummy (NULL=10000 for this routine)
        if(dummy_out_len < (C_tc_sb*R_tc_sb))
            N_dummy = C_tc_sb*R_tc_sb - dummy_out_len;
        else
            N_dummy = 0;
        endif
        tmp = [10000*ones(1,N_dummy), dummy_out(x+1,:)];
        idx = 0;
        for(n=0:R_tc_sb-1)
            for(m=0:C_tc_sb-1)
                sb_mat(n+1,m+1) = tmp(idx+1);
                idx             = idx + 1;
            endfor
        endfor

        if(x == 0 || x == 1)
            % Step 4: Inter-column permutation
            for(n=0:R_tc_sb-1)
                for(m=0:C_tc_sb-1)
                    sb_perm_mat(n+1,m+1) = sb_mat(n+1,ic_perm(m+1)+1);
                endfor
            endfor

            % Step 5: Read out the bits
            idx = 0;
            for(m=0:C_tc_sb-1)
                for(n=0:R_tc_sb-1)
                    v(x+1,idx+1) = sb_perm_mat(n+1,m+1);
                    idx          = idx + 1;
                endfor
            endfor
            K_pi = R_tc_sb*C_tc_sb;
        else
            % Step 4: Permutation for the last output
            K_pi = R_tc_sb*C_tc_sb;
            idx  = 0;
            for(n=0:R_tc_sb-1)
                for(m=0:C_tc_sb-1)
                    y_array(idx+1) = sb_mat(n+1,m+1);
                    idx            = idx + 1;
                endfor
            endfor
            for(n=0:K_pi-1)
                pi_idx     = mod(ic_perm(floor(n/R_tc_sb)+1)+C_tc_sb*mod(n,R_tc_sb)+1,K_pi);
                v(x+1,n+1) = y_array(pi_idx+1);
            endfor
        endif
    endfor

    % Undo bit collection, selection, and transmission by
    % recreating the circular buffer
    for(k=0:K_pi-1)
        w_dum(k+1)          = v(1,k+1);
        w_dum(K_pi+2*k+0+1) = v(2,k+1);
        w_dum(K_pi+2*k+1+1) = v(3,k+1);
    endfor
    K_w     = 3*K_pi;
    w       = 10000*ones(1,K_w);
    if(chan_type == "dlsch" || chan_type == "_pch_")
        if(tx_mode == 3 || tx_mode == 4 || tx_mode == 8 || tx_mode == 9)
            K_mimo = 2;
        else
            K_mimo = 1;
        endif
        M_limit = 8;
        N_ir    = floor(N_soft/(K_mimo*min(M_dl_harq,M_limit)));
        N_cb    = min(floor(N_ir/C), K_w);
    else
        N_cb = K_w;
    endif
    k_0   = R_tc_sb*(2*ceil(N_cb/(8*R_tc_sb))*rv_idx+2);
    k_idx = 0;
    j_idx = 0;
    while(k_idx < length(in_bits))
        if(w_dum(mod(k_0+j_idx, N_cb)+1) != 10000)
            % Soft combine the inputs
            if(w(mod(k_0+j_idx, N_cb)+1) == 10000)
                w(mod(k_0+j_idx, N_cb)+1) = in_bits(k_idx+1);
            elseif(in_bits(k_idx+1) != 10000)
                w(mod(k_0+j_idx, N_cb)+1) = w(mod(k_0+j_idx, N_cb)+1) + in_bits(k_idx+1);
            endif
            k_idx = k_idx + 1;
        endif
        j_idx = j_idx + 1;
    endwhile

    % Recreate the sub block interleaver output
    for(k=0:K_pi-1)
        v(1,k+1) = w(k+1);
        v(2,k+1) = w(K_pi+2*k+0+1);
        v(3,k+1) = w(K_pi+2*k+1+1);
    endfor

    % Sub block deinterleaving
    % Steps 5, 4, and 3
    for(x=0:3-1)
        if(x == 0 || x == 1)
            % Step 5: Load the permuted matrix
            idx = 0;
            for(m=0:C_tc_sb-1)
                for(n=0:R_tc_sb-1)
                    sb_perm_mat(n+1,m+1) = v(x+1,idx+1);
                    idx                  = idx + 1;
                endfor
            endfor

            % Step 4: Undo permutation
            for(n=0:R_tc_sb-1)
                for(m=0:C_tc_sb-1)
                    sb_mat(n+1,ic_perm(m+1)+1) = sb_perm_mat(n+1,m+1);
                endfor
            endfor
        else
            % Step 4: Permutation for the last output
            for(n=0:K_pi-1)
                pi_idx            = mod(ic_perm(floor(n/R_tc_sb)+1)+C_tc_sb*mod(n,R_tc_sb)+1,K_pi);
                y_array(pi_idx+1) = v(x+1,n+1);
            endfor
            idx = 0;
            for(n=0:R_tc_sb-1)
                for(m=0:C_tc_sb-1)
                    sb_mat(n+1,m+1) = y_array(idx+1);
                    idx             = idx + 1;
                endfor
            endfor
        endif

        % Step 3: Unpack the data and remove dummy
        if(dummy_out_len < (C_tc_sb*R_tc_sb))
            N_dummy = C_tc_sb*R_tc_sb - dummy_out_len;
        else
            N_dummy = 0;
        endif
        idx = 0;
        for(n=0:R_tc_sb-1)
            for(m=0:C_tc_sb-1)
                tmp(idx+1) = sb_mat(n+1,m+1);
                idx        = idx + 1;
            endfor
        endfor
        out_bits(x+1,:) = tmp(N_dummy+1:end);
    endfor
endfunction
