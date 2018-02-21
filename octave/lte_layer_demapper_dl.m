%
% Copyright 2011-2012, 2014 Ben Wojtowicz
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
% Function:    lte_layer_demapper_dl
% Description: De-maps one or several downlink layers into complex-valued
%              modulation symbols
% Inputs:      x            - Mapped complex valued modulation
%                             symbols
%              N_cw         - Number of codewords
%              style        - Style of mapping (tx_diversity or
%                             spatial_multiplex)
% Outputs:     d            - Complex valued modulation symbols
% Spec:        3GPP TS 36.211 section 6.3.3 v10.1.0
% Notes:       None
% Rev History: Ben Wojtowicz 10/28/2011 Created
%              Ben Wojtowicz 01/29/2012 Fixed license statement
%              Ben Wojtowicz 02/19/2012 Fixed check for NULL symbols
%              Ben Wojtowicz 03/26/2014 Changed name to lte_layer_demapper_dl
%
function [d] = lte_layer_demapper_dl(x, N_cw, style)

    [v, M_layer_symb] = size(x);

    if(v == 1 && N_cw == 1)
        % 36.211 Section 6.3.3.1 v10.1.0
        M_symb = M_layer_symb;
        for(n=0:M_layer_symb-1)
            d(0+1, n+1) = x(0+1, n+1);
        endfor
    else
        if(style == "tx_diversity")
            % 36.211 Section 6.3.3.3 v10.1.0
            if(v == 2 && N_cw == 1)
                M_symb = M_layer_symb*2;
                for(n=0:M_layer_symb-1)
                    d(0+1, 2*n+0+1) = x(0+1, n+1);
                    d(0+1, 2*n+1+1) = x(1+1, n+1);
                endfor
            elseif(v == 4 && N_cw == 1)
                for(n=0:M_layer_symb-1)
                    d(0+1, 4*n+0+1) = x(0+1, n+1);
                    d(0+1, 4*n+1+1) = x(1+1, n+1);
                    d(0+1, 4*n+2+1) = x(2+1, n+1);
                    d(0+1, 4*n+3+1) = x(3+1, n+1);
                endfor
                if(abs(x(3+1,M_layer_symb)) != 10)
                    M_symb = M_layer_symb*4;
                else
                    M_symb = M_layer_symb*4 - 2;
                    d      = d(:,1:M_symb);
                endif
            else
                printf("ERROR: Invalid v (%u) or N_cw (%u)\n", v, N_cw);
                d = 0;
                return;
            endif
        elseif(style == "spatial_multiplex")
            % 36.211 section 6.3.3.2 v10.1.0
            if(v == 2 && (N_cw == 1 || N_cw == 2))
                if(N_cw == 1)
                    M_symb = M_layer_symb*2;
                    for(n=0:M_layer_symb-1)
                        d(0+1, 2*n+0+1) = x(0+1, n+1);
                        d(0+1, 2*n+1+1) = x(1+1, n+1);
                    endfor
                else
                    M_symb = M_layer_symb;
                    for(n=0:M_layer_symb-1)
                        d(0+1, n+1) = x(0+1, n+1);
                        d(1+1, n+1) = x(1+1, n+1);
                    endfor
                endif
            elseif(v == 3 && (N_cw == 1 || N_cw == 2))
                if(N_cw == 1)
                    M_symb = M_layer_symb*3;
                    for(n=0:M_layer_symb-1)
                        d(0+1, 3*n+0+1) = x(0+1, n+1);
                        d(0+1, 3*n+1+1) = x(1+1, n+1);
                        d(0+1, 3*n+2+1) = x(2+1, n+1);
                    endfor
                else
                    M_symb = M_layer_symb*2;
                    d      = zeros(N_cw, M_symb);
                    for(n=0:M_layer_symb-1)
                        d(0+1,     n+1) = x(0+1, n+1);
                        d(1+1, 2*n+0+1) = x(1+1, n+1);
                        d(2+1, 2*n+1+1) = x(2+1, n+1);
                    endfor
                endif
            elseif(v == 4 && (N_cw == 1 || N_cw == 2))
                if(N_cw == 1)
                    M_symb = M_layer_symb*4;
                    for(n=0:M_layer_symb-1)
                        d(0+1, 4*n+0+1) = x(0+1, n+1);
                        d(0+1, 4*n+1+1) = x(1+1, n+1);
                        d(0+1, 4*n+2+1) = x(2+1, n+1);
                        d(0+1, 4*n+3+1) = x(3+1, n+1);
                    endfor
                else
                    M_symb = M_layer_symb*2;
                    for(n=0:M_layer_symb-1)
                        d(0+1, 2*n+0+1) = x(0+1, n+1);
                        d(0+1, 2*n+1+1) = x(1+1, n+1);
                        d(1+1, 2*n+0+1) = x(2+1, n+1);
                        d(1+1, 2*n+1+1) = x(3+1, n+1);
                    endfor
                endif
            elseif(v == 5 && N_cw == 2)
                M_symb = M_layer_symb*3;
                d      = zeros(N_cw, M_symb);
                for(n=0:M_laye_symb-1)
                    d(0+1, 2*n+0+1) = x(0+1, n+1);
                    d(0+1, 2*n+1+1) = x(1+1, n+1);
                    d(1+1, 3*n+0+1) = x(2+1, n+1);
                    d(1+1, 3*n+1+1) = x(3+1, n+1);
                    d(1+1, 3*n+2+1) = x(4+1, n+1);
                endfor
            elseif(v == 6 && N_cw == 2)
                M_symb = M_layer_symb*3;
                for(n=0:M_layer_symb-1)
                    d(0+1, 3*n+0+1) = x(0+1, n+1);
                    d(0+1, 3*n+1+1) = x(1+1, n+1);
                    d(0+1, 3*n+2+1) = x(2+1, n+1);
                    d(1+1, 3*n+0+1) = x(3+1, n+1);
                    d(1+1, 3*n+1+1) = x(4+1, n+1);
                    d(1+1, 3*n+2+1) = x(5+1, n+1);
                endfor
            elseif(v == 7 && N_cw == 2)
                M_symb = M_layer_symb*4;
                d      = zeros(N_cw, M_symb);
                for(n=0:M_layer_symb-1)
                    d(0+1, 3*n+0+1) = x(0+1, n+1);
                    d(0+1, 3*n+1+1) = x(1+1, n+1);
                    d(0+1, 3*n+2+1) = x(2+1, n+1);
                    d(1+1, 4*n+0+1) = x(3+1, n+1);
                    d(1+1, 4*n+1+1) = x(4+1, n+1);
                    d(1+1, 4*n+2+1) = x(5+1, n+1);
                    d(1+1, 4*n+3+1) = x(6+1, n+1);
                endfor
            elseif(v == 8 && N_cw == 2)
                M_symb = M_layer_symb*4;
                for(n=0:M_layer_symb-1)
                    d(0+1, 4*n+0+1) = x(0+1, n+1);
                    d(0+1, 4*n+1+1) = x(1+1, n+1);
                    d(0+1, 4*n+2+1) = x(2+1, n+1);
                    d(0+1, 4*n+3+1) = x(3+1, n+1);
                    d(1+1, 4*n+0+1) = x(4+1, n+1);
                    d(1+1, 4*n+1+1) = x(5+1, n+1);
                    d(1+1, 4*n+2+1) = x(6+1, n+1);
                    d(1+1, 4*n+3+1) = x(7+1, n+1);
                endfor
            else
                printf("ERROR: Invalid v (%u) or N_cw (%u)\n", v, N_cw);
                d = 0;
                return;
            endif
        else
            printf("ERROR: Invalid style (%s)\n", style);
            d = 0;
            return;
        endif
    endif
endfunction
