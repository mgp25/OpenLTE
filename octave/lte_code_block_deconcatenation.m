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
% Function:    lte_code_block_deconcatenation
% Description: Undoes code block concatenation for turbo coded channels
% Inputs:      in_bits  - Code block bits to be deconcatenated
%              tbs      - Transport block size for the channel
% Outputs:     out_bits - Code block deconcatenated bits
% Spec:        3GPP TS 36.212 section 5.1.5 v10.1.0
% Notes:       None
% Rev History: Ben Wojtowicz 01/10/2012 Created
%
function [out_bits] = lte_code_block_deconcatenation(in_bits, tbs)
    % Define K table 36.212 Table 5.1.3-3 v10.1.0
    k_table = [40,48,56,64,72,80,88,96,104,112,120,128,136,144,152,160,168,176,184,192,200,208,216,224,232,240,248,256,264,272,280,288,296,304,312,320,328,336,344,352,360,368,376,384,392,400,408,416,424,432,440,448,456,464,472,480,488,496,504,512,528,544,560,576,592,608,624,640,656,672,688,704,720,736,752,768,784,800,816,832,848,864,880,896,912,928,944,960,976,992,1008,1024,1056,1088,1120,1152,1184,1216,1248,1280,1312,1344,1376,1408,1440,1472,1504,1536,1568,1600,1632,1664,1696,1728,1760,1792,1824,1856,1888,1920,1952,1984,2016,2048,2112,2176,2240,2304,2368,2432,2496,2560,2624,2688,2752,2816,2880,2944,3008,3072,3136,3200,3264,3328,3392,3456,3520,3584,3648,3712,3776,3840,3904,3968,4032,4096,4160,4224,4288,4352,4416,4480,4544,4608,4672,4736,4800,4864,4928,4992,5056,5120,5184,5248,5312,5376,5440,5504,5568,5632,5696,5760,5824,5888,5952,6016,6080,6144];

    % Determine L, C, B', K+, C+, K-, and C- 36.212 section 5.1.2 v10.1.0
    Z = 6144;
    if(tbs <= Z)
        L       = 0;
        C       = 1;
        B_prime = length(in_bits);
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
        B       = tbs + 24;
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
    out_s.num_cb = C;

    out_bits_len = length(in_bits)/C;

    % Deconcatenate code blocks
    k_idx = 0;
    r_idx = 0;
    while(r_idx < C)
        j_idx = 0;
        while(j_idx < out_bits_len)
            out_bits(r_idx+1,j_idx+1) = in_bits(k_idx+1);
            k_idx                     = k_idx + 1;
            j_idx                     = j_idx + 1;
        endwhile
        r_idx = r_idx + 1;
    endwhile
endfunction
