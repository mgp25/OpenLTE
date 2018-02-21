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
% Function:    lte_generate_crs
% Description: Generates LTE cell specific reference signals
% Inputs:      N_s       - Slot number withing a radio frame
%              L         - OFDM symbol number within the slot
%              N_id_cell - Physical layer cell identity
% Outputs:     r         - Reference signal for this cell
% Spec:        3GPP TS 36.211 section 6.10.1.1 v10.1.0
% Notes:       Currently only handles normal CP
% Rev History: Ben Wojtowicz 10/28/2011 Created
%              Ben Wojtowicz 01/29/2012 Fixed license statement
%
function [r] = lte_generate_crs(N_s, L, N_id_cell)

    % Validate N_s
    if(~(N_s >= 0 && N_s <= 19))
        printf("ERROR: Invalid N_s (%u)\n", N_s);
        r = 0;
        return;
    endif

    % Validate L
    if(~(L >= 0 && L <= 6))
        printf("ERROR: Invalid L (%u)\n", L);
        r = 0;
        return;
    endif

    % Validate N_id_cell
    if(~(N_id_cell >= 0 && N_id_cell <= 503))
        printf("ERROR: Invalid N_id_cell (%u)\n", N_id_cell);
        r = 0;
        return;
    endif

    % Calculate c_init and sequence length
    N_cp        = 1;
    N_rb_dl_max = 110;
    c_init      = 1024 * (7 * (N_s+1) + L + 1) * (2 * N_id_cell + 1) + 2*N_id_cell + N_cp;
    len         = 2*N_rb_dl_max;

    % Generate the psuedo random sequence c
    c = lte_generate_prs_c(c_init, 2*(len-1)+1+1);

    % Construct r
    r = zeros(1,len);
    for(m=0:len-1)
        r(m+1) = (1/sqrt(2))*(1 - 2*c(2*m+1)) + j*(1/sqrt(2))*(1 - 2*c(2*m+1+1));
    endfor
endfunction
