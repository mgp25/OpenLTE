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
% Function:    lte_pre_decoder_and_matched_filter_ul
% Description: Matched filters and unmaps a block of uplink vectors
%              from resources on each antenna port
% Inputs:      y - Block of input vectors
%              h - Channel estimate
%              v - Number of layers
% Outputs:     x - Block of output vectors
% Spec:        3GPP TS 36.211 section 5.3.3A v10.1.0
% Notes:       Only supports single antenna.
% Rev History: Ben Wojtowicz 03/26/2014 Created
%
function [x] = lte_pre_decoder_and_matched_filter_ul(y, h, v)

    [N_ant, M_ap_symb] = size(h);

    if(N_ant == 1 && v == 1)
        % 36.211 Section 5.3.3A.1 v10.1.0
        M_layer_symb = M_ap_symb;
        for(n=0:M_ap_symb-1)
            x(n+1) = y(n+1)/h(n+1);
        endfor
    else
        printf("ERROR: Number of antenna ports %u not supported\n", N_ant);
        x = 0;
    endif
endfunction
