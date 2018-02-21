%
% Copyright 2014 Ben Wojtowicz
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
% Function:    lte_ulsch_channel_encode
% Description: Channel encodes the uplink shared channel
% Inputs:      ulsch_msg - Uplink shared channel bits
%              tx_mode   - Transmission mode used
%              N_prb     - Number of PRBs assigned to the UE
%              N_l       - Number of layers
%              Q_m       - Number of bits per modulation symbol
% Outputs:     ulsch_bits - Uplink shared channel encoded bits
% Spec:        3GPP TS 36.212 section 5.2.2 v10.1.0
% Rev History: Ben Wojtowicz 03/26/2014 Created
%
function [ulsch_bits] = lte_ulsch_channel_encode(ulsch_msg, tx_mode, N_prb, N_l, Q_m)
    % Definitions
    N_rb_sc   = 12;
    N_ul_symb = 7;

    % Define a_bits
    a_bits = ulsch_msg;

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
        G       = N_prb*N_rb_sc*(N_ul_symb-1)*2*Q_m*N_l;
        G_prime = G/(N_l*Q_m);
        lambda  = mod(G_prime, c_bits_s.num_cb);
        if(cb <= c_bits_s.num_cb - lambda - 1)
            E = N_l*Q_m*floor(G_prime/c_bits_s.num_cb);
        else
            E = N_l*Q_m*ceil(G_prime/c_bits_s.num_cb);
        endif
        e_bits = lte_rate_match_turbo(d_bits, E, c_bits_s.num_cb, tx_mode, 1, 1, "ulsch", 0);
    endfor

    % Determine f_bits
    f_bits = lte_code_block_concatenation(e_bits);

    % FIXME: Not handling control information
    q_cqi_bits  = 0;
    q_ri_bits   = 0;
    q_ack_bits  = 0;
    Q_CQI       = 0;
    Q_prime_RI  = 0;
    Q_prime_ACK = 0;

    % Data and control multiplexing
    g_bits = lte_ulsch_control_data_multiplexing(f_bits, q_cqi_bits, Q_CQI, N_l, Q_m);

    % Channel interleaver
    h_bits = lte_ulsch_channel_interleaver(g_bits, q_ri_bits, q_ack_bits, N_l, Q_m);

    % Return h_bits
    ulsch_bits = h_bits;
endfunction
