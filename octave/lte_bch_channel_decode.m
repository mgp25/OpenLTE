%
% Copyright 2011-2012 Ben Wojtowicz
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
% Function:    lte_bch_channel_decode
% Description: Channel decodes the broadcast channel
% Inputs:      bch_bits - Broadcast channel encoded
%                         bits
% Outputs:     mib      - Master information block bits
%              N_ant    - Number of antenna ports used
% Spec:        3GPP TS 36.212 section 5.3.1 v10.1.0
% Notes:       None
% Rev History: Ben Wojtowicz 11/12/2011 Created
%              Ben Wojtowicz 01/29/2012 Fixed license statement
%              Ben Wojtowicz 02/19/2012 Commonized rate unmatcher and
%                                       removed conversion to hard bits
%
function [mib, N_ant] = lte_bch_channel_decode(bch_bits)
    % Rate unmatch to get the d_bits
    d_bits = lte_rate_unmatch_conv(bch_bits, 40);
    d_bits = reshape(d_bits, 1, []);

    % Viterbi decode the d_bits to get the c_bits
    c_bits = cmn_viterbi_decode(d_bits, 7, 3, [133, 171, 165]);

    % Recover a_bits and p_bits
    a_bits = c_bits(1:24);
    p_bits = c_bits(25:40);

    % Calculate p_bits
    calc_p_bits = lte_calc_crc(a_bits, 16);

    % Try all p_bit masks
    ant_mask_1 = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
    ant_mask_2 = [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1];
    ant_mask_4 = [0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1];
    check_1    = 0;
    check_2    = 0;
    check_4    = 0;
    for(n=0:length(calc_p_bits)-1)
        bit     = mod(calc_p_bits(n+1) + ant_mask_1(n+1), 2);
        check_1 = check_1 + abs(bit-p_bits(n+1));
        bit     = mod(calc_p_bits(n+1) + ant_mask_2(n+1), 2);
        check_2 = check_2 + abs(bit-p_bits(n+1));
        bit     = mod(calc_p_bits(n+1) + ant_mask_4(n+1), 2);
        check_4 = check_4 + abs(bit-p_bits(n+1));
    endfor
    if(check_1 == 0)
        N_ant = 1;
        mib   = a_bits;
    elseif(check_2 == 0)
        N_ant = 2;
        mib   = a_bits;
    elseif(check_4 == 0)
        N_ant = 4;
        mib   = a_bits;
    else
        N_ant = 0;
        mib   = 0;
    endif
endfunction
