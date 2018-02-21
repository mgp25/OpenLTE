%
% Copyright 2011-2014 Ben Wojtowicz
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
% Function:    lte_fdd_dl_receive
% Description: Synchronizes to an FDD LTE signal and
%              decodes control information
% Inputs:      input_samps - LTE signal sampled at 30.72MHz
%                            of instantaneous BW.
% Outputs:     None
% Spec:        N/A
% Notes:       Only supports normal cyclic prefix
% Rev History: Ben Wojtowicz 12/26/2011 Created
%              Ben Wojtowicz 01/29/2012 Fixed license statement and bug
%                                       with find_pss_and_fine_timing
%              Ben Wojtowicz 02/16/2012 Added SIB1 decoding, fixed bugs
%                                       in channel estimator
%              Ben Wojtowicz 03/17/2013 Fixed a bug calculating the number
%                                       of CCE resources and skipped PBCH,
%                                       PSS and SSS while decoding PDSCH
%              Ben Wojtowicz 03/26/2014 Using the renamed lte_layer_demapper_dl
%                                       and lte_pre_decoder_and_matched_filter_dl
%                                       functions.
%
function [] = lte_fdd_dl_receive(input_samps)
    % DEFINES
    N_sc_rb           = 12; % Only dealing with normal cp at this time
    N_symb_dl         = 7;  % Only dealing with normal cp at this time
    N_slots_per_frame = 20;
    N_cp_l_0          = 160;
    N_cp_l_else       = 144;
    N_rb_dl_max       = 110;

    % Assume bandwidth of 1.4MHz until MIB is decoded
    N_rb_dl      = 6;
    FFT_pad_size = 988; % FFT_size = 2048

    % Check input length
    if(length(input_samps) < 307200*8)
        printf("ERROR: Not enough samples\n");
        return;
    endif

    % Find symbol start locations
    [start_loc_vec, freq_offset] = find_coarse_time_and_freq_offset(input_samps, N_cp_l_else);

    % Remove frequency error
    freq_offset_vec = cos((1:length(input_samps))*(-freq_offset)*2*pi*(0.0005/15360)) + j*sin((1:length(input_samps))*(-freq_offset)*2*pi*(0.0005/15360));
    input_samps     = input_samps ./ freq_offset_vec;

    % Search for PSS and find fine timing offset
    pss_symb = 0;
    [start_loc_vec, N_id_2, pss_symb, pss_thresh] = find_pss_and_fine_timing(input_samps, start_loc_vec);
    if(pss_symb ~= 0)
        printf("Found PSS-%u in %u (%f)\n", N_id_2, pss_symb, pss_thresh);
    else
        printf("ERROR: Didn't find PSS\n");
        return;
    endif

    % Find SSS
    f_start_idx = 0;
    pss_thresh;
    [N_id_1, f_start_idx] = find_sss(input_samps, N_id_2, start_loc_vec, pss_thresh);
    if(f_start_idx ~= 0)
        while(f_start_idx < 0)
            f_start_idx = f_start_idx + 307200;
        endwhile
        printf("Found SSS-%u, 0 index is %u, cell_id is %u\n", N_id_1, f_start_idx, 3*N_id_1 + N_id_2);
    else
        printf("ERROR: Didn't find SSS\n");
        return;
    endif

    % Redefine input and start vector to the 0 index
    input_samps   = input_samps(f_start_idx:end);
    f_start_idx   = 0;
    start_loc_vec = [1, 2209, 4401, 6593, 8785, 10977, 13169];

    % Construct N_id_cell
    N_id_cell = 3*N_id_1 + N_id_2;

    % Decode MIB
    num_decode_attempts = 0;
    while(num_decode_attempts < 4)
        [mib_symb, mib_ce] = get_subframe_and_ce(input_samps, f_start_idx, 0, FFT_pad_size, N_rb_dl, N_id_cell, 4);
        mib_s              = decode_mib(mib_ce, mib_symb, N_id_cell);
        if(mib_s.N_ant != 0)
            break;
        endif
        f_start_idx = f_start_idx + 307200;
        if(f_start_idx > length(input_samps))
            printf("ERROR: Ran out of samples\n");
            return;
        endif
        num_decode_attempts = num_decode_attempts + 1;
    endwhile
    if(mib_s.N_ant != 0)
        printf("Found BCH, N_ant = %u, N_rb_dl = %u, phich_duration = %s, phich_resource = %u, sfn = %u\n", mib_s.N_ant, mib_s.bw, mib_s.phich_dur, mib_s.phich_res, mib_s.sfn);
        sfn = mib_s.sfn;
    else
        printf("ERROR: BCH NOT FOUND\n");
        return;
    endif

    % Redefine N_rb_dl and FFT_pad_size with the actual bandwidth
    N_rb_dl = mib_s.bw;
    if(N_rb_dl == 6)
        FFT_pad_size = 988; % FFT_size = 2048
    elseif(N_rb_dl == 15)
        FFT_pad_size = 934; % FFT_size = 2048
    elseif(N_rb_dl == 25)
        FFT_pad_size = 874; % FFT_size = 2048
    elseif(N_rb_dl == 50)
        FFT_pad_size = 724; % FFT_size = 2048
    elseif(N_rb_dl == 75)
        FFT_pad_size = 574; % FFT_size = 2048
    else % N_rb_dl == 100
        FFT_pad_size = 424; % FFT_size = 2048
    endif

    % Decode SIB1
    % If sfn%2 != 0, move the f_start_idx forward by one frame
    if(mod(mib_s.sfn, 2) != 0)
        f_start_idx = f_start_idx + 307200;
        sfn         = sfn + 1;
    endif
    % Decode all control channels for subframe 5
    num_decode_attempts = 0;
    while(num_decode_attempts < 4)
        [si_symb, si_ce]             = get_subframe_and_ce(input_samps, f_start_idx, 5, FFT_pad_size, N_rb_dl, N_id_cell, mib_s.N_ant);
        [pcfich_s, phich_s, pdcch_s] = decode_pdcch(si_ce, si_symb, 5, mib_s, N_id_cell, N_sc_rb, N_rb_dl);
        if(pdcch_s.N_alloc != 0)
            break;
        endif
        f_start_idx = f_start_idx + 307200*2;
        sfn         = sfn + 2;
        if(f_start_idx > length(input_samps))
            printf("ERROR: Ran out of samples\n");
            return;
        endif
        num_decode_attempts = num_decode_attempts + 1;
    endwhile
    if(pdcch_s.N_alloc != 0)
        % Decode PDSCH
        [pdsch_s]   = decode_pdsch(si_ce, si_symb, 5, pcfich_s, phich_s, pdcch_s, mib_s, N_id_cell, N_sc_rb, N_rb_dl);
        sib1_decode = 0;
        for(n=1:pdsch_s.N_alloc)
            [sib_num, sib] = lte_bcch_dlsch_msg_unpack(pdsch_s.alloc_s(n).bits);
            if(sib_num == 1)
                [sib1_s] = lte_sib1_unpack(sib);
                if(sib1_s.plmn_ids.num_ids != 0)
                    sib1_decode = 1;
                endif
            endif
        endfor
        if(sib1_decode == 1)
            printf("Found SIB1, Band = %u, PLMN = %u-%u, TAC = 0x%04X, Cell ID = 0x%08X, Cell barred = %u\n", sib1_s.band, sib1_s.plmn_ids.id(1).mcc, sib1_s.plmn_ids.id(1).mnc, sib1_s.tac, sib1_s.cell_id, sib1_s.cell_barred);
        else
            printf("ERROR: SIB1 NOT FOUND\n");
            return;
        endif
    else
        printf("ERROR: SIB1 PDCCH NOT FOUND\n");
        return;
    endif
