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
% Function:    lte_ulsch_channel_decode
% Description: Channel decodes the uplink shared channel
% Inputs:      ulsch_bits - Uplink shared channel encoded bits
%              tbs        - Transport block size
%              N_l        - Number of layers used
%              Q_m        - Modulation order used
%              tx_mode    - Transmission mode used
% Outputs:     ulsch_msg - Uplink shared channel message bits
% Spec:        3GPP TS 36.212 section 5.2.2 v10.1.0
% Rev History: Ben Wojtowicz 03/26/2014 Created
%
function [ulsch_msg] = lte_ulsch_channel_decode(ulsch_bits, tbs, N_l, Q_m, tx_mode)
    % In order to decode an ULSCH message, the NULL bit pattern must be
    % determined by encoding a sequence of zeros.
    dummy_b_bits   = zeros(1,tbs+24);
    dummy_c_bits_s = lte_code_block_segmentation(dummy_b_bits);

    % Decode the ULSCH message
    % Define h_bits
    h_bits = ulsch_bits;

    % FIXME: Not handling control information at this point
    Q_prime_RI  = 0;
    Q_prime_ACK = 0;
    Q_CQI       = 0;

    % Channel de-interleaver
    [g_bits, q_ri_bits, q_ack_bits] = lte_ulsch_channel_deinterleaver(h_bits, Q_prime_RI, Q_prime_ACK, N_l, Q_m);

    % De-multiplexing of data and control
    [f_bits, q_cqi_bits] = lte_ulsch_control_data_demultiplexing(g_bits, Q_CQI, N_l, Q_m);

    % Determine e_bits
    e_bits          = lte_code_block_deconcatenation(f_bits, tbs);
    [C, e_bits_len] = size(e_bits);

    for(cb=0:C-1)
        % Construct dummy_d_bits
        dummy_d_bits = lte_turbo_encode(dummy_c_bits_s.cb(cb+1).bits, dummy_c_bits_s.F);

        % Determine d_bits
        d_bits = lte_rate_unmatch_turbo(e_bits, C, dummy_d_bits, tx_mode, 1, 1, "ulsch", 0);

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
        ulsch_msg = a_bits;
    else
        ulsch_msg = 0;
    endif
endfunction
