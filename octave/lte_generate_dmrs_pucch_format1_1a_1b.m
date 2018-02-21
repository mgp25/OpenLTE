%
% Copyright 2016 Ben Wojtowicz
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
% Function:    lte_generate_dmrs_pucch_format1_1a_1b
% Description: Generates LTE demodulation reference signal for PUCCH formats 1, 1A, and 1B
% Inputs:      N_subfr                  - Subframe number within a radio frame
%              N_id_cell                - Physical layer cell identity
%              delta_ss                 - Configurable portion of the sequence-shift pattern for PUSCH (sib2 groupAssignmentPUSCH)
%              group_hopping_enabled    - Boolean value determining if group hopping is enabled (sib2 groupHoppingEnabled)
%              sequence_hopping_enabled - Boolean value determining if sequence hopping is enabled (sib2 sequenceHoppingEnabled)
%              N_cs_1                   - Number of cyclic shifts used (sib2 nCS-AN)
%              N_1_p_pucch              - Resource index
%              delta_pucch_shift        - Controls the amount of multiplexing on PUCCH (sib2 deltaPUCCH-Shift)
%              N_ant                    - Number of antennas
% Outputs:     r - Demodulation reference signal for PUCCH format 1, 1A, and 1B
% Spec:        3GPP TS 36.211 section 5.5.2.2 v10.1.0
% Notes:       Currently only handles normal CP and 1 antenna
% Rev History: Ben Wojtowicz 03/12/2016 Created
%
function [r] = lte_generate_dmrs_pucch_format1_1a_1b(N_subfr, N_id_cell, delta_ss, group_hopping_enabled, sequence_hopping_enabled, N_cs_1, N_1_p_pucch, delta_pucch_shift, N_ant)

    % Defines
    N_ul_symb       = 7; % Only dealing with normal cp at this time
    N_sc_rb         = 12;
    M_pucch_rs      = 3;
    w_vector(0+1,:) = [1, 1, 1];
    w_vector(1+1,:) = [1, exp(j*2*pi/3), exp(j*4*pi/3)];
    w_vector(2+1,:) = [1, exp(j*4*pi/3), exp(j*2*pi/3)];

    % Validate N_subfr
    if(~(N_subfr >= 0 && N_subfr <= 9))
        printf("ERROR: Invalid N_subfr (%u)\n", N_subfr);
        r = 0;
        return;
    endif

    % Validate N_id_cell
    if(~(N_id_cell >= 0 && N_id_cell <= 503))
        printf("ERROR: Invalid N_id_cell (%u)\n", N_id_cell);
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

    % Validate N_cs_1
    if(~(N_cs_1 >= 0 && N_cs_1 <= 7))
        printf("ERROR: Invalid N_cs_1 (%u)\n", N_cs_1);
        r = 0;
        return;
    endif

    % Validate N_1_p_pucch
    if(~(N_1_p_pucch >= 0 && N_1_p_pucch < 50))
        printf("ERROR: Invalid N_1_p_pucch (%u)\n", N_1_p_pucch);
        r = 0;
        return;
    endif

    % Validate delta_pucch_shift
    if(~(delta_pucch_shift >= 0 && delta_pucch_shift <= 2))
        printf("ERROR: Invalid delta_pucch_shift (%u)\n", delta_pucch_shift);
        r = 0;
        return;
    endif

    % Validate N_ant
    if(~(N_ant == 1))
        printf("ERROR: Invalid N_ant (%u)\n", N_ant);
        r = 0;
        return;
    endif

    % Calculate N_s
    N_s = N_subfr*2;

    % Calculate N_prime
    if(N_1_p_pucch < 3*N_cs_1/delta_pucch_shift)
        N_prime = N_cs_1;
    else
        N_prime = N_sc_rb;
    endif

    % Calculate n_prime_p
    % FIXME: Only supporting normal cyclic prefix
    if(N_1_p_pucch < (3*N_cs_1/delta_pucch_shift))
        n_prime_p(1) = N_1_p_pucch;
        h_p          = mod(n_prime_p(1) + 2, 3*N_prime/delta_pucch_shift);
        n_prime_p(2) = floor(h_p/3) + mod(h_p, 3)*N_prime/delta_pucch_shift;
    else
        n_prime_p(1) = mod(N_1_p_pucch - 3*N_cs_1/delta_pucch_shift, 3*N_sc_rb/delta_pucch_shift);
        n_prime_p(2) = mod(3*(n_prime_p(1)+1), 3*N_sc_rb/delta_pucch_shift+1) - 1;
    endif

    % Calculate N_oc_p
    for(m=0:1)
        n_oc_p(m+1) = floor(n_prime_p(m+1)*delta_pucch_shift/N_prime);
    endfor

    % Generate c
    c = lte_generate_prs_c(N_id_cell, 8*N_ul_symb*20);

    % Calculate N_cs_cell
    for(m=0:1)
        for(n=0:M_pucch_rs-1)
            sum = 0;
            for(idx=0:7)
                sum = sum + c(8*N_ul_symb*(N_s+m) + 8*(2+n) + idx + 1) * 2^idx;
            endfor
            n_cs_cell(m+1, n+1) = sum;
        endfor
    endfor

    % Calculate N_cs_p
    % FIXME: Only supporting normal cyclic prefix
    for(m=0:1)
        for(n=0:M_pucch_rs-1)
            n_cs_p(m+1, n+1) = mod(n_cs_cell(m+1, n+1) + mod(n_prime_p(m+1)*delta_pucch_shift + mod(n_oc_p(m+1), delta_pucch_shift), N_prime), N_sc_rb);
        endfor
    endfor

    % Calculate alpha_p
    for(m=0:1)
        for(n=0:M_pucch_rs-1)
            alpha_p(m+1, n+1) = 2*pi*n_cs_p(m+1, n+1)/N_sc_rb;
        endfor
    endfor

    % Generate the base reference signal
    for(m=0:1)
        for(n=0:M_pucch_rs-1)
            r_u_v_alpha_p(m*M_pucch_rs + n + 1, :) = lte_generate_ul_rs(N_s + m, N_id_cell, "pucch", delta_ss, group_hopping_enabled, sequence_hopping_enabled, alpha_p(m+1, n+1), 1);
        endfor
    endfor

    % Generate the PUCCH demodulation reference signal sequence
    for(m_prime=0:1)
        for(m=0:M_pucch_rs-1)
            for(n=0:N_sc_rb-1)
                r(m_prime*M_pucch_rs*N_sc_rb + m*N_sc_rb + n + 1) = (1/sqrt(N_ant))*w_vector(n_oc_p(m_prime+1)+1, m+1)*r_u_v_alpha_p(m_prime*M_pucch_rs + m + 1, n + 1);
            endfor
        endfor
    endfor
endfunction
