/* -*- c++ -*- */

#define LTE_FDD_DL_FS_API

%include "gnuradio.i"			// the common stuff

%{
#include "LTE_fdd_dl_fs_samp_buf.h"
%}

GR_SWIG_BLOCK_MAGIC(LTE_fdd_dl_fs,samp_buf);
%include "LTE_fdd_dl_fs_samp_buf.h"
