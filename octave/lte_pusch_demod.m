%
% Copyright 2014 Ben Wojtowicz
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
% Function:    lte_pusch_demod
% Description: Demodulates PUSCH
% Inputs:      pusch_bb         - PUSCH baseband I/Q
%              prb_offset       - Starting PRB of the PUSCH for the UE
%              N_prb            - Number of PRBs assigned to the UE
%              N_id_cell        - Physical layer cell identity for the eNodeB
%              rnti             - RNTI assigned to the UE
%              N_subfr          - Subframe number of the PUSCH
%              delta_ss         - Configurable portion of the sequence-shift pattern for PUSCH (sib2 groupAssignmentPUSCH)
%              group_hop_en     - Boolean value determining if group hopping is enabled (sib2 groupHoppingEnabled)
%              seq_hop_en       - Boolean value determining if sequence hopping is enabled (sib2 sequenceHoppingEnabled)
%              cyclic_shift     - Broadcast cyclic shift to apply to base reference signal (sib2 cyclicShift)
%              cyclic_shift_dci - Scheduled cyclic shift to apply to base reference signal
%              w_config         - fixed or table
%              N_ul_rb          - Number of PRBs available in the uplink
%              fs               - Sampling rate
% Outputs:     soft_bits - Demodulated PUSCH soft bits
% Spec:        3GPP TS 36.211 section 5.3, 5.5, and 5.6 v10.1.0
% Notes:       Only handling normal CP
% Rev History: Ben Wojtowicz 03/26/2014 Created
%
function [soft_bits] = lte_pusch_demod(pusch_bb, prb_offset, N_prb, N_id_cell, rnti, N_subfr, delta_ss, group_hop_en, seq_hop_en, cyclic_shift, cyclic_shift_dci, w_config, N_ul_rb, fs)

    % Definitions
    M_pusch_rb       = N_prb;
    N_sc_rb          = 12;
    M_pusch_sc       = M_pusch_rb*N_sc_rb;
    N_cp_L_0         = 160; % FIXME: Only handling normal CP
    N_cp_L_else      = 144; % FIXME: Only handling normal CP
    N_symbs_per_slot = 7; % FIXME: Only handling normal CP

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

    % Determine optimal timing
    abs_corr = zeros(1,304);
    for(L=0:N_symbs_per_slot)
        if(L == 0)
            N_cp = N_cp_L_0 / downsample;
        else
            N_cp = N_cp_L_else / downsample;
        endif
        index = (FFT_size + (N_cp_L_else / downsample)) * L;
        if(L > 0)
            index = index + (16 / downsample);
        endif

        for(n=0:(304/downsample)-1)
            for(m=0:N_cp)
                abs_corr(n+1) = abs_corr(n+1) + pusch_bb(index+n+m+1)*conj(pusch_bb(index+n+m+FFT_size+1));
            endfor
        endfor
    endfor
    [val, start_loc] = max(abs(abs_corr));
    start_loc        = start_loc - (16/downsample);

    % Extract symbols
    for(L=0:(N_symbs_per_slot*2)-1)
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
        index = index + start_loc;

        tmp = fftshift(fft(pusch_bb(index+N_cp+1:index+N_cp+FFT_size), FFT_size*2));
        tmp = tmp(2:2:end);

        tmp_symbs = tmp(FFT_pad_size+1:FFT_size-FFT_pad_size);

        symbs(L+1,:) = tmp_symbs(prb_offset*12+1:prb_offset*12+(N_prb*12));
    endfor

    % Generate DMRS, assuming 1 layer
    dmrs = lte_generate_dmrs_pusch(N_subfr, N_id_cell, delta_ss, group_hop_en, seq_hop_en, cyclic_shift, cyclic_shift_dci, w_config, N_prb, 0);

    % Extract generated and received DMRS
    symb_0 = symbs(3+1,:);
    dmrs_0 = dmrs(0+1:M_pusch_sc);
    symb_1 = symbs(10+1,:);
    dmrs_1 = dmrs(M_pusch_sc+1:M_pusch_sc*2);

    % Determine channel estimates
    for(m=0:M_pusch_sc-1)
        tmp        = symb_0(m+1)/dmrs_0(m+1);
        mag_0(m+1) = abs(tmp);
        ang_0(m+1) = angle(tmp);

        tmp        = symb_1(m+1)/dmrs_1(m+1);
        mag_1(m+1) = abs(tmp);
        ang_1(m+1) = angle(tmp);
    endfor

    % Interpolate channel estimates
    for(m=0:M_pusch_sc-1)
        frac_mag = (mag_1(m+1) - mag_0(m+1))/7;
        frac_ang = ang_1(m+1) - ang_0(m+1);
        if(frac_ang >= pi) % Wrap angle
            frac_ang = frac_ang - 2*pi;
        elseif(frac_ang <= -pi)
            frac_ang = frac_ang + 2*pi;
        endif
        frac_ang = frac_ang/7;

        for(L=0:2)
            ce_mag(L+1, m+1)    = mag_0(m+1) - (3-L)*frac_mag;
            ce_ang(L+1, m+1)    = ang_0(m+1) - (3-L)*frac_ang;
            ce_mag(4+L+1, m+1)  = mag_0(m+1) + (1+L)*frac_mag;
            ce_ang(4+L+1, m+1)  = ang_0(m+1) + (1+L)*frac_ang;
            ce_mag(7+L+1, m+1)  = mag_1(m+1) - (3-L)*frac_mag;
            ce_ang(7+L+1, m+1)  = ang_1(m+1) - (3-L)*frac_ang;
            ce_mag(11+L+1, m+1) = mag_1(m+1) + (1+L)*frac_mag;
            ce_ang(11+L+1, m+1) = ang_1(m+1) + (1+L)*frac_ang;
        endfor
        ce_mag(3+1, m+1)  = mag_0(m+1);
        ce_ang(3+1, m+1)  = ang_0(m+1);
        ce_mag(10+1, m+1) = mag_1(m+1);
        ce_ang(10+1, m+1) = ang_1(m+1);
    endfor

    % Construct full channel estimates
    for(L=0:(N_symbs_per_slot*2)-1)
        for(m=0:M_pusch_sc-1)
            ce(L+1,m+1) = ce_mag(L+1,m+1)*(cos(ce_ang(L+1,m+1)) + j*sin(ce_ang(L+1,m+1)));
        endfor
    endfor

    z_est  = [symbs(0+1,:), symbs(1+1,:), symbs(2+1,:), symbs(4+1,:), symbs(5+1,:), symbs(6+1,:), symbs(7+1,:), symbs(8+1,:), symbs(9+1,:), symbs(11+1,:), symbs(12+1,:), symbs(13+1,:)];
    ce_tot = [ce(0+1,:),    ce(1+1,:),    ce(2+1,:),    ce(4+1,:),    ce(5+1,:),    ce(6+1,:),    ce(7+1,:),    ce(8+1,:),    ce(9+1,:),    ce(11+1,:),    ce(12+1,:),    ce(13+1,:)];
    y      = lte_pre_decoder_and_matched_filter_ul(z_est, ce_tot, 1);
    for(L=0:(length(y)/M_pusch_sc)-1)
        x(L*M_pusch_sc+1:(L+1)*M_pusch_sc) = sqrt(M_pusch_sc) * ifft(y(L*M_pusch_sc+1:(L+1)*M_pusch_sc));
    endfor
    d         = lte_layer_demapper_ul(x, 1, 1);
    soft_bits = lte_modulation_demapper(d, "qpsk");
    q         = 0; % Only allowing one codeword
    c_init    = rnti*2^14 + q*2^13 + N_subfr*2^9 + N_id_cell;
    c         = 1 - 2*lte_generate_prs_c(c_init, length(soft_bits));
    soft_bits = soft_bits.*c;

endfunction
