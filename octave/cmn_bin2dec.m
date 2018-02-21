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
% Function:    cmn_bin2dec
% Description: Converts an array of binary arrays
%              to an array of decimal numbers
% Inputs:      array    - Array of binary arrays
%              num_bits - Number of bits per decimal
%                         number
% Outputs:     dec      - Array of decimal numbers
% Spec:        N/A
% Notes:       None
% Rev History: Ben Wojtowicz 11/22/2011 Created
%              Ben Wojtowicz 01/29/2012 Fixed license statement
%              Ben Wojtowicz 02/19/2012 Added newline to EOF
%
function [dec] = cmn_bin2dec(array, num_bits)
    [num_array, junk] = size(array);

    for(n=1:num_array)
        dec(n) = 0;
        for(m=num_bits-1:-1:0)
            dec(n) = dec(n) + array(n,num_bits-m)*2^m;
        endfor
    endfor
endfunction
