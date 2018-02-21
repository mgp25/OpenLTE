%
% Copyright 2016-2017 Ben Wojtowicz
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
% Function:    lte_pucch_demod
% Description: Demodulates PUCCH
% Inputs:      pucch_bb          - PUCCH baseband I/Q
%              N_1_p_pucch       - Resource index
%              N_id_cell         - Physical layer cell identity for the eNodeB
%              N_subfr           - Subframe number of the PUSCH
%              delta_ss          - Configurable portion of the sequence-shift pattern for PUSCH (sib2 groupAssignmentPUSCH)
%              group_hop_en      - Boolean value determining if group hopping is enabled (sib2 groupHoppingEnabled)
%              seq_hop_en        - Boolean value determining if sequence hopping is enabled (sib2 sequenceHoppingEnabled)
%              N_cs_1            - Number of cyclic shifts used (sib2 nCS-AN)
%              delta_pucch_shift - Controls the amount of multiplexing on PUCCH (sib2 deltaPUCCH-Shift)
%              N_ul_rb           - Number of PRBs available in the uplink
%              fs                - Sampling rate
%              N_ant             - Number of antennas
% Outputs:     soft_bit - Demodulated PUCCH soft bit
% Spec:        3GPP TS 36.211 section 5.4, 5.5, and 5.6 v10.1.0
% Notes:       Only handling normal CP and 1 antenna
% Rev History: Ben Wojtowicz 03/12/2016 Created
%              Ben Wojtowicz 07/29/2017 Fixed a typo
%
function [soft_bit] = lte_pucch_demod(pucch_bb, N_1_p_pucch, N_id_cell, N_subfr, delta_ss, group_hop_en, seq_hop_en, N_cs_1, delta_pucch_shift, N_ul_rb, fs, N_ant)

    % Definitions
    N_cp_L_0         = 160; % FIXME: Only handling normal CP
    N_cp_L_else      = 144; % FIXME: Only handling normal CP
    N_symbs_per_slot = 7; % FIXME: Only handling normal CP
    N_sc_rb          = 12;
    N_ul_symb        = 7;
    N_pucch_seq      = 12;
    N_pucch_sf       = 4;
    w_vector(0+1,:)  = [1, 1, 1, 1];
    w_vector(1+1,:)  = [1, -1, 1, -1];
    w_vector(2+1,:)  = [1, -1, -1, 1];

    % Determine FFT size and FFT pad size
    if(fs == 1920000)
        FFT_size   = 128;
        downsample = 16;
        if(N_ul_rb == 6)
            FFT_pad_size = 28;
        else
            printf("ERROR: Invalid N_ul_rb (%u)\n", N_ul_rb);
            return
        endif
    elseif(fs == 3840000)
        FFT_size   = 256;
        downsample = 8;
        if(N_ul_rb == 6)
            FFT_pad_size = 92;
        elseif(N_ul_rb == 15)
            FFT_pad_size = 38;
        else
            printf("ERROR: Invalid N_ul_rb (%u)\n", N_ul_rb);
            return
        endif
    elseif(fs == 7680000)
        FFT_size   = 512;
        downsample = 4;
        if(N_ul_rb == 6)
            FFT_pad_size = 220;
        elseif(N_ul_rb == 15)
            FFT_pad_size = 166;
        elseif(N_ul_rb == 25)
            FFT_pad_size = 106;
        else
            printf("ERROR: Invalid N_ul_rb (%u)\n", N_ul_rb);
            return
        endif
    elseif(fs == 15360000)
        FFT_size   = 1024;
        downsample = 2;
        if(N_ul_rb == 6)
            FFT_pad_size = 476;
        elseif(N_ul_rb == 15)
            FFT_pad_size = 422;
        elseif(N_ul_rb == 25)
            FFT_pad_size = 362;
        elseif(N_ul_rb == 50)
            FFT_pad_size = 212;
        else
            printf("ERROR: Invalid N_ul_rb (%u)\n", N_ul_rb);
            return
        endif
    elseif(fs == 30720000)
        FFT_size   = 2048;
        downsample = 1;
        if(N_ul_rb == 6)
            FFT_pad_size = 988;
        elseif(N_ul_rb == 15)
            FFT_pad_size = 934;
        elseif(N_ul_rb == 25)
            FFT_pad_size = 874;
        elseif(N_ul_rb == 50)
            FFT_pad_size = 724;
        elseif(N_ul_rb == 75)
            FFT_pad_size = 574;
        elseif(N_ul_rb == 100)
            FFT_pad_size = 424;
        else
            printf("ERROR: Invalid N_ul_rb (%u)\n", N_ul_rb);
            return
        endif
    else
        printf("ERROR: Invalid fs (%u)\n", fs);
        return
    endif

    % Extract symbols
    N_prb = 1;
    for(L=0:(N_symbs_per_slot*2)-1)
        if(L < 7)
            prb_offset = N_1_p_pucch;
        else
            prb_offset = N_ul_rb-N_1_p_pucch-1;
        endif
        if(mod(L, 7) == 0)
            N_cp = N_cp_L_0 / downsample;
        else
            N_cp = N_cp_L_else / downsample;
        endif
        index = (FFT_size + (N_cp_L_else / downsample)) * L;
        if(L > 7)
            index = index + (32 / downsample);
        elseif(L > 0)
            index = index + (16 / downsample);
        endif

        tmp = fftshift(fft(pucch_bb(index+N_cp+1:index+N_cp+FFT_size), FFT_size*2));
        tmp = tmp(2:2:end);

        tmp_symbs = tmp(FFT_pad_size+1:FFT_size-FFT_pad_size);

        symbs(L+1,:) = tmp_symbs(prb_offset*12+1:prb_offset*12+(N_prb*12));
    endfor

    % Generate DMRS
    dmrs = lte_generate_dmrs_pucch_format1_1a_1b(N_subfr, N_id_cell, delta_ss, group_hop_en, seq_hop_en, N_cs_1, N_1_p_pucch, delta_pucch_shift, N_ant);

    % Determine channel estimates
    for(n=0:5)
        if(n < 3)
            symb_num = 2+n;
        else
            symb_num = 9+n-3;
        endif
        for(m=0:N_sc_rb-1)
            tmp = symbs(symb_num+1,m+1)/dmrs(n*N_sc_rb+m+1);
            mag(n+1,m+1) = abs(tmp);
            ang(n+1,m+1) = angle(tmp);
        endfor
    endfor

    % Average channel estimates
    ave_ang = zeros(2, N_sc_rb);
    ave_mag = zeros(2, N_sc_rb);
    for(n=0:2)
        for(m=0:N_sc_rb-1)
            ave_ang(1, m+1) = ave_ang(1, m+1) + (ang(n+1, m+1) / 3);
            ave_mag(1, m+1) = ave_mag(1, m+1) + (mag(n+1, m+1) / 3);
            ave_ang(2, m+1) = ave_ang(2, m+1) + (ang(n+3+1, m+1) / 3);
            ave_mag(2, m+1) = ave_mag(2, m+1) + (mag(n+3+1, m+1) / 3);
        endfor
    endfor

    % Construct full channel estimates
    for(L=0:(N_symbs_per_slot*2)-1)
        idx = floor(L/7);
        for(m=0:N_sc_rb-1)
            ce(L+1,m+1) = ave_mag(idx+1, m+1)*(cos(ave_ang(idx+1, m+1)) + j*sin(ave_ang(idx+1, m+1)));
        endfor
    endfor

    % Apply channel estimate to recover signal
    z_est  = [symbs(0+1,:), symbs(1+1,:), symbs(5+1,:), symbs(6+1,:), symbs(7+1,:), symbs(8+1,:), symbs(12+1,:), symbs(13+1,:)];
    ce_tot = [ce(0+1,:),    ce(1+1,:),    ce(5+1,:),    ce(6+1,:),    ce(7+1,:),    ce(8+1,:),    ce(12+1,:),    ce(13+1,:)];
    z      = lte_pre_decoder_and_matched_filter_ul(z_est, ce_tot, 1);

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

    % Calculate s_ns
    for(m=0:1)
        if(mod(n_prime_p(m+1), 2) == 0)
            s_ns(m+1) = 1;
        else
            s_ns(m+1) = exp(j*pi/2);
        endif
    endfor

    % Calculate N_prime
    if(N_1_p_pucch < 3*N_cs_1/delta_pucch_shift)
        N_prime = N_cs_1;
    else
        N_prime = N_sc_rb;
    endif

    % Calculate n_oc_p
    for(m=0:1)
        n_oc_p(m+1) = floor(n_prime_p(m+1)*delta_pucch_shift/N_prime);
    endfor

    % Recover yp from z
    for(m_prime=0:1)
        for(m=0:N_pucch_sf-1)
            for(n=0:N_pucch_seq-1)
                yp_est(m_prime*N_pucch_sf*N_pucch_seq + m*N_pucch_seq + n+1) = z(m_prime*N_pucch_sf*N_pucch_seq + m*N_pucch_seq + n + 1) / (s_ns(m_prime+1)*w_vector(n_oc_p(m_prime+1)+1, m+1));
            endfor
        endfor
    endfor

    % Calculate N_s
    N_s = N_subfr*2;

    % Generate c
    c = lte_generate_prs_c(N_id_cell, 8*N_ul_symb*20);

    % Calculate N_cs_cell
    for(m=0:1)
        for(n=0:N_symbs_per_slot-1)
            sum = 0;
            for(idx=0:7)
                sum = sum + c(8*N_ul_symb*(N_s+m) + 8*n + idx + 1) * 2^idx;
            endfor
            n_cs_cell(m+1, n+1) = sum;
        endfor
    endfor

    % Calculate N_cs_p
    % FIXME: Only supporting normal cyclic prefix
    for(m=0:1)
        for(n=0:N_symbs_per_slot-1)
            n_cs_p(m+1, n+1) = mod(n_cs_cell(m+1, n+1) + mod(n_prime_p(m+1)*delta_pucch_shift + mod(n_oc_p(m+1), delta_pucch_shift), N_prime), N_sc_rb);
        endfor
    endfor

    % Calculate alpha_p
    for(m=0:1)
        for(n=0:N_symbs_per_slot-1)
            alpha_p(m+1, n+1) = 2*pi*n_cs_p(m+1, n+1)/N_sc_rb;
        endfor
    endfor

    % Generate r_u_v_alpha_p
    for(m=0:1)
        for(n=0:N_symbs_per_slot-1)
            r_u_v_alpha_p(m*N_symbs_per_slot + n + 1, :) = lte_generate_ul_rs(N_s + m, N_id_cell, "pucch", delta_ss, group_hop_en, seq_hop_en, alpha_p(m+1, n+1), 1);
        endfor
    endfor

    % Recover d as a vector from yp
    for(m_prime=0:1)
        for(m=0:N_pucch_sf-1)
            symb_num = m;
            if(symb_num > 1)
                symb_num = m + 3;
            endif
            for(n=0:N_pucch_seq-1)
                d_est(m_prime*N_pucch_sf*N_pucch_seq + m*N_pucch_seq + n+1) = yp_est(m_prime*N_pucch_sf*N_pucch_seq + m*N_pucch_seq + n+1) / r_u_v_alpha_p(m_prime*N_symbs_per_slot + symb_num + 1, n+1);
            endfor
        endfor
    endfor

    % Collapse into the soft bit
    soft_bit = 0;
    for(n=0:length(d_est)-1)
        soft_bit = soft_bit + d_est(n+1);
    endfor
    soft_bit = soft_bit / length(d_est);
    soft_bit = soft_bit / (1 / sqrt(N_ant));
endfunction
