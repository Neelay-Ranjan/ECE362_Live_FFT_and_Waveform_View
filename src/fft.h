#ifndef FFT_H
#define FFT_H

#include <stdint.h>

typedef struct {
    float r;
    float i;
} cpx;

void fft_radix2(cpx *buf, int n, int dir);
void fft_compute_from_capture(const float *capture_buf, int N, float *fft_mag);

#endif