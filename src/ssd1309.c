#include "ssd1309.h"
#include <string.h>

#define CMD 0x00
#define DATA 0x40

#define SSD1309_COL_OFFSET 0x00

static const uint8_t font5x8[][5] = {
    // 0: ' '
    {0x00,0x00,0x00,0x00,0x00},

    // 1: 'H'
    {0xFE,0x10,0x10,0x10,0xFE},

    // 2: 'e'
    {0x7C,0x92,0x92,0x92,0x64},

    // 3: 'l'
    {0x00,0x82,0xFE,0x02,0x00},

    // 4: 'o'
    {0x7C,0x82,0x82,0x82,0x7C},

    // 5: 'W'
    {0xFE,0x40,0x20,0x40,0xFE},

    // 6: 'r'
    {0xFE,0x10,0x10,0x10,0x20},

    // 7: 'd'
    {0x70,0x88,0x88,0x88,0xFE},

    // 8: '0'
    {0x7C,0xA2,0x92,0x8A,0x7C},

    // 9: '1'
    {0x88,0x84,0xFE,0x80,0x80},

    // 10: '2'
    {0x84,0xC2,0xA2,0x92,0x8C},

    // 11: '3'
    {0x44,0x82,0x92,0x92,0x6C},

    // 12: '4'
    {0x30,0x28,0x24,0xFE,0x20},

    // 13: '5'
    {0x4E,0x8A,0x8A,0x8A,0x72},

    // 14: '6'
    {0x7C,0x92,0x92,0x92,0x64},

    // 15: '7'
    {0x02,0xE2,0x12,0x0A,0x06},

    // 16: '8'
    {0x6C,0x92,0x92,0x92,0x6C},

    // 17: '9'
    {0x4C,0x92,0x92,0x92,0x7C},

    // 18: '.'
    {0x00,0x00,0x80,0x00,0x00},

    // 19: 'R'
    {0xFE,0x12,0x12,0x12,0xEC},

    // 20: 'M'
    {0xFE,0x04,0x08,0x04,0xFE},

    // 21: 'S'
    {0x4C,0x92,0x92,0x92,0x64},

    // 22: '='
    {0x28,0x28,0x28,0x28,0x28},

    // 23: 'V'
    {0x1E,0x60,0x80,0x60,0x1E},

    // 24: '%'
    {0xC6,0xC8,0x30,0x16,0xC6},
};

static int charIndex(char c) {
    switch(c) {
        case ' ': return 0;
        case 'H': return 1;
        case 'e': return 2;
        case 'l': return 3;
        case 'o': return 4;
        case 'W': return 5;
        case 'r': return 6;
        case 'd': return 7;

        // digits
        case '0': return 8;
        case '1': return 9;
        case '2': return 10;
        case '3': return 11;
        case '4': return 12;
        case '5': return 13;
        case '6': return 14;
        case '7': return 15;
        case '8': return 16;
        case '9': return 17;

        case '.': return 18;

        // extra letters / symbols (for rms readout)
        case 'R': return 19;
        case 'M': return 20;
        case 'S': return 21;
        case '=': return 22;
        case 'V': return 23;
        case '%': return 24;

        default: return 0; // space
    }
}

// Send one command to SSD1309
static inline void send_cmd(SSD1309 *d, uint8_t cmd) {
    uint8_t buf[2] = { CMD, cmd };
    i2c_write_blocking(d->i2c, d->address, buf, 2, false);
}

// Initialize SSD1309 display
void ssd1309_init(SSD1309 *d, i2c_inst_t *i2c, uint8_t addr) {
    d->i2c = i2c;
    d->address = addr;
    d->width = SSD1309_WIDTH;
    d->height = SSD1309_HEIGHT;

    send_cmd(d, 0xAE);          // Display OFF
    send_cmd(d, 0xD5); send_cmd(d, 0xA0); // Clock divide
    send_cmd(d, 0xA8); send_cmd(d, 0x3F); // Mux ratio
    send_cmd(d, 0xD3); send_cmd(d, 0x00); // Display offset
    send_cmd(d, 0x40);          // Start line 0

    send_cmd(d, 0xA0); // flip segment order
    send_cmd(d, 0xC0); // keep vertical order 

    send_cmd(d, 0xDA); send_cmd(d, 0x12); // COM pins

    send_cmd(d, 0x81); send_cmd(d, 0x7F); // Contrast
    send_cmd(d, 0xD9); send_cmd(d, 0x22); // Precharge
    send_cmd(d, 0xDB); send_cmd(d, 0x35); // VCOM
    send_cmd(d, 0xA4);          // Display follows RAM
    send_cmd(d, 0xA6);          // Normal display
    send_cmd(d, 0x8D); send_cmd(d, 0x14); // Charge pump ON
    send_cmd(d, 0xAF);          // Display ON

    ssd1309_clear(d);
    ssd1309_sendBuffer(d);
}

// Clear framebuffer
void ssd1309_clear(SSD1309 *d) {
    memset(d->framebuffer, 0, SSD1309_FB_SIZE);
}

// Set pixel in framebuffer
void ssd1309_setPixel(SSD1309 *d, int16_t x, int16_t y, WriteMode mode) {
    if(x < 0 || x >= d->width || y < 0 || y >= d->height) return;
    uint16_t index = x + (y / 8) * d->width;
    uint8_t mask = 1 << (y & 7);

    switch(mode) {
        case MODE_ADD: d->framebuffer[index] |= mask; break;
        case MODE_SUBTRACT: d->framebuffer[index] &= ~mask; break;
        case MODE_INVERT: d->framebuffer[index] ^= mask; break;
    }
}

// Send framebuffer to display
void ssd1309_sendBuffer(SSD1309 *d) {
    for(uint8_t page = 0; page < (d->height / 8); page++) {
        uint8_t cmds[] = {
            CMD,
            (uint8_t)(0xB0 | page),
            (uint8_t)(SSD1309_COL_OFFSET & 0x0F),
            (uint8_t)(0x10 | (SSD1309_COL_OFFSET >> 4))
        };
        i2c_write_blocking(d->i2c, d->address, cmds, sizeof(cmds), false);

        uint8_t buf[129];
        buf[0] = DATA;
        memcpy(&buf[1], &d->framebuffer[page * d->width], d->width);
        i2c_write_blocking(d->i2c, d->address, buf, 129, false);
    }
}

// Draw single character
void ssd1309_drawChar(SSD1309 *d, int x, int y, char c) {
    const uint8_t *bitmap = font5x8[charIndex(c)];
    for(int col = 0; col < 5; col++)
        for(int row = 0; row < 8; row++)
            if(bitmap[col] & (1 << row))
                ssd1309_setPixel(d, x + col, y + row, MODE_ADD);
}

// Draw string of characters
void ssd1309_drawString(SSD1309 *d, int x, int y, const char *str) {
    while(*str) {
        ssd1309_drawChar(d, x, y, *str++);
        x += 6; // 5 pixels + 1 pixel gap
        if(x + 5 >= d->width) {
            x = 0;
            y += 8;
        }
    }
}
