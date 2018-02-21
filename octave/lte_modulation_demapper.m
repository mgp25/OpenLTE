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
% Function:    lte_modulation_demapper
% Description: Maps complex-valued modulation symbols to
%              binary digits
% Inputs:      symbs    - Complex-valued modulation symbols
%              mod_type - Modulation type (bpsk, qpsk, 16qam,
%                         or 64qam)
% Outputs:     bits     - Binary digits
% Spec:        3GPP TS 36.211 section 7.1 v10.1.0
% Notes:       None
% Rev History: Ben Wojtowicz 10/28/2011 Created
%              Ben Wojtowicz 01/29/2012 Fixed license statement
%              Ben Wojtowicz 02/19/2012 Added newline to EOF
%
function [bits] = lte_modulation_demapper(symbs, mod_type)

    N_symbs = length(symbs);

    if(mod_type == "bpsk")
        % 36.211 Section 7.1.1 v10.1.0
        for(n=0:N_symbs-1)
            ang = angle(symbs(n+1));
            if(ang > -pi/4 && ang < 3*pi/4)
                act_symb  = +1/sqrt(2) + j/sqrt(2);
                sd        = get_soft_decision(symbs(n+1), act_symb, 1);
                bits(n+1) = +sd;
            else
                act_symb  = -1/sqrt(2) - j/sqrt(2);
                sd        = get_soft_decision(symbs(n+1), act_symb, 1);
                bits(n+1) = -sd;
            endif
        endfor
    elseif(mod_type == "qpsk")
        % 36.211 Section 7.1.2 v10.1.0
        for(n=0:N_symbs-1)
            ang = angle(symbs(n+1));
            if(ang >= 0 && ang < pi/2)
                act_symb      = +1/sqrt(2) + j/sqrt(2);
                sd            = get_soft_decision(symbs(n+1), act_symb, 1);
                bits(n*2+0+1) = +sd;
                bits(n*2+1+1) = +sd;
            elseif(ang >= -pi/2 && ang < 0)
                act_symb      = +1/sqrt(2) - j/sqrt(2);
                sd            = get_soft_decision(symbs(n+1), act_symb, 1);
                bits(n*2+0+1) = +sd;
                bits(n*2+1+1) = -sd;
            elseif(ang >= pi/2 && ang < pi)
                act_symb      = -1/sqrt(2) + j/sqrt(2);
                sd            = get_soft_decision(symbs(n+1), act_symb, 1);
                bits(n*2+0+1) = -sd;
                bits(n*2+1+1) = +sd;
            else
                act_symb      = -1/sqrt(2) - j/sqrt(2);
                sd            = get_soft_decision(symbs(n+1), act_symb, 1);
                bits(n*2+0+1) = -sd;
                bits(n*2+1+1) = -sd;
            endif
        endfor
    elseif(mod_type == "16qam")
        % 36.211 Section 7.1.3 v10.1.0
        printf("ERROR: Not supporting 16qam at this time\n");
        bits = 0;
    elseif(mod_type == "64qam")
        % 36.211 Section 7.1.4 v10.1.0
        printf("ERROR: Not supporting 64qam at this time\n");
        bits = 0;
    else
        printf("ERROR: Invalid mod_type (%s)\n", mod_type);
        bits = 0;
    endif
endfunction

function [sd] = get_soft_decision(p1, p2, max_dist)
    dist = sqrt((real(p1)-real(p2))^2 + (imag(p1)-imag(p2))^2);

    if(dist >= max_dist)
        dist = max_dist - (max_dist/1000);
    endif

    sd = max_dist - dist;
endfunction
