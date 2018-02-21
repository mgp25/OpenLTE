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
% Function:    lte_fdd_dl_transmit
% Description: Creates an FDD LTE signal.
% Inputs:      bandwidth    - Desired bandwidth
%              N_frames     - Desired number of frames
%              N_id_2       - Physical layer identity
%              N_id_1       - Physical layer cell identity group 
%              N_ant        - Number of antenna ports
%              mcc          - Mobile Country Code
%              mnc          - Mobile Network Code
%              tac          - Tracking Area Code
%              cell_id      - Cell identity
%              band         - Frequency band
% Outputs:     output_samps - LTE signal sampled at 30.72MHz
%                             30.72MHz instantaneous BW.
% Spec:        N/A
% Notes:       Only supports normal cyclic prefix
% Rev History: Ben Wojtowicz 12/26/2011 Created
%              Ben Wojtowicz 01/29/2012 Fixed license statement
%              Ben Wojtowicz 02/16/2012 Added control channel and SIB1 encode
%              Ben Wojtowicz 12/14/2013 Fixed a bug in PDSCH mapping
%              Ben Wojtowicz 03/26/2014 Using the renamed lte_layer_mapper_dl
%                                       and lte_pre_coder_dl functions.
%              Ben Wojtowicz 04/12/2014 Fixed a bug in PDCCH REG mapping.
%
function [output_samps] = lte_fdd_dl_transmit(bandwidth, N_frames, N_id_2, N_id_1, N_ant, mcc, mnc, tac, cell_id, band)
    % DEFINES
    N_sc_rb           = 12; % Only dealing with normal cp at this time
    N_symb_dl         = 7;  % Only dealing with normal cp at this time
    N_slots_per_frame = 20;
    N_cp_l_0          = 160;
    N_cp_l_else       = 144;
    N_rb_dl_max       = 110;
    N_id_cell         = 3*N_id_1 + N_id_2;

    % Check bandwidth and set N_rb_dl: 36.104 section 5.6 v10.1.0
    if(bandwidth == 1.4)
        N_rb_dl      = 6;
        FFT_pad_size = 988; % FFT_size = 2048
    elseif(bandwidth == 3)
        N_rb_dl      = 15;
        FFT_pad_size = 934; % FFT_size = 2048
    elseif(bandwidth == 5)
        N_rb_dl      = 25;
        FFT_pad_size = 874; % FFT_size = 2048
    elseif(bandwidth == 10)
        N_rb_dl      = 50;
        FFT_pad_size = 724; % FFT_size = 2048
    elseif(bandwidth == 15)
        N_rb_dl      = 75;
        FFT_pad_size = 574; % FFT_size = 2048
    else
        N_rb_dl      = 100;
        FFT_pad_size = 424; % FFT_size = 2048
        if(bandwidth ~= 20)
            printf("WARNING: Invalid bandwidth (%u), using 20\n", bandwidth);
        endif
    endif

    % Check N_ant
    if(N_ant != 1 && N_ant != 2 && N_ant != 4)
        printf("ERROR: Only supporting 1, 2, or 4 antennas at this time\n");
        output_samps = 0;
        return;
    endif

    % Check N_id_1
    if(!(N_id_1 >= 0 && N_id_1 <= 167))
        printf("ERROR: Invalid N_id_1 (%u)\n", N_id_1);
        output_samps = 0;
        return;
    endif

    % Check N_id_2
    if(!(N_id_2 >= 0 && N_id_2 <= 2))
        printf("ERROR: Invalid N_id_2 (%u)\n", N_id_2);
        output_samps = 0;
        return;
    endif

    % Generate syncronization signals: 36.211 section 6.11 v10.1.0
    pss_d_u                = lte_generate_pss(N_id_2);
    [sss_d_u_0, sss_d_u_5] = lte_generate_sss(N_id_1, N_id_2);

    % Generate reference signals: 36.211 section 6.10 v10.1.0
    for(n=0:N_slots_per_frame-1)
        crs_r_0(n+1,:) = lte_generate_crs(n, 0, N_id_cell);
        crs_r_1(n+1,:) = lte_generate_crs(n, 1, N_id_cell);
        crs_r_4(n+1,:) = lte_generate_crs(n, 4, N_id_cell);
    endfor

    % Define PBCH variables and generate scrambling sequence
    phich_res = 1;
    phich_dur = "normal";
    pbch_c    = lte_generate_prs_c(N_id_cell, 1920);

    % Generate CFI scrambling sequences
    cfi = 2;
    for(n=0:9)
        c_init       = (n+1)*(2*N_id_cell + 1)*2^9 + N_id_cell;
        cfi_c(n+1,:) = lte_generate_prs_c(c_init, 32);
    endfor

    % Generate SIB1 BCCH DL-SCH message
    sib1                = lte_sib1_pack(mcc, mnc, tac, cell_id, -70, band);
    sib1_bcch_dlsch_msg = lte_bcch_dlsch_msg_pack(1, sib1);

    % Generate symbols
    v_shift  = mod(N_id_cell, 6);
    last_idx = 1;
    for(frame_num=0:N_frames-1)
        for(subfr_num=0:9)
            % Preload mod_vec
            for(p=0:N_ant-1)
                mod_vec(p+1,:,:) = zeros(14, N_rb_dl*N_sc_rb);
            endfor

            % PSS
            if(subfr_num == 0 || subfr_num == 5)
                for(p=0:N_ant-1)
                    for(n=0:61)
                        k                    = n - 31 + (N_rb_dl*N_sc_rb)/2;
                        mod_vec(p+1,6+1,k+1) = pss_d_u(n+1);
                    endfor
                endfor
            endif

            % SSS
            for(p=0:N_ant-1)
                if(subfr_num == 0)
                    for(n=0:61)
                        k                    = n - 31 + (N_rb_dl*N_sc_rb)/2;
                        mod_vec(p+1,5+1,k+1) = sss_d_u_0(n+1);
                    endfor
                elseif(subfr_num == 5)
                    for(n=0:61)
                        k                    = n - 31 + (N_rb_dl*N_sc_rb)/2;
                        mod_vec(p+1,5+1,k+1) = sss_d_u_5(n+1);
                    endfor
                endif
            endfor

            % CRS
            for(p=0:N_ant-1)
                if(p == 0)
                    v_1     = 0;
                    v_2     = 3;
                    v_add   = 0;
                    s_num_1 = 0;
                    s_num_2 = 4;
                    crs_1   = crs_r_0;
                    crs_2   = crs_r_4;
                elseif(p == 1)
                    v_1     = 3;
                    v_2     = 0;
                    v_add   = 0;
                    s_num_1 = 0;
                    s_num_2 = 4;
                    crs_1   = crs_r_0;
                    crs_2   = crs_r_4;
                elseif(p == 2)
                    v_1     = 0;
                    v_2     = 0;
                    v_add   = 3;
                    s_num_1 = 1;
                    s_num_2 = 1;
                    crs_1   = crs_r_1;
                    crs_2   = crs_r_1;
                else % p == 3
                    v_1     = 3;
                    v_2     = 3;
                    v_add   = 3;
                    s_num_1 = 1;
                    s_num_2 = 1;
                    crs_1   = crs_r_1;
                    crs_2   = crs_r_1;
                endif
                for(m=0:2*N_rb_dl-1)
                    k_1                            = 6*m + mod((v_1 + v_shift), 6);
                    k_2                            = 6*m + mod((v_2 + v_shift), 6);
                    k_3                            = 6*m + mod((v_1 + v_add + v_shift), 6);
                    k_4                            = 6*m + mod((v_2 + v_add + v_shift), 6);
                    m_prime                        = m + N_rb_dl_max - N_rb_dl;
                    mod_vec(p+1,s_num_1+1,k_1+1)   = crs_1(subfr_num*2+1,m_prime+1);
                    mod_vec(p+1,s_num_2+1,k_2+1)   = crs_2(subfr_num*2+1,m_prime+1);
                    mod_vec(p+1,s_num_1+7+1,k_3+1) = crs_1(subfr_num*2+1+1,m_prime+1);
                    mod_vec(p+1,s_num_2+7+1,k_4+1) = crs_2(subfr_num*2+1+1,m_prime+1);
                endfor
            endfor

            % PBCH
            if(subfr_num == 0)
                if(mod(frame_num, 4) == 0)
                    % Generate PBCH
                    pbch_mib   = lte_bcch_bch_msg_pack(N_rb_dl, phich_dur, phich_res, frame_num);
                    pbch_bits  = lte_bch_channel_encode(pbch_mib, N_ant);
                    pbch_bits  = mod(pbch_bits+pbch_c, 2);
                    pbch_symbs = lte_modulation_mapper(pbch_bits, "qpsk");
                    pbch_x     = lte_layer_mapper_dl(pbch_symbs, N_ant, "tx_diversity");
                    pbch_y     = lte_pre_coder_dl(pbch_x, N_ant, "tx_diversity");
                endif
                for(p=0:N_ant-1)
                    idx = 0;
                    for(n=0:71)
                        k = (N_rb_dl*N_sc_rb)/2 - 36 + n;
                        if(mod(N_id_cell, 3) != mod(k, 3))
                            mod_vec(p+1,7+1,k+1) = pbch_y(p+1,idx+1);
                            mod_vec(p+1,8+1,k+1) = pbch_y(p+1,idx+48+1);
                            idx                  = idx + 1;
                        endif
                        mod_vec(p+1,9+1,k+1)  = pbch_y(p+1,n+96+1);
                        mod_vec(p+1,10+1,k+1) = pbch_y(p+1,n+168+1);
                    endfor
                endfor
                pbch_y = pbch_y(:,241:end);
            endif

            % PDCCH & PDSCH
            if(mod(frame_num,2) == 0 && subfr_num == 5)
                % SIB1
                sib1_alloc_s.N_alloc     = 1;
                sib1_alloc_s.N_prb       = 3;
                sib1_alloc_s.prb         = [0, 1, 2];
                sib1_alloc_s.vrb_type    = "localized";
                sib1_alloc_s.modulation  = "qpsk";
                sib1_alloc_s.pre_coding  = "tx_diversity";
                sib1_alloc_s.tx_mode     = 2;
                sib1_alloc_s.rv_idx      = mod(ceil((3/2)*mod(frame_num/2,4)), 4);
                sib1_alloc_s.N_codewords = 1;
                sib1_alloc_s.mcs         = 2;
                sib1_alloc_s.tbs         = lte_get_tbs_7_1_7_2_1(sib1_alloc_s.mcs, sib1_alloc_s.N_prb);
                sib1_alloc_s.rnti        = 65535;
                sib1_alloc_s.bits        = [sib1_bcch_dlsch_msg, zeros(1,sib1_alloc_s.tbs-length(sib1_bcch_dlsch_msg))];
                sib1_alloc_s.N_out_res   = 0;

                [mod_vec, N_ctrl_symbs] = map_pdcch(mod_vec, cfi, cfi_c(subfr_num+1,:), subfr_num, N_id_cell, N_rb_dl, N_sc_rb, N_ant, phich_res, phich_dur, sib1_alloc_s);

                for(n=0:sib1_alloc_s.N_prb-1)
                    sib1_alloc_s.N_out_res = sib1_alloc_s.N_out_res + get_num_pdsch_res_in_prb(subfr_num, N_ctrl_symbs, sib1_alloc_s.prb(n+1), N_rb_dl, N_ant);
                endfor

                [mod_vec] = map_pdsch(mod_vec, N_ctrl_symbs, subfr_num, N_rb_dl, N_sc_rb, N_id_cell, N_ant, sib1_alloc_s);
            else
                alloc_s.N_alloc = 0;
                [mod_vec, N_ctrl_symbs] = map_pdcch(mod_vec, cfi, cfi_c(subfr_num+1,:), subfr_num, N_id_cell, N_rb_dl, N_sc_rb, N_ant, phich_res, phich_dur, alloc_s);
            endif

            % Construct the output
            for(n=0:13)
                for(p=0:N_ant-1)
                    % Pad to the next largest FFT size and insert the DC 0
                    ifft_input_vec = [zeros(1,FFT_pad_size), reshape(mod_vec(p+1,n+1,1:N_rb_dl*N_sc_rb/2),1,[]), 0, reshape(mod_vec(p+1,n+1,(N_rb_dl*N_sc_rb/2)+1:end),1,[]), zeros(1,FFT_pad_size-1)];

                    % Take IFFT to get output samples
                    ifft_output_vec = ifft(fftshift(ifft_input_vec));

                    % Add cyclic prefix
                    if(n == 0 || n == 7)
                        cp_output = [ifft_output_vec(length(ifft_output_vec)-N_cp_l_0+1:length(ifft_output_vec)), ifft_output_vec];
                    else
                        cp_output = [ifft_output_vec(length(ifft_output_vec)-N_cp_l_else+1:length(ifft_output_vec)), ifft_output_vec];
                    endif

                    % Concatenate output
                    final_out(p+1,last_idx:last_idx+length(cp_output)-1) = cp_output;
                endfor
                last_idx = last_idx + length(cp_output);
            endfor
        endfor
    endfor
    output_samps = final_out;
