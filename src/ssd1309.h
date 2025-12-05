#ifndef SSD1309_H
#define SSD1309_H

#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#define SSD1309_WIDTH 128
#define SSD1309_HEIGHT 64
#define SSD1309_FB_SIZE (SSD1309_WIDTH * SSD1309_HEIGHT / 8)

typedef enum {
    MODE_ADD,
    MODE_SUBTRACT,
    MODE_INVERT
} WriteMode;

typedef struct {
    i2c_inst_t *i2c;
    uint8_t address;
    uint8_t width;
    uint8_t height;
    uint8_t framebuffer[SSD1309_FB_SIZE];
} SSD1309;

void ssd1309_init(SSD1309 *d, i2c_inst_t *i2c, uint8_t addr);
void ssd1309_clear(SSD1309 *d);
void ssd1309_setPixel(SSD1309 *d, int16_t x, int16_t y, WriteMode mode);
void ssd1309_sendBuffer(SSD1309 *d);
void ssd1309_drawChar(SSD1309 *d, int x, int y, char c);
void ssd1309_drawString(SSD1309 *d, int x, int y, const char *str);

#endif
