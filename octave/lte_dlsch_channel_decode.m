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
% Function:    lte_dlsch_channel_decode
% Description: Channel decodes the downlink shared channel.
% Inputs:      dlsch_bits - Downlink shared channel encoded bits
%              tbs        - Transport block size
%              tx_mode    - Tranmission mode used
%              rv_idx     - Redundancy version number
%              M_dl_harq  - Maximum number of DL HARQ prcesses
%              N_soft     - Number of soft bits
% Outputs:     dlsch_msg  - Downlink shared channel bits
% Spec:        3GPP TS 36.212 section 5.3.2 v10.1.0
% Notes:       None
% Rev History: Ben Wojtowicz 02/19/2012 Created
%
function [dlsch_msg] = lte_dlsch_channel_decode(dlsch_bits, tbs, tx_mode, rv_idx, M_dl_harq, N_soft)
    % In order to decode a DLSCH message, the NULL bit pattern must be
    % determined by encoding a sequence of zeros.
    dummy_b_bits   = zeros(1,tbs+24);
    dummy_c_bits_s = lte_code_block_segmentation(dummy_b_bits);

    % Decode the DLSCH message
    % Define f_bits
    f_bits = dlsch_bits;

    % Determine e_bits
    e_bits          = lte_code_block_deconcatenation(f_bits, tbs);
    [C, e_bits_len] = size(e_bits);

    for(cb=0:C-1)
        % Construct dummy_d_bits
        dummy_d_bits = lte_turbo_encode(dummy_c_bits_s.cb(cb+1).bits, dummy_c_bits_s.F);

        % Determine d_bits
        d_bits = lte_rate_unmatch_turbo(e_bits, C, dummy_d_bits, tx_mode, N_soft, M_dl_harq, "dlsch", rv_idx);

        % Determine c_bits
        c_bits = lte_turbo_decode(d_bits, dummy_c_bits_s.F);
    endfor

    % Determine b_bits
    b_bits = lte_code_block_desegmentation(c_bits, tbs);

    % Recover a_bits and p_bits
    a_bits = b_bits(1:tbs);
    p_bits = b_bits(tbs+1:tbs+24);

    % Calculate p_bits
    calc_p_bits = lte_calc_crc(a_bits, "24A");

    % Check CRC
    ber = 0;
    for(n=0:length(p_bits)-1)
        if(p_bits(n+1) ~= calc_p_bits(n+1))
            ber = ber + 1;
        endif
    endfor
    if(ber == 0)
        dlsch_msg = a_bits;
    else
        dlsch_msg = 0;
    endif
endfunction
