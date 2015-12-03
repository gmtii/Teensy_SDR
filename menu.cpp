#include "menu.h"
#include "demod.h"
#include "freq_conv.h"

extern float I_gain;
extern float R_xgain;
extern float Q_gain;
extern int8_t RF_gain;
extern bool iq_correction;
extern bool decimate_SSB;
extern bool decimate_AM;
extern bool freqconv_sw;

extern AudioEffectFreqConv      FreqConv;

static OptionNumber s_currentOptionNumber = OPTION_FIRST;

typedef struct
{
  const char* Name;
  const int16_t Initial;
  const int16_t Minimum;
  const int16_t Maximum;
  const int16_t ChangeUnits;
  int16_t CurrentValue;
} OptionStruct;

// Order must match OptionNumber in options.h
static OptionStruct s_optionsData[] = {
  {
    /*Name*/ "Null  ",
    /*Init*/ 1,
    /*Min */ -10,
    /*Max */ 10,
    /*Rate*/ 1,
    /*Data*/ 0,
  },
  {
    /*Name*/ "Rx Amp.",
    /*Init*/ 5000,
    /*Min */ 1000,
    /*Max */ 10000,
    /*Rate*/ 100,
    /*Data*/ 0,
  },
  {
    /*Name*/ "Rx Phas",
    /*Init*/ 0,
    /*Min */ -10000,
    /*Max */ 10000,
    /*Rate*/ 100,
    /*Data*/ 0,
  },
  {
    /*Name*/ "RF Gain",
    /*Init*/ 2,
    /*Min */ -10,
    /*Max */ 10,
    /*Rate*/ 1,
    /*Data*/ 0,
  },
  {
    /*Name*/ "IQ Corr",
    /*Init*/ 1,
    /*Min */ 0,
    /*Max */ 1,
    /*Rate*/ 1,
    /*Data*/ 0,
  },
  {
    /*Name*/ "Dec SSB",
    /*Init*/ 0,
    /*Min */ 0,
    /*Max */ 1,
    /*Rate*/ 1,
    /*Data*/ 0,
  },
  {
    /*Name*/ "Dec AM.",
    /*Init*/ 0,
    /*Min */ 0,
    /*Max */ 1,
    /*Rate*/ 1,
    /*Data*/ 0,
  },
  {
    /*Name*/ "Frq Cnv",
    /*Init*/ 1,
    /*Min */ 0,
    /*Max */ 1,
    /*Rate*/ 1,
    /*Data*/ 0,
  },


};

uint16_t Options_GetMinimum(int optionIdx)
{
  if (optionIdx >= 0 && optionIdx < NUM_OPTIONS)
    return s_optionsData[optionIdx].Minimum;
  else
    return 0;
}
uint16_t Options_GetMaximum(int optionIdx)
{
  if (optionIdx >= 0 && optionIdx < NUM_OPTIONS)
    return s_optionsData[optionIdx].Maximum;
  else
    return 0;
}
const char* Options_GetName(int optionIdx)
{
  if (optionIdx >= 0 && optionIdx < NUM_OPTIONS)
    return s_optionsData[optionIdx].Name;
  else
    return 0;
}
int16_t Options_GetValue(int optionIdx)
{
  if (optionIdx >= 0 && optionIdx < NUM_OPTIONS)
    return s_optionsData[optionIdx].CurrentValue;
  else
    return 0;
}
uint16_t Options_GetChangeRate(int optionIdx)
{
  if (optionIdx >= 0 && optionIdx < NUM_OPTIONS)
    return s_optionsData[optionIdx].ChangeUnits;
  else
    return 0;
}


void Options_SetValue(int optionIdx, int16_t newValue)
{
  if (optionIdx >= 0 && optionIdx < NUM_OPTIONS)
    s_optionsData[optionIdx].CurrentValue = newValue;
  else
    return;

  /*
    Process each option individually as needed.
  */
  switch (optionIdx) {

    case OPTION_FIRST:
      RF_gain = (int8_t)newValue;
      break;

    case OPTION_RX_AMP:
      Q_gain = ((float) newValue) / 10000.0;
      break;

    case OPTION_RX_PHASE:
      R_xgain = ((float) newValue) / 10000.0;
      break;

    case OPTION_RX_GAIN:
      RF_gain = (int8_t) newValue;
      break;

    case OPTION_IQ_COMP:
      iq_correction = (bool) newValue;
      s_optionsData[optionIdx].CurrentValue = iq_correction;
      break;

    case OPTION_DEC_SSB:
      decimate_SSB = (bool) newValue;
      s_optionsData[optionIdx].CurrentValue = decimate_SSB;
      break;

    case OPTION_DEC_AM:
      decimate_AM = (bool) newValue;
      s_optionsData[optionIdx].CurrentValue = decimate_AM;
      break;

    case OPTION_FREQ_CONV:
      freqconv_sw = (bool) newValue;
      s_optionsData[optionIdx].CurrentValue = freqconv_sw;
      FreqConv.passthrough(freqconv_sw);
      break;


    default:
      break;

  }

}

void Options_ResetToDefaults(void)
{
  for (int i = 0; i < NUM_OPTIONS; i++) {
    Options_SetValue(i, s_optionsData[i].Initial);
  }
}

