#include "filters.h"

short RX_hilbert_Q_37k[HILBERT_COEFFS] = {
#include "RX_hilbert_Q_37k.h" 
};

short RX_hilbert_I_37k[HILBERT_COEFFS] = {
#include "RX_hilbert_I_37k.h" 
};

short RX_hilbert_Q_10k[HILBERT_COEFFS] = {
#include "RX_hilbert_Q_10k.h" 
};

short RX_hilbert_I_10k[HILBERT_COEFFS] = {
#include "RX_hilbert_I_10k.h" 
};

short firbpf_usb[BPF_COEFFS] = {
#include "fir_usb.h" 
};

short firbpf_dsb[BPF_COEFFS] = {
#include "fir_dsb.h" 
};

short firbpf_lsb[BPF_COEFFS] = {
#include "fir_lsb.h" 
};

short fir_input[BPF_COEFFS] = {
#include "fir_input.h" 
};

short postfir_bpf_am[BPF_COEFFS] = {
#include "postfir_bpf_am.h" 
};

short postfir_700[COEFF_700] = {
#include "postfir_700hz.h" 
};

short postfir_lpf[COEFF_LPF] = {
#include "postfir_lpf.h" 
};

short postfir_1700hz[55] = {
#include "postfir_1700hz.h" 
};

short postfir_3700hz[57] = {
#include "postfir_3700hz.h" 
};

short fir_decimate[4] = {
#include "fir_decimate.h" 
};

short fir_interpolate[16] = {
#include "fir_interpolate.h" 
};






