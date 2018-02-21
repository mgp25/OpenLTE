%
% Copyright 2013 Ben Wojtowicz
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
% Function:    lte_prach_bb_gen
% Description: Generates the baseband signal for a PRACH
% Inputs:      x_u_v             - The PRACH preamble signal to be transmitted
%              prach_freq_offset - Offset in the frequency domain of the PRACH
%              pre_format        - Preamble format of the PRACH
%              N_ul_rb           - Number of uplink resource blocks available
% Outputs:     prach_bb - PRACH baseband signal
% Spec:        3GPP TS 36.211 section 5.7.3 v10.1.0
% Rev History: Ben Wojtowicz 07/21/2013 Created
%              Ben Wojtowicz 08/26/2013 Fixed a bug in the frequency domain conversion
%
function [prach_bb] = lte_prach_bb_gen(x_u_v, prach_freq_offset, pre_format, N_ul_rb)

    % Determine the frequency delta between PRACH and PUSCH/PUCCH and the preamble length
    if(pre_format >= 0 && pre_format <= 3)
        delta_f_RA = 1250;
        N_zc       = 839;
        phi        = 7;
    else
        delta_f_RA = 7500;
        N_zc       = 139;
        phi        = 2;
    endif

    % Determine the FFT, sequence, and cyclic prefix lengths
    if(pre_format == 0)
        T_fft = 24576;
        T_seq = 24576;
        T_cp  = 3168;
    elseif(pre_format == 1)
        T_fft = 24576;
        T_seq = 24576;
        T_cp  = 21024;
    elseif(pre_format == 2)
        T_fft = 24576;
        T_seq = 2*24576;
        T_cp  = 6240;
    elseif(pre_format == 3)
        T_fft = 24576;
        T_seq = 2*24576;
        T_cp  = 21024;
    else
        T_fft = 4096;
        T_seq = 4096;
        T_cp  = 448;
    endif

    % Parameters
    % FIXME: Only supports 30.72MHz sampling rate
    N_ra_prb = prach_freq_offset;
    N_sc_rb  = 12;
    FFT_size = 2048;
    k_0      = N_ra_prb*N_sc_rb - N_ul_rb*N_sc_rb/2 + (FFT_size/2);
    K        = 15000/delta_f_RA;

    % Convert preamble to the frequency domain
    x_fd_sig = fftshift(fft(x_u_v));

    % Construct full frequency domain signal
    fd_sig                         = zeros(1,T_fft);
    start                          = phi+K*(k_0+0.5);
    fd_sig(start+1:start+N_zc-1+1) = x_fd_sig;

    % Construct output sequence
    out_seq = ifft(ifftshift(fd_sig));

    % Add cyclic prefix
    if(T_fft == T_seq)
        prach_bb = [out_seq(end-T_cp:end), out_seq];
    else
        prach_bb = [out_seq(end-T_cp:end), out_seq, out_seq];
    endif
endfunction
