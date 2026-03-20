// Programm zum Lernen von SPI Bus Prtokoll anhand des BME 280 Sensors.
// Vorteil: schneller als UART und I2C. supportet vollduplex kommunikation
// Wird oft benutzt für Datentransfer zwischen smart controller und einem "dummen" Gerät
// Sensoren, ADCs, DACs, real-time clocks, game controller, displays
// ein master, eine oder mehrere slaves --> keine arbitration!
// 4 Leitungen:
// CS/SS --> Chip select, auswahl des Kommunikationspartners, also der slave
// SCLK --> Synchronous Clock, für timing und synchronisation
// MOSI --> Master Out Slave in. Daten werden verschickt vom Master
// MISO --> Master In Slave out. Master bekommt Daten vom Slave


// Besonderheit bei SPI beim BME280: Das MSB ist immer 1 oder 0 abhängig von R/W!
// nur die 7 Bits danach sind die eigentliche Adresse
#include "driver/spi_master.h"
#include <stdint.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"

#define MOSI_PIN 14
#define MISO_PIN 13
#define CS_PIN 15
#define SCK_PIN 12

//statisches Register aus dem Datenblatt
static const uint8_t crtl_meas = 0xF4;

// device handle für alle Funktionen
static spi_device_handle_t bme280_handle;


// Write Funktion um nicht immer neuen struct für jede transaction machen zu müssen

static void bme280_write_reg(uint8_t reg_addr, uint8_t data) {
    uint8_t control_byte = reg_addr & 0x7F; // Write = 0 als MSB, dann rest der Adresse
    // 0x7F = 0111 1111

    spi_transaction_t write = {

    .flags = SPI_TRANS_USE_TXDATA,
    .length = 16,
    .tx_data = {control_byte, data}

    };

    spi_device_transmit(bme280_handle, &write);
}

// Read Funktion um Sensor register dann auszulesen

static uint8_t bme280_read_reg(uint8_t reg_addr) {
    uint8_t control_byte = reg_addr | 0x80;
    // 0x80 = 1000 0000 (MSB muss 1 sein)
    // weil control byte die Adresse enthält,
    // muss kein extra adressen byte geschickt werden
    spi_transaction_t read = {

    .flags = SPI_TRANS_USE_RXDATA,
    .length = 16,
    .rxlength = 16,
    .tx_data = {control_byte, 0},
    .rx_data = {0, 0} // Initialisierung, kann auch leer bleiben

    };

    spi_device_transmit(bme280_handle, &read);
    // speichert return in rx_data[0] und [1]
    // weil in [0] nur read bestätigung kommt, muss 1 gelesen werden
    // es muss mit dummy byte (0) gemacht werden, damit ein extra SPI
    // Takt erzeugt wird

    return read.rx_data[1]; // spuckt wert in [1] aus
}

static uint32_t bme280_read_raw_temp(void) {

    uint8_t msb = bme280_read_reg(0xFA); // MSB REG
    uint8_t lsb = bme280_read_reg(0xFB); // LSB REG
    uint8_t xlsb = bme280_read_reg(0xFC); // xLSB REG
    // nur die oberen 4 bits von 0xFC werden ankommen laut Datenblatt

    return (uint32_t) (((msb << 12) | (lsb << 4)) | (xlsb >> 4));
}

void app_main(void)

{

    spi_bus_config_t bus_cfg = {

    .mosi_io_num = MOSI_PIN,
    .miso_io_num = MISO_PIN,
    .sclk_io_num = SCK_PIN,

    };

    spi_device_interface_config_t dev_conf = {

    .mode = 0, // Sensor will 0 0 SPI Mode, das macht 0 hier
    .clock_speed_hz = 5000000, // 5 MHz
    .spics_io_num = CS_PIN,
    .queue_size = 3

    };

    spi_bus_initialize(SPI1_HOST, &bus_cfg, SPI_DMA_CH_AUTO);

    spi_bus_add_device(SPI1_HOST, &dev_conf, &bme280_handle);

    // Oversampling (x1) aktivieren
    // 0x23 (Bits 7,6,5 müssen 0 0 1 sein für oversampling x1 und 1,0 müssen 1 1 sein für normal mode)
    // crtl_meas (register für modus)

    bme280_write_reg(crtl_meas, 0x23);

    while(1) {

        uint32_t raw_temp = bme280_read_raw_temp();
        printf("Raw Temp: %lu\n", raw_temp); //lu ist unsigned long
         vTaskDelay(pdMS_TO_TICKS(700));
    }
}
