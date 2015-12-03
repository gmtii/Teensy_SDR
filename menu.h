#ifndef menu_h_
#define menu_h_

#include <stdint.h>

#define START__OPTIONS OPTION_FIRST
#define END__OPTIONS NUM_OPTIONS

typedef enum {
  OPTION_FIRST = 0,
  OPTION_RX_AMP,
  OPTION_RX_PHASE,
  OPTION_RX_GAIN,
  OPTION_IQ_COMP,
  OPTION_DEC_SSB,
  OPTION_DEC_AM,
  OPTION_FREQ_CONV,
  NUM_OPTIONS
} OptionNumber;


// Initialization
void Options_Initialize(void);
void Options_ResetToDefaults(void);

// Work with option data
const char* Options_GetName(int optionIdx);
int16_t Options_GetValue(int optionIdx);
void     Options_SetValue(int optionIdx, int16_t newValue);
uint16_t Options_GetMinimum(int optionIdx);
uint16_t Options_GetMaximum(int optionIdx);
uint16_t Options_GetChangeRate(int optionIdx);

// Option selection
OptionNumber Options_GetSelectedOption(void);
void         Options_SetSelectedOption(OptionNumber newOption);

#endif
