#include "filters.h"

short RX_hilbert45[HILBERT_COEFFS] = {
#include "RX_hilbert_45.h" 
};

short RX_hilbertm45[HILBERT_COEFFS] = {
#include "RX_hilbert_m45.h" 
};

short TX_hilbert45[HILBERT_COEFFS] = {
#include "TX_hilbert_45.h" 
};

short TX_hilbertm45[HILBERT_COEFFS] = {
#include "TX_hilbert_m45.h" 
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

