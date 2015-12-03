// Number of coefficients

#define BPF_COEFFS 100
#define HILBERT_COEFFS 100

extern short RX_hilbert_Q_37k[];
extern short RX_hilbert_I_37k[];

extern short RX_hilbert_Q_10k[];
extern short RX_hilbert_I_10k[];

extern short firbpf_usb[];
extern short firbpf_dsb[];
extern short firbpf_lsb[];


extern short fir_input[];

extern short postfir_bpf_am[];

#define COEFF_LPF  54
#define COEFF_3700 120

extern short postfir_700[];
extern short postfir_1700hz[];
extern short postfir_3700hz[];
extern short postfir_20khz[];

#define COEFF_700 120
extern short postfir_lpf[];

extern short fir_decimate[];
extern short fir_interpolate[];





