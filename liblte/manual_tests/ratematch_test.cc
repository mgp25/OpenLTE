/*******************************************************************************

    Copyright 2015 Ziming He (ziming.he@pathintel.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*******************************************************************************

/*********************************************************************
    Name: ratematch.cc

    Description: unit tests for openlte turbo rate matching/unmatching
                  compile and run under path: /openlte-code/tests/
    complile:    make -f Makefile_ratematch
    run:         ./ratematch
    Author:      Ziming He <ziming.he@pathintel.com>
    Company:     Path Intelligence Limited, Portsmouth, England, UK

*********************************************************************/

#include "liblte_phy.h"
#include "itpp/itcomm.h"
using namespace itpp;
using std::cout;
using std::endl;

uint32 TURBO_K_TABLE[188] = {  40,  48,  56,  64,  72,  80,  88,  96, 104, 112,
                              120, 128, 136, 144, 152, 160, 168, 176, 184, 192,
                              200, 208, 216, 224, 232, 240, 248, 256, 264, 272,
                              280, 288, 296, 304, 312, 320, 328, 336, 344, 352,
                              360, 368, 376, 384, 392, 400, 408, 416, 424, 432,
                              440, 448, 456, 464, 472, 480, 488, 496, 504, 512,
                              528, 544, 560, 576, 592, 608, 624, 640, 656, 672,
                              688, 704, 720, 736, 752, 768, 784, 800, 816, 832,
                              848, 864, 880, 896, 912, 928, 944, 960, 976, 992,
                             1008,1024,1056,1088,1120,1152,1184,1216,1248,1280,
                             1312,1344,1376,1408,1440,1472,1504,1536,1568,1600,
                             1632,1664,1696,1728,1760,1792,1824,1856,1888,1920,
                             1952,1984,2016,2048,2112,2176,2240,2304,2368,2432,
                             2496,2560,2624,2688,2752,2816,2880,2944,3008,3072,
                             3136,3200,3264,3328,3392,3456,3520,3584,3648,3712,
                             3776,3840,3904,3968,4032,4096,4160,4224,4288,4352,
                             4416,4480,4544,4608,4672,4736,4800,4864,4928,4992,
                             5056,5120,5184,5248,5312,5376,5440,5504,5568,5632,
                             5696,5760,5824,5888,5952,6016,6080,6144};

