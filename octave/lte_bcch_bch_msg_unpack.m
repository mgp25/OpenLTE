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
% Function:    lte_bcch_bch_msg_unpack
% Description: Unpacks all of the fields from the BCCH BCH message
% Inputs:      bcch_bch_msg - Packed BCCH BCH message
% Outputs:     bw           - System bandwidth (in number of RBs)
%              phich_dur    - PHICH Duration (normal or extended)
%              phich_res    - Number of PHICH groups (1/6, 1/2, 1, 2)
%              sfn          - System frame number
% Spec:        3GPP TS 36.331 section 6.2.1 and 6.2.2 v10.0.0
% Notes:       None
% Rev History: Ben Wojtowicz 10/30/2011 Created
%              Ben Wojtowicz 01/29/2012 Fixed license statement
%              Ben Wojtowicz 02/19/2012 Changed function name to match spec
%
function [bw, phich_dur, phich_res, sfn] = lte_bcch_bch_msg_unpack(bcch_bch_msg)
    % Check bcch_bch_msg
    if(length(bcch_bch_msg) ~= 24)
        printf("ERROR: Invalid bcch_bch_msg (length is %u, should be 24)\n", length(bcch_bch_msg));
        bw        = 0;
        phich_dur = 0;
        phich_res = 0;
        sfn       = 0;
        return;
    endif

    % Unpack the BCCH BCH message
    act_bw        = bcch_bch_msg(1:3);
    act_phich_dur = bcch_bch_msg(4);
    act_phich_res = bcch_bch_msg(5:6);
    act_sfn       = bcch_bch_msg(7:14);

    % Construct bandwidth
    act_bw = 4*act_bw(1) + 2*act_bw(2) + act_bw(3);
    if(act_bw == 0)
        bw = 6;
    elseif(act_bw == 1)
        bw = 15;
    elseif(act_bw == 2)
        bw = 25;
    elseif(act_bw == 3)
        bw = 50;
    elseif(act_bw == 4)
        bw = 75;
    elseif(act_bw == 5)
        bw = 100;
    else
        printf("ERROR: Invalid act_bw (%u)\n", act_bw);
        bw = 0;
    endif

    % Construct phich_dur
    if(act_phich_dur == 0)
        phich_dur = "normal";
    else
        phich_dur = "extended";
    endif

    % Construct phich_res
    act_phich_res = 2*act_phich_res(1) + act_phich_res(2);
    if(act_phich_res == 0)
        phich_res = 1/6;
    elseif(act_phich_res == 1)
        phich_res = 1/2;
    elseif(act_phich_res == 2)
        phich_res = 1;
    else
        phich_res = 2;
    endif

    % Construct SFN
    sfn = 0;
    for(n=0:7)
        sfn = sfn + act_sfn(n+1)*2^(9-n);
    endfor
endfunction
