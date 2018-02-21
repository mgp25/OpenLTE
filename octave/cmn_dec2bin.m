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
% Function:    cmn_dec2bin
% Description: Converts an array of decimal numbers
%              to an array of binary arrays
% Inputs:      dec      - Array of decimal numbers
%              num_bits - Number of bits per decimal
%                         number
% Outputs:     array    - Array of binary arrays
% Spec:        N/A
% Notes:       None
% Rev History: Ben Wojtowicz 11/22/2011 Created
%              Ben Wojtowicz 01/29/2012 Fixed license statement
%
function [array] = cmn_dec2bin(dec, num_bits)
    [junk, num_dec] = size(dec);

    for(n=1:num_dec)
        tmp = dec(n);
        for(m=num_bits-1:-1:0)
            array(n,num_bits-m) = floor(tmp/2^m);
            tmp                 = tmp - floor(tmp/2^m)*2^m;
        endfor
    endfor
endfunction
