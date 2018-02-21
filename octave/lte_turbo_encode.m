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
% Function:    lte_turbo_encode
% Description: Turbo encodes data using the LTE Parallel Concatenated
%              Convolutional Code
% Inputs:      in_bits  - Input bits to turbo code
%              F        - Number of filler bits used
% Outputs:     out_bits - Turbo coded bits
% Spec:        3GPP TS 36.212 section 5.1.3.2 v10.1.0
% Notes:       Currently not handling filler bits
% Rev History: Ben Wojtowicz 02/18/2012 Created
%
function [out_bits] = lte_turbo_encode(in_bits, F)
    % Determine the length of the input
    K = length(in_bits);

    % Construct z
    [z, fb_one] = constituent_encoder(in_bits, K);

    % Construct x
    x = [in_bits, fb_one(K:K+3)];

    % Construct c_prime
    c_prime = internal_interleaver(in_bits, K);

    % Construct z_prime
    [z_prime, x_prime] = constituent_encoder(c_prime, K);

    % Construct d0
    out_bits(1,:) = [x(0+1:K-1+1), x(K+1), z(K+1+1), x_prime(K+1), z_prime(K+1+1)];

    % Construct d1
    out_bits(2,:) = [z(0+1:K-1+1), z(K+1), x(K+2+1), z_prime(K+1), x_prime(K+2+1)];

    % Construct d2
    out_bits(3,:) = [z_prime(0+1:K-1+1), x(K+1+1), z(K+2+1), x_prime(K+1+1), z_prime(K+2+1)];
endfunction

function [out, fb] = constituent_encoder(in, K)
    % Define constraint length
    k = 4;

    % Initialize shift register
    for(n=0:k-1)
        s_reg(n+1) = 0;
    endfor

    g_array  = [1,1,0,1];
    fb_array = [0,0,1,1];

    for(n=0:K-1)
        % Sequence the shift register
        for(m=k:-1:2)
            s_reg(m) = s_reg(m-1);
        endfor

        % Calculate the feedback bit
        fb(n+1) = 0;
        for(m=0:k-1)
            fb(n+1) = fb(n+1) + s_reg(m+1)*fb_array(m+1);
        endfor
        fb(n+1) = mod(fb(n+1), 2);

        % Add the next bit to the shift register
        s_reg(1) = mod(fb(n+1) + in(n+1), 2);

        % Calculate the output bit
        out(n+1) = 0;
        for(m=0:k-1)
            out(n+1) = out(n+1) + s_reg(m+1)*g_array(m+1);
        endfor
        out(n+1) = mod(out(n+1), 2);
    endfor

    % Trellis termination
    for(n=K:K+3)
        % Sequence the shift register
        for(m=k:-1:2)
            s_reg(m) = s_reg(m-1);
        endfor

        % Calculate the feedback bit
        fb(n+1) = 0;
        for(m=0:k-1)
            fb(n+1) = fb(n+1) + s_reg(m+1)*fb_array(m+1);
        endfor
        fb(n+1) = mod(fb(n+1), 2);

        % Add the next bit to the shift register
        s_reg(1) = mod(fb(n+1) + fb(n+1), 2);

        % Calculate the output bit
        out(n+1) = 0;
        for(m=0:k-1)
            out(n+1) = out(n+1) + s_reg(m+1)*g_array(m+1);
        endfor
        out(n+1) = mod(out(n+1), 2);
    endfor
endfunction

function [out] = internal_interleaver(in, K)
    K_table = [40,48,56,64,72,80,88,96,104,112,120,128,136,144,152,160,168,176,184,192,200,208,216,224,232,240,248,256,264,272,280,288,296,304,312,320,328,336,344,352,360,368,376,384,392,400,408,416,424,432,440,448,456,464,472,480,488,496,504,512,528,544,560,576,592,608,624,640,656,672,688,704,720,736,752,768,784,800,816,832,848,864,880,896,912,928,944,960,976,992,1008,1024,1056,1088,1120,1152,1184,1216,1248,1280,1312,1344,1376,1408,1440,1472,1504,1536,1568,1600,1632,1664,1696,1728,1760,1792,1824,1856,1888,1920,1952,1984,2016,2048,2112,2176,2240,2304,2368,2432,2496,2560,2624,2688,2752,2816,2880,2944,3008,3072,3136,3200,3264,3328,3392,3456,3520,3584,3648,3712,3776,3840,3904,3968,4032,4096,4160,4224,4288,4352,4416,4480,4544,4608,4672,4736,4800,4864,4928,4992,5056,5120,5184,5248,5312,5376,5440,5504,5568,5632,5696,5760,5824,5888,5952,6016,6080,6144];
    f1_table = [3,7,19,7,7,11,5,11,7,41,103,15,9,17,9,21,101,21,57,23,13,27,11,27,85,29,33,15,17,33,103,19,19,37,19,21,21,115,193,21,133,81,45,23,243,151,155,25,51,47,91,29,29,247,29,89,91,157,55,31,17,35,227,65,19,37,41,39,185,43,21,155,79,139,23,217,25,17,127,25,239,17,137,215,29,15,147,29,59,65,55,31,17,171,67,35,19,39,19,199,21,211,21,43,149,45,49,71,13,17,25,183,55,127,27,29,29,57,45,31,59,185,113,31,17,171,209,253,367,265,181,39,27,127,143,43,29,45,157,47,13,111,443,51,51,451,257,57,313,271,179,331,363,375,127,31,33,43,33,477,35,233,357,337,37,71,71,37,39,127,39,39,31,113,41,251,43,21,43,45,45,161,89,323,47,23,47,263];
    f2_table = [10,12,42,16,18,20,22,24,26,84,90,32,34,108,38,120,84,44,46,48,50,52,36,56,58,60,62,32,198,68,210,36,74,76,78,120,82,84,86,44,90,46,94,48,98,40,102,52,106,72,110,168,114,58,118,180,122,62,84,64,66,68,420,96,74,76,234,80,82,252,86,44,120,92,94,48,98,80,102,52,106,48,110,112,114,58,118,60,122,124,84,64,66,204,140,72,74,76,78,240,82,252,86,88,60,92,846,48,28,80,102,104,954,96,110,112,114,116,354,120,610,124,420,64,66,136,420,216,444,456,468,80,164,504,172,88,300,92,188,96,28,240,204,104,212,192,220,336,228,232,236,120,244,248,168,64,130,264,134,408,138,280,142,480,146,444,120,152,462,234,158,80,96,902,166,336,170,86,174,176,178,120,182,184,186,94,190,480];

    % Determine f1 and f2
    for(n=0:length(K_table)-1)
        if(K == K_table(n+1))
            f1 = f1_table(n+1);
            f2 = f2_table(n+1);
            break;
        endif
    endfor

    for(n=0:length(in)-1)
        out(n+1) = in(mod(f1*n + f2*(n^2), K)+1);
    endfor
endfunction
