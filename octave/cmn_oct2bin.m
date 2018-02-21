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
% Function:    cmn_oct2bin
% Description: Converts an array of octal numbers
%              to an array of binary arrays
% Inputs:      oct      - Array of octal numbers
%              num_bits - Number of bits per octal
%                         number
% Outputs:     array    - Array of binary arrays
% Spec:        N/A
% Notes:       None
% Rev History: Ben Wojtowicz 11/22/2011 Created
%              Ben Wojtowicz 01/29/2012 Fixed license statement
%              Ben Wojtowicz 02/19/2012 Fixed mod(num_bits,3)==0 bug
%
function [array] = cmn_oct2bin(oct, num_bits)
    [junk, num_oct] = size(oct);

    for(n=1:num_oct)
        % Convert whole digits
        tmp = oct(n);
        idx = num_bits;
        for(m=1:floor(num_bits/3))
            dig                = mod(tmp, 10);
            array(n,idx-2:idx) = cmn_dec2bin(dig, 3);
            tmp                = floor(tmp/10);
            idx                = idx - 3;
        endfor

        if(mod(num_bits, 3) != 0)
            % Convert non-whole digits
            dig             = mod(tmp, 10);
            array(n, 1:idx) = cmn_dec2bin(dig, mod(num_bits, 3));
        endif
    endfor
endfunction
