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
% Function:    lte_dci_channel_decode
% Description: Channel decodes the Downlink Control Information
% Inputs:      dci_bits - Downlink Control Information channel encoded
%                         bits
%              rnti     - Radio Network Temporary Identifier
%              ue_ant   - UE transmit antenna
%              dci_size - Size of the expected DCI in bits
% Outputs:     dci      - Downlink Control Information bits
% Spec:        3GPP TS 36.212 section 5.3.3 v10.1.0
% Notes:       None
% Rev History: Ben Wojtowicz 01/02/2012 Created
%
function [dci] = lte_dci_channel_decode(dci_bits, rnti, ue_ant, dci_size)
    % Check rnti
    x_rnti_bits = cmn_dec2bin(rnti, 16);

    % Check ue_ant
    if(ue_ant == 0)
        x_as_bits = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
    elseif(ue_ant == 1)
        x_as_bits = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1];
    else
        printf("ERROR: Invalid ue_ant %u\n", ue_ant);
        dci = 0;
        return;
    endif

    % Check dci_size
    if(dci_size > length(dci_bits))
        printf("ERROR: Not enough bits (%u) for dci_size (%u)\n", length(dci_bits), dci_size);
        dci = 0;
        return;
    endif

    % Rate unmatch to get the d_bits
    d_bits = lte_rate_unmatch_conv(dci_bits, dci_size+16);
    d_bits = reshape(d_bits, 1, []);

    % Viterbi decode the d_bits to get the c_bits
    c_bits = cmn_viterbi_decode(d_bits, 7, 3, [133, 171, 165]);

    % Recover a_bits and p_bits
    a_bits = c_bits(1:dci_size);
    p_bits = c_bits(dci_size+1:end);

    % Calculate p_bits
    calc_p_bits = lte_calc_crc(a_bits, 16);

    % Mask p_bits
    for(n=0:length(calc_p_bits)-1)
        calc_p_bits(n+1) = mod(calc_p_bits(n+1) + x_rnti_bits(n+1) + x_as_bits(n+1), 2);
    endfor

    % Check CRC
    ber = 0;
    for(n=0:length(p_bits)-1)
        if(p_bits(n+1) ~= calc_p_bits(n+1))
            ber = ber + 1;
        endif
    endfor
    if(ber == 0)
        dci = a_bits;
    else
        dci = 0;
    endif
endfunction
