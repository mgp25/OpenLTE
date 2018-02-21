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
% Function:    lte_bch_channel_encode
% Description: Channel encodes the broadcast channel
% Inputs:      mib      - Master information block bits
%              N_ant    - Number of antenna ports to use
% Outputs:     bch_bits - Broadcast channel encoded
%                         bits
% Spec:        3GPP TS 36.212 section 5.3.1 v10.1.0
% Notes:       None
% Rev History: Ben Wojtowicz 10/30/2011 Created
%              Ben Wojtowicz 01/29/2012 Fixed license statement
%              Ben Wojtowicz 02/19/2012 Commonized rate matcher
%
function [bch_bits] = lte_bch_channel_encode(mib, N_ant)
    % Check mib
    if(length(mib) ~= 24)
        printf("ERROR: Invalid mib (length is %u, should be 24)\n", length(mib));
        bch_bits = 0;
        return;
    endif

    % Check N_ant
    if(N_ant == 1)
        ant_mask = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
    elseif(N_ant == 2)
        ant_mask = [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1];
    elseif(N_ant == 4)
        ant_mask = [0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1];
    else
        printf("ERROR: Invalid N_ant (%u)\n", N_ant);
        bch_bits = 0;
        return;
    endif

    % Define a_bits
    a_bits = mib;

    % Calculate p_bits
    p_bits = lte_calc_crc(a_bits, 16);

    % Mask p_bits
    for(n=0:length(p_bits)-1)
        p_bits(n+1) = mod(p_bits(n+1) + ant_mask(n+1), 2);
    endfor

    % Construct c_bits
    c_bits = [a_bits, p_bits];

    % Determine d_bits
    d_bits = cmn_conv_encode(c_bits, 7, 3, [133, 171, 165], 1);
    d_bits = reshape(d_bits, 3, []);

    % Determine e_bits
    e_bits = lte_rate_match_conv(d_bits, 1920);

    % Return the e_bits
    bch_bits = e_bits;
endfunction
