%
% Copyright 2013 Ben Wojtowicz
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
% Function:    lte_generate_ul_rs
% Description: Generates LTE base reference signal sequence for the UL
% Inputs:      N_s                      - Slot number within a radio frame
%              N_id_cell                - Physical layer cell identity
%              chan_type                - pusch or pucch
%              delta_ss                 - Configurable portion of the sequence-shift pattern for PUSCH (sib2 groupAssignmentPUSCH)
%              group_hopping_enabled    - Boolean value determining if group hopping is enabled (sib2 groupHoppingEnabled)
%              sequence_hopping_enabled - Boolean value determining if sequence hopping is enabled (sib2 sequenceHoppingEnabled)
%              alpha                    - Cyclic shift
%              N_prbs                   - Number of PRBs used for the uplink grant
% Outputs:     r - Base reference signal
% Spec:        3GPP TS 36.211 section 5.5.1 v10.1.0
% Rev History: Ben Wojtowicz 09/28/2013 Created
%
function [r] = lte_generate_ul_rs(N_s, N_id_cell, chan_type, delta_ss, group_hopping_enabled, sequence_hopping_enabled, alpha, N_prbs)
    % Defines
    N_rb_ul_max             = 110;
    N_sc_rb                 = 12;
    table_5_5_1_2_1(0+1,:)  = [-1, 1, 3,-3, 3, 3, 1, 1, 3, 1,-3, 3];
    table_5_5_1_2_1(1+1,:)  = [ 1, 1, 3, 3, 3,-1, 1,-3,-3, 1,-3, 3];
    table_5_5_1_2_1(2+1,:)  = [ 1, 1,-3,-3,-3,-1,-3,-3, 1,-3, 1,-1];
    table_5_5_1_2_1(3+1,:)  = [-1, 1, 1, 1, 1,-1,-3,-3, 1,-3, 3,-1];
    table_5_5_1_2_1(4+1,:)  = [-1, 3, 1,-1, 1,-1,-3,-1, 1,-1, 1, 3];
    table_5_5_1_2_1(5+1,:)  = [ 1,-3, 3,-1,-1, 1, 1,-1,-1, 3,-3, 1];
    table_5_5_1_2_1(6+1,:)  = [-1, 3,-3,-3,-3, 3, 1,-1, 3, 3,-3, 1];
    table_5_5_1_2_1(7+1,:)  = [-3,-1,-1,-1, 1,-3, 3,-1, 1,-3, 3, 1];
    table_5_5_1_2_1(8+1,:)  = [ 1,-3, 3, 1,-1,-1,-1, 1, 1, 3,-1, 1];
    table_5_5_1_2_1(9+1,:)  = [ 1,-3,-1, 3, 3,-1,-3, 1, 1, 1, 1, 1];
    table_5_5_1_2_1(10+1,:) = [-1, 3,-1, 1, 1,-3,-3,-1,-3,-3, 3,-1];
    table_5_5_1_2_1(11+1,:) = [ 3, 1,-1,-1, 3, 3,-3, 1, 3, 1, 3, 3];
    table_5_5_1_2_1(12+1,:) = [ 1,-3, 1, 1,-3, 1, 1, 1,-3,-3,-3, 1];
    table_5_5_1_2_1(13+1,:) = [ 3, 3,-3, 3,-3, 1, 1, 3,-1,-3, 3, 3];
    table_5_5_1_2_1(14+1,:) = [-3, 1,-1,-3,-1, 3, 1, 3, 3, 3,-1, 1];
    table_5_5_1_2_1(15+1,:) = [ 3,-1, 1,-3,-1,-1, 1, 1, 3, 1,-1,-3];
    table_5_5_1_2_1(16+1,:) = [ 1, 3, 1,-1, 1, 3, 3, 3,-1,-1, 3,-1];
    table_5_5_1_2_1(17+1,:) = [-3, 1, 1, 3,-3, 3,-3,-3, 3, 1, 3,-1];
    table_5_5_1_2_1(18+1,:) = [-3, 3, 1, 1,-3, 1,-3,-3,-1,-1, 1,-3];
    table_5_5_1_2_1(19+1,:) = [-1, 3, 1, 3, 1,-1,-1, 3,-3,-1,-3,-1];
    table_5_5_1_2_1(20+1,:) = [-1,-3, 1, 1, 1, 1, 3, 1,-1, 1,-3,-1];
    table_5_5_1_2_1(21+1,:) = [-1, 3,-1, 1,-3,-3,-3,-3,-3, 1,-1,-3];
    table_5_5_1_2_1(22+1,:) = [ 1, 1,-3,-3,-3,-3,-1, 3,-3, 1,-3, 3];
    table_5_5_1_2_1(23+1,:) = [ 1, 1,-1,-3,-1,-3, 1,-1, 1, 3,-1, 1];
    table_5_5_1_2_1(24+1,:) = [ 1, 1, 3, 1, 3, 3,-1, 1,-1,-3,-3, 1];
    table_5_5_1_2_1(25+1,:) = [ 1,-3, 3, 3, 1, 3, 3, 1,-3,-1,-1, 3];
    table_5_5_1_2_1(26+1,:) = [ 1, 3,-3,-3, 3,-3, 1,-1,-1, 3,-1,-3];
    table_5_5_1_2_1(27+1,:) = [-3,-1,-3,-1,-3, 3, 1,-1, 1, 3,-3,-3];
    table_5_5_1_2_1(28+1,:) = [-1, 3,-3, 3,-1, 3, 3,-3, 3, 3,-1,-1];
    table_5_5_1_2_1(29+1,:) = [ 3,-3,-3,-1,-1,-3,-1, 3,-3, 3, 1,-1];
    table_5_5_1_2_2(0+1,:)  = [-1, 3, 1,-3, 3,-1, 1, 3,-3, 3, 1, 3,-3, 3, 1, 1,-1, 1, 3,-3, 3,-3,-1,-3];
    table_5_5_1_2_2(1+1,:)  = [-3, 3,-3,-3,-3, 1,-3,-3, 3,-1, 1, 1, 1, 3, 1,-1, 3,-3,-3, 1, 3, 1, 1,-3];
    table_5_5_1_2_2(2+1,:)  = [ 3,-1, 3, 3, 1, 1,-3, 3, 3, 3, 3, 1,-1, 3,-1, 1, 1,-1,-3,-1,-1, 1, 3, 3];
    table_5_5_1_2_2(3+1,:)  = [-1,-3, 1, 1, 3,-3, 1, 1,-3,-1,-1, 1, 3, 1, 3, 1,-1, 3, 1, 1,-3,-1,-3,-1];
    table_5_5_1_2_2(4+1,:)  = [-1,-1,-1,-3,-3,-1, 1, 1, 3, 3,-1, 3,-1, 1,-1,-3, 1,-1,-3,-3, 1,-3,-1,-1];
    table_5_5_1_2_2(5+1,:)  = [-3, 1, 1, 3,-1, 1, 3, 1,-3, 1,-3, 1, 1,-1,-1, 3,-1,-3, 3,-3,-3,-3, 1, 1];
    table_5_5_1_2_2(6+1,:)  = [ 1, 1,-1,-1, 3,-3,-3, 3,-3, 1,-1,-1, 1,-1, 1, 1,-1,-3,-1, 1,-1, 3,-1,-3];
    table_5_5_1_2_2(7+1,:)  = [-3, 3, 3,-1,-1,-3,-1, 3, 1, 3, 1, 3, 1, 1,-1, 3, 1,-1, 1, 3,-3,-1,-1, 1];
    table_5_5_1_2_2(8+1,:)  = [-3, 1, 3,-3, 1,-1,-3, 3,-3, 3,-1,-1,-1,-1, 1,-3,-3,-3, 1,-3,-3,-3, 1,-3];
    table_5_5_1_2_2(9+1,:)  = [ 1, 1,-3, 3, 3,-1,-3,-1, 3,-3, 3, 3, 3,-1, 1, 1,-3, 1,-1, 1, 1,-3, 1, 1];
    table_5_5_1_2_2(10+1,:) = [-1, 1,-3,-3, 3,-1, 3,-1,-1,-3,-3,-3,-1,-3,-3, 1,-1, 1, 3, 3,-1, 1,-1, 3];
    table_5_5_1_2_2(11+1,:) = [ 1, 3, 3,-3,-3, 1, 3, 1,-1,-3,-3,-3, 3, 3,-3, 3, 3,-1,-3, 3,-1, 1,-3, 1];
    table_5_5_1_2_2(12+1,:) = [ 1, 3, 3, 1, 1, 1,-1,-1, 1,-3, 3,-1, 1, 1,-3, 3, 3,-1,-3, 3,-3,-1,-3,-1];
    table_5_5_1_2_2(13+1,:) = [ 3,-1,-1,-1,-1,-3,-1, 3, 3, 1,-1, 1, 3, 3, 3,-1, 1, 1,-3, 1, 3,-1,-3, 3];
    table_5_5_1_2_2(14+1,:) = [-3,-3, 3, 1, 3, 1,-3, 3, 1, 3, 1, 1, 3, 3,-1,-1,-3, 1,-3,-1, 3, 1, 1, 3];
    table_5_5_1_2_2(15+1,:) = [-1,-1, 1,-3, 1, 3,-3, 1,-1,-3,-1, 3, 1, 3, 1,-1,-3,-3,-1,-1,-3,-3,-3,-1];
    table_5_5_1_2_2(16+1,:) = [-1,-3, 3,-1,-1,-1,-1, 1, 1,-3, 3, 1, 3, 3, 1,-1, 1,-3, 1,-3, 1, 1,-3,-1];
    table_5_5_1_2_2(17+1,:) = [ 1, 3,-1, 3, 3,-1,-3, 1,-1,-3, 3, 3, 3,-1, 1, 1, 3,-1,-3,-1, 3,-1,-1,-1];
    table_5_5_1_2_2(18+1,:) = [ 1, 1, 1, 1, 1,-1, 3,-1,-3, 1, 1, 3,-3, 1,-3,-1, 1, 1,-3,-3, 3, 1, 1,-3];
    table_5_5_1_2_2(19+1,:) = [ 1, 3, 3, 1,-1,-3, 3,-1, 3, 3, 3,-3, 1,-1, 1,-1,-3,-1, 1, 3,-1, 3,-3,-3];
    table_5_5_1_2_2(20+1,:) = [-1,-3, 3,-3,-3,-3,-1,-1,-3,-1,-3, 3, 1, 3,-3,-1, 3,-1, 1,-1, 3,-3, 1,-1];
    table_5_5_1_2_2(21+1,:) = [-3,-3, 1, 1,-1, 1,-1, 1,-1, 3, 1,-3,-1, 1,-1, 1,-1,-1, 3, 3,-3,-1, 1,-3];
    table_5_5_1_2_2(22+1,:) = [-3,-1,-3, 3, 1,-1,-3,-1,-3,-3, 3,-3, 3,-3,-1, 1, 3, 1,-3, 1, 3, 3,-1,-3];
    table_5_5_1_2_2(23+1,:) = [-1,-1,-1,-1, 3, 3, 3, 1, 3, 3,-3, 1, 3,-1, 3,-1, 3, 3,-3, 3, 1,-1, 3, 3];
    table_5_5_1_2_2(24+1,:) = [ 1,-1, 3, 3,-1,-3, 3,-3,-1,-1, 3,-1, 3,-1,-1, 1, 1, 1, 1,-1,-1,-3,-1, 3];
    table_5_5_1_2_2(25+1,:) = [ 1,-1, 1,-1, 3,-1, 3, 1, 1,-1,-1,-3, 1, 1,-3, 1, 3,-3, 1, 1,-3,-3,-1,-1];
    table_5_5_1_2_2(26+1,:) = [-3,-1, 1, 3, 1, 1,-3,-1,-1,-3, 3,-3, 3, 1,-3, 3,-3, 1,-1, 1,-3, 1, 1, 1];
    table_5_5_1_2_2(27+1,:) = [-1,-3, 3, 3, 1, 1, 3,-1,-3,-1,-1,-1, 3, 1,-3,-3,-1, 3,-3,-1,-3,-1,-3,-1];
    table_5_5_1_2_2(28+1,:) = [-1,-3,-1,-1, 1,-3,-1,-1, 1,-1,-3, 1, 1,-3, 1,-3,-3, 3, 1, 1,-1, 3,-1,-1];
    table_5_5_1_2_2(29+1,:) = [ 1, 1,-1,-1,-3,-1, 3,-1, 3,-1, 1, 3, 1,-1, 3, 1, 3,-3,-3, 1,-1,-1, 1, 3];
    prime_nums_to_2048      = [2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97,101,103,107,109,113,127,131,137,139,149,151,157,163,167,173,179,181,191,193,197,199,211,223,227,229,233,239,241,251,257,263,269,271,277,281,283,293,307,311,313,317,331,337,347,349,353,359,367,373,379,383,389,397,401,409,419,421,431,433,439,443,449,457,461,463,467,479,487,491,499,503,509,521,523,541,547,557,563,569,571,577,587,593,599,601,607,613,617,619,631,641,643,647,653,659,661,673,677,683,691,701,709,719,727,733,739,743,751,757,761,769,773,787,797,809,811,821,823,827,829,839,853,857,859,863,877,881,883,887,907,911,919,929,937,941,947,953,967,971,977,983,991,997,1009,1013,1019,1021,1031,1033,1039,1049,1051,1061,1063,1069,1087,1091,1093,1097,1103,1109,1117,1123,1129,1151,1153,1163,1171,1181,1187,1193,1201,1213,1217,1223,1229,1231,1237,1249,1259,1277,1279,1283,1289,1291,1297,1301,1303,1307,1319,1321,1327,1361,1367,1373,1381,1399,1409,1423,1427,1429,1433,1439,1447,1451,1453,1459,1471,1481,1483,1487,1489,1493,1499,1511,1523,1531,1543,1549,1553,1559,1567,1571,1579,1583,1597,1601,1607,1609,1613,1619,1621,1627,1637,1657,1663,1667,1669,1693,1697,1699,1709,1721,1723,1733,1741,1747,1753,1759,1777,1783,1787,1789,1801,1811,1823,1831,1847,1861,1867,1871,1873,1877,1879,1889,1901,1907,1913,1931,1933,1949,1951,1973,1979,1987,1993,1997,1999,2003,2011,2017,2027,2029,2039];

    % Validate N_s
    if(~(N_s >= 0 && N_s <= 19))
        printf("ERROR: Invalid N_s (%u)\n", N_s);
        r = 0;
        return;
    endif

    % Validate N_id_cell
    if(~(N_id_cell >= 0 && N_id_cell <= 503))
        printf("ERROR: Invalid N_id_cell (%u)\n", N_id_cell);
        r = 0;
        return;
    endif

    % Validate chan_type
    if(~(chan_type == "pusch" || chan_type == "pucch"))
        printf("ERROR: Invalid chan_type (%s)\n", chan_type);
        r = 0;
        return;
    endif

    % Validate delta_ss
    if(~(delta_ss >= 0 && delta_ss <= 29))
        printf("ERROR: Invalid delta_ss (%u)\n", delta_ss);
        r = 0;
        return;
    endif

    % Validate group_hopping_enabled
    if(~(group_hopping_enabled == 1 || group_hopping_enabled == 0))
        printf("ERROR: Invalid group_hopping_enabled (%u)\n", group_hopping_enabled);
        r = 0;
        return;
    endif

    % Validate sequence_hopping_enabled
    if(~(sequence_hopping_enabled == 1 || sequence_hopping_enabled == 0))
        printf("ERROR: Invalid sequence_hopping_enabled (%u)\n", sequence_hopping_enabled);
        r = 0;
        return;
    endif

    % Validate alpha
    if(~(alpha >= 0 && alpha < 2*pi))
        printf("ERROR: Invalid alpha (%f)\n", alpha);
        r = 0;
        return;
    endif

    % Validate N_prbs
    if(~(N_prbs >= 1 && N_prbs <= N_rb_ul_max))
        printf("ERROR: Invalid N_prbs (%u)\n", N_prbs);
        r = 0;
        return;
    endif

    % Calculate M_sc_rs
    M_sc_rs = N_prbs*N_sc_rb;

    % Determine N_zc_rs
    for(n=length(prime_nums_to_2048):-1:1)
        if(prime_nums_to_2048(n) < M_sc_rs)
            N_zc_rs = prime_nums_to_2048(n);
            break;
        endif
    endfor

    % Determine f_ss
    if("pusch" == chan_type)
        f_ss = mod(mod(N_id_cell, 30) + delta_ss, 30);
    else % pucch
        f_ss = mod(N_id_cell, 30);
    endif

    % Determine u
    if(1 == group_hopping_enabled)
        c    = lte_generate_prs_c(floor(N_id_cell/30), 160);
        f_gh = 0;
        for(n=0:7)
            f_gh = f_gh + c(8*N_s + n + 1)*(2^n);
        endfor
        f_gh = mod(f_gh, 30);
        u    = mod(f_gh + f_ss, 30);
    else
        u = mod(f_ss, 30);
    endif

    % Determine v
    if(M_sc_rs < 6*N_sc_rb)
        v = 0;
    else
        if(0 == group_hopping_enabled && 1 == sequence_hopping_enabled)
            c = lte_generate_prs_c(floor(N_id_cell/30)*2^5 + f_ss, 20);
            v = c(N_s+1);
        else
            v = 0;
        endif
    endif

    % Determine r_bar_u_v
    if(M_sc_rs >= 3*N_sc_rb)
        q_bar = N_zc_rs*(u+1)/31;
        q     = floor(q_bar + 1/2) + v*(-1)^floor(2*q_bar);
        for(m=0:N_zc_rs-1)
            x_q(m+1) = cos(-pi*q*m*(m+1)/N_zc_rs) + j*sin(-pi*q*m*(m+1)/N_zc_rs);
        endfor
        for(n=0:M_sc_rs-1)
            r_bar_u_v(n+1) = x_q(mod(n, N_zc_rs)+1);
        endfor
    elseif(M_sc_rs == N_sc_rb)
        for(n=0:M_sc_rs-1)
            r_bar_u_v(n+1) = cos(table_5_5_1_2_1(u+1,n+1)*pi/4) + j*sin(table_5_5_1_2_1(u+1,n+1)*pi/4);
        endfor
    else % M_sc_rs == 2*N_sc_rb
        for(n=0:M_sc_rs-1)
            r_bar_u_v(n+1) = cos(table_5_5_1_2_2(u+1,n+1)*pi/4) + j*sin(table_5_5_1_2_2(u+1,n+1)*pi/4);
        endfor
    endif

    % Calculate r_u_v
    for(n=0:M_sc_rs-1)
        r_u_v(n+1) = (cos(alpha*n) + j*sin(alpha*n)) * r_bar_u_v(n+1);
    endfor

    % Save output
    r = r_u_v;
endfunction
