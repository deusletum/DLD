/*
 * leds.c
 */

#include "globals.h"
#include "leds.h"
#include "dig_inouts.h"
#include "timekeeper.h"
#include "params.h"


extern volatile uint32_t ping_ledbut_tmr;
extern volatile uint32_t loopled_tmr[2];
extern volatile uint32_t divmult_time[2];
extern volatile uint32_t ping_time;
extern uint8_t mode[NUM_CHAN][NUM_CHAN_MODES];
extern uint8_t global_mode[NUM_GLOBAL_MODES];

extern uint32_t flash_firmware_version;

uint16_t loop_led_brightness=2;

uint8_t loop_led_state[NUM_CHAN]={0,0};


void update_ping_ledbut(void)
{
	if (global_mode[CALIBRATE])
		LED_PINGBUT_OFF;
	else if (global_mode[SYSTEM_SETTINGS])
	{
		//LED_PINGBUT_OFF;
	}
	else
	{
		if (ping_ledbut_tmr>=ping_time)
		{
			LED_PINGBUT_ON;
			reset_ping_ledbut_tmr();
		}
		else if (ping_ledbut_tmr >= (ping_time>>1))
		{
			LED_PINGBUT_OFF;
		}
	}
}

/*
  			|| (mode[channel][WINDOWMODE_POT]==WINDOW && (flicker_ctr & 0x3FFFF) <= 0x5000)
			|| (mode[channel][TIMEMODE_POT]==MOD_READWRITE_TIME_NOQ && (flicker_ctr & 0xFFFF) > 0xC000)

 */



void update_channel_leds(void)
{
	static uint32_t led_flasher=0;
	uint8_t channel;

	for (channel=0;channel<NUM_CHAN;channel++)
	{ //350ns - 450ns
		if (loopled_tmr[channel] >= divmult_time[channel]){
			reset_loopled_tmr(channel);
		}

		else if (loopled_tmr[channel] >= (divmult_time[channel]>>1))
		{

			loop_led_state[channel]=0;

			if (channel==0) {
				CLKOUT1_OFF;
			} else {
				CLKOUT2_OFF;
			}
		}

		else if (mode[channel][LOOP_CLOCK_GATETRIG] == TRIG_MODE && loopled_tmr[channel] >= TRIG_TIME)
		{
			if (channel==0)
				CLKOUT1_OFF;
			 else
				CLKOUT2_OFF;

		}

	}
}

void update_INF_REV_ledbut(uint8_t channel){
	static uint32_t led_flasher=0;
	uint8_t switch1, switch2;

	switch1=TIMESW_CH1;
	switch2=TIMESW_CH2;

	if (global_mode[CALIBRATE])
	{
		//Flash button LEDs to indicate we're in Calibrate mode
		led_flasher+=10000;
		if (led_flasher<UINT32_MAX/20)
		{
			LED_INF1_OFF;
			LED_INF2_OFF;
			LED_REV1_OFF;
			LED_REV2_OFF;
		}
		else
		{
			LED_INF1_ON;
			LED_INF2_ON;
			LED_REV1_ON;
			LED_REV2_ON;
		}
	}
	else if (global_mode[SYSTEM_SETTINGS] && switch1==SWITCH_UP && switch2==SWITCH_UP)
	{
		//Display firmware version

			if (flash_firmware_version & 0b0001)
				LED_REV1_ON;
			else
				LED_REV1_OFF;

			if (flash_firmware_version & 0b0010)
				LED_INF1_ON;
			else
				LED_INF1_OFF;

			if (flash_firmware_version & 0b0100)
				LED_INF2_ON;
			else
				LED_INF2_OFF;

			if (flash_firmware_version & 0b1000)
				LED_REV2_ON;
			else
				LED_REV2_OFF;
	}

	else if (global_mode[SYSTEM_SETTINGS] && switch1==SWITCH_UP && switch2==SWITCH_DOWN)
	{
		//Display Trig/Gate settings

			if (mode[0][LOOP_CLOCK_GATETRIG] == GATE_MODE)
				LED_REV1_ON;
			else
				LED_REV1_OFF;

			if (mode[0][MAIN_CLOCK_GATETRIG] == GATE_MODE)
				LED_INF1_ON;
			else
				LED_INF1_OFF;

			if (mode[1][LOOP_CLOCK_GATETRIG] == GATE_MODE)
				LED_REV2_ON;
			else
				LED_REV2_OFF;

			LED_INF2_OFF;
	}

	else if (global_mode[SYSTEM_SETTINGS] && switch1==SWITCH_DOWN && switch2==SWITCH_UP)
	{
		// Display Auto-Mute and Soft Clip settings

		if (global_mode[AUTO_MUTE])
			LED_REV1_ON;
		else
			LED_REV1_OFF;

		if (global_mode[SOFTCLIP])
			LED_REV2_ON;
		else
			LED_REV2_OFF;

		LED_INF1_OFF;
		LED_INF2_OFF;
	}

	else
	{
		//Display Reverse and Infinite settings

		if (!mode[channel][REV])
		{
			if (channel==0)	LED_REV1_OFF;
			else			LED_REV2_OFF;
		}
		else
		{
			if (channel==0)	LED_REV1_ON;
			else 			LED_REV2_ON;
		}

		if (!mode[channel][INF])
		{
			if (channel==0)	LED_INF1_OFF;
			else			LED_INF2_OFF;
		}
		else
		{
			if (channel==0)	LED_INF1_ON;
			else 			LED_INF2_ON;
		}
	}

}

void init_LED_PWM_IRQ(void)
{
	TIM_TimeBaseInitTypeDef tim;
	NVIC_InitTypeDef nvic;

	RCC_APB1PeriphClockCmd(LED_TIM_RCC, ENABLE);

	nvic.NVIC_IRQChannel = LED_TIM_IRQn;
	nvic.NVIC_IRQChannelPreemptionPriority = 0;
	nvic.NVIC_IRQChannelSubPriority = 2;
	nvic.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic);

	TIM_TimeBaseStructInit(&tim);
	tim.TIM_Period = 17500; //168MHz / 2 / 17500 = 4.8kHz (208.3us) ... / 32 =
	tim.TIM_Prescaler = 0;
	tim.TIM_ClockDivision = 0;
	tim.TIM_CounterMode = TIM_CounterMode_Up;

	TIM_TimeBaseInit(LED_TIM, &tim);
	TIM_ITConfig(LED_TIM, TIM_IT_Update, ENABLE);
	TIM_Cmd(LED_TIM, ENABLE);
}



//runs @ 208uS (4.8kHz), with 32 steps => 6.6ms PWM period = 150Hz
void LED_PWM_IRQHandler(void)
{
	static uint32_t loop_led_PWM_ctr=0;
	if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
		 //begin1: 300ns - 450ns

		if (loop_led_state[0] && (loop_led_PWM_ctr<loop_led_brightness))
			LED_LOOP1_ON;
		else
			LED_LOOP1_OFF;

		if (loop_led_state[1] && (loop_led_PWM_ctr<loop_led_brightness))
			LED_LOOP2_ON;
		else
			LED_LOOP2_OFF;

		if (loop_led_PWM_ctr++>32)
			loop_led_PWM_ctr=0;

		//end1: 300ns - 450ns

		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

	}

}