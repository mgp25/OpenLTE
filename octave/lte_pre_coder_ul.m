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
% Function:    lte_pre_coder_ul
% Description: Generates a block of vectors to be mapped onto
%              resources on each uplink antenna port
% Inputs:      x     - Block of input vectors
%              N_ant - Number of antennas
% Outputs:     y - Block of output vectors
% Spec:        3GPP TS 36.211 section 5.3.3A v10.1.0
% Notes:       Only supports single antenna.
% Rev History: Ben Wojtowicz 03/26/2014 Created
%
function [y] = lte_pre_coder_ul(x, N_ant)

    [v, M_layer_symb] = size(x);

    if(N_ant == 1 && v == 1)
        % 36.211 Section 5.3.3A.1 v10.1.0
        M_ap_symb = M_layer_symb;
        for(n=0:M_ap_symb-1)
            y(n+1) = x(n+1);
        endfor
    else
        printf("ERROR: Number of antenna ports %u not supported\n", N_ant);
        y = 0;
    endif
endfunction
