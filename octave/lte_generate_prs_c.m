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
% Function:    lte_generate_prs_c
% Description: Generates the psuedo random sequence c
% Inputs:      c_init    - Initialization value for the
%                          sequence
%              seq_len   - Length of the output sequence
% Outputs:     c         - Psuedo random sequence
% Spec:        3GPP TS 36.211 section 7.2 v10.1.0
% Notes:       None
% Rev History: Ben Wojtowicz 10/28/2011 Created
%              Ben Wojtowicz 01/29/2012 Fixed license statement
%
function [c] = lte_generate_prs_c(c_init, seq_len)
    % Initialize the m-sequences
    x1  = zeros(1,1600+seq_len);
    x2  = zeros(1,1600+seq_len);
    tmp = c_init;
    for(n=0:30)
        x2(30-n+1) = floor(tmp/(2^(30-n)));
        tmp        = tmp - (floor(tmp/(2^(30-n)))*2^(30-n));
    endfor
    x1(0+1) = 1;

    % Advance m-sequences
    for(n=0:1600+seq_len)
        x1(n+31+1) = mod(x1(n+3+1) + x1(n+1), 2);
        x2(n+31+1) = mod(x2(n+3+1) + x2(n+2+1) + x2(n+1+1) + x2(n+1), 2);
    endfor

    % Generate c
    for(n=0:seq_len-1)
        c(n+1) = mod(x1(n+1600+1) + x2(n+1600+1), 2);
    endfor
endfunction
