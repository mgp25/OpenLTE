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
% Function:    lte_pre_decoder_and_matched_filter_dl
% Description: Matched filters and unmaps a block of downlink vectors
%              from resources on each antenna port
% Inputs:      y            - Block of input vectors
%              h            - Channel estimate
%              style        - Style of mapping (tx_diversity or
%                             spatial_multiplex)
% Outputs:     x            - Block of output vectors
% Spec:        3GPP TS 36.211 section 6.3.4 v10.1.0
% Notes:       Only supports single antenna or tx_diversity.
% Rev History: Ben Wojtowicz 10/28/2011 Created
%              Ben Wojtowicz 01/29/2012 Fixed license statement
%              Ben Wojtowicz 02/19/2012 Added newline to EOF
%              Ben Wojtowicz 03/26/2014 Changed name to lte_pre_decoder_and_matched_filter_dl
%
function [x] = lte_pre_decoder_and_matched_filter_dl(y, h, style)

    [N_ant, M_ap_symb] = size(h);

    if(N_ant == 1)
        % 36.211 Section 6.3.4.1 v10.1.0
        M_layer_symb = M_ap_symb;
        for(n=0:M_ap_symb-1)
            x(n+1) = y(n+1)/h(n+1);
        endfor
    elseif(N_ant == 2)
        if(style == "tx_diversity")
            % 36.211 Section 6.3.4.3 v10.1.0
            M_layer_symb = M_ap_symb/2;
            for(n=0:M_layer_symb-1)
                h_norm     = abs(h(1,n*2+0+1))^2 + abs(h(2,n*2+0+1))^2;
                x(0+1,n+1) = (conj(h(1,n*2+0+1))*y(n*2+0+1) + h(2,n*2+0+1)*conj(y(n*2+1+1)))/h_norm;
                x(1+1,n+1) = conj(-conj(h(2,n*2+0+1))*y(n*2+0+1) + h(1,n*2+0+1)*conj(y(n*2+1+1)))/h_norm;
            endfor
        else
            printf("ERROR: Style %s not supported\n", style);
            x = 0;
        endif
    elseif(N_ant == 4)
        if(style == "tx_diversity")
            % 36.211 Section 6.3.4.3 v10.1.0
            if(mod(M_ap_symb, 4) == 0)
                M_layer_symb = M_ap_symb/4;
            else
                M_layer_symb = (M_ap_symb+2)/4;
                y(:,M_ap_symb+1) = [10; 10; 10; 10];
                y(:,M_ap_symb+2)   = [10; 10; 10; 10];
            endif
            for(n=0:M_layer_symb-1)
                h_norm_13  = abs(h(1,n*4+0+1))^2 + abs(h(3,n*4+0+1))^2;
                h_norm_24  = abs(h(2,n*4+2+1))^2 + abs(h(4,n*4+2+1))^2;
                x(0+1,n+1) = (conj(h(1,n*4+0+1))*y(n*4+0+1) + h(3,n*4+0+1)*conj(y(n*4+1+1)))/h_norm_13;
                x(1+1,n+1) = conj(-conj(h(3,n*4+0+1))*y(n*4+0+1) + h(1,n*4+0+1)*conj(y(n*4+1+1)))/h_norm_13;
                x(2+1,n+1) = (conj(h(2,n*4+2+1))*y(n*4+2+1) + h(4,n*4+2+1)*conj(y(n*4+3+1)))/h_norm_24;
                x(3+1,n+1) = conj(-conj(h(4,n*4+2+1))*y(n*4+2+1) + h(2,n*4+2+1)*conj(y(n*4+3+1)))/h_norm_24;
            endfor
        else
            printf("ERROR: Style %s not supported\n", style);
            x = 0;
        endif
    else
        printf("ERROR: Number of antenna ports %u not supported\n", N_ant);
        x = 0;
    endif
endfunction