endfunction

function [coarse_start, freq_offset] = find_coarse_time_and_freq_offset(in, N_cp_l_else)

    % Decompose input
    in_re = real(in);
    in_im = imag(in);

    % Can only rely on symbols 0 and 4 because of CRS

    % Rough correlation
    abs_corr = zeros(1,15360);
    for(slot=0:10)
        for(n=1:40:15360)
            corr_re = 0;
            corr_im = 0;
            for(z=1:N_cp_l_else)
                index   = (slot*15360) + n-1 + z;
                corr_re = corr_re + in_re(index)*in_re(index+2048) + in_im(index)*in_im(index+2048);
                corr_im = corr_im + in_re(index)*in_im(index+2048) - in_im(index)*in_re(index+2048);
            endfor
            abs_corr(n) = abs_corr(n) + corr_re*corr_re + corr_im*corr_im;
        endfor
    endfor

    % Find first and second max
    abs_corr_idx = zeros(1,2);
    for(m=0:1)
        abs_corr_max = 0;
        for(n=1:7680)
            if(abs_corr((m*7680)+n) > abs_corr_max)
                abs_corr_max      = abs_corr((m*7680)+n);
                abs_corr_idx(m+1) = (m*7680)+n;
            endif
        endfor
    endfor

    % Fine correlation and fraction frequency offset
    abs_corr      = zeros(1,15360);
    corr_freq_err = zeros(1,15360);
    for(slot=1:10)
        for(idx=1:2)
            if((abs_corr_idx(idx) - 40) < 1)
                abs_corr_idx(idx) = 41;
            endif
            if((abs_corr_idx(idx) + 40) > 15360)
                abs_corr_idx(idx) = 15360 - 40;
            endif
            for(n=abs_corr_idx(idx)-40:abs_corr_idx(idx)+40)
                corr_re = 0;
                corr_im = 0;
                for(z=1:N_cp_l_else)
                    index = (slot*15360) + n-1 + z;
                    corr_re = corr_re + in_re(index)*in_re(index+2048) + in_im(index)*in_im(index+2048);
                    corr_im = corr_im + in_re(index)*in_im(index+2048) - in_im(index)*in_re(index+2048);
                endfor
                abs_corr(n)      = abs_corr(n) + corr_re*corr_re + corr_im*corr_im;
                corr_freq_err(n) = corr_freq_err(n) + atan2(corr_im, corr_re)/(2048*2*pi*(0.0005/15360));
            endfor
        endfor
    endfor

    % Find first and second max
    abs_corr_idx = zeros(1,2);
    for(m=0:1)
        abs_corr_max = 0;
        for(n=1:7680)
            if(abs_corr((m*7680)+n) > abs_corr_max)
                abs_corr_max      = abs_corr((m*7680)+n);
                abs_corr_idx(m+1) = (m*7680)+n;
            endif
        endfor
    endfor

    % Determine frequency offset FIXME No integer offset is calculated here
    freq_offset = (corr_freq_err(abs_corr_idx(1))/10 + corr_freq_err(abs_corr_idx(2))/10)/2;

    % Determine the symbol start locations from the correlation peaks
    % FIXME Needs some work
    tmp = abs_corr_idx(1);
    while(tmp > 0)
        tmp = tmp - 2192;
    endwhile
    for(n=1:7)
        coarse_start(n) = tmp + (n*2192);
    endfor
endfunction

function [fine_start, N_id_2, pss_symb, pss_thresh] = find_pss_and_fine_timing(input, coarse_start)
    % DEFINES Assuming 20MHz
    N_rb_dl      = 100;
    N_sc_rb      = 12; % Only dealing with normal cp at this time
    N_symb_dl    = 7;  % Only dealing with normal cp at this time
    N_cp_l_else  = 144;
    FFT_pad_size = 424;

    % Generate primary synchronization signals
    pss_mod_vec = zeros(3, N_rb_dl*N_sc_rb);
    for(loc_N_id_2=0:2)
        pss = lte_generate_pss(loc_N_id_2);
        for(n=0:61)
            k = n - 31 + (N_rb_dl*N_sc_rb)/2;
            pss_mod_vec(loc_N_id_2+1, k+1) = pss(n+1);
        endfor
    endfor

    % Demod symbols and correlate with primary synchronization signals
    num_slots_to_demod = floor(200000/15360)-1;
    for(n=0:num_slots_to_demod-1)
        for(m=1:N_symb_dl)
            symb = conj(samps_to_symbs(input, coarse_start(m)+(15360*n), 0, FFT_pad_size, 0)); % FIXME Not sure why conj is needed

            for(loc_N_id_2=0:2)
                pss_corr = 0;
                for(z=1:N_rb_dl*N_sc_rb)
                    pss_corr = pss_corr + symb(z)*pss_mod_vec(loc_N_id_2+1, z);
                endfor
                pss_corr_mat((n*N_symb_dl)+m, loc_N_id_2+1) = abs(pss_corr);
            endfor
        endfor
    endfor

    % Find maximum
    [val, abs_slot_num]  = max(pss_corr_mat);
    [val, N_id_2_plus_1] = max(val);
    pss_symb             = abs_slot_num(N_id_2_plus_1);

    % Find optimal timing
    N_s    = floor((abs_slot_num(N_id_2_plus_1)-1)/7);
    N_symb = mod(abs_slot_num(N_id_2_plus_1)-1, 7)+1;
    N_id_2 = N_id_2_plus_1 - 1;
    for(timing=-40:40)
        symb = conj(samps_to_symbs(input, coarse_start(N_symb)+timing+(15360*N_s), 0, FFT_pad_size, 0)); % FIXME Not sure why conj is needed

        pss_corr = 0;
        for(z=1:N_rb_dl*N_sc_rb)
            pss_corr = pss_corr + symb(z)*pss_mod_vec(N_id_2+1, z);
        endfor
        timing_vec(timing+41) = abs(pss_corr);
    endfor
    [pss_thresh, timing_plus_41] = max(timing_vec);

    % Construct fine symbol start locations
    pss_timing_idx = coarse_start(N_symb)+(15360*N_s)+timing_plus_41-41;
    fine_start(1)  = pss_timing_idx + (2048+144)*1 - 15360;
    fine_start(2)  = pss_timing_idx + (2048+144)*1 + 2048+160 - 15360;
    fine_start(3)  = pss_timing_idx + (2048+144)*2 + 2048+160 - 15360;
    fine_start(4)  = pss_timing_idx + (2048+144)*3 + 2048+160 - 15360;
    fine_start(5)  = pss_timing_idx + (2048+144)*4 + 2048+160 - 15360;
    fine_start(6)  = pss_timing_idx + (2048+144)*5 + 2048+160 - 15360;
    fine_start(7)  = pss_timing_idx + (2048+144)*6 + 2048+160 - 15360;
    while(fine_start(1) < 1)
        fine_start = fine_start + 307200;
    endwhile
