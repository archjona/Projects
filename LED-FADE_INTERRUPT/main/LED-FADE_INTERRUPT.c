#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/ledc.h"
#include "inttypes.h"

#define interrupt_pin 4
volatile bool button_state = false;
volatile uint16_t interrupt_count = 0;

static void IRAM_ATTR gpio_isr_handler(void *arg) {

	interrupt_count++;
	button_state = true;

}

void app_main(void)
{

    gpio_reset_pin(interrupt_pin);
	gpio_set_direction(interrupt_pin, GPIO_MODE_INPUT);

	gpio_set_pull_mode(interrupt_pin, GPIO_PULLUP_ONLY);
	gpio_set_intr_type(interrupt_pin, GPIO_INTR_POSEDGE); 

	gpio_install_isr_service(0); 
	gpio_isr_handler_add(interrupt_pin, gpio_isr_handler, (void*) interrupt_pin); 
	gpio_intr_enable(interrupt_pin);

	ledc_timer_config_t timer = { // einstellung

		.speed_mode = LEDC_LOW_SPEED_MODE,
		.duty_resolution = LEDC_TIMER_8_BIT, // duty werte (0-255 bei 8 bit)
		.timer_num = LEDC_TIMER_0,
		.freq_hz = 10000, //frequenz des timers
		.clk_cfg = LEDC_AUTO_CLK // automatische anpassung an duty resolution, usw.

	};

	ledc_timer_config(&timer); // pass in funktion

	ledc_channel_config_t channel = { // einstellung

		.gpio_num = 5,
		.speed_mode = LEDC_LOW_SPEED_MODE,
		.channel = LEDC_CHANNEL_0, 
		.timer_sel = LEDC_TIMER_0,
		.duty = 0,
		.hpoint = 0 // startpunkt, quasi phasenverschiebung für HIGH

	};

	ledc_channel_config(&channel); //pass in funktion

	ledc_fade_func_install(0); //initialisierung

	while(1) {

	/*	ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 255, 1000);
		ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);
		ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0, 1000);
		ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);
		vTaskDelay(1200 / portTICK_PERIOD_MS);

		MÖGLICHKEIT 2: (mit interrupt) */
		for (int i = 0; i <= 255; i++) {

		if(button_state) break;

		ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, i, 0);
		vTaskDelay(pdMS_TO_TICKS(10));

	 	}
	 	for (int i = 255; i >=0; i--) {

	 	if(button_state) break;

		ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, i, 0);
		vTaskDelay(pdMS_TO_TICKS(10));
	 	}
	 	if(button_state) {

	 		printf("INTERRUPT NUMMER %d !\n", interrupt_count);
			ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0, 0);
			button_state = false;
			vTaskDelay(pdMS_TO_TICKS(20));

	 	}
	}

}
