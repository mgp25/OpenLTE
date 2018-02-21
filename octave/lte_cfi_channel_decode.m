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
% Function:    lte_cfi_channel_decode
% Description: Channel decodes the control format
%              indicator channel
% Inputs:      cfi_bits - Control format indicator
%                         channel encoded bits
% Outputs:     cfi      - Control format indicator
% Spec:        3GPP TS 36.212 section 5.3.4 v10.1.0
% Notes:       None
% Rev History: Ben Wojtowicz 12/26/2011 Created
%              Ben Wojtowicz 01/29/2012 Fixed license statement
%              Ben Wojtowicz 02/19/2012 Added conversion to hard bits
%
function [cfi] = lte_cfi_channel_decode(cfi_bits)
    % Convert from soft NRZ to hard bits
    for(n=1:length(cfi_bits))
        if(cfi_bits(n) >= 0)
            cfi_bits(n) = 0;
        else
            cfi_bits(n) = 1;
        endif
    endfor

    % Calculate the number of bit errors for each CFI
    cfi_bits_1 = [0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1];
    cfi_bits_2 = [1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0];
    cfi_bits_3 = [1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1];
    cfi_bits_4 = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
    bit_errs   = zeros(1,4);
    for(n=1:32)
        if(cfi_bits_1(n) != cfi_bits(n))
            bit_errs(1) = bit_errs(1) + 1;
        endif
        if(cfi_bits_2(n) != cfi_bits(n))
            bit_errs(2) = bit_errs(2) + 1;
        endif
        if(cfi_bits_3(n) != cfi_bits(n))
            bit_errs(3) = bit_errs(3) + 1;
        endif
        if(cfi_bits_4(n) != cfi_bits(n))
            bit_errs(4) = bit_errs(4) + 1;
        endif
    endfor

    % Find the CFI with the least bit errors
    min_val = 32;
    min_idx = 0;
    for(n=1:4)
        if(bit_errs(n) < min_val)
            min_val = bit_errs(n);
            min_idx = n;
        endif
    endfor

    % Threshold is set at 25% BER (8 errors)
    if(min_val < 8)
        cfi = min_idx;
    else
        cfi = 0;
    endif
endfunction