endfunction

function [N_id_1, f_start_idx] = find_sss(input, N_id_2, start, pss_thresh)
    % DEFINES Assuming 20MHz
    N_rb_dl      = 100;
    N_sc_rb      = 12; % Only dealing with normal cp at this time
    N_symb_dl    = 7;  % Only dealing with normal cp at this time
    N_cp_l_0     = 160;
    N_cp_l_else  = 144;
    FFT_pad_size = 424;

    % Generate secondary synchronization signals
    sss_mod_vec_0 = zeros(167, N_rb_dl*N_sc_rb);
    sss_mod_vec_5 = zeros(167, N_rb_dl*N_sc_rb);
    for(loc_N_id_1=0:167)
        [sss_0, sss_5] = lte_generate_sss(loc_N_id_1, N_id_2);
        for(m=0:61)
            k = m - 31 + (N_rb_dl*N_sc_rb)/2;
            sss_mod_vec_0(loc_N_id_1+1, k+1) = sss_0(m+1);
            sss_mod_vec_5(loc_N_id_1+1, k+1) = sss_5(m+1);
        endfor
    endfor
    sss_thresh = pss_thresh * .9;

    % Demod symbol and search for secondary synchronization signals
    symb = samps_to_symbs(input, start(6), 0, FFT_pad_size, 0);
    for(loc_N_id_1=0:167)
        sss_corr = 0;
        for(m=1:(N_rb_dl*N_sc_rb))
            sss_corr = sss_corr + symb(m)*sss_mod_vec_0(loc_N_id_1+1, m);
        endfor
        if(abs(sss_corr) > sss_thresh)
            N_id_1      = loc_N_id_1;
            f_start_idx = start(6) - ((2048+N_cp_l_else)*4 + 2048+N_cp_l_0);
            break;
        endif

        sss_corr = 0;
        for(m=1:(N_rb_dl*N_sc_rb))
            sss_corr = sss_corr + symb(m)*sss_mod_vec_5(loc_N_id_1+1, m);
        endfor
        if(abs(sss_corr) > sss_thresh)
            N_id_1      = loc_N_id_1;
            f_start_idx = start(6) - ((2048+N_cp_l_else)*4 + 2048+N_cp_l_0);
            f_start_idx = f_start_idx - 15360*10;
            break;
        endif
    endfor
endfunction

