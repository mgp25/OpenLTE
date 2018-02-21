%
% Copyright 2013-2014 Ben Wojtowicz
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
% Function:    lte_generate_dmrs_pusch
% Description: Generates LTE demodulation reference signal for PUSCH
% Inputs:      N_subfr                  - Subframe number within a radio frame
%              N_id_cell                - Physical layer cell identity
%              delta_ss                 - Configurable portion of the sequence-shift pattern for PUSCH (sib2 groupAssignmentPUSCH)
%              group_hopping_enabled    - Boolean value determining if group hopping is enabled (sib2 groupHoppingEnabled)
%              sequence_hopping_enabled - Boolean value determining if sequence hopping is enabled (sib2 sequenceHoppingEnabled)
%              cyclic_shift             - Broadcast cyclic shift to apply to base reference signal (sib2 cyclicShift)
%              cyclic_shift_dci         - Scheduled cyclic shift to apply to base reference signal
%              w_config                 - fixed or table
%              N_prbs                   - Number of PRBs used for the uplink grant
%              layer                    - Which diversity layer to generate reference signals for
% Outputs:     r - Demodulation reference signal for PUSCH
% Spec:        3GPP TS 36.211 section 5.5.2.1 v10.1.0
% Notes:       Currently only handles normal CP
% Rev History: Ben Wojtowicz 09/28/2013 Created
%              Ben Wojtowicz 11/13/2013 Bug fix for second slot reference signal
%              Ben Wojtowicz 03/26/2014 Added a note to add the last precoding step
%
function [r] = lte_generate_dmrs_pusch(N_subfr, N_id_cell, delta_ss, group_hopping_enabled, sequence_hopping_enabled, cyclic_shift, cyclic_shift_dci, w_config, N_prbs, layer)

    % Defines
    N_rb_ul_max            = 110;
    N_ul_symb              = 7; % Only dealing with normal cp at this time
    N_sc_rb                = 12;
    N_1_DMRS               = [0,2,3,4,6,8,9,10];
    N_2_DMRS_LAMBDA(0+1,:) = [ 0, 6, 3, 9];
    N_2_DMRS_LAMBDA(1+1,:) = [ 6, 0, 9, 3];
    N_2_DMRS_LAMBDA(2+1,:) = [ 3, 9, 6, 0];
    N_2_DMRS_LAMBDA(3+1,:) = [ 4,10, 7, 1];
    N_2_DMRS_LAMBDA(4+1,:) = [ 2, 8, 5,11];
    N_2_DMRS_LAMBDA(5+1,:) = [ 8, 2,11, 5];
    N_2_DMRS_LAMBDA(6+1,:) = [10, 4, 1, 7];
    N_2_DMRS_LAMBDA(7+1,:) = [ 9, 3, 0, 6];
    W_VECTOR(0+1,:,:)      = [ 1, 1; 1, 1; 1,-1; 1,-1];
    W_VECTOR(1+1,:,:)      = [ 1,-1; 1,-1; 1, 1; 1, 1];
    W_VECTOR(2+1,:,:)      = [ 1,-1; 1,-1; 1, 1; 1, 1];
    W_VECTOR(3+1,:,:)      = [ 1, 1; 1, 1; 1, 1; 1, 1];
    W_VECTOR(4+1,:,:)      = [ 1, 1; 1, 1; 1, 1; 1, 1];
    W_VECTOR(5+1,:,:)      = [ 1,-1; 1,-1; 1,-1; 1,-1];
    W_VECTOR(6+1,:,:)      = [ 1,-1; 1,-1; 1,-1; 1,-1];
    W_VECTOR(7+1,:,:)      = [ 1, 1; 1, 1; 1,-1; 1,-1];

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

    % Validate cyclic_shift
    if(~(cyclic_shift >= 0 && cyclic_shift <= 7))
        printf("ERROR: Invalid cyclic_shift (%u)\n", cyclic_shift);
        r = 0;
        return;
    endif

    % Validate cyclic_shift_dci
    if(~(cyclic_shift_dci >= 0 && cyclic_shift_dci <= 7))
        printf("ERROR: Invalid cyclic_shift_dci (%u)\n", cyclic_shift_dci);
        r = 0;
        return;
    endif

    % Validate w_config
    if(~(w_config == "fixed" || w_config == "table"))
        printf("ERROR: Invalid w_config (%s)\n", w_config);
        r = 0;
        return;
    endif

    % Validate N_prbs
    if(~(N_prbs >= 1 && N_prbs <= N_rb_ul_max))
        printf("ERROR: Invalid N_prbs (%u)\n", N_prbs);
        r = 0;
        return;
    endif

    % Validate layer
    if(~(layer >= 0 && layer <= 3))
        printf("ERROR: Invalid layer (%u)\n", layer);
        r = 0;
        return;
    endif

    % Calculate N_s
    N_s = N_subfr*2;

    % Set lambda
    lambda = layer;

    % Calculate f_ss_pusch
    f_ss_pusch = mod(mod(N_id_cell, 30) + delta_ss, 30);

    % Generate c
    c_init = floor(N_id_cell/30)*2^5 + f_ss_pusch;
    c      = lte_generate_prs_c(c_init, 8*N_ul_symb*20);

    % Calculate n_pn_ns
    n_pn_ns_1 = 0;
    n_pn_ns_2 = 0;
    for(n=0:7)
        n_pn_ns_1 = n_pn_ns_1 + c(8*N_ul_symb*N_s + n + 1)*2^n;
        n_pn_ns_2 = n_pn_ns_2 + c(8*N_ul_symb*(N_s+1) + n + 1)*2^n;
    endfor

    % Determine n_1_dmrs
    n_1_dmrs = N_1_DMRS(cyclic_shift+1);

    % Determine n_2_dmrs_lambda
    n_2_dmrs_lambda = N_2_DMRS_LAMBDA(cyclic_shift_dci+1, lambda+1);

    % Calculate n_cs_lambda
    n_cs_lambda_1 = mod(n_1_dmrs + n_2_dmrs_lambda + n_pn_ns_1, 12);
    n_cs_lambda_2 = mod(n_1_dmrs + n_2_dmrs_lambda + n_pn_ns_2, 12);

    % Calculate alpha_lambda
    alpha_lambda_1 = 2*pi*n_cs_lambda_1/12;
    alpha_lambda_2 = 2*pi*n_cs_lambda_2/12;

    % Generate the base reference signal
    r_u_v_alpha_lambda(1,:) = lte_generate_ul_rs(N_s, N_id_cell, "pusch", delta_ss, group_hopping_enabled, sequence_hopping_enabled, alpha_lambda_1, N_prbs);
    r_u_v_alpha_lambda(2,:) = lte_generate_ul_rs(N_s+1, N_id_cell, "pusch", delta_ss, group_hopping_enabled, sequence_hopping_enabled, alpha_lambda_2, N_prbs);

    % Determine w vector
    if(w_config == "fixed")
        w_vector = [1, 1];
    else
        w_vector = reshape(W_VECTOR(cyclic_shift_dci+1, lambda+1, :), 1, 2);
    endif

    % Calculate M_sc_rb
    M_sc_rb = N_prbs*N_sc_rb;

    % Generate the PUSCH demodulation reference signal sequence
    for(m=0:1)
        for(n=0:M_sc_rb-1)
            r(m*M_sc_rb + n + 1) = w_vector(m+1)*r_u_v_alpha_lambda(m+1,n+1);
        endfor
    endfor

    % FIXME: Add precoding to arrive at r_tilda
endfunction