endfunction

function [num_res] = get_num_pdsch_res_in_prb(sub_fn, num_cfi_symbs, prb, N_rb_dl, N_ant)
    % Determine first and last PBCH, PSS, and SSS PRBs
    if(N_rb_dl == 6)
        partial_prb = 0;
        first_prb   = 0;
        last_prb    = 5;
    elseif(N_rb_dl == 15)
        partial_prb = 1;
        first_prb   = 4;
        last_prb    = 10;
    elseif(N_rb_dl == 25)
        partial_prb = 1;
        first_prb   = 9;
        last_prb    = 15;
    elseif(N_rb_dl == 50)
        partial_prb = 0;
        first_prb   = 22;
        last_prb    = 27;
    elseif(N_rb_dl == 75)
        partial_prb = 1;
        first_prb   = 34;
        last_prb    = 40;
    else % N_rb_dl == 100
        partial_prb = 0;
        first_prb   = 47;
        last_prb    = 52;
    endif

    if(N_ant == 1)
        % Start with raw number of resource elements
        num_res = (14-4)*12 + 4*10;

        % Remove CFI symbols
        num_res = num_res - (num_cfi_symbs-1)*12 - 10;

        % Remove PBCH, PSS, and SSS
        if(prb >= first_prb && prb <= last_prb)
            if(partial_prb == 1 && (prb == first_prb || prb == last_prb))
                if(sub_fn == 0)
                    num_res = num_res - 5*6 - 5;
                elseif(sub_fn == 5)
                    num_res = num_res - 2*6;
                endif
            else
                if(sub_fn == 0)
                    num_res = num_res - 5*12 - 10;
                elseif(sub_fn == 5)
                    num_res = num_res - 2*12;
                endif
            endif
        endif
    elseif(N_ant == 2)
        % Start with raw number of resource elements
        num_res = (14-4)*12 + 4*8;

        % Remove CFI symbols
        num_res = num_res - (num_cfi_symbs-1)*12 - 8;

        % Remove PBCH, PSS, and SSS
        if(prb >= first_prb && prb <= last_prb)
            if(partial_prb == 1 && (prb == first_prb || prb == last_prb))
                if(sub_fn == 0)
                    num_res = num_res - 5*6 - 4;
                elseif(sub_fn == 5)
                    num_res = num_res - 2*6;
                endif
            else
                if(sub_fn == 0)
                    num_res = num_res - 5*12 - 8;
                elseif(sub_fn == 5)
                    num_res = num_res - 2*12;
                endif
            endif
        endif
    else % N_ant == 4
        % Start with raw number of resource elements
        num_res = (14-6)*12 + 6*8;

        % Remove CFI symbols
        if(num_cfi_symbs == 1)
            num_res = num_res - 8;
        else
            num_res = num_res - (num_cfi_symbs-2)*12 - 2*8;
        endif

        % Remove PBCH, PSS, and SSS
        if(prb >= first_prb && prb <= last_prb)
            if(partial_prb == 1 && (prb == first_prb || prb == last_prb))
                if(sub_fn == 0)
                    num_res = num_res - 4*6 - 2*4;
                elseif(sub_fn == 5)
                    num_res = num_res - 2*6;
                endif
            else
                if(sub_fn == 0)
                    num_res = num_res - 4*12 - 2*8;
                elseif(sub_fn == 5)
                    num_res = num_res - 2*12;
                endif
            endif
        endif
    endif