function [symb, ce] = get_subframe_and_ce(samps, f_start_idx, N_sf, FFT_pad_size, N_rb_dl, N_id_cell, N_ant)
    N_sc_rb      = 12; % Only dealing with normal cp at this time
    N_rb_dl_max  = 110;
    v_shift      = mod(N_id_cell, 6);
    sf_start_idx = f_start_idx + N_sf*30720;
    crs0         = lte_generate_crs(mod(N_sf*2+0, 20), 0, N_id_cell);
    crs1         = lte_generate_crs(mod(N_sf*2+0, 20), 1, N_id_cell);
    crs4         = lte_generate_crs(mod(N_sf*2+0, 20), 4, N_id_cell);
    crs7         = lte_generate_crs(mod(N_sf*2+1, 20), 0, N_id_cell);
    crs8         = lte_generate_crs(mod(N_sf*2+1, 20), 1, N_id_cell);
    crs11        = lte_generate_crs(mod(N_sf*2+1, 20), 4, N_id_cell);
    crs14        = lte_generate_crs(mod(N_sf*2+2, 20), 0, N_id_cell);
    crs15        = lte_generate_crs(mod(N_sf*2+2, 20), 1, N_id_cell);

    for(n=0:15)
        if(n < 7)
            idx = sf_start_idx;
        elseif(n < 14)
            idx = sf_start_idx + 15360;
        else
            idx = sf_start_idx + 2*15360;
        endif
        symb(n+1,:) = samps_to_symbs(samps, idx, mod(n,7), FFT_pad_size, 0);
    endfor

    for(p=0:N_ant-1)
        % Define v, crs, sym, and N_sym
        if(p == 0)
            v     = [0, 3, 0, 3, 0];
            crs   = [crs0; crs4; crs7; crs11; crs14];
            sym   = [symb(0+1,:); symb(4+1,:); symb(7+1,:); symb(11+1,:); symb(14+1,:)];
            N_sym = 5;
        elseif(p == 1)
            v     = [3, 0, 3, 0, 3];
            crs   = [crs0; crs4; crs7; crs11; crs14];
            sym   = [symb(0+1,:); symb(4+1,:); symb(7+1,:); symb(11+1,:); symb(14+1,:)];
            N_sym = 5;
        elseif(p == 2)
            v     = [0, 3, 0];
            crs   = [crs1; crs8; crs15];
            sym   = [symb(1+1,:); symb(8+1,:); symb(15+1,:)];
            N_sym = 3;
        else % p == 3
            v     = [3, 6, 3];
            crs   = [crs1; crs8; crs15];
            sym   = [symb(1+1,:); symb(8+1,:); symb(15+1,:)];
            N_sym = 3;
        endif

        for(n=1:N_sym)
            for(m=0:2*N_rb_dl-1)
                k          = 6*m + mod((v(n) + v_shift), 6);
                m_prime    = m + N_rb_dl_max - N_rb_dl;
                tmp        = sym(n,k+1)/crs(n,m_prime+1);
                mag(n,k+1) = abs(tmp);
                ang(n,k+1) = angle(tmp);

                % Unwrap phase
                if(m > 0)
                    while((ang(n,k+1) - ang(n,k-6+1)) > pi)
                        ang(n,k+1) = ang(n,k+1) - 2*pi;
                    endwhile
                    while((ang(n,k+1) - ang(n,k-6+1)) < -pi)
                        ang(n,k+1) = ang(n,k+1) + 2*pi;
                    endwhile
                endif

                % Interpolate between CRSs (simple linear interpolation)
                if(m > 0)
                    frac_mag = (mag(n,k+1) - mag(n,k-6+1))/6;
                    frac_ang = (ang(n,k+1) - ang(n,k-6+1))/6;
                    for(o=1:5)
                        mag(n,k-o+1) = mag(n,k-(o-1)+1) - frac_mag;
                        ang(n,k-o+1) = ang(n,k-(o-1)+1) - frac_ang;
                    endfor
                endif

                % Interpolate before 1st CRS
                if(m == 1)
                    for(o=1:mod(v(n) + v_shift, 6))
                        mag(n,k-6-o+1) = mag(n,k-6-(o-1)+1) - frac_mag;
                        ang(n,k-6-o+1) = ang(n,k-6-(o-1)+1) - frac_ang;
                    endfor
                endif
            endfor

            % Interpolate after last CRS
            for(o=1:(5-mod(v(n) + v_shift, 6)))
                mag(n,k+o+1) = mag(n,k+(o-1)+1) - frac_mag;
                ang(n,k+o+1) = ang(n,k+(o-1)+1) - frac_ang;
            endfor
        endfor

        % Interpolate between symbols and construct channel estimates
        if(N_sym == 3)
            for(n=1:N_sc_rb*N_rb_dl)
                % Construct symbol 1 and 8 channel estimates directly
                ce(p+1,1+1,n) = mag(1,n)*(cos(ang(1,n)) + j*sin(ang(1,n)));
                ce(p+1,8+1,n) = mag(2,n)*(cos(ang(2,n)) + j*sin(ang(2,n)));

                % Interpolate for symbol 2, 3, 4, 5, 6, and 7 channel estimates
                frac_mag = (mag(2,n) - mag(1,n))/7;
                frac_ang = ang(2,n) - ang(1,n);
                if(frac_ang >= pi) % Wrap angle
                    frac_ang = frac_ang - 2*pi;
                elseif(frac_ang <= -pi)
                    frac_ang = frac_ang + 2*pi;
                endif
                frac_ang = frac_ang/7;
                ce_mag   = mag(2,n);
                ce_ang   = ang(2,n);
                for(o=7:-1:2)
                    ce_mag        = ce_mag - frac_mag;
                    ce_ang        = ce_ang - frac_ang;
                    ce(p+1,o+1,n) = ce_mag*(cos(ce_ang) + j*sin(ce_ang));
                endfor

                % Interpolate for symbol 0 channel estimate
                % FIXME: Use previous slot to do this correctly
                ce_mag        = mag(1,n) - frac_mag;
                ce_ang        = ang(1,n) - frac_ang;
                ce(p+1,0+1,n) = ce_mag*(cos(ce_ang) + j*sin(ce_ang));

                % Interpolate for symbol 9, 10, 11, 12, and 13 channel estimates
                frac_mag = (mag(3,n) - mag(2,n))/7;
                frac_ang = ang(3,n) - ang(2,n);
                if(frac_ang >= pi) % Wrap angle
                    frac_ang = frac_ang - 2*pi;
                elseif(frac_ang <= -pi)
                    frac_ang = frac_ang + 2*pi;
                endif
                frac_ang = frac_ang/7;
                ce_mag   = mag(3,n) - frac_mag;
                ce_ang   = ang(3,n) - frac_ang;
                for(o=13:-1:9)
                    ce_mag        = ce_mag - frac_mag;
                    ce_ang        = ce_ang - frac_ang;
                    ce(p+1,o+1,n) = ce_mag*(cos(ce_ang) + j*sin(ce_ang));
                endfor
            endfor
        else
            for(n=1:N_sc_rb*N_rb_dl)
                % Construct symbol 0, 4, 7, and 11 channel estimates directly
                ce(p+1,0+1,n)  = mag(1,n)*(cos(ang(1,n)) + j*sin(ang(1,n)));
                ce(p+1,4+1,n)  = mag(2,n)*(cos(ang(2,n)) + j*sin(ang(2,n)));
                ce(p+1,7+1,n)  = mag(3,n)*(cos(ang(3,n)) + j*sin(ang(3,n)));
                ce(p+1,11+1,n) = mag(4,n)*(cos(ang(4,n)) + j*sin(ang(4,n)));

                % Interpolate for symbol 1, 2, and 3 channel estimates
                frac_mag = (mag(2,n) - mag(1,n))/4;
                frac_ang = ang(2,n) - ang(1,n);
                if(frac_ang >= pi) % Wrap angle
                    frac_ang = frac_ang - 2*pi;
                elseif(frac_ang <= -pi)
                    frac_ang = frac_ang + 2*pi;
                endif
                frac_ang = frac_ang/4;
                ce_mag   = mag(2,n);
                ce_ang   = ang(2,n);
                for(o=3:-1:1)
                    ce_mag        = ce_mag - frac_mag;
                    ce_ang        = ce_ang - frac_ang;
                    ce(p+1,o+1,n) = ce_mag*(cos(ce_ang) + j*sin(ce_ang));
                endfor

                % Interpolate for symbol 5 and 6 channel estimates
                frac_mag = (mag(3,n) - mag(2,n))/3;
                frac_ang = ang(3,n) - ang(2,n);
                if(frac_ang >= pi) % Wrap angle
                    frac_ang = frac_ang - 2*pi;
                elseif(frac_ang <= -pi)
                    frac_ang = frac_ang + 2*pi;
                endif
                frac_ang = frac_ang/3;
                ce_mag   = mag(3,n);
                ce_ang   = ang(3,n);
                for(o=6:-1:5)
                    ce_mag        = ce_mag - frac_mag;
                    ce_ang        = ce_ang - frac_ang;
                    ce(p+1,o+1,n) = ce_mag*(cos(ce_ang) + j*sin(ce_ang));
                endfor

                % Interpolate for symbol 8, 9, and 10 channel estimates
                frac_mag = (mag(4,n) - mag(3,n))/4;
                frac_ang = ang(4,n) - ang(3,n);
                if(frac_ang >= pi) % Wrap angle
                    frac_ang = frac_ang - 2*pi;
                elseif(frac_ang <= -pi)
                    frac_ang = frac_ang + 2*pi;
                endif
                frac_ang = frac_ang/4;
                ce_mag   = mag(4,n);
                ce_ang   = ang(4,n);
                for(o=10:-1:8)
                    ce_mag        = ce_mag - frac_mag;
                    ce_ang        = ce_ang - frac_ang;
                    ce(p+1,o+1,n) = ce_mag*(cos(ce_ang) + j*sin(ce_ang));
                endfor

                % Interpolate for symbol 12 and 13 channel estimates
                frac_mag = (mag(5,n) - mag(4,n))/3;
                frac_ang = ang(5,n) - ang(4,n);
                if(frac_ang >= pi) % Wrap angle
                    frac_ang = frac_ang - 2*pi;
                elseif(frac_ang <= -pi)
                    frac_ang = frac_ang + 2*pi;
                endif
                frac_ang = frac_ang/3;
                ce_mag   = mag(5,n);
                ce_ang   = ang(5,n);
                for(o=13:-1:12)
                    ce_mag        = ce_mag - frac_mag;
                    ce_ang        = ce_ang - frac_ang;
                    ce(p+1,o+1,n) = ce_mag*(cos(ce_ang) + j*sin(ce_ang));
                endfor
            endfor
        endif
    endfor
