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
% Function:    lte_bcch_dlsch_msg_pack
% Description: Packs all of the fields into the BCCH DLSCH message
% Inputs:      sib_num        - SIB number
%              sib            - SIB message
% Outputs:     bcch_dlsch_msg - Packed BCCH DLSCH message
% Spec:        3GPP TS 36.331 section 6.2.1 and 6.2.2 v10.0.0
% Notes:       Currently only supports SIB1
% Rev History: Ben Wojtowicz 1/22/2012 Created
%
function [bcch_dlsch_msg] = lte_bcch_dlsch_msg_pack(sib_num, sib)
    % Check sib_num
    if(sib_num == 1)
        ext_bit        = 0;
        sib_choice_bit = 1;
        bcch_dlsch_msg = [ext_bit, sib_choice_bit, sib];
    else
        printf("ERROR: Invalid sib_num %u\n", sib_num);
        bcch_dlsch_msg = 0;
        return;
    endif
endfunction