endfunction

function [mod_vec_out, N_pdcch_symbs] = map_pdcch(mod_vec_in, cfi, cfi_c, N_sf, N_id_cell, N_rb_dl, N_sc_rb, N_ant, phich_res, phich_dur, alloc_s)
    % Copy mod_vec to output
    mod_vec_out = mod_vec_in;

    % Calculate number of symbols 36.211 section 6.7 v10.1.0
    N_pdcch_symbs = cfi;
    if(N_rb_dl <= 10)
        N_pdcch_symbs = N_pdcch_symbs + 1;
    endif

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    % PCFICH
    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    % Map resources 36.211 section 6.7 v10.1.0
    N_pcfich_reg = 4;
    cfi_bits     = mod(lte_cfi_channel_encode(cfi) + cfi_c, 2);
    cfi_symbs    = lte_modulation_mapper(cfi_bits, "qpsk");
    cfi_x        = lte_layer_mapper_dl(cfi_symbs, N_ant, "tx_diversity");
    cfi_y        = lte_pre_coder_dl(cfi_x, N_ant, "tx_diversity");
    k_hat        = (N_sc_rb/2)*mod(N_id_cell, 2*N_rb_dl);
    for(n=0:N_pcfich_reg-1)
        cfi_k(n+1) = mod(k_hat + floor(n*N_rb_dl/2)*N_sc_rb/2, N_rb_dl*N_sc_rb);
        cfi_n(n+1) = cfi_k(n+1)/6 - 0.5;
        for(p=0:N_ant-1)
            idx = 0;
            for(m=0:5)
                if(mod(N_id_cell, 3) != mod(m, 3))
                    mod_vec_out(p+1,0+1,cfi_k(n+1)+m+1) = cfi_y(p+1,idx+(n*4)+1);
                    idx                                 = idx + 1;
                endif
            endfor
        endfor
    endfor

    if(alloc_s.N_alloc != 0)
        %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        % PHICH
        %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        % Map resources 36.211 section 6.9 v10.1.0
        N_phich_group = ceil(phich_res*(N_rb_dl/8)); % FIXME: Only handles normal CP
        N_phich_reg   = N_phich_group*3;
        % Step 4
        m_prime = 0;
        idx     = 0;
        % Step 10
        while(m_prime < N_phich_group)
            if(phich_dur == "normal")
                % Step 7
                l_prime = 0;
                % Step 1, 2, and 3
                n_l_prime = N_rb_dl*2 - N_pcfich_reg;
                % Step 8
                for(n=0:2)
                    n_hat(n+1) = mod(N_id_cell + m_prime + floor(n*n_l_prime/3), n_l_prime);
                endfor
                % Avoid PCFICH
                for(n=0:N_pcfich_reg-1)
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
                return;
            endif
            % Step 9
            m_prime = m_prime + 1;
        endwhile

        %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        % PDCCH
        %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        % Calculate resources 36.211 section 6.8.1 v10.1.0
        N_reg_rb        = 3;
        N_pdcch_reg_cce = 9;
        N_pdcch_reg     = N_pdcch_symbs*(N_rb_dl*N_reg_rb) - N_rb_dl - N_pcfich_reg - N_phich_reg;
        if(N_ant == 4)
            N_pdcch_reg = N_pdcch_reg - N_rb_dl; % Remove CRS
        endif
        N_pdcch_cce = floor(N_pdcch_reg/9);
        % Pack the DCI
        pdcch_dci = lte_dci_1a_pack(alloc_s, 0, N_rb_dl);
        if(length(pdcch_dci) == 12 ||
           length(pdcch_dci) == 14 ||
           length(pdcch_dci) == 16 ||
           length(pdcch_dci) == 20 ||
           length(pdcch_dci) == 24 ||
           length(pdcch_dci) == 26 ||
           length(pdcch_dci) == 32 ||
           length(pdcch_dci) == 40 ||
           length(pdcch_dci) == 44 ||
           length(pdcch_dci) == 56)
            pdcch_dci = [pdcch_dci, 0];
        endif
        pdcch_bits   = lte_dci_channel_encode(pdcch_dci, alloc_s.rnti, 0);
        pdcch_c_init = N_sf*2^9 + N_id_cell;
        pdcch_c      = lte_generate_prs_c(pdcch_c_init, length(pdcch_bits));
        pdcch_bits   = mod(pdcch_bits+pdcch_c, 2);
        pdcch_d      = lte_modulation_mapper(pdcch_bits, alloc_s.modulation);
        pdcch_x      = lte_layer_mapper_dl(pdcch_d, N_ant, alloc_s.pre_coding);
        pdcch_y      = lte_pre_coder_dl(pdcch_x, N_ant, alloc_s.pre_coding);
        pdcch_cce    = zeros(N_ant, N_pdcch_cce, 4*N_pdcch_reg_cce);
        if(alloc_s.rnti == 65535)
            % Common search space
            % Always use first PDCCH candidate with aggregation level of 8
            for(p=0:N_ant-1)
                idx = 0;
                for(n=0:7)
                    for(m=0:(4*N_pdcch_reg_cce)-1)
                        pdcch_cce(p+1,n+1,m+1) = pdcch_y(p+1,idx+1);
                        idx                    = idx + 1;
                    endfor
                endfor
            endfor
        endif
        % Construct REGs
        pdcch_reg = zeros(N_ant, N_pdcch_reg, 4);
        for(p=0:N_ant-1)
            for(n=0:N_pdcch_cce-1)
                for(m=0:N_pdcch_reg_cce-1)
                    pdcch_reg(p+1,n*N_pdcch_reg_cce+m+1,:) = pdcch_cce(p+1,n+1,m*4+1:m*4+4);
                endfor
            endfor
        endfor
        % Permute REGs
        pdcch_reg_vec = [0:N_pdcch_reg-1];
        % Sub block intereaving
        % Step 1
        C_cc_sb = 32;
        % Step 2
        R_cc_sb = 0;
        while(N_pdcch_reg > (C_cc_sb*R_cc_sb))
            R_cc_sb = R_cc_sb + 1;
        endwhile
        % Inter-column permutation values
        ic_perm = [1,17,9,25,5,21,13,29,3,19,11,27,7,23,15,31,0,16,8,24,4,20,12,28,2,18,10,26,6,22,14,30];
        % Step 3
        if(N_pdcch_reg < (C_cc_sb*R_cc_sb))
            N_dummy = C_cc_sb*R_cc_sb - N_pdcch_reg;
        else
            N_dummy = 0;
        endif
        tmp = [10000*ones(1, N_dummy), pdcch_reg_vec];
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
        K_pi  = R_cc_sb*C_cc_sb;
        k_idx = 0;
        j_idx = 0;
        while(k_idx < N_pdcch_reg)
            if(v(mod(j_idx, K_pi)+1) != 10000)
                pdcch_reg_perm_vec(k_idx+1) = v(mod(j_idx, K_pi)+1);
                k_idx                       = k_idx + 1;
            endif
            j_idx = j_idx + 1;
        endwhile
        for(p=0:N_ant-1)
            for(n=0:N_pdcch_reg-1)
                pdcch_reg_perm(p+1,n+1,:) = pdcch_reg(p+1,pdcch_reg_perm_vec(n+1)+1,:);
            endfor
        endfor
        % Cyclic shift the REGs
        for(p=0:N_ant-1)
            for(n=0:N_pdcch_reg-1)
                shift_idx                  = mod(n+N_id_cell, N_pdcch_reg);
                pdcch_reg_shift(p+1,n+1,:) = pdcch_reg_perm(p+1,shift_idx+1,:);
            endfor
        endfor
        % Map REGs
        % Step 1 and 2
        m_prime = 0;
        k_prime = 0;
        % Step 10
        while(k_prime < (N_rb_dl*N_sc_rb))
            % Step 3
            l_prime = 0;
            while(l_prime < N_pdcch_symbs)
                if(l_prime == 0)
                    % Step 4
                    % Avoid PCFICH and PHICH
                    valid_reg = 1;
                    for(n=0:N_pcfich_reg-1)
                        if(k_prime == cfi_k(n+1))
                            valid_reg = 0;
                        endif
                    endfor
                    for(n=0:N_phich_reg-1)
                        if(k_prime == phich_k(n+1))
                            valid_reg = 0;
                        endif
                    endfor
                    if(mod(k_prime, 6) == 0 && valid_reg == 1)
                        if(m_prime < N_pdcch_reg)
                            % Step 5
                            idx = 0;
                            for(n=0:5)
                                % Avoid CRS
                                if(mod(N_id_cell, 3) != mod(n, 3))
                                    for(p=0:N_ant-1)
                                        mod_vec_out(p+1,l_prime+1,k_prime+n+1) = pdcch_reg_shift(p+1,m_prime+1,idx+1);
                                    endfor
                                    idx = idx + 1;
                                endif
                            endfor
                            % Step 6
                            m_prime = m_prime + 1;
                        endif
                    endif
                elseif(l_prime == 1 && N_ant == 4)
                    % Step 4
                    if(mod(k_prime, 6) == 0)
                        if(m_prime < N_pdcch_reg)
                            % Step 5
                            idx = 0;
                            for(n=0:5)
                                % Avoid CRS
                                if(mod(N_id_cell, 3) != mod(n, 3))
                                    for(p=0:N_ant-1)
                                        mod_vec_out(p+1,l_prime+1,k_prime+n+1) = pdcch_reg_shift(p+1,m_prime+1,idx+1);
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
                        if(m_prime < N_pdcch_reg)
                            % Step 5
                            for(n=0:3)
                                for(p=0:N_ant-1)
                                    mod_vec_out(p+1,l_prime+1,k_prime+n+1) = pdcch_reg_shift(p+1,m_prime+1,n+1);
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
    endif
