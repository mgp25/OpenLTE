/* -*- c++ -*- */

#define LTE_FDD_DL_FG_API

%include "gnuradio.i"			// the common stuff

%{
#include "LTE_fdd_dl_fg_samp_buf.h"
%}

GR_SWIG_BLOCK_MAGIC(LTE_fdd_dl_fg,samp_buf);
%include "LTE_fdd_dl_fg_samp_buf.h"
