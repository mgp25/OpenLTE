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
% Function:    cmn_conv_encode
% Description: Convolutionally encodes the input
%              bit array according to the parameters
% Inputs:      in       - Input bit array
%              k        - Constraint length
%              r        - Rate
%              g        - Polynomial definition
%                         array in octal
%              tail_bit - Whether to use tail
%                         biting convolutional
%                         coding or not (0 = no,
%                         1 = yes)
% Outputs:     out      - Ouput bit array
% Spec:        N/A
% Notes:       None
% Rev History: Ben Wojtowicz 11/22/2011 Created
%              Ben Wojtowicz 01/29/2012 Fixed license statement
%
function [out] = cmn_conv_encode(in, k, r, g, tail_bit)

    % Check constraint length
    if(k > 9)
        printf("ERROR: Maximum supported constraint length = 9\n");
        out = 0;
        return;
    endif

    % Check r and g
    if(length(g) != r)
        printf("ERROR: Invalid rate (%u) or polynomial definition (%u)\n", r, length(g));
        out = 0;
        return;
    endif

    % Check for tail biting
    if(tail_bit == 1)
        % Initialize shift register with tail bits of input
        for(n=0:k-1)
            s_reg(n+1) = in(length(in)-n-1+1);
        endfor
    else
        % Initialize shift register with zeros
        for(n=0:k-1)
            s_reg(n+1) = 0;
        endfor
    endif

    % Convert g from octal to binary array
    g_array = cmn_oct2bin(g, k);

    % Convolutionally encode input
    idx = 1;
    for(n=0:length(in)-1)
        % Add next bit to shift register
        for(m=k:-1:2)
            s_reg(m) = s_reg(m-1);
        endfor
        s_reg(1) = in(n+1);

        % Determine the output bits
        for(m=0:r-1)
            out(idx) = 0;

            for(o=0:k-1)
                out(idx) = out(idx) + s_reg(o+1)*g_array(m+1,o+1);
            endfor
            out(idx) = mod(out(idx), 2);
            idx      = idx + 1;
        endfor
    endfor
endfunction
