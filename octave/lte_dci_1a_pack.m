%
% Copyright 2011 Ben Wojtowicz
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
% Function:    lte_dci_1a_pack
% Description: Packs all of the fields into the Downlink Control
%              Indicator Format 1A.
% Inputs:      alloc_s    - Allocation description structure containing:
%                           N_prb - number of allocated PRBs, prb - allocated
%                           PRBs, vrb_type - "localized" or "distributed",
%                           modulation, pre_coding, tx_mode, rv_idx -
%                           redundancy version number, N_codewords,
%                           mcs - modulation and coding scheme, tbs -
%                           transport block size, rnti
%              ci_present - Flag indicating if the carrier indicator is
%                           present (0 = no, 1 = yes)
%              N_rb_dl    - Number of resource blocks in the downlink
% Outputs:     dci        - Packed Downlink Control Information
% Spec:        3GPP TS 36.212 section 5.3.3.1.3 v10.1.0 and
%              3GPP TS 36.213 section 7.1.6.3 v10.3.0 and
%              3GPP TS 36.213 section 7.1.7 v10.3.0
% Notes:       Currently only handles SI-RNTI or P-RNTI, and localized
%              virtual resource blocks
% Rev History: Ben Wojtowicz 01/02/2012 Created
%
function [dci] = lte_dci_1a_pack(alloc_s, ci_present, N_rb_dl)
    % Check carrier indicator
    if(ci_present == 1)
        printf("ERROR: Not handling carrier indicator\n");
        dci = 0;
        return;
    endif

    % Format 0/1A indicator is set to 1A (1)
    act_format_0_1a_ind = [1];

    % Resource block assignment
    if(alloc_s.rnti == 65535 || alloc_s.rnti == 65534)
        if(alloc_s.vrb_type == "localized")
            act_vrb_type_ind = [0];
            RIV_length       = ceil(log(N_rb_dl*(N_rb_dl+1)/2)/log(2));
            if((alloc_s.N_prb-1) <= floor(N_rb_dl/2))
                RIV = N_rb_dl*(alloc_s.N_prb-1) + alloc_s.prb(1);
            else
                RIV = N_rb_dl*(N_rb_dl-alloc_s.N_prb+1) + (N_rb_dl + alloc_s.prb(1));
            endif
            act_riv = cmn_dec2bin(RIV, RIV_length);
            act_rba = [act_vrb_type_ind, act_riv];
        else
            printf("ERROR: Invalid vrb_type %s (only hangling localized)\n", alloc_s.vrb_type);
            dci = 0;
            return;
        endif

        % Modulation and coding scheme
        act_mcs = cmn_dec2bin(alloc_s.mcs, 5);

        % HARQ process number
        act_harq_proc_num = [0,0,0]; % FIXME: FDD Only

        % New data indicator
        act_new_data_ind = [0];

        % Redundancy version
        act_redund_ver = cmn_dec2bin(alloc_s.rv_idx, 2);

        % TPC
        if(alloc_s.N_prb == 2)
            act_tpc = [0,0];
        else
            act_tpc = [0,1];
        endif

        % Pack the fields into the DCI
        dci = [act_format_0_1a_ind, act_rba, act_mcs, act_harq_proc_num, act_new_data_ind, act_redund_ver, act_tpc];
    else
        printf("ERROR: Not handling C-RNTI DCIs\n");
        dci = 0;
        return;
    endif
endfunction
