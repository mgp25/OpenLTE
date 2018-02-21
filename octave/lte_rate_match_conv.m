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
% Function:    lte_rate_match_conv
% Description: Rate matches convolutionally encoded data
% Inputs:      in_bits  - Input bits to rate matcher
%              E        - Rate matching output sequence length
% Outputs:     out_bits - Output bits from the rate matcher
% Spec:        3GPP TS 36.212 section 5.1.4.2 v10.1.0
% Notes:       None
% Rev History: Ben Wojtowicz 01/02/2012 Created
%
function [out_bits] = lte_rate_match_conv(in_bits, E)
    % Check in bits
    [this_is_three, in_bits_len] = size(in_bits);
    if(this_is_three ~= 3)
        printf("ERROR: Invalid dimension for in_bits (%u should be 3)\n", this_is_three);
        out_bits = 0;
        return;
    endif

    % Sub block intereaving
    % Step 1: Assign C_cc_sb to 32
    C_cc_sb = 32;

    % Step 2: Determine the number of rows
    R_cc_sb = 0;
    while(in_bits_len > (C_cc_sb*R_cc_sb))
        R_cc_sb = R_cc_sb + 1;
    endwhile

    % Inter-column permutation values
    ic_perm = [1,17,9,25,5,21,13,29,3,19,11,27,7,23,15,31,0,16,8,24,4,20,12,28,2,18,10,26,6,22,14,30];

    % Steps 3, 4, and 5
    for(x=0:this_is_three-1)
        % Step 3: Pack data into matrix and pad with dummy (NULL==10000 for this routine)
        if(in_bits_len < (C_cc_sb*R_cc_sb))
            N_dummy = C_cc_sb*R_cc_sb - in_bits_len;
        else
            N_dummy = 0;
        endif
        tmp = [10000*ones(1, N_dummy), in_bits(x+1,:)];
        idx = 0;
        for(n=0:R_cc_sb-1)
            for(m=0:C_cc_sb-1)
                sb_mat(n+1,m+1) = tmp(idx+1);
                idx             = idx + 1;
            endfor
        endfor

        % Step 4: Inter-column permutation
        for(n=0:R_cc_sb-1)
            for(m=0:C_cc_sb-1)
                sb_perm_mat(n+1,m+1) = sb_mat(n+1,ic_perm(m+1)+1);
            endfor
        endfor

        % Step 5: Read out the bits
        idx = 0;
        for(m=0:C_cc_sb-1)
            for(n=0:R_cc_sb-1)
                v(x+1,idx+1) = sb_perm_mat(n+1,m+1);
                idx          = idx + 1;
            endfor
        endfor
    endfor
    K_pi = R_cc_sb*C_cc_sb;

    % Bit collection, selection, and transmission
    % Create circular buffer
    w   = [v(1,:), v(2,:), v(3,:)];
    K_w = 3*K_pi;
    % Create output
    k_idx = 0;
    j_idx = 0;
    while(k_idx < E)
        if(w(mod(j_idx, K_w)+1) ~= 10000)
            out_bits(k_idx+1) = w(mod(j_idx, K_w)+1);
            k_idx             = k_idx + 1;
        endif
        j_idx = j_idx + 1;
    endwhile
endfunction
