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
% Function:    lte_code_block_concatenation
% Description: Performs code block concatenation for turbo coded channels
% Inputs:      in_bits  - Code block bits to be concatenated
% Outputs:     out_bits - Code block concatenated bits
% Spec:        3GPP TS 36.212 section 5.1.5 v10.1.0
% Notes:       None
% Rev History: Ben Wojtowicz 01/09/2012 Created
%
function [out_bits] = lte_code_block_concatenation(in_bits)
    [C, in_bits_len] = size(in_bits);

    % Concatenate code blocks
    k_idx = 0;
    r_idx = 0;
    while(r_idx < C)
        j_idx = 0;
        while(j_idx < in_bits_len)
            out_bits(k_idx+1) = in_bits(r_idx+1,j_idx+1);
            k_idx             = k_idx + 1;
            j_idx             = j_idx + 1;
        endwhile
        r_idx = r_idx + 1;
    endwhile
endfunction