int main(void)
{

    int i, k, g;
    LIBLTE_PHY_STRUCT *phy_struct_tx;
    phy_struct_tx = (LIBLTE_PHY_STRUCT *)malloc(sizeof(LIBLTE_PHY_STRUCT));
    LIBLTE_PHY_STRUCT *phy_struct_rx;
    phy_struct_rx = (LIBLTE_PHY_STRUCT *)malloc(sizeof(LIBLTE_PHY_STRUCT));
    uint32 N_b_bits; // N_b_bits=N_c_bits=K for single codeblock case
    uint32 D; // defined in 36.212 5.1.4.1.1
    uint32 N_tx_d_bits; // number of transmitted d bits
    uint32 tx_mode = 2; // transmission mode
    LIBLTE_PHY_CHAN_TYPE_ENUM  chan_type = LIBLTE_PHY_CHAN_TYPE_DLSCH;
    uint32 rv_idx = 0; // redundancy version, either 0, 1, 2 or 3
    uint32 N_soft = 250368; // N_{soft} cat 1 UE
    uint32 M_dl_harq = 4; // 8 for FDD-LTE
    uint32 N_codeblocks = 1; // number of code blocks
    uint32 cb = 0; // there is only 1 codeblock
    uint32 G; // minimum value >= 3*D, N_bits_tot (defined in 36.212 5.1.4.1.2, total available bits for one transport block)
    uint32 N_l = 2; // 2 for transmit diversity
    uint32 Q_m = 2; // 2 for QPSK
    uint32 G_prime; // G' defined in 36.212 5.1.4.1.2
    uint32 lambda; // gamma defined in 36.212 5.1.4.1.2
    uint32 N_fill_bits; // not used
    uint32 N_rx_d_bits; // need to be determined at receiver
    BERC berc_d_bits; // d bits transmission error evaluation
    uint32 count=0; // test fail counter

    // for evaluating K_w and N_cb in 36.212 5.1.4.1.2
    uint32 R_tc_sb;
    uint32 K_pi;
    uint32 K_w;
    uint32 K_mimo;
    uint32 N_ir;
    uint32 N_cb;

    for (k=0; k<188; k++){ // turbo code internal interleaver size

        for (g=0; g<1; g++){ // try different values of G = 3*D+g

            // generate transmitted bits
            N_b_bits = TURBO_K_TABLE[k];
            D = N_b_bits + 4;
            N_tx_d_bits = 3*D;
            bvec dlsch_tx_d_bits_itpp(N_tx_d_bits);
            RNG_randomize();
            dlsch_tx_d_bits_itpp = randb(N_tx_d_bits);
            for (i=0; i<N_tx_d_bits; i++){
                // = d^(0)_0, d^(0)_1, ..., d^(0)_{block_length+4-1}, d^(1)_0, d^(1)_1, ..., d^(1)_{block_length+4-1}, d^(2)_0, d^(2)_1, ..., d^(2)_{block_length+4-1}
                phy_struct_tx->dlsch_tx_d_bits[i] = int(dlsch_tx_d_bits_itpp(i));
            }

            G = 3*D + g; // min=3*D, otherwise causes error in d bits transmission

            // calculate K_w and N_cb
            R_tc_sb = 0;
            while(D > (32*R_tc_sb))
            {
                R_tc_sb++;
            }
            K_pi = 32*R_tc_sb;
            K_w = 3*K_pi;
            if(tx_mode == 3 ||
               tx_mode == 4 ||
               tx_mode == 8 ||
               tx_mode == 9)
            {
                K_mimo = 2;
            }else{
                K_mimo = 1;
            }
            if(M_dl_harq < 8)
            {
                N_ir = N_soft/(K_mimo*M_dl_harq);
            }else{
                N_ir = N_soft/(K_mimo*8);
            }
            if(LIBLTE_PHY_CHAN_TYPE_DLSCH == chan_type ||
               LIBLTE_PHY_CHAN_TYPE_PCH   == chan_type)
            {
                if((N_ir/N_codeblocks) < K_w)
                {
                    N_cb = N_ir/N_codeblocks;
                }else{
                    N_cb = K_w;
                }
            }else{
                N_cb = K_w;
            }
            if (N_cb != K_w){
                cout << "N_cb is not equal to K_w, skips the tests when K = " << N_b_bits << endl;
                break;
            }
          
            // Determine E (number of e bits for a codeblock)
            G_prime = G/(N_l*Q_m);
            lambda = G_prime % N_codeblocks; 
            if(cb <= (N_codeblocks - lambda - 1))
            {
                phy_struct_tx->dlsch_N_e_bits[cb] = N_l*Q_m*(G_prime/N_codeblocks);
            }else{
                phy_struct_tx->dlsch_N_e_bits[cb] = N_l*Q_m*(uint32)ceilf((float)G_prime/(float)N_codeblocks);
            }

            liblte_phy_rate_match_turbo(phy_struct_tx,
                                        phy_struct_tx->dlsch_tx_d_bits,
                                        N_tx_d_bits,
                                        N_codeblocks,
                                        tx_mode,
                                        N_soft,
                                        M_dl_harq,
                                        LIBLTE_PHY_CHAN_TYPE_DLSCH,
                                        rv_idx,
                                        phy_struct_tx->dlsch_N_e_bits[cb],
                                        phy_struct_tx->dlsch_tx_e_bits[cb]);

            // error-free reception of e bits
            // number of e bits (E) and e bits are obtained after code_block_deconcatenation
            for (i=0; i<phy_struct_tx->dlsch_N_e_bits[cb]; i++){
                if(phy_struct_tx->dlsch_tx_e_bits[cb][i] == 0){
                    phy_struct_rx->dlsch_rx_e_bits[cb][i] = 1.0;
                }else{
                    phy_struct_rx->dlsch_rx_e_bits[cb][i] = -1.0;
                }
            }
            phy_struct_rx->dlsch_N_e_bits[cb] = phy_struct_tx->dlsch_N_e_bits[cb];

            // determine N_codeblocks and number of b, c, d bits for a codeblock
            memset(phy_struct_rx->dlsch_b_bits, 0, sizeof(uint8)*N_b_bits); // N_b_bits obtained from tbs+24, where tbs known at the receiver
            liblte_phy_code_block_segmentation(phy_struct_rx->dlsch_b_bits,
                                               N_b_bits,
                                               &N_codeblocks, // output, also determined by code_block_deconcatenation
                                               &N_fill_bits,
                                               phy_struct_rx->dlsch_c_bits[0], // output (not used)
                                               LIBLTE_PHY_MAX_CODE_BLOCK_SIZE,
                                               phy_struct_rx->dlsch_N_c_bits); // output
            N_rx_d_bits = 3*(phy_struct_rx->dlsch_N_c_bits[cb]+4);

            // generate 0 bits to determine NULL bit locations in virtual circular buffer wk
            for (i=0; i<N_rx_d_bits; i++){
                phy_struct_rx->dlsch_tx_d_bits[i] = 0;
            }
            liblte_phy_rate_unmatch_turbo(phy_struct_rx,
                                          phy_struct_rx->dlsch_rx_e_bits[cb], // input
                                          phy_struct_rx->dlsch_N_e_bits[cb], // E, input
                                          phy_struct_rx->dlsch_tx_d_bits, //all 0 bits input
                                          N_rx_d_bits/3, // = D, input
                                          N_codeblocks, // input
                                          tx_mode, // input
                                          N_soft, // input
                                          M_dl_harq, // input
                                          LIBLTE_PHY_CHAN_TYPE_DLSCH, // input
                                          rv_idx,// input
                                          phy_struct_rx->dlsch_rx_d_bits, // output
                                          &N_rx_d_bits); //=3*D
            // =========================================================================================

            // compares tx-rx d bits
            bvec dlsch_rx_d_bits_itpp_tmp(N_rx_d_bits);  // output d_bits from rate_unmatch_turbo
            for (i=0; i<N_rx_d_bits; i++){
                if(phy_struct_rx->dlsch_rx_d_bits[i] > 0.0){
                    dlsch_rx_d_bits_itpp_tmp(i) = bin(0);
                }else{
                    dlsch_rx_d_bits_itpp_tmp(i) = bin(1);
                }
            }
            bvec dlsch_rx_d_bits_itpp(N_rx_d_bits);  // transfer to match the tx d_bit format
            for (i=0; i < D; i++){
                dlsch_rx_d_bits_itpp(i) = dlsch_rx_d_bits_itpp_tmp(3*i);
                dlsch_rx_d_bits_itpp(i+D) = dlsch_rx_d_bits_itpp_tmp(3*i+1);
                dlsch_rx_d_bits_itpp(i+2*D) = dlsch_rx_d_bits_itpp_tmp(3*i+2);
            }
            berc_d_bits.clear();
            berc_d_bits.count(dlsch_tx_d_bits_itpp, dlsch_rx_d_bits_itpp);
            if(berc_d_bits.get_errors() == 0){
                // cout << "test pass for K = " << N_b_bits << "; G = " << G << "; K_w = " << K_w << "; N_cb = " << N_cb << endl;
            }else{
                cout << "test fails for K = " << N_b_bits << "; G = " << G << "; K_w = " << K_w << "; N_cb = " << N_cb << "; BER = " << berc_d_bits.get_errorrate() << endl;
                count++;
            }

        } // g loop

    } // k loop

    if(count == 0){
        cout << "all tests pass !! " << endl;
    }else{
        // cout << "some tests fails !! " << endl;
    }

    return 0;

}
