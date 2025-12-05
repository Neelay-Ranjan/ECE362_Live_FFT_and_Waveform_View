#include "fft.h"
#include <stdint.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define FFT_N 2048

void fft_radix2(cpx *buf, int n, int dir) {
    int j = 0;
    for (int i = 0; i < n; i++) {
        if (i < j) {
            cpx tmp = buf[i];
            buf[i] = buf[j];
            buf[j] = tmp;
        }
        int m = n >> 1;
        while (m >= 1 && j >= m) {
            j -= m;
            m >>= 1;
        }
        j += m;
    }

    for (int len = 2; len <= n; len <<= 1) {
        float ang = (dir * -2.0f * (float)M_PI) / (float)len;
        float wlen_r = cosf(ang);
        float wlen_i = sinf(ang);

        for (int i = 0; i < n; i += len) {
            float wr = 1.0f;
            float wi = 0.0f;
            for (int k = 0; k < len / 2; k++) {
                int u = i + k;
                int v = i + k + len / 2;

                float xr = buf[v].r * wr - buf[v].i * wi;
                float xi = buf[v].r * wi + buf[v].i * wr;

                float ur = buf[u].r;
                float ui = buf[u].i;

                buf[v].r = ur - xr;
                buf[v].i = ui - xi;
                buf[u].r = ur + xr;
                buf[u].i = ui + xi;

                float tmp_r = wr * wlen_r - wi * wlen_i;
                wi = wr * wlen_i + wi * wlen_r;
                wr = tmp_r;
            }
        }
    }
}

void fft_compute_from_capture(const float *capture_buf, int N, float *fft_mag) {
    static cpx buf_static[FFT_N];
    cpx *buf = buf_static;

    // Convert to volts and compute mean
    float mean = 0.0f;
    for (int i = 0; i < N; i++) {
        float v = capture_buf[i];
        buf[i].r = v;
        buf[i].i = 0.0f;
        mean += v;
    }
    mean /= (float)N;

    // Remove DC and apply Hann window
    for (int i = 0; i < N; i++) {
        float v = buf[i].r - mean;
        float w = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * i / (N - 1)));
        buf[i].r = v * w;
        buf[i].i = 0.0f;
    }

    fft_radix2(buf, N, +1);

    for (int k = 0; k < N / 2; k++) {
        float re = buf[k].r;
        float im = buf[k].i;
        fft_mag[k] = sqrtf(re * re + im * im);
    }
}
