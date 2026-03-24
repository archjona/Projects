#include "driver/uart.h"
#include <stdint.h>
#include <stdio.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "hal/uart_types.h"

static const int port = UART_NUM_1;
static const int rx_buffer_size = 1024;
static const int tx_buffer_size = 0; // man schreibt selber nichts an den gps tracker, also hier nicht
static const int queue_size = 20;
static QueueHandle_t uart_queue;
static const int intr_alloc_flags = 0; //erstmal keine interrupts

#define TX_PIN 32
#define RX_PIN 33

void app_main(void)
{
    // Treiber installieren
    esp_err_t ret = uart_driver_install(port, rx_buffer_size, tx_buffer_size, queue_size, &uart_queue, intr_alloc_flags);
     if (ret != ESP_OK) {

        printf("Driver installation failed: %s\n", esp_err_to_name(ret));
     }

    uart_config_t config = {
    .baud_rate = 9600,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE // kein extra Clear to send und Request to send nötig hier
    };

    ret = uart_param_config(port, &config);
     if (ret != ESP_OK) {

        printf("Configuration failed: %s\n", esp_err_to_name(ret));
     }

    ret = uart_set_pin(port, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
     if (ret != ESP_OK) {

        printf("Setting the pin failed: %s\n", esp_err_to_name(ret));
     }

    uint8_t buffer[256]; // NMEA Protokol, 256 reichen also
    uint32_t data_length = sizeof(buffer);

 while(1) {
        int len = uart_read_bytes(port, &buffer, data_length, pdMS_TO_TICKS(1000));
        printf("DATA: %d \n", len);
    }


}
