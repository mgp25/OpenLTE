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
% Function:    lte_dci_channel_encode
% Description: Channel encodes the Downlink Control Information
% Inputs:      dci      - Downlink Control Information bits
%              rnti     - Radio Network Temporary Identifier
%              ue_ant   - UE transmit antenna
% Outputs:     dci_bits - Downlink Control Information channel encoded
%                         bits
% Spec:        3GPP TS 36.212 section 5.3.3 v10.1.0
% Notes:       None
% Rev History: Ben Wojtowicz 01/02/2012 Created
%
function [dci_bits] = lte_dci_channel_encode(dci, rnti, ue_ant)
    % Check rnti
    x_rnti_bits = cmn_dec2bin(rnti, 16);
    if(length(x_rnti_bits) > 16)
        printf("ERROR: Invalid RNTI (length is %u)\n", length(x_rnti_bits));
        dci_bits = 0;
        return;
    elseif(length(x_rnti_bits) < 16)
        x_rnti_bits = [zeros(1,16-length(x_rnti_bits)), x_rnti_bits];
    endif

    % Check ue_ant
    if(ue_ant == 0)
        x_as_bits = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
    elseif(ue_ant == 1)
        x_as_bits = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1];
    else
        printf("ERROR: Invalid ue_ant %u\n", ue_ant);
        dci_bits = 0;
        return;
    endif

    % Define a_bits
    a_bits = dci;

    % Calculate p_bits
    p_bits = lte_calc_crc(a_bits, 16);

    % Mask p_bits
    for(n=0:length(p_bits)-1)
        p_bits(n+1) = mod(p_bits(n+1) + x_rnti_bits(n+1) + x_as_bits(n+1), 2);
    endfor

    % Construct c_bits
    c_bits = [a_bits, p_bits];

    % Determine d_bits
    d_bits = cmn_conv_encode(c_bits, 7, 3, [133, 171, 165], 1);
    d_bits = reshape(d_bits, 3, []);

    % Determine e_bits
    % Always use 576 as the number of bits can be reduced later
    e_bits = lte_rate_match_conv(d_bits, 576);

    % Return the e_bits
    dci_bits = e_bits;
endfunction
