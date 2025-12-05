#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "ssd1309.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include <string.h>
#include "hardware/spi.h"
#include <stdio.h>
#include "pico/time.h"
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/irq.h"

// Prototype for FFT function implemented in fft.c (prevents implicit declaration)
void fft_compute_from_capture(const float *capture, int n, float *out);

#define OLED_ADDR 0x3C  // Adjust if your module responds to 0x3D

#define DISP_WIDTH  128
#define DISP_HEIGHT 64
#define AVG_COUNT 128
#define BUTTON_PIN 10

const int SPI_DISP_SCK = 42; // Replace with your SCK pin number for the LCD/OLED display
const int SPI_DISP_CSn = 41; // Replace with your CSn pin number for the LCD/OLED display
const int SPI_DISP_TX = 43; // Replace with your TX pin number for the LCD/OLED display

#define FFT_N 2048
static float capture_buf[FFT_N];
static float fft_mag[FFT_N / 2];

#define THRESHOLD 0.1f  
#define MIN_P_VOLTAGE 1.65f + THRESHOLD
#define MIN_N_VOLTAGE 1.65f - THRESHOLD

float samples[DISP_WIDTH];
int sample_index = 0;

int voltage_to_y(float v) {
    if (v < 0) v = 0;
    if (v > 3.3f) v = 3.3f;

    float normalized = v / 3.3f;          // 0.0 at 0V, 1.0 at 3.3V
    int y = (int)((1.0f - normalized) * (DISP_HEIGHT - 1));
    // normalized=0 -> y=63 (bottom), normalized=1 -> y=0 (top)
    return y;
}

void draw_waveform(SSD1309 *d) {
    ssd1309_clear(d);

    const int baseline = 31;  // bottom row of a 0–63 screen
    int previous_y = baseline;
    for (int x = 0; x < DISP_WIDTH; x++) {
        int idx = (sample_index + x) % DISP_WIDTH;
        float v = samples[idx];
        int y = voltage_to_y(v);   // 0..63

        // Draw from baseline up to y (or down to y)
        int y0 = (y < previous_y) ? y : previous_y;
        int y1 = (y < previous_y) ? previous_y : y;

        for (int yy = y0; yy <= y1; yy++) {
            ssd1309_setPixel(d, x, yy, MODE_ADD);
        }
        previous_y = y;
    }
}

uint32_t adc_fifo_out = 0;
static int capture_time_us = 0;
static float sample_rate_hz  = 0.0f;

void capture_and_show_fft(SSD1309 *d) {
    // 1) Capture FFT_N samples as fast as possible
    uint64_t t_start = time_us_64();            // time start
    for (int i = 0; i < FFT_N; i++) {
        float sum = 0.0f;
        int count = 8;
        for (int i = 0; i < count; i++) {
            sum += ((adc_fifo_out * 3.3f) / 4095.0f) - 1.65f;
        }
        capture_buf[i] = sum / count;
       
    }

    uint64_t t_end = time_us_64();              // time end
    capture_time_us = t_end - t_start;          

    // Compute sampling rate (Hz)
    float capture_time_s = (float)capture_time_us / 1e6f;
    sample_rate_hz = (float)FFT_N / capture_time_s;

    //printf("Capture time: %.3f ms (%.3f s), Fs ≈ %.1f Hz\n", capture_time_s * 1000.0f,capture_time_s,sample_rate_hz);
    char line1[17];

    snprintf(line1, sizeof(line1), "T=%.2fs Fn=%.0fkHz", capture_time_s, (sample_rate_hz/2)/1000.0f);

    // show on character LCD
    cd_display1(line1);

    // 2) Compute FFT magnitudes using your fft.c
    fft_compute_from_capture(capture_buf, FFT_N, fft_mag);

    // Ignore DC for scaling
    float max_mag = 0.0f;
    for (int k = 1; k < FFT_N / 2; k++) {
        if (fft_mag[k] > max_mag) max_mag = fft_mag[k];
    }
    if (max_mag <= 0.0f) max_mag = 1.0f;

    ssd1309_clear(d);

    for (int x = 0; x < DISP_WIDTH; x++) {
        int bin = (x * (FFT_N / 2)) / DISP_WIDTH;
        if (bin == 0) bin = 1;

        float m = fft_mag[bin] / max_mag;
        if (m < 0.0f) m = 0.0f;
        if (m > 1.0f) m = 1.0f;

        // pseudo‑log so weak tones are visible
        m = sqrtf(m);  // or powf(m, 0.3f)

        int h = (int)(m * (DISP_HEIGHT - 1));
        for (int y = 0; y < h; y++) {
            int yy = (DISP_HEIGHT - 1) - y;
            ssd1309_setPixel(d, x, yy, MODE_ADD);
        }
    }

    int dom_bin = 1;
    float dom_mag = fft_mag[1];
    for (int k = 2; k < FFT_N / 2; k++) {
        if (fft_mag[k] > dom_mag) {
            dom_mag = fft_mag[k];
            dom_bin = k;
        }
    }

    // Convert bin index to frequency in Hz
    float dom_freq_hz = (sample_rate_hz * dom_bin) / (float)FFT_N;

    int dom_x = (dom_bin * DISP_WIDTH) / (FFT_N / 2);
    // e.g., draw a small marker at the top row of that column
    ssd1309_setPixel(d, dom_x, 0, MODE_ADD);

    char f_str[17];
    snprintf(f_str, sizeof(f_str), "F0=%.0fHz", dom_freq_hz);
    cd_display2(f_str);

    ssd1309_sendBuffer(d);
    sleep_ms(10000);
    // show on character LCD
    cd_display1("                 ");
    cd_display2("                 ");
}