endfunction

function [mib_s] = decode_mib(ce_sf, symb, N_id_cell)
    % Unmap PBCH and channel estimate from resource elements
    N_id_cell_mod_3 = mod(N_id_cell, 3);
    idx             = 0;
    for(n=0:71)
        if(N_id_cell_mod_3 != mod(n,3))
            y_est(idx+1)    = symb(7+1,n+1);
            y_est(idx+48+1) = symb(8+1,n+1);
            for(p=0:3)
                ce(p+1,idx+1)    = ce_sf(p+1,7+1,n+1);
                ce(p+1,idx+48+1) = ce_sf(p+1,8+1,n+1);
            endfor
            idx = idx + 1;
        endif
        y_est(n+96+1)  = symb(9+1,n+1);
        y_est(n+168+1) = symb(10+1,n+1);
        for(p=0:3)
            ce(p+1,n+96+1)  = ce_sf(p+1,9+1,n+1);
            ce(p+1,n+168+1) = ce_sf(p+1,10+1,n+1);
        endfor
    endfor

    % Generate the scrambling sequence in NRZ
    c = 1 - 2*lte_generate_prs_c(N_id_cell, 1920);

    % Try decoding with 1, 2, and 4 antennas
    for(n=[1,2,4])
        x    = lte_pre_decoder_and_matched_filter_dl(y_est, ce(1:n,:), "tx_diversity");
        d    = lte_layer_demapper_dl(x, 1, "tx_diversity");
        bits = lte_modulation_demapper(d, "qpsk");

        % Try decoding at each mod 4 offset
        [msg, mib_s.N_ant] = lte_bch_channel_decode([bits.*c(1:480), 10000*ones(1,1440)]);
        if(mib_s.N_ant != 0)
            % Unpack the mib and convert sfn_mod_4 to sfn
            [mib_s.bw, mib_s.phich_dur, mib_s.phich_res, sfn_mod_4] = lte_bcch_bch_msg_unpack(msg);
            mib_s.sfn                                               = sfn_mod_4;
            break;
        endif
        [msg, mib_s.N_ant] = lte_bch_channel_decode([10000*ones(1,480), bits.*c(481:960), 10000*ones(1,960)]);
        if(mib_s.N_ant != 0)
            % Unpack the mib and convert sfn_mod_4 to sfn
            [mib_s.bw, mib_s.phich_dur, mib_s.phich_res, sfn_mod_4] = lte_bcch_bch_msg_unpack(msg);
            mib_s.sfn                                               = sfn_mod_4 + 1;
            break;
        endif
        [msg, mib_s.N_ant] = lte_bch_channel_decode([10000*ones(1,960), bits.*c(961:1440), 10000*ones(1,480)]);
        if(mib_s.N_ant != 0)
            % Unpack the mib and convert sfn_mod_4 to sfn
            [mib_s.bw, mib_s.phich_dur, mib_s.phich_res, sfn_mod_4] = lte_bcch_bch_msg_unpack(msg);
            mib_s.sfn                                               = sfn_mod_4 + 2;
            break;
        endif
        [msg, mib_s.N_ant] = lte_bch_channel_decode([10000*ones(1,1440), bits.*c(1441:end)]);
        if(mib_s.N_ant != 0)
            % Unpack the mib and convert sfn_mod_4 to sfn
            [mib_s.bw, mib_s.phich_dur, mib_s.phich_res, sfn_mod_4] = lte_bcch_bch_msg_unpack(msg);
            mib_s.sfn                                               = sfn_mod_4 + 3;
            break;
        endif
    endfor
endfunction

