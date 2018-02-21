%
% Copyright 2012 Ben Wojtowicz
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
% Function:    lte_code_block_desegmentation
% Description: Performs code block desegmentation for turbo coded channels
% Inputs:      in_bits  - Bits to be code block desegmented
%              tbs      - Transport block size
% Outputs:     out_bits - Code block desegmented bits
% Spec:        3GPP TS 36.212 section 5.1.2 v10.1.0
% Notes:       None
% Rev History: Ben Wojtowicz 01/10/2012 Created
%
function [out_bits] = lte_code_block_desegmentation(in_bits, tbs)
    % Define K table 36.212 Table 5.1.3-3 v10.1.0
    k_table = [40,48,56,64,72,80,88,96,104,112,120,128,136,144,152,160,168,176,184,192,200,208,216,224,232,240,248,256,264,272,280,288,296,304,312,320,328,336,344,352,360,368,376,384,392,400,408,416,424,432,440,448,456,464,472,480,488,496,504,512,528,544,560,576,592,608,624,640,656,672,688,704,720,736,752,768,784,800,816,832,848,864,880,896,912,928,944,960,976,992,1008,1024,1056,1088,1120,1152,1184,1216,1248,1280,1312,1344,1376,1408,1440,1472,1504,1536,1568,1600,1632,1664,1696,1728,1760,1792,1824,1856,1888,1920,1952,1984,2016,2048,2112,2176,2240,2304,2368,2432,2496,2560,2624,2688,2752,2816,2880,2944,3008,3072,3136,3200,3264,3328,3392,3456,3520,3584,3648,3712,3776,3840,3904,3968,4032,4096,4160,4224,4288,4352,4416,4480,4544,4608,4672,4736,4800,4864,4928,4992,5056,5120,5184,5248,5312,5376,5440,5504,5568,5632,5696,5760,5824,5888,5952,6016,6080,6144];

    % Define Z
    Z = 6144;

    % Determine L, C, B', K+, C+, K-, and C-
    if(tbs+24 <= Z)
        L       = 0;
        C       = 1;
        B_prime = tbs+24;
        for(n=0:length(k_table)-1)
            if(C*k_table(n+1) >= B_prime)
                K_plus = k_table(n+1);
                break;
            endif
        endfor
        K_minus = 0;
        C_plus  = 1;
        C_minus = 0;
    else
        B       = tbs+24;
        L       = 24;
        C       = ceil(B/(Z-L));
        B_prime = B + C*L;
        for(n=0:length(k_table)-1)
            if(C*k_table(n+1) >= B_prime)
                K_plus = k_table(n+1);
                break;
            endif
        endfor
        for(n=length(k_table)-1:-1:0)
            if(k_table(n+1) < K_plus)
                K_minus = k_table(n+1);
                break;
            endif
        endfor
        K_delta = K_plus - K_minus;
        C_minus = floor((C*K_plus - B_prime)/K_delta);
        C_plus  = C - C_minus;
    endif

    % Determine the number of filler bits
    F = C_plus*K_plus + C_minus*K_minus - B_prime;

    % Determine the output bits
    k_idx = F;
    s_idx = 0;
    for(r_idx=0:C-1)
        % Determine the K for this code block
        if(r_idx < C_minus)
            K_r = K_minus;
        else
            K_r = K_plus;
        endif

        % Check CRC if more than 1 code block is present
        if(C > 1)
            calc_p_cb_bits = lte_calc_crc(in_bits(r_idx+1,1:K_r-24), "24B");
            p_cb_bits      = in_bits(r_idx+1,K_r-24+1:K_r);
            ber            = 0;
            for(n=0:length(p_cb_bits)-1)
                if(p_cb_bits(n+1) ~= calc_p_cb_bits(n+1))
                    ber = ber + 1;
                endif
            endfor
            if(ber == 0)
                printf("CB CRC CORRECT\n");
            else
                printf("CB CRC INCORRECT\n");
            endif
        endif

        % Create the output bits
        while(k_idx < K_r - L)
            out_bits(s_idx+1) = in_bits(r_idx+1,k_idx+1);
            k_idx             = k_idx + 1;
            s_idx             = s_idx + 1;
        endwhile
        k_idx = 0;
    endfor
endfunction
