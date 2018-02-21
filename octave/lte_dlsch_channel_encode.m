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
% Function:    lte_dlsch_channel_encode
% Description: Channel encodes the downlink shared channel
% Inputs:      dlsch_msg  - Downlink shared channel bits
%              tx_mode    - Transmission mode used
%              rv_idx     - Redundancy version number
%              G          - Number of bits available for transmission
%              N_l        - Number of layers (for tx_diversity N_l = 2)
%              Q_m        - Number of bits per modulation symbol
%              N_soft     - Number of soft bits
%              M_dl_harq  - Maximum number of DL HARQ processes
% Outputs:     dlsch_bits - Downlink shared channel encoded bits
% Spec:        3GPP TS 36.212 section 5.3.2 v10.1.0
% Notes:       None
% Rev History: Ben Wojtowicz 02/19/2012 Created
%
function [dlsch_bits] = lte_dlsch_channel_encode(dlsch_msg, tx_mode, rv_idx, G, N_l, Q_m, N_soft, M_dl_harq)
    % Define a_bits
    a_bits = dlsch_msg;

    % Calculate p_bits
    p_bits = lte_calc_crc(a_bits, "24A");

    % Construct b_bits
    b_bits = [a_bits, p_bits];

    % Determine c_bits
    c_bits_s = lte_code_block_segmentation(b_bits);

    for(cb=0:c_bits_s.num_cb-1)
        % Determine d_bits
        d_bits = lte_turbo_encode(c_bits_s.cb(cb+1).bits, c_bits_s.F);

        % Determine e_bits and number of e_bits, E
        G_prime = G/(N_l*Q_m);
        lambda  = mod(G_prime, c_bits_s.num_cb);
        if(cb <= c_bits_s.num_cb - lambda - 1)
            E = N_l*Q_m*floor(G_prime/c_bits_s.num_cb);
        else
            E = N_l*Q_m*ceil(G_prime/c_bits_s.num_cb);
        endif
        e_bits = lte_rate_match_turbo(d_bits, E, c_bits_s.num_cb, tx_mode, N_soft, M_dl_harq, "dlsch", rv_idx);
    endfor

    % Determine f_bits
    f_bits = lte_code_block_concatenation(e_bits);

    % Return f_bits
    dlsch_bits = f_bits;
endfunction
