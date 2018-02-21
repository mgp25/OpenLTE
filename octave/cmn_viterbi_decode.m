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
% Function:    cmn_viterbi_decode
% Description: Viterbi decodes a convolutionally
%              coded input bit array using the
%              provided parameters
% Inputs:      in       - Input bit array
%              k        - Constraint length
%              r        - Rate
%              g        - Polynomial definition
%                         array in octal
% Outputs:     out      - Ouput bit array
% Spec:        N/A
% Notes:       N/A
% Rev History: Ben Wojtowicz 11/22/2011 Created
%              Ben Wojtowicz 01/29/2012 Fixed license statement
%              Ben Wojtowicz 02/19/2012 Added soft bit decoding
%
function [out] = cmn_viterbi_decode(in, k, r, g)

    % Check constraint length
    if(k > 9)
        printf("ERROR: Maximum supported constraint length = 9\n");
        out = 0;
        return;
    endif

    % Check r and g
    if(length(g) != r)
        printf("ERROR: Invalid rate (%u) or polynomial definition (%u)\n", r, length(g));
        out = 0;
        return;
    endif

    % Determine number of states
    num_states = 2^(k-1);

    % Convert g from octal to binary array
    g_array = cmn_oct2bin(g, k);

    % Precalculate state transition outputs
    for(n=0:num_states-1)
        % Determine the input path
        if(n < (num_states/2))
            in_path = 0;
        else
            in_path = 1;
        endif

        % Determine the outputs based on the previous state and input path
        for(m=0:1)
            prev_state = mod(bitshift(n, 1) + m, num_states);
            s_reg      = cmn_dec2bin(prev_state, k);
            s_reg(1)   = in_path;
            output     = zeros(1, r);

            for(o=0:r-1)
                for(p=0:k-1)
                    output(o+1) = output(o+1) + s_reg(p+1)*g_array(o+1,p+1);
                endfor
                output(o+1) = mod(output(o+1), 2);
            endfor

            st_output(n+1,m+1) = cmn_bin2dec(output, r);
        endfor
    endfor

    % Calculate branch and path metrics
    path_metric = zeros(num_states, (length(in)/r)+10);
    for(n=0:(length(in)/r)-1)
        br_metric = zeros(num_states, 2);
        p_metric  = zeros(num_states, 2);
        w_metric  = zeros(num_states, 2);

        for(m=0:num_states-1)
            % Calculate the accumulated branch metrics for each state
            for(o=0:1)
                prev_state        = mod(bitshift(m, 1) + o, num_states);
                p_metric(m+1,o+1) = path_metric(prev_state+1,n+1);
                st_arr            = cmn_dec2bin(st_output(m+1,o+1), r);
                for(p=0:r-1)
                    if(in(n*r+p+1) >= 0)
                        in_bit = 0;
                    else
                        in_bit = 1;
                    endif
                    br_metric(m+1,o+1) = br_metric(m+1,o+1) + mod(st_arr(p+1)+in_bit, 2);
                    w_metric(m+1,o+1)  = w_metric(m+1,o+1) + abs(in(n*r+p+1));
                endfor
            endfor

            % Keep the smallest branch metric as the path metric, weight the branch metric
            tmp1 = br_metric(m+1,1) + p_metric(m+1,1);
            tmp2 = br_metric(m+1,2) + p_metric(m+1,2);
            if(tmp1 > tmp2)
                path_metric_new(m+1) = p_metric(m+1,2) + w_metric(m+1,2)*br_metric(m+1,2);
            else
                path_metric_new(m+1) = p_metric(m+1,1) + w_metric(m+1,1)*br_metric(m+1,1);
            endif
        endfor
        path_metric(:,n+2) = path_metric_new';
    endfor

    % Find the minimum metric for the last iteration
    init_min      = 1000000;
    idx           = 1;
    tb_state(idx) = 1000000;
    for(n=0:num_states-1)
        if(path_metric(n+1,(length(in)/r)+1) < init_min)
            init_min      = path_metric(n+1,(length(in)/r)+1);
            tb_state(idx) = n;
        endif
    endfor
    idx = idx + 1;

    % Traceback to find the minimum path metrics at each iteration
    for(n=(length(in)/r)-1:-1:0)
        prev_state_0 = mod(bitshift(tb_state(idx-1), 1) + 0, num_states);
        prev_state_1 = mod(bitshift(tb_state(idx-1), 1) + 1, num_states);

        % Keep the smallest state
        if(path_metric(prev_state_0+1,n+1) > path_metric(prev_state_1+1,n+1))
            tb_state(idx) = prev_state_1;
        else
            tb_state(idx) = prev_state_0;
        endif
        idx = idx + 1;
    endfor

    % Read through the traceback to determine the input bits
    idx = 1;
    for(n=length(tb_state)-2:-1:0)
        % If transition has resulted in a lower valued state,
        % the output is 0 and vice-versa
        if(tb_state(n+1) < tb_state(n+1+1))
            out(idx) = 0;
        elseif(tb_state(n+1) > tb_state(n+1+1))
            out(idx) = 1;
        else
            % Check to see if the transition has resulted in the same state
            % In this case, if state is 0 then output is 0
            if(tb_state(n+1) == 0)
                out(idx) = 0;
            else
                out(idx) = 1;
            endif
        endif
        idx = idx + 1;
    endfor
endfunction
