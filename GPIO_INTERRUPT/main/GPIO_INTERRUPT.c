#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "inttypes.h"

// Interrupt vs Polling:

// Polling: die ganze Zeit im while loop fragen nach button_state --> inneffektiv, verbraucht viel
// Rechenleistung und kurzes Signal könnte nicht wahrgenommen werden

// Interrupt: Hardware selbst überwacht den pin, nicht nur die CPU in dem while loop
// sobald eine Spannungsveränderung vorliegt --> sofort wakeup der CPU für interrupt

#define interrupt_pin 5
volatile uint16_t interrupt_count = 0; // volatile == kann sich jederzeit ändern
volatile bool button_state = false;

static void IRAM_ATTR gpio_isr_handler(void *arg) { //IRAM_ATTR ist makro für speicher im RAM --> schneller

	// gpio_isr_handler_add wird nur wenn ein signal an Pin 5 wahrgenommen wird aufgerufen
	// kein if nötig

	interrupt_count++;
	button_state = true;

}

void app_main(void)
{

	gpio_reset_pin(interrupt_pin);
	gpio_set_direction(interrupt_pin, GPIO_MODE_INPUT);

	gpio_set_pull_mode(interrupt_pin, GPIO_PULLUP_ONLY);
	gpio_set_intr_type(interrupt_pin, GPIO_INTR_POSEDGE); // rising edge interrupt: signalchange -> interrupt

	gpio_install_isr_service(0); // initialisierung
	gpio_isr_handler_add(interrupt_pin, gpio_isr_handler, (void*) interrupt_pin); // (void*) wegen typecast 
	// 1. Pin, 2. Funktion, in die argument geladen wird 3. Argument
	gpio_intr_enable(interrupt_pin);

	while(1) {

		if(button_state == true) {

			printf("Number of Interrupts: %d\n", interrupt_count);
			button_state = false;

		}
		vTaskDelay(10);
	}



}