endfunction

function [mod_vec_out] = map_pdsch(mod_vec_in, N_ctrl_symbs, N_sf, N_rb_dl, N_sc_rb, N_id_cell, N_ant, alloc_s)
    % Copy mod_vec to output
    mod_vec_out = mod_vec_in;

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

    % Map the PDSCH
    pdsch_bits = lte_dlsch_channel_encode(alloc_s.bits, alloc_s.tx_mode, alloc_s.rv_idx, alloc_s.N_out_res*2, 2, 2, 250368, 8);
    for(q=0:alloc_s.N_codewords-1)
        c_init   = alloc_s.rnti*2^14 + q*2^13 + N_sf*2^9 + N_id_cell;
        c(q+1,:) = lte_generate_prs_c(c_init, length(pdsch_bits));
    endfor
    pdsch_bits = mod(pdsch_bits+c, 2);
    pdsch_d    = lte_modulation_mapper(pdsch_bits, alloc_s.modulation);
    pdsch_x    = lte_layer_mapper_dl(pdsch_d, N_ant, alloc_s.pre_coding);
    pdsch_y    = lte_pre_coder_dl(pdsch_x, N_ant, alloc_s.pre_coding);
    for(p=0:N_ant-1)
        idx = 0;
        for(L=N_ctrl_symbs:13)
            for(n=0:N_rb_dl-1)
                for(prb_idx=0:alloc_s.N_prb-1)
                    if(n == alloc_s.prb(prb_idx+1))
                        for(m=0:N_sc_rb-1)
                            if(N_ant == 1 &&
                               mod(L, 7) == 0 &&
                               mod(N_id_cell, 6) == mod(m, 6))
                                % CRS
                            elseif(N_ant == 1 &&
                                   mod(L, 7) == 4 &&
                                   mod(N_id_cell+3, 6) == mod(m, 6))
                                % CRS
                            elseif((N_ant == 2 || N_ant == 4) &&
                                   (mod(L, 7) == 0 || mod(L, 7) == 4) &&
                                   mod(N_id_cell, 3) == mod(m, 3))
                                % CRS
                            elseif(N_ant == 4 &&
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
                                mod_vec_out(p+1,L+1,n*N_sc_rb+m+1) = pdsch_y(p+1,idx+1);
                                idx                                = idx + 1;
                            endif
                        endfor
                    endif
                endfor
            endfor
        endfor
    endfor
endfunction
