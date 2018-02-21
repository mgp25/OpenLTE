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
% Function:    lte_layer_mapper_ul
% Description: Maps complex-valued modulation symbols onto one
%              or several uplink layers
% Inputs:      d     - Complex valued modulation symbols
%              N_ant - Number of antennas
% Outputs:     x - Mapped complex valued modulation symbols
% Spec:        3GPP TS 36.211 section 5.3.2A v10.1.0
% Notes:       Only supports single antenna.
% Rev History: Ben Wojtowicz 03/26/2014 Created
%
function [x] = lte_layer_mapper_ul(d, N_ant)

    [N_cw, M_symb] = size(d);

    if(N_ant == 1 && N_cw == 1)
        % 36.211 Section 5.3.2A.1 and 5.3.2A.2 v10.1.0
        M_layer_symb = M_symb;
        for(n=0:M_layer_symb-1)
            x(0+1, n+1) = d(0+1, n+1);
        endfor
    else
        % FIXME
    endif
endfunction
