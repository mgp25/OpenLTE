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
% Function:    lte_ulsch_control_data_multiplexing
% Description: Multiplexes the control and data bits for ULSCH.
% Inputs:      data_bits      - Data bits to multiplex
%              control_bits   - Control bits to multiplex
%              N_control_bits - Number of control bits to multiplex
%              N_l            - Number of layers
%              Q_m            - Number of bits per modulation symbol
% Outputs:     out_bits - Multiplexed bits
% Spec:        3GPP TS 36.212 section 5.2.2.7 v10.1.0
% Notes:       N/A
% Rev History: Ben Wojtowicz 03/26/2014 Created
%
function [out_bits] = lte_ulsch_control_data_multiplexing(data_bits, control_bits, N_control_bits, N_l, Q_m)

    % Set i, j, k to 0 (using n for i and m for j)
    n = 0;
    m = 0;
    k = 0;
    while(m < N_control_bits)
        out_bits(k+1,:) = control_bits(m+1:m+N_l*Q_m-1+1);
        m               = m + N_l*Q_m;
        k               = k + 1;
    endwhile
    while(n < length(data_bits))
        out_bits(k+1,:) = data_bits(n+1:n+N_l*Q_m-1+1);
        n               = n + N_l*Q_m;
        k               = k + 1;
    endwhile
endfunction
