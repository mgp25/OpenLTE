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
% Function:    lte_generate_pss
% Description: Generates an LTE primary synchronization signal
% Inputs:      N_id_2  - Physical layer identity
% Outputs:     pss_d_u - The sequence d(n) used for the primary
%                        synchronization signal, generated from
%                        a frequency domain Zadoff-Chu sequence.
% Spec:        3GPP TS 36.211 section 6.11.1.1 v10.1.0
% Notes:       None
% Rev History: Ben Wojtowicz 10/28/2011 Created
%              Ben Wojtowicz 01/29/2012 Fixed license statement
%              Ben Wojtowicz 02/19/2012 Removed newline at EOF
%
function [pss_d_u] = lte_generate_pss(N_id_2)
    % Validate N_id_2 and get the root index
    if(N_id_2 == 0)
        root_idx = 25;
    elseif(N_id_2 == 1)
        root_idx = 29;
    elseif(N_id_2 == 2)
        root_idx = 34;
    else
        printf("ERROR: Invalid N_id_2 (%u)\n", N_id_2);
        pss_d_u = 0;
        return;
    endif

    % Generate PSS for n=0,1,...,30
    for(n=0:30)
        pss_d_u(n+1) = cos(-pi*root_idx*n*(n+1)/63) + j*sin(-pi*root_idx*n*(n+1)/63);
    endfor

    % Generate PSS for n=31,32,...,61
    for(n=31:61)
        pss_d_u(n+1) = cos(-pi*root_idx*(n+1)*(n+2)/63) + j*sin(-pi*root_idx*(n+1)*(n+2)/63);
    endfor
endfunction
