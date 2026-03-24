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
#include "freertos/task.h"  // WICHTIG: Für vTaskDelay
#include "esp_log.h"        // Für Debug-Ausgaben (optional, aber hilfreich)

#define CS_PIN 15
#define SCK_PIN 14
#define MISO_PIN 12
#define MOSI_PIN 26

// statisches Register aus dem Datenblatt
static const uint8_t crtl_meas = 0xF4;

// device handle für alle Funktionen
static spi_device_handle_t bme280_handle;


// Write Funktion um nicht immer neuen struct für jede transaction machen zu müssen
static void bme280_write_reg(uint8_t reg_addr, uint8_t data) {
    uint8_t control_byte = reg_addr & 0x7F; // Write = 0 als MSB, dann rest der Adresse
    // 0x7F = 0111 1111

    spi_transaction_t write = {0};  // Alle Felder auf 0 setzen

    write.flags = SPI_TRANS_USE_TXDATA;
    write.length = 16;  // 2 Bytes * 8 Bits
    write.tx_data[0] = control_byte;
    write.tx_data[1] = data;

    esp_err_t ret = spi_device_transmit(bme280_handle, &write);
    if (ret != ESP_OK) {
        printf("SPI Write failed: %s\n", esp_err_to_name(ret));
    }
}

// Read Funktion um Sensor register dann auszulesen
static uint8_t bme280_read_reg(uint8_t reg_addr) {
    uint8_t control_byte = reg_addr | 0x80;
    // 0x80 = 1000 0000 (MSB muss 1 sein)
    // weil control byte die Adresse enthält,
    // muss kein extra adressen byte geschickt werden

    spi_transaction_t read = {0};  // Alle Felder auf 0 setzen

    read.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
    read.length = 16;      // 2 Bytes senden
    read.rxlength = 16;    // 2 Bytes empfangen
    read.tx_data[0] = control_byte;
    read.tx_data[1] = 0;   // Dummy byte für Takt
    // rx_data wird automatisch von der SPI-Funktion gefüllt

    esp_err_t ret = spi_device_transmit(bme280_handle, &read);
    if (ret != ESP_OK) {
        printf("SPI Read failed: %s\n", esp_err_to_name(ret));
        return 0;
    }

    // speichert return in rx_data[0] und [1]
    // weil in [0] nur read bestätigung kommt, muss 1 gelesen werden
    // es muss mit dummy byte (0) gemacht werden, damit ein extra SPI
    // Takt erzeugt wird
    return read.rx_data[1]; // spuckt wert in [1] aus
}

static uint32_t bme280_read_raw_temp(void) {
    uint8_t msb = bme280_read_reg(0xFA); // MSB REG
    vTaskDelay(pdMS_TO_TICKS(1));  // Kleine Verzögerung
    uint8_t lsb = bme280_read_reg(0xFB); // LSB REG
    vTaskDelay(pdMS_TO_TICKS(1));  // Kleine Verzögerung
    uint8_t xlsb = bme280_read_reg(0xFC); // xLSB REG
    // nur die oberen 4 bits von 0xFC werden ankommen laut Datenblatt

    return ((uint32_t)msb << 12) | ((uint32_t)lsb << 4) | (xlsb >> 4); // FIX: sicheres casten vor dem shiften
}

void app_main(void)
{
    esp_err_t ret; //zum error erkennen

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = MOSI_PIN,
        .miso_io_num = MISO_PIN,
        .sclk_io_num = SCK_PIN,
        .quadwp_io_num = -1,   // Nicht verwendet
        .quadhd_io_num = -1,   // Nicht verwendet
        .max_transfer_sz = 32  // Maximale Transfergröße
    };

    spi_device_interface_config_t dev_conf = {
        .mode = 0,
        .clock_speed_hz = 1000000,
        .spics_io_num = CS_PIN,
        .queue_size = 1,
        .flags = 0
    };

    ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_DISABLED);
    if (ret != ESP_OK) {
        printf("Failed to initialize SPI bus: %s\n", esp_err_to_name(ret));
        return;
    }
    printf("SPI bus initialized successfully\n");

    ret = spi_bus_add_device(SPI2_HOST, &dev_conf, &bme280_handle);
    if (ret != ESP_OK) {
        printf("Failed to add SPI device: %s\n", esp_err_to_name(ret));
        return;
    }
    printf("SPI device added successfully\n");

    if (bme280_handle == NULL) {
        printf("Device handle is NULL!\n");
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    uint8_t chip_id = bme280_read_reg(0xD0);  // Chip ID Register
    printf("BME280 Chip ID: 0x%02X (sollte 0x60 sein)\n", chip_id);

    if (chip_id != 0x60) {
        printf("WARNING: Wrong chip ID! Check wiring. Got 0x%02X\n", chip_id);
        // Trotzdem weitermachen, aber Warnung ausgeben
    }

    // Oversampling (x1) aktivieren
    // 0x23 (Bits 7,6,5 müssen 0 0 1 sein für oversampling x1 und 1,0 müssen 1 1 sein für normal mode)
    // crtl_meas (register für modus)
    bme280_write_reg(crtl_meas, 0x23);
    printf("BME280 configured\n");

    vTaskDelay(pdMS_TO_TICKS(50));

    while(1) {
        uint32_t raw_temp = bme280_read_raw_temp();
        printf("Raw Temp: %lu\n", raw_temp); //lu ist unsigned long

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
