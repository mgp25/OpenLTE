%
% Copyright 2012, 2014 Ben Wojtowicz
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
% Function:    lte_turbo_decode
% Description: Turbo decodes data according to the LTE Parallel Concatenated
%              Convolutional Code.  The design of this decoder is based on
%              the conversion of the constituent coder from:
%                                -------->+---------------->+---- out
%                                |        ^                 ^
%                        in_act  |   |-|  |   |-|      |-|  |
%              in --->+------------->|D|----->|D|----->|D|---
%                     ^              |-|      |-|  |   |-|  |
%                     |                            v        |
%                     -----------------------------+<--------
%              to:
%                        ------->+---------------->+------------- out
%                        |       ^                 ^
%                        |  |-|  |   |-|      |-|  |       
%              in_act ------|D|----->|D|----->|D|---         
%                        |  |-|      |-|  |   |-|  |          
%                        |                v        v         
%                        ---------------->+------->+------------- in
%              "in_act" can be determined using viterbi decoding and a second
%              copy of "in" can be calculated using "in_act"
% Inputs:      in_bits  - Turbo coded bits
%              F        - Number of filler bits used
% Outputs:     out_bits - Decoded bits
% Spec:        3GPP TS 36.212 section 5.1.3.2 v10.1.0
% Notes:       Currently not handling filler bits
% Rev History: Ben Wojtowicz 02/18/2012 Created
%              Ben Wojtowicz 03/26/2014 Added hard decision conversion before
%                                       combining the 4 bit streams
%
function [out_bits] = lte_turbo_decode(in_bits, F)
    % Undo trellis termination
    K       = length(in_bits(1,1:end-4));
    rx_in_1 = [in_bits(1,1:end-4)];
    rx_in_2 = [in_bits(2,1:end-4)];
    rx_in_3 = [in_bits(3,1:end-4)];

    % Step 1: Calculate in_act_1 using rx_in_1 and rx_in_2
    tmp_in   = reshape([rx_in_2; rx_in_1], 1, []);
    in_act_1 = cmn_viterbi_decode(tmp_in, 4, 2, [15, 13]);

    % Step 2: Calculate fb_1 using in_act_1
    fb_1 = [0, cmn_conv_encode(in_act_1, 3, 1, [3], 0)];

    % Step 3: Calculate in_calc_1 using in_act_1 and fb_1
    for(n=0:length(in_act_1)-1)
        in_calc_1(n+1) = 1-2*mod(in_act_1(n+1) + fb_1(n+1), 2);
    endfor

    % Step 4: Calculate in_int using rx_in_1
    in_int = internal_interleaver(rx_in_1, K);

    % Step 5: Calculate in_int_1 using in_calc_1
    in_int_1 = internal_interleaver(in_calc_1, K);

    % Step 6: Calculate int_act_1 using in_int and rx_in_3
    tmp_in    = reshape([rx_in_3; in_int], 1, []);
    int_act_1 = cmn_viterbi_decode(tmp_in, 4, 2, [15, 13]);

    % Step 7: Calculate int_act_2 using in_int_1 and rx_in_3
    tmp_in    = reshape([rx_in_3; in_int_1], 1, []);
    int_act_2 = cmn_viterbi_decode(tmp_in, 4, 2, [15, 13]);

    % Step 8: Calculate fb_int_1 using int_act_1
    fb_int_1 = [0, cmn_conv_encode(int_act_1, 3, 1, [3], 0)];

    % Step 9: Calculate fb_int_2 using int_act_2
    fb_int_2 = [0, cmn_conv_encode(int_act_2, 3, 1, [3], 0)];

    % Step 10: Calculate int_calc_1 using int_act_1 and fb_int_1
    for(n=0:length(int_act_1)-1)
        int_calc_1(n+1) = 1-2*mod(int_act_1(n+1) + fb_int_1(n+1), 2);
    endfor

    % Step 11: Calculate int_calc_2 using int_act_2 and fb_int_2
    for(n=0:length(int_act_2)-1)
        int_calc_2(n+1) = 1-2*mod(int_act_2(n+1) + fb_int_2(n+1), 2);
    endfor

    % Step 12: Calculate in_calc_2 using int_calc_1
    in_calc_2 = internal_deinterleaver(int_calc_1, K);

    % Step 13: Calculate in_calc_3 using int_calc_2
    in_calc_3 = internal_deinterleaver(int_calc_2, K);

    % Step 14: Soft combine rx_in_1, in_calc_1, in_calc_2, and in_calc_3 to get output
    for(n=0:length(rx_in_1)-1)
        if(rx_in_1(n+1) > 0)
            rx_in_1(n+1) = 1;
        else
            rx_in_1(n+1) = -1;
        endif
        tmp = rx_in_1(n+1) + in_calc_1(n+1) + in_calc_2(n+1) + in_calc_3(n+1);
        if(tmp >= 0)
            out_bits(n+1) = 0;
        else
            out_bits(n+1) = 1;
        endif
    endfor
endfunction

function [out] = internal_deinterleaver(in, K)
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
        out(mod(f1*n + f2*(n^2), K) + 1) = in(n+1);
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
