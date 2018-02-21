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
% Function:    lte_bcch_dlsch_msg_unpack
% Description: Unpacks all of the fields from the BCCH DLSCH message
% Inputs:      bcch_dlsch_msg - Packed BCCH DLSCH message
% Outputs:     sib_num        - SIB number
%              sib            - SIB message
% Spec:        3GPP TS 36.331 section 6.2.1 and 6.2.2 v10.0.0
% Notes:       Currently only supports SIB1
% Rev History: Ben Wojtowicz 1/22/2012 Created
%
function [sib_num, sib] = lte_bcch_dlsch_msg_unpack(bcch_dlsch_msg)
    % Initialize the message index
    idx = 0;

    % Skip the extension bit
    idx = idx + 1;

    % Extract the sib1_choice_bit
    sib1_choice_bit = bcch_dlsch_msg(idx+1);
    idx             = idx + 1;

    % Determine if SIB1 or another SIB was received
    if(sib1_choice_bit == 1)
        % SIB 1 was received
        sib_num = 1;
        sib     = bcch_dlsch_msg(idx+1:end);
    else
        printf("ERROR: Invalid BCCH DLSCH message\n");
        sib_num = 0;
        sib     = 0;
    endif
endfunction