void init_adc_freerun() {
    adc_hw->cs = ADC_CS_EN_BITS;
    adc_gpio_init(45);
    hw_write_masked(&adc_hw->cs, 5 << ADC_CS_AINSEL_LSB, ADC_CS_AINSEL_BITS);
    hw_set_bits(&adc_hw->cs, ADC_CS_START_MANY_BITS);
}

void init_dma() {
    dma_hw->ch[0].read_addr = (uintptr_t) &adc_hw->fifo;
    dma_hw->ch[0].write_addr = (uintptr_t) &adc_fifo_out;
    dma_hw->ch[0].transfer_count = (1u << DMA_CH0_TRANS_COUNT_MODE_LSB) | 1u;
    dma_hw->ch[0].ctrl_trig = 0u;

    uint32_t temp = 0;
    temp |= (DMA_SIZE_16 << DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB);
    temp |= (DREQ_ADC << DMA_CH0_CTRL_TRIG_TREQ_SEL_LSB);
    temp |= (1u << DMA_CH0_CTRL_TRIG_EN_LSB);
    dma_hw->ch[0].ctrl_trig = temp;
}

void init_adc_dma() {
    //initialize DMA request as well as the FIFO request source, also letting the ADC run in free mode to detect the voltage using the potentiometer
    init_dma();
    adc_fifo_setup(true, true, 1, false, false);
    init_adc_freerun();
}

int main() {
    stdio_init_all();
    sleep_ms(100);

    // Oled display init
    init_chardisp_pins();
    cd_init();

    // I2C + display init
    i2c_init(i2c0, 400000);
    gpio_set_function(4, GPIO_FUNC_I2C);
    gpio_set_function(5, GPIO_FUNC_I2C);
    gpio_pull_up(4);
    gpio_pull_up(5);

    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_down(BUTTON_PIN);   // active-low button to GND

    SSD1309 display;
    ssd1309_init(&display, i2c0, 0x3C);

    init_adc_dma();

    for (int i = 0; i < DISP_WIDTH; i++) {
        samples[i] = 1.65f;   // start at mid‑scale
    }


    // Main loop: sample-average + plot
    uint update_counter = 0;
    bool triggered = false;
    while (1) {
        // Check button (active low)
        if (gpio_get(BUTTON_PIN) == 1 && triggered == false) {
            // Debounce: simple delay and re-check
            sleep_ms(50);
            if (gpio_get(BUTTON_PIN) == 1) {
                // While button held, capture & show FFT once
                capture_and_show_fft(&display);
                triggered = true;
            }
        }

        // ---- Existing waveform code below ----
        float sum = 0.0f;
        int count = 0;
        for (int i = 0; i < AVG_COUNT; i++) {
            float v = ((adc_fifo_out * 3.3f) / 4095.0f);
            if (v >= MIN_P_VOLTAGE || v <= MIN_N_VOLTAGE) {
                sum += v;
            } else {
                sum += 1.65f;
            }
            count++;
        }

        float v_avg = sum / count;
        samples[sample_index] = v_avg;
        sample_index = (sample_index + 1) % DISP_WIDTH;

        uint update_freq = 128;
        if (++update_counter >= update_freq) {
            update_counter = 0;
            draw_waveform(&display);

            float rms = 0.0f;
            for(int i=0; i<DISP_WIDTH; i++){
                rms+=((samples[i]-1.65f)*2.0f)*((samples[i]-1.65f)*2.0f);
            }
            rms=sqrtf(rms/DISP_WIDTH);

            char rms_str[14];
            snprintf(rms_str, sizeof(rms_str), "RMS=%.2f V", rms);
            ssd1309_drawString(&display, 0, 0, rms_str);

            ssd1309_sendBuffer(&display);
        }
    }
}