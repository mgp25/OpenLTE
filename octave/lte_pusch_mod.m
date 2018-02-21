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
% Function:    lte_pusch_mod
% Description: Modulates PUSCH
% Inputs:      bits             - Array of bits to modulate
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
% Outputs:     pusch_bb - PUSCH baseband I/Q
% Spec:        3GPP TS 36.211 section 5.3, 5.5, and 5.6 v10.1.0
% Notes:       Only handling normal CP
% Rev History: Ben Wojtowicz 03/26/2014 Created
%
function [pusch_bb] = lte_pusch_mod(bits, prb_offset, N_prb, N_id_cell, rnti, N_subfr, delta_ss, group_hop_en, seq_hop_en, cyclic_shift, cyclic_shift_dci, w_config, N_ul_rb)
    [N_codewords, M_bits] = size(bits);
    N_cp_L_0              = 160; % FIXME: Only handling normal CP
    N_cp_L_else           = 144; % FIXME: Only handling normal CP
    N_symbs_per_slot      = 7; % FIXME: Only handling normal CP
    N_sc_rb               = 12;
    M_pusch_rb            = N_prb;
    M_pusch_sc            = M_pusch_rb*N_sc_rb;
    FFT_size              = 2048;

    % Check prb_offset
    if(~(prb_offset >= 0 && prb_offset < N_ul_rb))
        printf("ERROR: Invalid prb_offset (%u)\n", prb_offset);
        pusch_bb = 0;
        return;
    endif

    % Check N_prb
    if(~(N_prb > 0 && N_prb <= N_ul_rb))
        printf("ERROR: Invalid N_prb (%u)\n", N_prb);
        pusch_bb = 0;
        return;
    endif

    % Check N_id_cell
    if(~(N_id_cell >= 0 && N_id_cell <= 501))
        printf("ERROR: Invalid N_id_cell (%u)\n", N_id_cell);
        pusch_bb = 0;
        return;
    endif

    % Check rnti
    if(~(rnti >= 0 && rnti <= 65535))
        printf("ERROR: Invalid rnti (%u)\n", rnti);
        pusch_bb = 0;
        return;
    endif

    % Check N_subfr
    if(~(N_subfr >= 0 && N_subfr <= 9))
        printf("ERROR: Invalid N_subfr (%u)\n", N_subfr);
        pusch_bb = 0;
        return;
    endif

    % Check delta_ss
    if(~(delta_ss >= 0 && delta_ss <= 29))
        printf("ERROR: Invalid delta_ss (%u)\n", delta_ss);
        pusch_bb = 0;
        return;
    endif

    % Check group_hop_en
    if(~(group_hop_en == 1 || group_hop_en == 0))
        printf("ERROR: Invalid group_hop_en (%u)\n", group_hop_en);
        pusch_bb = 0;
        return;
    endif

    % Check seq_hop_en
    if(~(seq_hop_en == 1 || seq_hop_en == 0))
        printf("ERROR: Invalid seq_hop_en (%u)\n", seq_hop_en);
        pusch_bb = 0;
        return;
    endif

    % Check cyclic_shift
    if(~(cyclic_shift >= 0 && cyclic_shift <= 7))
        printf("ERROR: Invalid cyclic_shift (%u)\n", cyclic_shift);
        pusch_bb = 0;
        return;
    endif

    % Check cyclic_shift_dci
    if(~(cyclic_shift_dci >= 0 && cyclic_shift_dci <= 7))
        printf("ERROR: Invalid cyclic_shift_dci (%u)\n", cyclic_shift_dci);
        pusch_bb = 0;
        return;
    endif

    % Check w_config
    if(~(w_config == "fixed" || w_config == "table"))
        printf("ERROR: Invalid w_config (%s)\n", w_config);
        pusch_bb = 0;
        return;
    endif

    % Check N_ul_rb
    if(~(N_ul_rb >= 6 && N_ul_rb <= 110))
        printf("ERROR: Invalid N_ul_rb (%u)\n", N_ul_rb);
        pusch_bb = 0;
        return;
    endif

    % Scrambling
    % FIXME: Handle ACK and RI bits
    for(q=0:N_codewords-1)
        c_init   = rnti*2^14 + q*2^13 + N_subfr*2^9 + N_id_cell;
        c(q+1,:) = lte_generate_prs_c(c_init, M_bits);
    endfor
    pusch_b_tilda = mod(bits+c, 2);

    % Modulation
    pusch_d = lte_modulation_mapper(pusch_b_tilda, "qpsk");

    % Layer mapping
    pusch_x = lte_layer_mapper_ul(pusch_d, 1);

    % Transform precoding
    % FIXME: Normalization
    for(L=0:(length(pusch_x)/M_pusch_sc)-1)
        pusch_y(L*M_pusch_sc+1:(L+1)*M_pusch_sc) = fft(pusch_x(L*M_pusch_sc+1:(L+1)*M_pusch_sc));
    endfor

    % Precoding
    pusch_z = lte_pre_coder_ul(pusch_y, 1);

    % Generate DMRS, assuming 1 layer
    dmrs = lte_generate_dmrs_pusch(N_subfr, N_id_cell, delta_ss, group_hop_en, seq_hop_en, cyclic_shift, cyclic_shift_dci, w_config, N_prb, 0);

    % Map to physical resources
    idx = 0;
    for(L=0:(N_symbs_per_slot*2)-1)
        if(3 == L)
            for(k=0:M_pusch_sc-1)
                % DMRS
                mod_vec_out(L+1,k+1) = dmrs(k+1);
            endfor
        elseif(10 == L)
            for(k=0:M_pusch_sc-1)
                % DMRS
                mod_vec_out(L+1,k+1) = dmrs(M_pusch_sc+k+1);
            endfor
        else
            for(k=0:M_pusch_sc-1)
                % PUSCH
                mod_vec_out(L+1,k+1) = pusch_z(idx+1);
                idx                  = idx + 1;
            endfor
        endif
    endfor

    % Baseband signal generation
    last_idx = 1;
    for(L=0:(N_symbs_per_slot*2)-1)
        ifft_input_vec                 = zeros(1,FFT_size*2);
        start                          = FFT_size - (N_ul_rb*N_sc_rb) + (prb_offset*N_sc_rb*2);
        stop                           = start + M_pusch_rb*12*2;
        ifft_input_vec(start+2:2:stop) = mod_vec_out(L+1,:);

        ifft_output_vec        = ifft(ifftshift(ifft_input_vec), FFT_size*2);
        resamp_ifft_output_vec = ifft_output_vec(1:end/2);

        % Add cyclic prefix
        if(L == 0 || L == 7)
            cp_output = [resamp_ifft_output_vec(length(resamp_ifft_output_vec)-N_cp_L_0+1:length(resamp_ifft_output_vec)), resamp_ifft_output_vec];
        else
            cp_output = [resamp_ifft_output_vec(length(resamp_ifft_output_vec)-N_cp_L_else+1:length(resamp_ifft_output_vec)), resamp_ifft_output_vec];
        endif

        % Concatenate output
        final_out(last_idx:last_idx+length(cp_output)-1) = cp_output;
        last_idx                                         = last_idx + length(cp_output);
    endfor

    pusch_bb = final_out;

endfunction