function [pcfich_s, phich_s, pdcch_s] = decode_pdcch(ce_sf, symb, N_sf, mib_s, N_id_cell, N_sc_rb, N_rb_dl)
    % Defines
    SI_RNTI = 65535;

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    % PCFICH
    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    % Calculate resources 36.211 section 6.7.4 v10.1.0
    pcfich_s.N_reg = 4;
    k_hat          = (N_sc_rb/2)*mod(N_id_cell, 2*N_rb_dl);
    for(n=0:pcfich_s.N_reg-1)
        cfi_k(n+1) = mod(k_hat + floor(n*N_rb_dl/2)*N_sc_rb/2, N_rb_dl*N_sc_rb);
        cfi_n(n+1) = cfi_k(n+1)/6 - 0.5;

        % Extract resource elements and channel estimate
        idx = 0;
        for(m=0:5)
            if(mod(N_id_cell, 3) != mod(m, 3))
                pcfich_y_est(idx+(n*4)+1) = symb(1,cfi_k(n+1)+m+1);
                for(p=0:mib_s.N_ant-1)
                    pcfich_ce(p+1,idx+(n*4)+1) = ce_sf(p+1,1,cfi_k(n+1)+m+1);
                endfor
                idx = idx + 1;
            endif
        endfor
    endfor
    % Decode 36.211 section 6.7 v10.1.0
    pcfich_c_init = (N_sf + 1)*(2*N_id_cell + 1)*2^9 + N_id_cell;
    pcfich_c      = 1 - 2*lte_generate_prs_c(pcfich_c_init, 32);
    pcfich_x      = lte_pre_decoder_and_matched_filter_dl(pcfich_y_est, pcfich_ce, "tx_diversity");
    pcfich_d      = lte_layer_demapper_dl(pcfich_x, 1, "tx_diversity");
    pcfich_bits   = lte_modulation_demapper(pcfich_d, "qpsk");
    pcfich_s.cfi  = lte_cfi_channel_decode(pcfich_bits.*pcfich_c);

    if(pcfich_s.cfi != 0)
        %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        % PHICH
        %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        % Calculate resources 36.211 section 6.9 v10.1.0
        phich_s.N_group = ceil(mib_s.phich_res*(N_rb_dl/8)); % FIXME: Only handles normal CP
        phich_s.N_reg   = phich_s.N_group*3;
        % Step 4
        m_prime = 0;
        idx     = 0;
        % Step 10
        while(m_prime < phich_s.N_group)
            if(mib_s.phich_dur == "normal")
                % Step 7
                l_prime = 0;
                % Step 1, 2, and 3
                n_l_prime = N_rb_dl*2 - pcfich_s.N_reg;
                % Step 8
                for(n=0:2)
                    n_hat(n+1) = mod(N_id_cell + m_prime + floor(n*n_l_prime/3), n_l_prime);
                endfor
                % Avoid PCFICH
                for(n=0:pcfich_s.N_reg-1)
                    for(m=0:2)
                        if(n_hat(m+1) > cfi_n(n+1))
                            n_hat(m+1) = n_hat(m+1) + 1;
                        endif
                    endfor
                endfor
                % Step 5
                for(n=0:2)
                    phich_k(idx+n+1) = n_hat(n+1)*6;
                    % FIXME: Not currently implementing step 6
                endfor
                idx = idx + 3;
            else
                printf("ERROR: Not handling extended PHICH duration\n");
                phich_s.N_group = 0;
                pdcch_s.N_alloc = 0;
                return;
            endif
            % Step 9
            m_prime = m_prime + 1;
        endwhile

        %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        % PDCCH
        %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        % Calculate number of symbols 36.211 section 6.7 v10.1.0
        pdcch_s.N_symbs = pcfich_s.cfi;
        if(N_rb_dl <= 10)
            pdcch_s.N_symbs = pdcch_s.N_symbs + 1;
        endif
        % Calculate resources 36.211 section 6.8.1 v10.1.0
        N_reg_rb      = 3;
        pdcch_s.N_reg = pdcch_s.N_symbs*(N_rb_dl*N_reg_rb) - N_rb_dl - pcfich_s.N_reg - phich_s.N_reg;
        if(mib_s.N_ant == 4)
            pdcch_s.N_reg = pdcch_s.N_reg - N_rb_dl; % Remove CRS
        endif
        pdcch_s.N_cce = floor(pdcch_s.N_reg/9);
        % Extract resource elements and channel estimate 36.211 section 6.8.5 v10.1.0
        % Step 1 and 2
        m_prime = 0;
        k_prime = 0;
        % Step 10
        while(k_prime < (N_rb_dl*N_sc_rb))
            % Step 3
            l_prime = 0;
            % Step 8
            while(l_prime < pdcch_s.N_symbs)
                if(l_prime == 0)
                    % Step 4
                    % Avoid PCFICH and PHICH
                    valid_reg = 1;
                    for(n=0:pcfich_s.N_reg-1)
                        if(k_prime == cfi_k(n+1))
                            valid_reg = 0;
                        endif
                    endfor
                    for(n=0:phich_s.N_reg-1)
                        if(k_prime == phich_k(n+1))
                            valid_reg = 0;
                        endif
                    endfor
                    if(mod(k_prime, 6) == 0 && valid_reg == 1)
                        if(m_prime < pdcch_s.N_reg)
                            % Step 5
                            idx = 0;
                            for(n=0:5)
                                % Avoid CRS
                                if(mod(N_id_cell, 3) != mod(n, 3))
                                    pdcch_reg(m_prime+1,idx+1) = symb(l_prime+1,k_prime+n+1);
                                    for(p=0:mib_s.N_ant-1)
                                        pdcch_ce(p+1,m_prime+1,idx+1) = ce_sf(p+1,l_prime+1,k_prime+n+1);
                                    endfor
                                    idx = idx + 1;
                                endif
                            endfor
                            % Step 6
                            m_prime = m_prime + 1;
                        endif
                    endif
                elseif(l_prime == 1 && mib_s.N_ant == 4)
                    % Step 4
                    if(mod(k_prime, 6) == 0)
                        if(m_prime < pdcch_s.N_reg)
                            % Step 5
                            idx = 0;
                            for(n=0:5)
                                % Avoid CRS
                                if(mod(N_id_cell, 3) != mod(n, 3))
                                    pdcch_reg(m_prime+1,idx+1) = symb(l_prime+1,k_prime+n+1);
                                    for(p=0:mib_s.N_ant-1)
                                        pdcch_ce(p+1,m_prime+1,idx+1) = ce_sf(p+1,l_prime+1,k_prime+n+1);
                                    endfor
                                    idx = idx + 1;
                                endif
                            endfor
                            % Step 6
                            m_prime = m_prime + 1;
                        endif
                    endif
                else
                    % Step 4
                    if(mod(k_prime, 4) == 0)
                        if(m_prime < pdcch_s.N_reg)
                            % Step 5
                            for(n=0:3)
                                pdcch_reg(m_prime+1,n+1) = symb(l_prime+1,k_prime+n+1);
                                for(p=0:mib_s.N_ant-1)
                                    pdcch_ce(p+1,m_prime+1,n+1) = ce_sf(p+1,l_prime+1,k_prime+n+1);
                                endfor
                            endfor
                            % Step 6
                            m_prime = m_prime + 1;
                        endif
                    endif
                endif
                % Step 7
                l_prime = l_prime + 1;
            endwhile
            % Step 9
            k_prime = k_prime + 1;
        endwhile
        % Undo cyclic shift of the REGs
        for(n=0:pdcch_s.N_reg-1)
            shift_idx                      = mod(n+N_id_cell, pdcch_s.N_reg);
            pdcch_reg_shift(shift_idx+1,:) = pdcch_reg(n+1,:);
            for(p=0:mib_s.N_ant-1)
                pdcch_ce_shift(p+1,shift_idx+1,:) = pdcch_ce(p+1,n+1,:);
            endfor
        endfor
        % Undo permutation of the REGs 36.212 section 5.1.4.2.1 v10.1.0
        pdcch_reg_vec = [0:pdcch_s.N_reg-1];
        % In order to recreate circular buffer, a dummy block must be
        % sub block interleaved to determine where NULL bits are to be
        % inserted
        % Step 1
        C_cc_sb = 32;
        % Step 2
        R_cc_sb = 0;
        while(pdcch_s.N_reg > (C_cc_sb*R_cc_sb))
            R_cc_sb = R_cc_sb + 1;
        endwhile
        % Inter-column permutation values
        ic_perm = [1,17,9,25,5,21,13,29,3,19,11,27,7,23,15,31,0,16,8,24,4,20,12,28,2,18,10,26,6,22,14,30];
        % Step 3
        if(pdcch_s.N_reg < (C_cc_sb*R_cc_sb))
            N_dummy = C_cc_sb*R_cc_sb - pdcch_s.N_reg;
        else
            N_dummy = 0;
        endif
        tmp = [10000*ones(1,N_dummy), zeros(1,pdcch_s.N_reg)];
        idx = 0;
        for(n=0:R_cc_sb-1)
            for(m=0:C_cc_sb-1)
                sb_mat(n+1,m+1) = tmp(idx+1);
                idx             = idx + 1;
            endfor
        endfor
        % Step 4
        for(n=0:R_cc_sb-1)
            for(m=0:C_cc_sb-1)
                sb_perm_mat(n+1,m+1) = sb_mat(n+1,ic_perm(m+1)+1);
            endfor
        endfor
        % Step 5
        idx = 0;
        for(m=0:C_cc_sb-1)
            for(n=0:R_cc_sb-1)
                v(idx+1) = sb_perm_mat(n+1,m+1);
                idx      = idx + 1;
            endfor
        endfor
        % Recreate circular buffer
        K_pi  = R_cc_sb*C_cc_sb;
        k_idx = 0;
        j_idx = 0;
        while(k_idx < pdcch_s.N_reg)
            if(v(mod(j_idx, K_pi)+1) != 10000)
                v(mod(j_idx, K_pi)+1) = pdcch_reg_vec(k_idx+1);
                k_idx = k_idx + 1;
            endif
            j_idx = j_idx + 1;
        endwhile
        % Sub block deinterleaving
        % Step 5
        idx = 0;
        for(m=0:C_cc_sb-1)
            for(n=0:R_cc_sb-1)
                sb_perm_mat(n+1,m+1) = v(idx+1);
                idx                  = idx + 1;
            endfor
        endfor
        % Step 4
        for(n=0:R_cc_sb-1)
            for(m=0:C_cc_sb-1)
                sb_mat(n+1,ic_perm(m+1)+1) = sb_perm_mat(n+1,m+1);
            endfor
        endfor
        % Step 3
        idx = 0;
        for(n=0:R_cc_sb-1)
            for(m=0:C_cc_sb-1)
                tmp(idx+1) = sb_mat(n+1,m+1);
                idx        = idx + 1;
            endfor
        endfor
        for(n=0:pdcch_s.N_reg-1)
            pdcch_reg_perm(n+1,:) = pdcch_reg_shift(tmp(N_dummy+n+1)+1,:);
            for(p=0:mib_s.N_ant-1)
                pdcch_ce_perm(p+1,n+1,:) = pdcch_ce_shift(p+1,tmp(N_dummy+n+1)+1,:);
            endfor
        endfor
        % Construct CCEs
        N_reg_cce = 9;
        for(n=0:pdcch_s.N_cce-1)
            for(m=0:N_reg_cce-1)
                pdcch_cce(n+1,m*4+1:m*4+4) = pdcch_reg_perm(n*N_reg_cce+m+1,:);
                for(p=0:mib_s.N_ant-1)
                    pdcch_cce_ce(p+1,n+1,m*4+1:m*4+4) = reshape(pdcch_ce_perm(p+1,n*9+m+1,:),1,[]);
                endfor
            endfor
        endfor
        % Generate the scrambling sequence in NRZ
        pdcch_c_init = N_sf*2^9 + N_id_cell;
        pdcch_c      = 1 - 2*lte_generate_prs_c(pdcch_c_init, 1152);
        % Determine the size of DCI 1A and 1C FIXME: Clean this up
        if(N_rb_dl == 6)
            dci_1a_size = 21;
            dci_1c_size = 9;
        elseif(N_rb_dl == 15)
            dci_1a_size = 22;
            dci_1c_size = 11;
        elseif(N_rb_dl == 25)
            dci_1a_size = 25;
            dci_1c_size = 13;
        elseif(N_rb_dl == 50)
            dci_1a_size = 27;
            dci_1c_size = 13;
        elseif(N_rb_dl == 75)
            dci_1a_size = 27;
            dci_1c_size = 14;
        else % N_rb_dl == 100
            dci_1a_size = 28;
            dci_1c_size = 15;
        endif

        % Try decoding DCI 1A and 1C for SI in the common search space
        alloc_idx = 0;
        for(n=0:3)
            idx         = n*4;
            pdcch_y_est = [pdcch_cce(idx+1,:), pdcch_cce(idx+2,:), pdcch_cce(idx+3,:), pdcch_cce(idx+4,:)];
            pdcch_ce    = [reshape(pdcch_cce_ce(:,idx+1,:),mib_s.N_ant,[])];
            pdcch_ce    = [pdcch_ce, reshape(pdcch_cce_ce(:,idx+2,:),mib_s.N_ant,[])];
            pdcch_ce    = [pdcch_ce, reshape(pdcch_cce_ce(:,idx+3,:),mib_s.N_ant,[])];
            pdcch_ce    = [pdcch_ce, reshape(pdcch_cce_ce(:,idx+4,:),mib_s.N_ant,[])];
            pdcch_x     = lte_pre_decoder_and_matched_filter_dl(pdcch_y_est, pdcch_ce, "tx_diversity");
            pdcch_d     = lte_layer_demapper_dl(pdcch_x, 1, "tx_diversity");
            pdcch_bits  = lte_modulation_demapper(pdcch_d, "qpsk");
            pdcch_bits  = pdcch_bits.*pdcch_c((n*288)+1:(n+1)*288);
            pdcch_dci   = lte_dci_channel_decode(pdcch_bits, SI_RNTI, 0, dci_1a_size);
            if(length(pdcch_dci) != 1)
                pdcch_s.alloc_s(alloc_idx+1) = lte_dci_1a_unpack(pdcch_dci, 0, SI_RNTI, N_rb_dl, mib_s.N_ant);
                alloc_idx                    = alloc_idx + 1;
            endif
            pdcch_dci = lte_dci_channel_decode(pdcch_bits, SI_RNTI, 0, dci_1c_size);
            if(length(pdcch_dci) != 1)
                printf("DCI 1C FOUND L = 4, %u\n", n);
                pdcch_dci
            endif
        endfor
        for(n=0:1)
            idx         = n*8;
            pdcch_y_est = [pdcch_cce(idx+1,:), pdcch_cce(idx+2,:), pdcch_cce(idx+3,:), pdcch_cce(idx+4,:)];
            pdcch_y_est = [pdcch_y_est, pdcch_cce(idx+5,:), pdcch_cce(idx+6,:), pdcch_cce(idx+7,:), pdcch_cce(idx+8,:)];
            pdcch_ce    = [reshape(pdcch_cce_ce(:,idx+1,:),mib_s.N_ant,[])];
            pdcch_ce    = [pdcch_ce, reshape(pdcch_cce_ce(:,idx+2,:),mib_s.N_ant,[])];
            pdcch_ce    = [pdcch_ce, reshape(pdcch_cce_ce(:,idx+3,:),mib_s.N_ant,[])];
            pdcch_ce    = [pdcch_ce, reshape(pdcch_cce_ce(:,idx+4,:),mib_s.N_ant,[])];
            pdcch_ce    = [pdcch_ce, reshape(pdcch_cce_ce(:,idx+5,:),mib_s.N_ant,[])];
            pdcch_ce    = [pdcch_ce, reshape(pdcch_cce_ce(:,idx+6,:),mib_s.N_ant,[])];
            pdcch_ce    = [pdcch_ce, reshape(pdcch_cce_ce(:,idx+7,:),mib_s.N_ant,[])];
            pdcch_ce    = [pdcch_ce, reshape(pdcch_cce_ce(:,idx+8,:),mib_s.N_ant,[])];
            pdcch_x     = lte_pre_decoder_and_matched_filter_dl(pdcch_y_est, pdcch_ce, "tx_diversity");
            pdcch_d     = lte_layer_demapper_dl(pdcch_x, 1, "tx_diversity");
            pdcch_bits  = lte_modulation_demapper(pdcch_d, "qpsk");
            pdcch_bits  = pdcch_bits.*pdcch_c((n*576)+1:(n+1)*576);
            pdcch_dci   = lte_dci_channel_decode(pdcch_bits, SI_RNTI, 0, dci_1a_size);
            if(length(pdcch_dci) != 1)
                pdcch_s.alloc_s(alloc_idx+1) = lte_dci_1a_unpack(pdcch_dci, 0, SI_RNTI, N_rb_dl, mib_s.N_ant);
                alloc_idx                    = alloc_idx + 1;
            endif
            pdcch_dci = lte_dci_channel_decode(pdcch_bits, SI_RNTI, 0, dci_1c_size);
            if(length(pdcch_dci) != 1)
                printf("DCI 1C FOUND L = 8, %u\n", n);
                pdcch_dci
            endif
        endfor
        pdcch_s.N_alloc = alloc_idx;
    else
        phich_s.N_group = 0;
        pdcch_s.N_alloc = 0;
    endif
