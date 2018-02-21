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
% Function:    lte_cfi_channel_encode
% Description: Channel encodes the control format
%              indicator channel
% Inputs:      cfi - Control format indicator
% Outputs:     cfi_bits - Control format indicator
%                         channel encoded bits
% Spec:        3GPP TS 36.212 section 5.3.4 v10.1.0
% Notes:       None
% Rev History: Ben Wojtowicz 12/26/2011 Created
%              Ben Wojtowicz 01/29/2012 Fixed license statement
%
function [cfi_bits] = lte_cfi_channel_encode(cfi)
    % Check cfi
    if(cfi == 1)
        cfi_bits = [0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1];
    elseif(cfi == 2)
        cfi_bits = [1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0];
    elseif(cfi == 3)
        cfi_bits = [1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1];
    elseif(cfi == 4)
        cfi_bits = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
    else
        printf("ERROR: Invalid cfi %u\n", cfi);
        cfi_bits = 0;
    endif
endfunction
