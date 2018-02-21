%
% Copyright 2011-2012 Ben Wojtowicz
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
% Function:    lte_bcch_bch_msg_pack
% Description: Packs all of the fields into the BCCH BCH message
% Inputs:      bw           - System bandwidth (in number of RBs)
%              phich_dur    - PHICH Duration (normal or extended)
%              phich_res    - Number of PHICH groups (1/6, 1/2, 1, 2)
%              sfn          - System frame number
% Outputs:     bcch_bch_msg - Packed BCCH BCH message
% Spec:        3GPP TS 36.331 section 6.2.1 and 6.2.2 v10.0.0
% Notes:       None
% Rev History: Ben Wojtowicz 10/30/2011 Created
%              Ben Wojtowicz 01/29/2012 Fixed license statement
%              Ben Wojtowicz 02/19/2012 Changed function name to match spec
%
function [bcch_bch_msg] = lte_bcch_bch_msg_pack(bw, phich_dur, phich_res, sfn)
    % Check bandwidth
    if(bw == 6)
        act_bw = [0,0,0];
    elseif(bw == 15)
        act_bw = [0,0,1];
    elseif(bw == 25)
        act_bw = [0,1,0];
    elseif(bw == 50)
        act_bw = [0,1,1];
    elseif(bw == 75)
        act_bw = [1,0,0];
    elseif(bw == 100)
        act_bw = [1,0,1];
    else
        printf("ERROR: Invalid bw (%u)\n", bw);
        bcch_bch_msg = 0;
        return;
    endif

    % Check phich_dur
    if(phich_dur == "normal")
        act_phich_dur = 0;
    elseif(phich_dur == "extended")
        act_phich_dur = 1;
    else
        printf("ERROR: Invalid phich_dur (%s)\n", phich_dur);
        bcch_bch_msg = 0;
        return;
    endif

    % Check phich_res
    if(phich_res == 1/6)
        act_phich_res = [0,0];
    elseif(phich_res == 1/2)
        act_phich_res = [0,1];
    elseif(phich_res == 1)
        act_phich_res = [1,0];
    elseif(phich_res == 2)
        act_phich_res = [1,1];
    else
        printf("ERROR: Invalid phich_res (%f)\n", phich_res);
        bcch_bch_msg = 0;
        return;
    endif

    % Check SFN
    if(sfn >= 0 && sfn <= 1023)
        tmp = sfn;
        for(n=0:7)
            act_sfn(n+1) = floor(tmp/2^(9-n));
            tmp          = tmp - floor(tmp/2^(9-n))*2^(9-n);
        endfor
    else
        printf("ERROR: Invalid sfn (%u)\n", sfn);
        bcch_bch_msg = 0;
        return;
    endif

    % Pack the BCCH BCH message
    bcch_bch_msg = [act_bw, act_phich_dur, act_phich_res, act_sfn, zeros(1,10)];
endfunction
