#if defined (__USE_LPCOPEN)
#if defined(NO_BOARD_LIB)
#include "chip.h"
#else
#include "board.h"
#endif
#endif
#include <cr_section_macros.h>
#include <math.h>
#include "arm_math.h"
#include "oled.h"

#define _LPC_ADC_ID LPC_ADC
#define FS 32000
#define FFT_N 256
#define FFT_2N (FFT_N*2)
#define FFT_N2 (FFT_N/2)

arm_cfft_radix4_instance_f32 fftInstance;
float32_t x_buff[FFT_2N] = { 0 };
float32_t peak_buff[FFT_2N] = { 0 };
float32_t win[FFT_N];
float32_t emph[FFT_N];
float32_t in_buff[FFT_N];
uint32_t wr = 0;
uint16_t dataADC;

volatile bool f1 = false;
/*--------------------------------------------------------------------------------*/
void Spectrum_Line_f32(float32_t *pSrc, uint32_t numSamples) {
	uint16_t y = 0, y1 = 63;
	for (int i = 1; i < numSamples; i++) {
		y = 63 - pSrc[i];
		OLED_Draw_Line(i - 1, y1, i, y);
		y1 = y;
	}
}
/*--------------------------------------------------------------------------------*/
void Spectrum_Log_Line_f32(float32_t *pSrc, uint32_t numSamples) {
	uint16_t y = 0, y1 = 63;
	for (int i = 1; i < numSamples; i++) {
		y = 40 * log10(1 / ((pSrc[i] / (float32_t) FFT_N2) + 0.026)); // -32 dB
		OLED_Draw_Line(i - 1, y1, i, y);
		y1 = y;
	}
}
/*--------------------------------------------------------------------------------*/
void Spectrum_Bar_f32(float32_t *pSrc, uint32_t numSamples, bool peak)
{
	uint16_t y = 0, y1 = 63;
	for (int i=1; i<numSamples; i++)
	{
		y = 40*log10(1/((pSrc[i*2]/(float32_t) FFT_N2)+0.026));
		OLED_Draw_Line(i*2,y1,i*2,y);
		if(peak)
		{
			if(peak_buff[i*2]>=y)
				peak_buff[i*2] = y;
			OLED_Draw_Point(i*2,peak_buff[i*2],1);
			peak_buff[i*2]++;
		}
	}
}
/*--------------------------------------------------------------------------------*/
void ADC_IRQHandler(void) {
	Chip_ADC_ReadValue(_LPC_ADC_ID, ADC_CH0, &dataADC);
	if (f1 == false) {
		in_buff[wr++] = (dataADC - 2048) / 2048.0;
		if (wr == FFT_N) {
			f1 = true;
			wr = 0;
		}
	}
}
/*--------------------------------------------------------------------------------*/
int main(void) {
	arm_cfft_radix4_init_f32(&fftInstance, FFT_N, 0, 1);
	static ADC_CLOCK_SETUP_T ADCSetup;
	uint32_t timerFreq;
#if defined (__USE_LPCOPEN)
// Read clock settings and update SystemCoreClock variable
	SystemCoreClockUpdate();
#if !defined(NO_BOARD_LIB)
// Set up and initialize all required blocks and
// functions related to the board hardware
	Board_Init();
// Set the LED to the state of "On"
	Board_LED_Set(0, true);
#endif
#endif
// TODO: insert code here
	/*--------------------- OLED Init -----------------------*/
	OLED_Init();
	/*--------------------- ADC Init ------------------------*/
	Chip_ADC_Init(_LPC_ADC_ID, &ADCSetup);
	Chip_ADC_Int_SetChannelCmd(_LPC_ADC_ID, ADC_CH0, ENABLE);
	Chip_ADC_EnableChannel(_LPC_ADC_ID, ADC_CH0, ENABLE);
	NVIC_ClearPendingIRQ(ADC_IRQn);
	NVIC_EnableIRQ(ADC_IRQn);
	Chip_ADC_SetStartMode(_LPC_ADC_ID, ADC_START_ON_ADCTRIG0,
			ADC_TRIGGERMODE_RISING);
	/*-------------------- Timer32_0 Init --------------------*/
	Chip_TIMER_Init(LPC_TIMER32_0);
	timerFreq = Chip_Clock_GetSystemClockRate();
	Chip_TIMER_Reset(LPC_TIMER32_0);
	Chip_TIMER_MatchEnableInt(LPC_TIMER32_0, 0);
	Chip_TIMER_SetMatch(LPC_TIMER32_0, 0, (timerFreq / (2 * FS)));
	Chip_TIMER_ResetOnMatchEnable(LPC_TIMER32_0, 0);
	Chip_TIMER_ExtMatchControlSet(LPC_TIMER32_0, 0, TIMER_EXTMATCH_TOGGLE, 0);
	Chip_TIMER_Enable(LPC_TIMER32_0);
	/*-------------------- End of Init ----------------------*/
// Time Window
	for (int n = 0; n < FFT_N; n++)
	{
		//win[n]=1; // Rect
		win[n] = (0.5 * (1 - arm_cos_f32(2 * PI * n / (FFT_N - 1)))); // Hann
	}
	while (1)
	{
		if (f1)
		{
			for (int i = 0; i < FFT_N; i++)
			{
				x_buff[i * 2] = win[i] * in_buff[i];
				x_buff[i * 2 + 1] = 0;
			}
			arm_cfft_radix4_f32(&fftInstance, x_buff);
			arm_cmplx_mag_f32(x_buff, x_buff, FFT_N);
			OLED_Clear_Screen(0);
			OLED_Puts(0, 0, "Real-Time FFT Radix-4");
			for (int i = 0; i < FFT_N; i++)
				x_buff[i] *= (0.25 * i); // emphase
			//Spectrum_Log_Line_f32(x_buff, FFT_N2);
			Spectrum_Bar_f32(x_buff, FFT_N2, 1);

			OLED_Refresh_Gram();
			f1 = false;
		}
	}
	return 0;
}
