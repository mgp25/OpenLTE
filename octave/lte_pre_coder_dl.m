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
% Function:    lte_pre_coder_dl
% Description: Generates a block of vectors to be mapped onto
%              resources on each downlink antenna port
% Inputs:      x            - Block of input vectors
%              N_ant        - Number of antennas
%              style        - Style of mapping (tx_diversity or
%                             spatial_multiplex)
% Outputs:     y            - Block of output vectors
% Spec:        3GPP TS 36.211 section 6.3.4 v10.1.0
% Notes:       Only supports single antenna or tx_diversity.
% Rev History: Ben Wojtowicz 10/28/2011 Created
%              Ben Wojtowicz 01/29/2012 Fixed license statement
%              Ben Wojtowicz 02/19/2012 Added newline to EOF
%              Ben Wojtowicz 03/26/2014 Changed name to lte_pre_coder_dl
%
function [y] = lte_pre_coder_dl(x, N_ant, style)

    [v, M_layer_symb] = size(x);

    if(N_ant == 1)
        % 36.211 Section 6.3.4.1 v10.1.0
        M_ap_symb = M_layer_symb;
        for(n=0:M_ap_symb-1)
            y(n+1) = x(n+1);
        endfor
    elseif(N_ant == 2)
        if(style == "tx_diversity")
            % 36.211 section 6.3.4.3 v10.1.0
            for(n=0:M_layer_symb-1)
                y(0+1, 2*n+1)   = (1/sqrt(2))*(x(0+1, n+1));
                y(1+1, 2*n+1)   = (1/sqrt(2))*(-conj(x(1+1, n+1)));
                y(0+1, 2*n+1+1) = (1/sqrt(2))*(x(1+1, n+1));
                y(1+1, 2*n+1+1) = (1/sqrt(2))*(conj(x(0+1, n+1)));
            endfor
            M_ap_symb = 2*M_layer_symb;
        else
            printf("ERROR: Style %s not supported\n", style);
            y = 0;
        endif
    elseif(N_ant == 4)
        if(style == "tx_diversity")
            % 36.211 section 6.3.4.3 v10.1.0
            for(n=0:M_layer_symb-1)
                y(0+1, 4*n+1)   = (1/sqrt(2))*(x(0+1, n+1));
                y(1+1, 4*n+1)   = 0;
                y(2+1, 4*n+1)   = (1/sqrt(2))*(-conj(x(1+1, n+1)));
                y(3+1, 4*n+1)   = 0;
                y(0+1, 4*n+1+1) = (1/sqrt(2))*(x(1+1, n+1));
                y(1+1, 4*n+1+1) = 0;
                y(2+1, 4*n+1+1) = (1/sqrt(2))*(conj(x(0+1, n+1)));
                y(3+1, 4*n+1+1) = 0;
                y(0+1, 4*n+1+2) = 0;
                y(1+1, 4*n+1+2) = (1/sqrt(2))*(x(2+1, n+1));
                y(2+1, 4*n+1+2) = 0;
                y(3+1, 4*n+1+2) = (1/sqrt(2))*(-conj(x(3+1, n+1)));
                y(0+1, 4*n+1+3) = 0;
                y(1+1, 4*n+1+3) = (1/sqrt(2))*(x(3+1, n+1));
                y(2+1, 4*n+1+3) = 0;
                y(3+1, 4*n+1+3) = (1/sqrt(2))*(conj(x(2+1, n+1)));
            endfor
            if(x(2+1,M_layer_symb) ~= 10 && x(3+1,M_layer_symb) ~= 10)
                M_ap_symb = 4*M_layer_symb;
            else
                M_ap_symb = 4*M_layer_symb - 2;
                y         = y(:,1:M_ap_symb);
            endif
        else
            printf("ERROR: Style %s not supported\n", style);
            y = 0;
        endif
    else
        printf("ERROR: Number of antenna ports %u not supported\n", N_ant);
        y = 0;
    endif
endfunction