endfunction

function [pdsch_s] = decode_pdsch(ce_sf, symb, N_sf, pcfich_s, phich_s, pdcch_s, mib_s, N_id_cell, N_sc_rb, N_rb_dl)
    % Determine first and last PBCH, PSS, and SSS subcarriers
    if(N_rb_dl == 6)
        first_sc = 0;
        last_sc  = (6*N_sc_rb)-1;
    elseif(N_rb_dl == 15)
        first_sc = (4*N_sc_rb)+6;
        last_sc  = (11*N_sc_rb)-7;
    elseif(N_rb_dl == 25)
        first_sc = (9*N_sc_rb)+6;
        last_sc  = (16*N_sc_rb)-7;
    elseif(N_rb_dl == 50)
        first_sc = 22*N_sc_rb;
        last_sc  = (28*N_sc_rb)-1;
    elseif(N_rb_dl == 75)
        first_sc = (34*N_sc_rb)+6;
        last_sc  = (41*N_sc_rb)-7;
    else % N_rb_dl == 100
        first_sc = 47*N_sc_rb;
        last_sc  = (53*N_sc_rb)-1;
    endif

    % Extract resource elements and channel estimate 36.211 section 6.3.5 v10.1.0
    % FIXME: Handle PBCH/PSS/SSS
    for(alloc_idx=0:pdcch_s.N_alloc-1)
        clear y_est
        clear ce
        idx = 0;
        for(L=pdcch_s.N_symbs:13)
            for(n=0:N_rb_dl-1)
                for(prb_idx=0:pdcch_s.alloc_s(alloc_idx+1).N_prb-1)
                    if(n == pdcch_s.alloc_s(alloc_idx+1).prb(prb_idx+1))
                        for(m=0:N_sc_rb-1)
                            if(mib_s.N_ant == 1 &&
                               mod(L, 7) == 0 &&
                               mod(N_id_cell, 6) == mod(m, 6))
                                % CRS
                            elseif(mib_s.N_ant == 1 &&
                                   mod(L, 7) == 4 &&
                                   mod(N_id_cell+3, 6) == mod(m, 6))
                                % CRS
                            elseif((mib_s.N_ant == 2 || mib_s.N_ant == 4) &&
                                   (mod(L, 7) == 0 || mod(L, 7) == 4) &&
                                   mod(N_id_cell, 3) == mod(m, 3))
                                % CRS
                            elseif(mib_s.N_ant == 4 &&
                                   mod(L, 7) == 1 &&
                                   mod(N_id_cell, 3) == mod(m, 3))
                                % CRS
                            elseif(N_sf == 0 &&
                                   (n*N_sc_rb+m) >= first_sc &&
                                   (n*N_sc_rb+m) <= last_sc &&
                                   L >= 7 &&
                                   L <= 10)
                                % PBCH
                            elseif((N_sf == 0 || N_sf == 5) &&
                                   (n*N_sc_rb+m) >= first_sc &&
                                   (n*N_sc_rb+m) <= last_sc &&
                                   L == 6)
                                % PSS
                            elseif((N_sf == 0 || N_sf == 5) &&
                                   (n*N_sc_rb+m) >= first_sc &&
                                   (n*N_sc_rb+m) <= last_sc &&
                                   L == 5)
                                % SSS
                            else
                                y_est(idx+1) = symb(L+1,n*N_sc_rb+m+1);
                                for(p=0:mib_s.N_ant-1)
                                    ce(p+1,idx+1) = ce_sf(p+1,L+1,n*N_sc_rb+m+1);
                                endfor
                                idx = idx + 1;
                            endif
                        endfor
                    endif
                endfor
            endfor
        endfor
        pre_coder   = pdcch_s.alloc_s(alloc_idx+1).pre_coding;
        N_codewords = pdcch_s.alloc_s(alloc_idx+1).N_codewords;
        mod_type    = pdcch_s.alloc_s(alloc_idx+1).modulation;
        rnti        = pdcch_s.alloc_s(alloc_idx+1).rnti;
        tbs         = pdcch_s.alloc_s(alloc_idx+1).tbs;
        tx_mode     = pdcch_s.alloc_s(alloc_idx+1).tx_mode;
        rv_idx      = pdcch_s.alloc_s(alloc_idx+1).rv_idx;
        x           = lte_pre_decoder_and_matched_filter_dl(y_est, ce, pre_coder);
        d           = lte_layer_demapper_dl(x, N_codewords, pre_coder);
        bits        = lte_modulation_demapper(d, mod_type);
        for(q=0:N_codewords-1)
            c_init   = rnti*2^14 + q*2^13 + N_sf*2^9 + N_id_cell;
            c(q+1,:) = 1 - 2*lte_generate_prs_c(c_init, length(bits));
        endfor
        bits                              = bits.*c;
        pdsch_s.alloc_s(alloc_idx+1).bits = lte_dlsch_channel_decode(bits, tbs, tx_mode, rv_idx, 8, 250368); % FIXME: Currently using N_soft from a cat 1 UE (36.306)
        if(length(pdsch_s.alloc_s(alloc_idx+1).bits) != 1)
            alloc_idx = alloc_idx + 1;
        endif
    endfor
    pdsch_s.N_alloc = alloc_idx;
endfunction

function [symbs] = samps_to_symbs(samps, slot_start_idx, symb_offset, FFT_pad_size, scale)
    % Calculate index and CP length
    if(mod(symb_offset, 7) == 0)
        CP_len = 160;
    else
        CP_len = 144;
    endif
    index = slot_start_idx + (2048+144)*symb_offset;
    if(symb_offset > 0)
        index = index + 16;
    endif

    % Take FFT
    tmp = fftshift(fft(samps(index+CP_len:index+CP_len+2047)));

    % Remove DC subcarrier
    tmp_symbs = [tmp(FFT_pad_size+1:1024), tmp(1026:2048-(FFT_pad_size-1))];

    if(scale == 0)
        symbs = tmp_symbs;
    else
        for(n=1:length(tmp_symbs))
            symbs(n) = cos(angle(tmp_symbs(n))) + j*sin(angle(tmp_symbs(n)));
        endfor
    endif
endfunction
