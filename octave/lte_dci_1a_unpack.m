%
% Copyright 2012 Ben Wojtowicz
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
% Function:    lte_dci_1a_unpack
% Description: Unpacks all of the fields from the Downlink
%              Control Information format 1A.
% Inputs:      dci        - Downlink Control Information bits
%              ci_present - Flag indicating if the carrier indicator is
%                           present (0 = no, 1 = yes)
%              rnti       - Radio Network Temporary Identifier used to
%                           send the DCI
%              N_rb_dl    - Number of resource blocks in the downlink
%              N_ant      - Number of transmit antennas
% Outputs:     alloc_s    - Allocation description structure containing:
%                           N_prb - number of allocated PRBs, prb - allocated
%                           PRBs, modulation, pre_coding, tx_mode, rv_idx -
%                           redundancy version number, N_codewords, tbs -\
%                           transport block size, rnti
% Spec:        3GPP TS 36.212 section 5.3.3.1.3 v10.1.0 and
%              3GPP TS 36.213 section 7.1.6.3 v10.3.0 and
%              3GPP TS 36.213 section 7.1.7 v10.3.0
% Notes:       Currently only handles SI-RNTI or P-RNTI, and localized
%              virtual resource blocks
% Rev History: Ben Wojtowicz 01/02/2012 Created
%
function [alloc_s] = lte_dci_1a_unpack(dci, ci_present, rnti, N_rb_dl, N_ant)
    index = 0;
    % Check carrier indicator
    if(ci_present == 1)
        printf("WARNING: Not handling carrier indicator\n");
        index = index + 3;
    endif

    % Check DCI 0/1A flag 36.212 section 5.3.3.1.3 v10.1.0
    dci_0_1a_flag = dci(index+1);
    index         = index + 1;
    if(dci_0_1a_flag == 0)
        printf("ERROR: DCI 1A flagged as DCI 0\n");
        return;
    endif

    if(rnti == 65535 || rnti == 65534)
        % Handle SI- or P-RNTI

        % Determine if RIV uses local or distributed VRBs
        loc_or_dist = dci(index+1);
        index       = index + 1;

        % Find the RIV that was sent 36.213 section 7.1.6.3 v10.3.0
        RIV_length = ceil(log(N_rb_dl*(N_rb_dl+1)/2)/log(2));
        RIV        = cmn_bin2dec(dci(index+1:index+RIV_length-1+1), RIV_length);
        index      = index + RIV_length;
        for(n=1:N_rb_dl)
            for(m=0:(N_rb_dl-n))
                if((n-1) <= floor(N_rb_dl/2))
                    if(RIV == N_rb_dl*(n-1)+m)
                        L_crb    = n;
                        RB_start = m;
                    endif
                else
                    if(RIV == N_rb_dl*(N_rb_dl-n+1)+(N_rb_dl-1-m))
                        L_crb    = n;
                        RB_start = m;
                    endif
                endif
            endfor
        endfor

        % Extract the rest of the fields
        mcs           = cmn_bin2dec(dci(index+1:index+5-1+1),5);
        index         = index + 5;
        index         = index + 3; % HARQ process number is reserved, FIXME: FDD Only
        new_data_ind  = dci(index+1);
        index         = index + 1;
        redund_vers   = cmn_bin2dec(dci(index+1:index+2-1+1),2);
        index         = index + 2;
        tpc           = cmn_bin2dec(dci(index+1:index+2-1+1),2);
        index         = index + 2;

        % Parse the data
        if(mod(tpc,2) == 0)
            N_prb_1a = 2;
        else
            N_prb_1a = 3;
        endif
        if(loc_or_dist == 1)
            % FIXME: Figure out gapping
            % FIXME: Convert to localized blocks
        else
            % Convert allocation into array of prbs
            prbs = [RB_start:RB_start+L_crb-1];
        endif

        % Fill in the allocation structure 36.213 section 7.1.7 v10.3.0
        i_tbs              = mcs;
        alloc_s.N_prb      = L_crb;
        alloc_s.prb        = prbs;
        alloc_s.modulation = "qpsk";
        alloc_s.pre_coding = "tx_diversity";
        if(N_ant == 1)
            alloc_s.tx_mode = 1;
        else
            alloc_s.tx_mode = 2;
        endif
        alloc_s.rv_idx      = redund_vers;
        alloc_s.N_codewords = 1;
        alloc_s.tbs         = lte_get_tbs_7_1_7_2_1(i_tbs, N_prb_1a);
        alloc_s.rnti        = rnti;
    else
        printf("ERROR: Not handling DCI 1As for C-RNTI\n");
        alloc_s = 0;
        return;
    endif
endfunction
