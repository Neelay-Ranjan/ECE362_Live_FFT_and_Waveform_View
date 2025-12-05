#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "chardisp.h"

// Make sure to set these in main.c
extern const int SPI_DISP_SCK; extern const int SPI_DISP_CSn; extern const int SPI_DISP_TX;

/***************************************************************** */

// "chardisp" stands for character display, which can be an LCD or OLED
void init_chardisp_pins() {
    // Set pins to SPI function
    gpio_set_function(SPI_DISP_TX, GPIO_FUNC_SPI);
    gpio_set_function(SPI_DISP_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SPI_DISP_CSn, GPIO_FUNC_SPI); 

    // SPI init: 10 kHz, 10 bits per frame, CPOL=0, CPHA=0, MSB first
    spi_init(spi1, 10000); 
    spi_set_format(spi1, 10, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    // Chip select 
    gpio_put(SPI_DISP_CSn, 1); // Make sure CS is high (inactive)  
}

void send_spi_cmd(spi_inst_t* spi, uint16_t value) {
    // Wait until SPI is not busy
    while (spi_is_busy(spi));

    // Send command 
    spi_get_hw(spi)->dr = value; 
}

void send_spi_data(spi_inst_t* spi, uint16_t value) {
    send_spi_cmd(spi, value | 0x200);   
}

void cd_init() {
    sleep_ms(5);
    
    // Function Set: 8-bit interface, English/Japanese character set
    // Command: 0x38 for 8-bit, 2-line, 5x8 font
    // Or 0x39 for Western European charset
    send_spi_cmd(spi1, 0x38);
    
    // Display On/Off: Display on, cursor off, blink off
    // Command: 0x0C (Display=1, Cursor=0, Blink=0)
    send_spi_cmd(spi1, 0x0C);
    
    // Clear Display
    // Command: 0x01
    send_spi_cmd(spi1, 0x01);
    sleep_ms(5);  // Clear display is slow
    
    // Entry Mode Set: Increment cursor, no display shift
    // Command: 0x06 (I/D=1, S=0)
    send_spi_cmd(spi1, 0x06);
    
    // Return Home
    // Command: 0x02
    send_spi_cmd(spi1, 0x02);
}

void cd_display1(const char *str) {
    // Set DDRAM address to start of line 1 (0x80)
    send_spi_cmd(spi1, 0x80);
    
    // Send 16 characters to display
    for (int i = 0; i < 16 && str[i] != '\0'; i++) {
        send_spi_data(spi1, str[i]);
    }
}
void cd_display2(const char *str) {
    // Set DDRAM address to start of line 2 (0xC0 = 0x80 + 0x40)
    send_spi_cmd(spi1, 0xC0);
    
    // Send 16 characters to display
    for (int i = 0; i < 16 && str[i] != '\0'; i++) {
        send_spi_data(spi1, str[i]);
    }
}

/***************************************************************** */