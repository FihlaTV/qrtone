/*
 * BSD 3-Clause License
 *
 * Copyright (c) Unité Mixte de Recherche en Acoustique Environnementale (univ-gustave-eiffel)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 *  Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 *  Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/**
 * @file qrtone.c
 * @author Nicolas Fortin @NicolasCumu
 * @date 24/03/2020
 * @brief QRTone C99 implementation. The size of the code is reduced at the expense of the speed of execution. ex no crc tables
 */
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#ifdef _WIN32
#include <crtdbg.h>
#endif
#endif

#include <stdlib.h>
#include <string.h>
#include "qrtone.h"
#include "reed_solomon.h"
#include "math.h"

#define QRTONE_2PI 6.283185307179586

// Column and rows of DTMF that make a char
#define FREQUENCY_ROOT 16

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))


#ifdef SIGN
#undef SIGN
#endif
#define SIGN(d) (d >= 0.0 ? 1 : -1)

#ifndef M_PI
#define M_PI 3.1415926535897932385
#endif

#define CRC_BYTE_LENGTH 2

#define HEADER_SIZE 3
#define HEADER_ECC_SYMBOLS 2
#define HEADER_SYMBOLS HEADER_SIZE * 2 + HEADER_ECC_SYMBOLS

#ifdef TRUE
#undef TRUE
#endif

#define TRUE 1


#ifdef FALSE
#undef FALSE
#endif

#define FALSE 0

// Number of symbols and ecc symbols for each level of ecc
// Low / Medium / Quality / High
const int32_t ECC_SYMBOLS[][2] = { {14, 2}, {14, 4}, {12, 6}, {10, 6} };

#define QRTONE_MULT_SEMITONE 1.0472941228206267
#define QRTONE_WORD_TIME 0.06
#define QRTONE_WORD_SILENCE_TIME 0.01
#define QRTONE_GATE_TIME 0.12
#define QRTONE_AUDIBLE_FIRST_FREQUENCY 1720
#define QRTONE_DEFAULT_TRIGGER_SNR 15
#define QRTONE_DEFAULT_ECC_LEVEL QRTONE_ECC_Q
#define QRTONE_PERCENTILE_BACKGROUND 0.5
#define QRTONE_TUKEY_ALPHA 0.5

enum QRTONE_STATE { QRTONE_WAITING_TRIGGER, QRTONE_PARSING_SYMBOLS };

typedef struct _qrtonecomplex
{
    double r;
    double i;
} qrtonecomplex;

struct _qrtonecomplex NEW_CX(double r, double i) {
    qrtonecomplex ret;
    ret.r = r;
    ret.i = i;
    return ret;
}

struct _qrtonecomplex CX_ADD(const qrtonecomplex c1, const qrtonecomplex c2) {
    qrtonecomplex ret;
    ret.r = c1.r + c2.r;
    ret.i = c1.i + c2.i;
    return ret;
}

struct _qrtonecomplex CX_SUB(const qrtonecomplex c1, const qrtonecomplex c2) {
    qrtonecomplex ret;
    ret.r = c1.r - c2.r;
    ret.i = c1.i - c2.i;
    return ret;
}

struct _qrtonecomplex CX_MUL(const qrtonecomplex c1, const qrtonecomplex c2) {
    qrtonecomplex ret;
    ret.r = c1.r * c2.r - c1.i * c2.i;
    ret.i = c1.r * c2.i + c1.i * c2.r;
    return ret;
}

struct _qrtonecomplex CX_EXP(const qrtonecomplex c1) {
    qrtonecomplex ret;
    ret.r = cos(c1.r);
    ret.i = -sin(c1.r);
    return ret;
}


// DTMF 16*16 frequencies
#define QRTONE_NUM_FREQUENCIES 32

typedef struct _qrtone_crc8_t {
    int32_t crc8;
} qrtone_crc8_t;

typedef struct _qrtone_crc16_t {
    int32_t crc16;
} qrtone_crc16_t;

typedef struct _qrtone_goertzel_t {
    double s0;
    double s1;
    double s2;
    double cos_pik_term2;
    double pik_term;
    float last_sample;
    double sample_rate;
    int32_t window_size;
    int32_t processed_samples;
} qrtone_goertzel_t;

typedef struct _qrtone_percentile_t {
    double* q;
    double* dn;
    double* np;
    int32_t* n;
    int32_t count;
    int32_t marker_count;
} qrtone_percentile_t;

typedef struct _qrtone_array_t {
    float* values;
    int32_t values_length;
    int32_t cursor;
    int32_t inserted;
} qrtone_array_t;

typedef struct _qrtone_peak_finder_t {
    int8_t increase; // boolean increase state
    double old_val;
    int64_t old_index;
    int8_t added;
    double last_peak_value;
    int64_t last_peak_index;
    int32_t increase_count;
    int32_t decrease_count;
    int32_t min_increase_count;
    int32_t min_decrease_count;
} qrtone_peak_finder_t;

typedef struct _qrtone_header_t {
    uint8_t length; // payload length
    int8_t crc;
    int8_t ecc_level;
    int32_t payload_symbols_size;
    int32_t payload_byte_size;
    int32_t number_of_blocks;
    int32_t number_of_symbols;
} qrtone_header_t;

typedef struct _qrtone_trigger_analyzer_t {
    int32_t processed_window_alpha;
    int32_t processed_window_beta;
    int32_t window_offset;
    int32_t gate_length;
    qrtone_goertzel_t frequency_analyzers_alpha[2];
    qrtone_goertzel_t frequency_analyzers_beta[2];
    qrtone_percentile_t background_noise_evaluator;
    qrtone_array_t spl_history[2];
    qrtone_peak_finder_t peak_finder;
    int32_t window_analyze;
    int64_t total_processed;
    double frequencies[2];
    double sample_rate;
    double trigger_snr;
    int64_t first_tone_location;
    qrtone_level_callback_t level_callback;
    void* level_callback_data;
} qrtone_trigger_analyzer_t;

typedef struct _qrtone_t {
    int8_t qr_tone_state;
    qrtone_goertzel_t frequency_analyzers[QRTONE_NUM_FREQUENCIES];
    int64_t first_tone_sample_index;
    int32_t word_length;
    int32_t gate_length;
    int32_t word_silence_length;
    double gate1_frequency;
    double gate2_frequency;
    double sample_rate;
    double frequencies[QRTONE_NUM_FREQUENCIES];
    qrtone_trigger_analyzer_t trigger_analyzer;
    int8_t* symbols_to_deliver;
    int32_t symbols_to_deliver_length;
    int8_t* symbols_cache;
    int32_t symbols_cache_length;
    qrtone_header_t* header_cache;
    int64_t pushed_samples;
    int32_t symbol_index;
    int8_t* payload;
    int32_t payload_length;
    int32_t fixed_errors;
    ecc_reed_solomon_encoder_t encoder;
} qrtone_t;

qrtone_crc8_t* qrtone_crc8_new(void) {
    return malloc(sizeof(qrtone_crc8_t));
}

void qrtone_crc8_init(qrtone_crc8_t* self) {
    self->crc8 = 0;
}

void qrtone_crc8_add(qrtone_crc8_t* self, const int8_t data) {
    int32_t crc = 0;
    int32_t accumulator = (self->crc8 ^ data) & 0x0FF;
    int32_t j;
    for (j = 0; j < 8; j++) {
        if (((accumulator ^ crc) & 0x01) == 0x01) {
            crc = ((crc ^ 0x18) >> 1) | 0x80;
        }
        else {
            crc = crc >> 1;
        }
        accumulator = accumulator >> 1;
    }
    self->crc8 = crc;
}

void qrtone_crc8_add_array(qrtone_crc8_t* self, const int8_t* data, const int32_t data_length) {
    int32_t i;
    for (i = 0; i < data_length; i++) {
        qrtone_crc8_add(self, data[i]);
    }
}

uint8_t qrtone_crc8_get(qrtone_crc8_t* self) {
    return self->crc8 & 0xFF;
}

qrtone_crc16_t* qrtone_crc16_new(void) {
    return malloc(sizeof(qrtone_crc16_t));
}

void qrtone_crc16_init(qrtone_crc16_t* self) {
    self->crc16 = 0;
}

void qrtone_crc16_add_array(qrtone_crc16_t* self, const int8_t* data, const int32_t data_length) {
    int32_t i;
    uint16_t crcXor;
    uint16_t c;
    uint8_t j;
    for (i = 0; i < data_length; i++) {
        c = (self->crc16 ^ data[i]) & 0x00FF;
        crcXor = 0;
        for (j = 0; j < 8; j++) {
            if (((crcXor ^ c) & 0x0001) != 0) {
                crcXor = (crcXor >> 1) ^ 0xA001;
            }
            else {
                crcXor = crcXor >> 1;
            }
            c = c >> 1;
        }
        self->crc16 = self->crc16 >> 8 ^ crcXor;
    }
}


int32_t qrtone_crc16_get(qrtone_crc16_t* self) {
    return self->crc16;
}

qrtone_goertzel_t* qrtone_goertzel_new(void) {
    return malloc(sizeof(qrtone_goertzel_t));
}

void qrtone_goertzel_reset(qrtone_goertzel_t* self) {
    self->s0 = 0;
    self->s1 = 0;
    self->s2 = 0;
    self->processed_samples = 0;
    self->last_sample = 0;
}

void qrtone_goertzel_init(qrtone_goertzel_t* self, double sample_rate, double frequency, int32_t window_size) {
        self->sample_rate = sample_rate;
        self->window_size = window_size;
        // Fix frequency using the sampleRate of the signal
        double samplingRateFactor = window_size / sample_rate;
        self->pik_term = QRTONE_2PI * (frequency * samplingRateFactor) / window_size;
        self->cos_pik_term2 = cos(self->pik_term) * 2.0;
        qrtone_goertzel_reset(self);
}


void qrtone_goertzel_process_samples(qrtone_goertzel_t* self, float* samples,int32_t samples_len) {
    if (self->processed_samples + samples_len <= self->window_size) {
        int32_t size;
        if (self->processed_samples + samples_len == self->window_size) {
            size = samples_len - 1;
            self->last_sample = samples[size];
        } else {
            size = samples_len;
        }
        int32_t i;
        for (i = 0; i < size; i++) {
            self->s0 = samples[i] + self->cos_pik_term2 * self->s1 - self->s2;
            self->s2 = self->s1;
            self->s1 = self->s0;
        }
        self->processed_samples += samples_len;
    }
}

double qrtone_goertzel_compute_rms(qrtone_goertzel_t* self) {
    // final computations
    self->s0 = self->last_sample + self->cos_pik_term2 * self->s1 - self->s2;

    qrtonecomplex cc = CX_EXP(NEW_CX(self->pik_term, 0));
    // complex multiplication substituting the last iteration
    // and correcting the phase for (potentially) non - integer valued
    // frequencies at the same time
    qrtonecomplex parta = CX_SUB(NEW_CX(self->s0, 0), CX_MUL(NEW_CX(self->s1, 0), cc));
    qrtonecomplex partb = CX_EXP(NEW_CX(self->pik_term * (self->window_size - 1.), 0));
    qrtonecomplex y = CX_MUL(parta, partb);
    // phase = atan2(y.i, y.r);
    // Compute RMS
    qrtone_goertzel_reset(self);
    return sqrt((y.r * y.r + y.i * y.i) * 2) / self->window_size;
}

/**
 * Simple bubblesort, because bubblesort is efficient for small count, and count is likely to be small
 * https://github.com/absmall/p2
 *
 * @author Aaron Small
 */
void qrtone_percentile_sort(double* q, int32_t q_length) {
    double k;
    int32_t i = 0;
    int32_t j = 0;
    for (j = 1; j < q_length; j++) {
        k = q[j];
        i = j - 1;

        while (i >= 0 && q[i] > k) {
            q[i + 1] = q[i];
            i--;
        }
        q[i + 1] = k;
    }
}


/**
 * @author Aaron Small
 */
void qrtone_percentile_update_markers(qrtone_percentile_t* self) {
    qrtone_percentile_sort(self->dn, self->marker_count);

    int32_t i;
    /* Then entirely reset np markers, since the marker count changed */
    for (i = 0; i < self->marker_count; i++) {
        self->np[i] = ((int64_t)self->marker_count - 1) * self->dn[i] + 1;
    }
}


/**
 * @author Aaron Small
 */
void qrtone_percentile_add_end_markers(qrtone_percentile_t* self) {
    self->marker_count = 2;
    self->q = malloc(sizeof(double) * self->marker_count);
    self->dn = malloc(sizeof(double) * self->marker_count);
    self->np = malloc(sizeof(double) * self->marker_count);
    self->n = malloc(sizeof(double) * self->marker_count);
    memset(self->q, 0, sizeof(double) * self->marker_count);
    memset(self->dn, 0, sizeof(double) * self->marker_count);
    memset(self->np, 0, sizeof(double) * self->marker_count);
    memset(self->n, 0, sizeof(double) * self->marker_count);
    self->dn[0] = 0.0;
    self->dn[1] = 1.0;
    qrtone_percentile_update_markers(self);
}


/**
 * @author Aaron Small
 */
void qrtone_percentile_init(qrtone_percentile_t* self) {
    self->count = 0;
    qrtone_percentile_add_end_markers(self);
}


/**
 * @author Aaron Small
 */
int32_t qrtone_percentile_allocate_markers(qrtone_percentile_t* self, int32_t count) {
    double* new_q = malloc(sizeof(double) * ((int64_t)self->marker_count + (int64_t)count));
    double* new_dn = malloc(sizeof(double) * ((int64_t)self->marker_count + (int64_t)count));
    double* new_np = malloc(sizeof(double) * ((int64_t)self->marker_count + (int64_t)count));
    int32_t* new_n = malloc(sizeof(int32_t) * ((int64_t)self->marker_count + (int64_t)count));

    memset(new_q + self->marker_count, 0, sizeof(double) * count);
    memset(new_dn + self->marker_count, 0, sizeof(double) * count);
    memset(new_np + self->marker_count, 0, sizeof(double) * count);
    memset(new_n + self->marker_count, 0, sizeof(int32_t) * count);

    memcpy(new_q, self->q, sizeof(double) * self->marker_count);
    memcpy(new_dn, self->dn, sizeof(double) * self->marker_count);
    memcpy(new_np, self->np, sizeof(double) * self->marker_count);
    memcpy(new_n, self->n, sizeof(int32_t) * self->marker_count);

    free(self->q);
    free(self->dn);
    free(self->np);
    free(self->n);

    self->q = new_q;
    self->dn = new_dn;
    self->np = new_np;
    self->n = new_n;

    self->marker_count += count;

    return self->marker_count - count;
}

/**
 * @author Aaron Small
 */
void qrtone_percentile_add_quantile(qrtone_percentile_t* self, double quant) {
    int32_t index = qrtone_percentile_allocate_markers(self, 3);

    /* Add in appropriate dn markers */
    self->dn[index] = quant / 2.0;
    self->dn[index + 1] = quant;
    self->dn[index + 2] = (1.0 + quant) / 2.0;

    qrtone_percentile_update_markers(self);
}

qrtone_percentile_t* qrtone_percentile_new(void) {
    return malloc(sizeof(qrtone_percentile_t));
}
/**
 * P^2 algorithm as documented in "The P-Square Algorithm for Dynamic Calculation of Percentiles and Histograms
 * without Storing Observations," Communications of the ACM, October 1985 by R. Jain and I. Chlamtac.
 * Converted from Aaron Small C code under MIT License
 * https://github.com/absmall/p2
 *
 * @author Aaron Small
 */
void qrtone_percentile_init_quantile(qrtone_percentile_t* self, double quant) {
    if (quant >= 0 && quant <= 1) {
        self->count = 0;
        qrtone_percentile_add_end_markers(self);
        qrtone_percentile_add_quantile(self, quant);
    }
}

/**
 * @author Aaron Small
 */
double qrtone_percentile_linear(qrtone_percentile_t* self, int32_t i, int32_t d) {
    return self->q[i] + d * (self->q[i + d] - self->q[i]) /((int64_t)self->n[i + d] - self->n[i]);
}

/**
 * @author Aaron Small
 */
double qrtone_percentile_parabolic(qrtone_percentile_t* self, int32_t i, int32_t d) {
    return self->q[i] + d / (double)((int64_t)self->n[i + 1] - self->n[i - 1]) * (((int64_t)self->n[i] - self->n[i - 1] + d) * (self->q[i + 1] - self->q[i]) /
        ((int64_t)self->n[i + 1] - self->n[i]) + ((int64_t)self->n[i + 1] - self->n[i] - d) * (self->q[i] - self->q[i - 1]) / ((int64_t)self->n[i] - self->n[i - 1]));
}

/**
 * @author Aaron Small
 */
void qrtone_percentile_add(qrtone_percentile_t* self, double data) {
    int32_t i;
    int32_t k = 0;
    double d;
    double newq;

    if (self->count >= self->marker_count) {
        self->count++;

        // B1
        if (data < self->q[0]) {
            self->q[0] = data;
            k = 1;
        }
        else if (data >= self->q[self->marker_count - 1]) {
            self->q[self->marker_count - 1] = data;
            k = self->marker_count - 1;
        }
        else {
            for (i = 1; i < self->marker_count; i++) {
                if (data < self->q[i]) {
                    k = i;
                    break;
                }
            }
        }

        // B2
        for (i = k; i < self->marker_count; i++) {
            self->n[i]++;
            self->np[i] = self->np[i] + self->dn[i];
        }
        for (i = 0; i < k; i++) {
            self->np[i] = self->np[i] + self->dn[i];
        }

        // B3
        for (i = 1; i < self->marker_count - 1; i++) {
            d = self->np[i] - self->n[i];
            if ((d >= 1.0 && self->n[i + 1] - self->n[i] > 1) || (d <= -1.0 && self->n[i - 1] - self->n[i] < -1)) {
                newq = qrtone_percentile_parabolic(self, i, SIGN(d));
                if (self->q[i - 1] < newq && newq < self->q[i + 1]) {
                    self->q[i] = newq;
                }
                else {
                    self->q[i] = qrtone_percentile_linear(self, i, SIGN(d));
                }
                self->n[i] += SIGN(d);
            }
        }
    }
    else {
        self->q[self->count] = data;
        self->count++;

        if (self->count == self->marker_count) {
            // We have enough to start the algorithm, initialize
            qrtone_percentile_sort(self->q, self->marker_count);

            for (i = 0; i < self->marker_count; i++) {
                self->n[i] = i + 1;
            }
        }
    }
}

double qrtone_percentile_result_quantile(qrtone_percentile_t* self, double quantile) {
    int32_t i;
    if (self->count < self->marker_count) {
        int32_t closest = 1;
        qrtone_percentile_sort(self->q, self->count);
        for (i = 2; i < self->count; i++) {
            if (fabs(((double)i) / self->count - quantile) < fabs(((double)closest) / self->marker_count - quantile)) {
                closest = i;
            }
        }
        return self->q[closest];
    } else {
        // Figure out which quantile is the one we're looking for by nearest dn
        int32_t closest = 1;
        for (i = 2; i < self->marker_count - 1; i++) {
            if (fabs(self->dn[i] - quantile) < fabs(self->dn[closest] - quantile)) {
                closest = i;
            }
        }
        return self->q[closest];
    }
}

double qrtone_percentile_result(qrtone_percentile_t* self) {
    return qrtone_percentile_result_quantile(self, self->dn[(self->marker_count - 1) / 2]);
}

void qrtone_percentile_free(qrtone_percentile_t* self) {
    free(self->q);
    free(self->dn);
    free(self->np);
    free(self->n);
}

qrtone_array_t* qrtone_array_new(void) {
    return malloc(sizeof(qrtone_array_t));
}

void qrtone_array_init(qrtone_array_t* self, int32_t length) {
    self->values = malloc(sizeof(float) * length);
    memset(self->values, 0, sizeof(float) * length);
    self->values_length = length;
    self->cursor = 0;
    self->inserted = 0;
}

float qrtone_array_get(qrtone_array_t* self, int32_t index) {
    int32_t circular_index = self->cursor - self->inserted + index;
    if (circular_index < 0) {
        circular_index += self->values_length;
    }
    return self->values[circular_index];
}

void qrtone_array_clear(qrtone_array_t* self) {
    self->cursor = 0;
    self->inserted = 0;
}

int32_t qrtone_array_size(qrtone_array_t* self) {
    return self->inserted;
}

float qrtone_array_last(qrtone_array_t* self) {
    return qrtone_array_get(self, qrtone_array_size(self) - 1);
}

void qrtone_array_free(qrtone_array_t* self) {
    free(self->values);
}

void qrtone_array_add(qrtone_array_t* self, float value) {
    self->values[self->cursor] = value;
    self->cursor += 1;
    if (self->cursor == self->values_length) {
        self->cursor = 0;
    }
    self->inserted = min(self->values_length, self->inserted + 1);
}

qrtone_peak_finder_t* qrtone_peak_finder_new(void) {
    return malloc(sizeof(qrtone_peak_finder_t));
}

void qrtone_peak_finder_init(qrtone_peak_finder_t* self, int32_t min_increase_count, int32_t min_decrease_count) {
    self->increase = TRUE;
    self->old_val = -99999999999999999.0;
    self->old_index = 0;
    self->added = FALSE;
    self->last_peak_value = 0;
    self->last_peak_index = 0;
    self->increase_count = 0;
    self->decrease_count = 0;
    self->min_increase_count = min_increase_count;
    self->min_decrease_count = min_decrease_count;
}

int8_t qrtone_peak_finder_add(qrtone_peak_finder_t* self, int64_t index, float value) {
    int8_t ret = FALSE;
    double diff = value - self->old_val;
    // Detect switch from increase to decrease/stall
    if (diff <= 0 && self->increase) {
        if (self->increase_count >= self->min_increase_count) {
            self->last_peak_index = self->old_index;
            self->last_peak_value = self->old_val;
            self->added = TRUE;
            if (self->min_decrease_count <= 1) {
                ret = TRUE;
            }
        }
    } else if (diff > 0 && !self->increase) {
        // Detect switch from decreasing to increase
        if (self->added && self->min_decrease_count != -1 && self->decrease_count < self->min_decrease_count) {
            self->last_peak_index = 0;
            self->added = FALSE;
        }
    }
    self->increase = diff > 0;
    if (self->increase) {
        self->increase_count++;
        self->decrease_count = 0;
    } else {
        self->decrease_count++;
        if (self->decrease_count >= self->min_decrease_count && self->added) {
            // condition for decrease fulfilled
            self->added = FALSE;
            ret = TRUE;
        }
        self->increase_count = 0;
    }
    self->old_val = value;
    self->old_index = index;
    return ret;
}

int64_t qrtone_peak_finder_get_last_peak_index(qrtone_peak_finder_t* self) {
    return self->last_peak_index;
}

qrtone_header_t* qrtone_header_new(void) {
    return malloc(sizeof(qrtone_header_t));
}

uint8_t qrtone_header_get_length(qrtone_header_t* self) {
    return self->length;
}

int8_t qrtone_header_get_crc(qrtone_header_t* self) {
    return self->crc;
}

int8_t qrtone_header_get_ecc_level(qrtone_header_t* self) {
    return self->ecc_level;
}

int32_t qrtone_header_get_payload_symbols_size(qrtone_header_t* self) {
    return self->payload_symbols_size;
}

int32_t qrtone_header_get_payload_byte_size(qrtone_header_t* self) {
    return self->payload_byte_size;
}
int32_t qrtone_header_get_number_of_blocks(qrtone_header_t* self) {
    return self->number_of_blocks;
}
int32_t qrtone_header_get_number_of_symbols(qrtone_header_t* self) {
    return self->number_of_symbols;
}

void qrtone_header_init(qrtone_header_t* self, uint8_t length, int32_t block_symbols_size, int32_t block_ecc_symbols, int8_t crc, int8_t ecc_level) {
    self->length = length;
    int32_t crc_length = 0;
    if (crc) {
        crc_length = CRC_BYTE_LENGTH;
    }
    self->payload_symbols_size = block_symbols_size - block_ecc_symbols;
    self->payload_byte_size = self->payload_symbols_size / 2;
    self->number_of_blocks = (int32_t)ceilf((((int32_t)length + crc_length) * 2) / (float)self->payload_symbols_size);
    self->number_of_symbols = self->number_of_blocks * block_ecc_symbols + (length + crc_length) * 2;
    self->crc = crc;
    self->ecc_level = ecc_level;
}

void qrtone_header_encode(qrtone_header_t* self, int8_t* data) {
    // Payload length
    data[0] = self->length;
    // ECC level
    data[1] = self->ecc_level & 0x3;
    if (self->crc) {
        // has crc ? third bit from the right
        data[1] |= 0x01 << 3;
    }
    qrtone_crc8_t crc8;
    qrtone_crc8_init(&crc8);
    qrtone_crc8_add(&crc8, data[0]);
    qrtone_crc8_add(&crc8, data[1]);
    data[2] = qrtone_crc8_get(&crc8);
}


int8_t qrtone_header_init_from_data(qrtone_header_t* self, int8_t* data) {
    // Check CRC
    qrtone_crc8_t crc8;
    qrtone_crc8_init(&crc8);
    qrtone_crc8_add(&crc8, data[0]);
    qrtone_crc8_add(&crc8, data[1]);
    int8_t expected_crc = qrtone_crc8_get(&crc8);
    if (expected_crc != data[HEADER_SIZE - 1]) {
        // CRC error
        return FALSE;
    }
    self->ecc_level = data[1] & 0x3;

    qrtone_header_init(self, data[0], ECC_SYMBOLS[self->ecc_level][0], ECC_SYMBOLS[self->ecc_level][1], (int8_t)(data[1] >> 3), self->ecc_level);
    return TRUE;
}

void qrtone_compute_frequencies(double* frequencies) {
    // Precompute pitch frequencies
    int32_t i;
    for (i = 0; i < QRTONE_NUM_FREQUENCIES; i++) {
        frequencies[i] = QRTONE_AUDIBLE_FIRST_FREQUENCY * pow(QRTONE_MULT_SEMITONE, i);
    }
}

void qrtone_trigger_analyzer_init(qrtone_trigger_analyzer_t* self, double sample_rate, int32_t gate_length, double gate_frequencies[2], double trigger_snr) {
    self->processed_window_alpha = 0;
    self->processed_window_beta = 0;
    self->total_processed = 0;
    self->level_callback = NULL;
    self->level_callback_data = NULL;
    self->first_tone_location = -1;
    self->window_analyze = gate_length / 3;
    self->sample_rate = sample_rate;
    self->trigger_snr = trigger_snr;
    self->gate_length = gate_length;
    // 50% overlap
    self->window_offset = self->window_analyze / 2;
    qrtone_percentile_init_quantile(&(self->background_noise_evaluator), QRTONE_PERCENTILE_BACKGROUND);
    int32_t i;
    for (i = 0; i < 2; i++) {
        self->frequencies[i] = gate_frequencies[i];
        qrtone_goertzel_init(&(self->frequency_analyzers_alpha[i]), sample_rate, gate_frequencies[i], self->window_analyze);
        qrtone_goertzel_init(&(self->frequency_analyzers_beta[i]), sample_rate, gate_frequencies[i], self->window_analyze);
        qrtone_array_init(&(self->spl_history[i]), (gate_length * 3) / self->window_offset);
    }
    int32_t slopeWindows = max(1, gate_length / self->window_offset / 2 - 1);
    qrtone_peak_finder_init(&(self->peak_finder), slopeWindows, slopeWindows);
}

void qrtone_trigger_analyzer_free(qrtone_trigger_analyzer_t* self) {
    qrtone_percentile_free(&(self->background_noise_evaluator));
    int32_t i;
    for (i = 0; i < 2; i++) {
        qrtone_array_free(&(self->spl_history[i]));
    }
}

void qrtone_trigger_analyzer_reset(qrtone_trigger_analyzer_t* self) {
    self->first_tone_location = -1;
    qrtone_peak_finder_init(&(self->peak_finder), self->peak_finder.min_increase_count, self->peak_finder.min_decrease_count);
    self->processed_window_alpha = 0;
    self->processed_window_beta = 0;
    self->total_processed = 0;
    int32_t i;
    for (i = 0; i < 2; i++) {
        qrtone_goertzel_reset(&(self->frequency_analyzers_alpha[i]));
        qrtone_goertzel_reset(&(self->frequency_analyzers_beta[i]));
        qrtone_array_clear(&(self->spl_history[i]));
    }
}

/**
 * Apply hann window on provided signal
 * @param signal Signal to update
 * @param signal_length Signal length to update
 * @param windowLength hann window length
 * @param offset If the signal length is inferior than windowLength, give the offset of the hann window
 */
void qrtone_hann_window(float* signal, int32_t signal_length, int32_t window_length, int32_t offset) {
    int32_t i;
    for (i = 0; i < signal_length && offset + i < window_length; i++) {
        signal[i] = (float_t)((double)signal[i] * (0.5 - 0.5 * cos((QRTONE_2PI * ((int64_t)i + offset)) / ((int64_t)window_length - 1))));
    }
}

/**
 * Apply tukey window on specified array
 * @param signal Audio samples
 * @param signal_length Audio samples length
 * @param alpha Tukay alpha (0-1)
 * @param windowLength full length of tukey window
 * @param offset Offset of the provided signal buffer (> 0)
 */
void qrtone_tukey_window(float* signal, double alpha, int32_t signal_length, int32_t window_length, int32_t offset) {
    int32_t index_begin_flat = (int)(floor(alpha * ((int64_t)window_length - 1) / 2.0));
    int32_t index_end_flat = window_length - index_begin_flat;
    double window_value = 0;
    int32_t i;

    // begin hann part
    for (i = offset; i < index_begin_flat + 1 && i - offset < signal_length; i++) {
        window_value = 0.5 * (1 + cos(M_PI * (-1 + 2.0 * i / alpha / ((int64_t)window_length - 1))));
        signal[i - offset] *= (float)window_value;
    }

    // end hann part
    for (i = max(offset, index_end_flat - 1); i < window_length && i - offset < signal_length; i++) {
        window_value = 0.5 * (1 + cos(M_PI * (-2.0 / alpha + 1 + 2.0 * i / alpha / ((int64_t)window_length - 1))));
        signal[i - offset] *= (float)window_value;
    }

}



/**
 * Quadratic interpolation of three adjacent samples
 * @param p0 y value of left point
 * @param p1 y value of center point (maximum height)
 * @param p2 y value of right point
 * @return location [-1; 1] relative to center point, height and half-curvature of a parabolic fit through
 * @link https://www.dsprelated.com/freebooks/sasp/Sinusoidal_Peak_Interpolation.html
 * three points
 */
void qrtone_quadratic_interpolation(double p0, double p1, double p2, double* location, double* height, double* half_curvature) {
    *location = (p2 - p0) / (2.0 * (2.0 * p1 - p2 - p0));
    *height = p1 - 0.25 * (p0 - p2) * *location;
    *half_curvature = 0.5 * (p0 - 2.0 * p1 + p2);
}

/**
 * Evaluate peak location of a gaussian
 * @param p0 y value of left point
 * @param p1 y value of center point (maximum height)
 * @param p2 y value of right point
 * @param p1Location x value of p1
 * @param windowLength x delta between points
 * @return Peak x value
 */
int64_t qrtone_find_peak_location(double p0, double p1, double p2, int64_t p1_location, int32_t window_length) {
    double location, height, half_curvature;
    qrtone_quadratic_interpolation(p0, p1, p2, &location, &height, &half_curvature);
    return p1_location + (int64_t)(location * window_length);
}

void qrtone_trigger_analyzer_process(qrtone_trigger_analyzer_t* self, float* samples, int32_t samples_length, int32_t* window_processed, qrtone_goertzel_t* frequency_analyzers) {
    int32_t processed = 0;
    while (self->first_tone_location == -1 && processed < samples_length) {
        int32_t to_process = min(samples_length - processed, self->window_analyze - *window_processed);
        qrtone_hann_window(samples + processed, to_process, self->window_analyze, *window_processed);
        int32_t id_freq;
        for (id_freq = 0; id_freq < 2; id_freq++) {
            qrtone_goertzel_process_samples(frequency_analyzers + id_freq, samples + processed, to_process);
        }
        processed += to_process;
        *window_processed += to_process;
        if (*window_processed == self->window_analyze) {
            *window_processed = 0;
            double spl_levels[2];
            for (id_freq = 0; id_freq < 2; id_freq++) {
                double spl_level = 20 * log10(qrtone_goertzel_compute_rms(frequency_analyzers + id_freq));
                spl_levels[id_freq] = spl_level;
                qrtone_array_add(self->spl_history + id_freq, (float)spl_level);
            }
            if(self->level_callback != NULL) {
                self->level_callback(self->level_callback_data, 0, (float)spl_levels[0], (float)spl_levels[1]);
            }
            qrtone_percentile_add(&(self->background_noise_evaluator), spl_levels[1]);
            int64_t location = self->total_processed + processed - self->window_analyze;
            if (qrtone_peak_finder_add(&(self->peak_finder), location, (float)spl_levels[1])) {
                // We found a peak
                int64_t element_index = self->peak_finder.last_peak_index;
                double element_value = self->peak_finder.last_peak_value;
                double background_noise_second_peak = qrtone_percentile_result(&(self->background_noise_evaluator));
                // Check if peak value is greater than specified Signal Noise ratio
                if (element_value > background_noise_second_peak + self->trigger_snr) {
                    // Check if the level on other triggering frequencies is below triggering level (at the same time)
                    int32_t peak_index = qrtone_array_size(self->spl_history + 1) - 1 - (int32_t)(location / self->window_offset - element_index / self->window_offset);
                    if (peak_index >= 0 && peak_index < qrtone_array_size(self->spl_history) && qrtone_array_get(self->spl_history, peak_index) < element_value - self->trigger_snr) {
                        int32_t first_peak_index = peak_index - (self->gate_length / self->window_offset);
                        // Check if for the first peak the level was inferior than trigger level
                        if (first_peak_index >= 0 && first_peak_index < qrtone_array_size(self->spl_history) &&
                            qrtone_array_get(self->spl_history, first_peak_index) > element_value - self->trigger_snr &&
                            qrtone_array_get(self->spl_history + 1, first_peak_index) < element_value - self->trigger_snr) {
                            // All trigger conditions are met
                            // Evaluate the exact position of the first tone
                            int64_t peak_location = qrtone_find_peak_location(qrtone_array_get(self->spl_history + 1, peak_index - 1),
                                qrtone_array_get(self->spl_history + 1, peak_index), qrtone_array_get(self->spl_history + 1, peak_index + 1),
                                element_index, self->window_offset);
                            self->first_tone_location = peak_location + self->gate_length / 2 + self->window_offset;
                        }
                    }
                }
            }
        }
    }
}

void qrtone_trigger_analyzer_process_samples(qrtone_trigger_analyzer_t* self, float* samples, int32_t samples_length) {
    float* samples_alpha = malloc(sizeof(float) * samples_length);
    memcpy(samples_alpha, samples, sizeof(float) * samples_length);
    qrtone_trigger_analyzer_process(self, samples_alpha, samples_length, &(self->processed_window_alpha), self->frequency_analyzers_alpha);
    free(samples_alpha);
    if (self->total_processed > self->window_offset) {
        float* samples_beta = malloc(sizeof(float) * samples_length);
        memcpy(samples_beta, samples, sizeof(float) * samples_length);
        qrtone_trigger_analyzer_process(self, samples_beta, samples_length, &(self->processed_window_beta), self->frequency_analyzers_beta);
        free(samples_beta);
    } else if (self->window_offset - self->total_processed < samples_length) {
        // Start to process on the part used by the offset window
        int32_t from = (int32_t)(self->window_offset - self->total_processed);
        int32_t length = samples_length - from;
        float* samples_beta = malloc(sizeof(float) * length);
        memcpy(samples_beta, samples + from, sizeof(float) * length);
        qrtone_trigger_analyzer_process(self, samples_beta, length, &(self->processed_window_beta), self->frequency_analyzers_beta);
        free(samples_beta);
    }
    self->total_processed += samples_length;
}

int32_t qrtone_trigger_maximum_window_length(qrtone_trigger_analyzer_t * self) {
    return min(self->window_analyze - self->processed_window_alpha, self->window_analyze - self->processed_window_beta);
}

void qrtone_interleave_symbols(int8_t* symbols, int32_t symbols_length, int32_t block_size) {
    int8_t* symbols_output = malloc(symbols_length);
    int32_t insertion_cursor = 0;
    int32_t j;
    for (j = 0; j < block_size; j++) {
        int32_t cursor = j;
        while (cursor < symbols_length) {
            symbols_output[insertion_cursor++] = symbols[cursor];
            cursor += block_size;
        }
    }
    memcpy(symbols, symbols_output, symbols_length);
    free(symbols_output);
}

void qrtone_deinterleave_symbols(int8_t* symbols, int32_t symbols_length, int32_t block_size) {
    int8_t* symbols_output = malloc(symbols_length);
    int32_t insertion_cursor = 0;
    int32_t j;
    for (j = 0; j < block_size; j++) {
        int32_t cursor = j;
        while (cursor < symbols_length) {
            symbols_output[cursor] = symbols[insertion_cursor++];
            cursor += block_size;
        }
    }
    memcpy(symbols, symbols_output, symbols_length);
    free(symbols_output);
}

void qrtone_init(qrtone_t* self, double sample_rate) {
    self->symbols_cache = NULL;
    self->symbols_cache_length = 0;
    self->symbols_to_deliver = NULL;
    self->symbols_to_deliver_length = 0;
    self->payload = NULL;
    self->payload_length = 0;
    self->first_tone_sample_index = -1;
    self->pushed_samples = 0;
    self->symbol_index = 0;
    self->fixed_errors = 0;
    self->qr_tone_state = QRTONE_WAITING_TRIGGER;
    self->sample_rate = sample_rate;
    self->word_length = (int32_t)(sample_rate * QRTONE_WORD_TIME);
    self->gate_length = (int32_t)(sample_rate * QRTONE_GATE_TIME);
    self->word_silence_length = (int32_t)(sample_rate * QRTONE_WORD_SILENCE_TIME);
    qrtone_compute_frequencies(self->frequencies);
    self->gate1_frequency = self->frequencies[FREQUENCY_ROOT];
    self->gate2_frequency = self->frequencies[FREQUENCY_ROOT + 2];
    double gates_freq[2];
    gates_freq[0] = self->gate1_frequency;
    gates_freq[1] = self->gate2_frequency;
    qrtone_trigger_analyzer_init(&(self->trigger_analyzer), sample_rate, self->gate_length, gates_freq, QRTONE_DEFAULT_TRIGGER_SNR);
    int32_t idfreq;
    for (idfreq = 0; idfreq < QRTONE_NUM_FREQUENCIES; idfreq++) {        
        qrtone_goertzel_init(&(self->frequency_analyzers[idfreq]), sample_rate, self->frequencies[idfreq], self->word_length);
    }
    ecc_reed_solomon_encoder_init(&(self->encoder), 0x13, 16, 1);
    self->header_cache = NULL;
}

void qrtone_set_level_callback(qrtone_t* self, void* data, qrtone_level_callback_t lvl_callback) {    
    self->trigger_analyzer.level_callback = lvl_callback;
    self->trigger_analyzer.level_callback_data = data;
}

int32_t qrtone_get_maximum_length(qrtone_t* self) {
    if (self->qr_tone_state == QRTONE_WAITING_TRIGGER) {
        return qrtone_trigger_maximum_window_length(&(self->trigger_analyzer));
    } else {
        return self->frequency_analyzers[0].window_size - self->frequency_analyzers[0].processed_samples;
    }
}

void qrtone_arraycopy_to8bits(int32_t* src, int32_t src_pos, int8_t* dest, int32_t dest_pos, int32_t length) {
    int32_t i;
    for (i = 0; i < length; i++) {
        dest[i + dest_pos] = (int8_t)(src[i + src_pos] & 0xFF);
    }
}

void qrtone_arraycopy_to32bits(int8_t* src, int32_t src_pos, int32_t* dest, int32_t dest_pos, int32_t length) {
    int32_t i;
    for (i = 0; i < length; i++) {
        dest[i + dest_pos] = src[i + src_pos];
    }
}

void qrtone_payload_to_symbols(qrtone_t* self, int8_t* payload, uint8_t payload_length, int32_t block_symbols_size, int32_t block_ecc_symbols, int8_t has_crc, int8_t* symbols){
    qrtone_header_t header;
    qrtone_header_init(&header, payload_length, block_symbols_size, block_ecc_symbols, has_crc, 0);
    int8_t* payload_bytes;
    if (has_crc) {
        payload_bytes = malloc((size_t)payload_length + CRC_BYTE_LENGTH);
        memcpy(payload_bytes, payload, payload_length);
        qrtone_crc16_t crc;
        qrtone_crc16_init(&crc);
        qrtone_crc16_add_array(&crc, payload_bytes, payload_length);
        payload_bytes[payload_length] = (int8_t)(crc.crc16 >> 8);
        payload_bytes[payload_length + 1] = (int8_t)(crc.crc16 & 0xFF);
        payload_length += CRC_BYTE_LENGTH;
    } else {
        payload_bytes = payload;
    }
    int32_t block_id, i;
    int32_t* block_symbols = malloc(sizeof(int32_t) * block_symbols_size);
    for (block_id = 0; block_id < header.number_of_blocks; block_id++) {
        memset(block_symbols, 0, sizeof(int32_t) * block_symbols_size);
        int32_t payload_size = min(header.payload_byte_size, payload_length - block_id * header.payload_byte_size);
        for (i = 0; i < payload_size; i++) {
            // offset most significant bits to the right without keeping sign
            block_symbols[i * 2] = (payload_bytes[i + block_id * header.payload_byte_size] >> 4) & 0x0F;
            // keep only least significant bits for the second hexadecimal symbol
            block_symbols[i * 2 + 1] = payload_bytes[i + block_id * header.payload_byte_size] & 0x0F;
        }
        // Add ECC parity symbols
        ecc_reed_solomon_encoder_encode(&(self->encoder), block_symbols, block_symbols_size, block_ecc_symbols);
        // Copy data to main symbols
        qrtone_arraycopy_to8bits(block_symbols, 0, symbols, block_id * block_symbols_size, payload_size * 2);
        // Copy parity to main symbols
        qrtone_arraycopy_to8bits(block_symbols, header.payload_symbols_size, symbols, block_id * block_symbols_size + payload_size * 2, block_ecc_symbols);
    }
    // Permute symbols
    qrtone_interleave_symbols(symbols, header.number_of_symbols, block_symbols_size);
    free(block_symbols);
    if (payload_bytes != payload) {
        free(payload_bytes);
    }
}

int32_t qrtone_set_payload_ext(qrtone_t* self, int8_t* payload, uint8_t payload_length, int8_t ecc_level, int8_t add_crc) {
    if (ecc_level < 0 || ecc_level > QRTONE_ECC_H) {
        return 0;
    }
    qrtone_header_t header;
    qrtone_header_init(&header, payload_length, ECC_SYMBOLS[ecc_level][0], ECC_SYMBOLS[ecc_level][1], add_crc, ecc_level);
    if (self->symbols_to_deliver != NULL) {
        free(self->symbols_to_deliver);
        self->symbols_to_deliver = NULL;
        self->symbols_to_deliver_length = 0;
    }
    self->symbols_to_deliver_length = header.number_of_symbols + HEADER_SYMBOLS;
    self->symbols_to_deliver = malloc(self->symbols_to_deliver_length);
    int8_t header_data[HEADER_SIZE];
    qrtone_header_encode(&header, header_data);
    // Encode header symbols
    qrtone_payload_to_symbols(self, header_data, HEADER_SIZE, HEADER_SYMBOLS, HEADER_ECC_SYMBOLS, 0, self->symbols_to_deliver);
    // Encode payload symbols
    qrtone_payload_to_symbols(self, payload, payload_length, ECC_SYMBOLS[ecc_level][0], ECC_SYMBOLS[ecc_level][1], add_crc, self->symbols_to_deliver + HEADER_SYMBOLS);
    // return number of samples
    return 2 * self->gate_length + (self->symbols_to_deliver_length / 2) * (self->word_silence_length + self->word_length);
}

int32_t qrtone_set_payload(qrtone_t* self, int8_t* payload, uint8_t payload_length) {
    return qrtone_set_payload_ext(self, payload, payload_length, QRTONE_DEFAULT_ECC_LEVEL, 1);
}

void qrtone_generate_pitch(float* samples, int32_t samples_length, int32_t offset, double sample_rate, float frequency, double power_peak) {
    const double t_step = 1 / sample_rate;
    int32_t i;
    for (i = 0; i < samples_length; i++) {
        samples[i] += (float_t)(sin(((int64_t)i + offset) * t_step * QRTONE_2PI * frequency) * power_peak);
    }
}

void qrtone_get_samples(qrtone_t* self, float* samples, int32_t samples_length, int32_t offset, float power) {
    int32_t cursor = 0;
    // First gate tone
    if (cursor + self->gate_length - offset >= 0) {
        qrtone_generate_pitch(samples + max(0, cursor - offset), max(0, min(self->gate_length - max(0, offset - cursor), samples_length - max(0, cursor - offset))), max(0, offset - cursor), (float)self->sample_rate, (float)self->gate1_frequency, power);
        qrtone_hann_window(samples + max(0, cursor - offset), max(0, min(self->gate_length - max(0, offset - cursor), samples_length - max(0, cursor - offset))), self->gate_length, max(0, offset - cursor));
    }
    cursor += self->gate_length;
    if (cursor > offset + samples_length) {
        return;
    }
    // Second gate tone
    if (cursor + self->gate_length - offset >= 0) {
        qrtone_generate_pitch(samples + max(0, cursor - offset), max(0, min(self->gate_length - max(0, offset - cursor), samples_length - max(0, cursor - offset))), max(0, offset - cursor), (float)self->sample_rate, (float)self->gate2_frequency, power);
        qrtone_hann_window(samples + max(0, cursor - offset), max(0, min(self->gate_length - max(0, offset - cursor), samples_length - max(0, cursor - offset))), self->gate_length, max(0, offset - cursor));
    }
    cursor += self->gate_length;
    if (cursor > offset + samples_length) {
        return;
    }
    // Symbols
    int32_t i;
    for (i = 0; i < self->symbols_to_deliver_length; i += 2) {
        cursor += self->word_silence_length;
        if (cursor + self->word_length - offset >= 0) {
            float f1 = (float)self->frequencies[self->symbols_to_deliver[i]];
            float f2 = (float)self->frequencies[self->symbols_to_deliver[i + 1] + FREQUENCY_ROOT];
            qrtone_generate_pitch(samples + max(0, cursor - offset), max(0, min(self->word_length - max(0, offset - cursor), samples_length - max(0, cursor - offset))), max(0, offset - cursor), (float)self->sample_rate, f1, power / 2);
            qrtone_generate_pitch(samples + max(0, cursor - offset), max(0, min(self->word_length - max(0, offset - cursor), samples_length - max(0, cursor - offset))), max(0, offset - cursor), (float)self->sample_rate, f2, power / 2);
            qrtone_tukey_window(samples + max(0, cursor - offset), QRTONE_TUKEY_ALPHA, max(0, min(self->word_length - max(0, offset - cursor), samples_length - max(0, cursor - offset))), self->word_length, max(0, offset - cursor));
        }
        cursor += self->word_length;
        if (cursor > offset + samples_length) {
            return;
        }
    }
}

qrtone_t* qrtone_new(void) {
    qrtone_t* self = malloc(sizeof(qrtone_t));
    return self;
}

void qrtone_free(qrtone_t* self) {
    if (self->payload != NULL) {
        free(self->payload);
    }
    if (self->symbols_to_deliver != NULL) {
        free(self->symbols_to_deliver);
    }
    if (self->symbols_cache != NULL) {
        free(self->symbols_cache);
    }
    if(self->header_cache != NULL) {
        free(self->header_cache);
    }
    ecc_reed_solomon_encoder_free(&(self->encoder));
    qrtone_trigger_analyzer_free(&(self->trigger_analyzer));
}

void qrtone_reset(qrtone_t* self) {
    if (self->symbols_cache != NULL) {
        free(self->symbols_cache);
        self->symbols_cache = NULL;
        self->symbols_cache_length = 0;
    }
    if (self->header_cache != NULL) {
        free(self->header_cache);
        self->header_cache = NULL;
    }
    if (self->symbols_to_deliver != NULL) {
        free(self->symbols_to_deliver);
        self->symbols_to_deliver = NULL;
        self->symbols_to_deliver_length = 0;
    }
    qrtone_trigger_analyzer_reset(&(self->trigger_analyzer));
    int32_t idfreq;
    for (idfreq = 0; idfreq < QRTONE_NUM_FREQUENCIES; idfreq++) {
        qrtone_goertzel_reset(&(self->frequency_analyzers[idfreq]));
    }
    self->qr_tone_state = QRTONE_WAITING_TRIGGER;
    self->symbol_index = 0;
    self->first_tone_sample_index = -1;
}

int8_t* qrtone_symbols_to_payload(qrtone_t* self, int8_t* symbols, int32_t symbols_length, int32_t block_symbols_size, int32_t block_ecc_symbols, int8_t has_crc) {
    int32_t payload_symbols_size = block_symbols_size - block_ecc_symbols;
    int32_t payload_byte_size = payload_symbols_size / 2;
    int32_t payload_length = ((symbols_length / block_symbols_size) * payload_symbols_size + max(0, symbols_length % block_symbols_size - block_ecc_symbols)) / 2;
    int32_t number_of_blocks = (int32_t)ceil(symbols_length / (double)block_symbols_size);

    // Cancel permutation of symbols
    qrtone_deinterleave_symbols(symbols, symbols_length, block_symbols_size);
    int32_t offset = 0;
    if(has_crc) {
        offset = -CRC_BYTE_LENGTH;
    }
    int8_t* payload = malloc((size_t)payload_length + offset);
    int32_t crc_value[CRC_BYTE_LENGTH];
    memset(crc_value, 0, sizeof(int32_t) * CRC_BYTE_LENGTH);
    int32_t crc_index = 0;
    int32_t block_id;
    int32_t* block_symbols = malloc(sizeof(int32_t) * block_symbols_size);
    for(block_id = 0; block_id < number_of_blocks; block_id++) {
        memset(block_symbols, 0, sizeof(int32_t) * block_symbols_size);
        int32_t payload_symbols_length = min(payload_symbols_size, symbols_length - block_ecc_symbols - block_id * block_symbols_size);
        // Copy payload symbols
        qrtone_arraycopy_to32bits(symbols, block_id * block_symbols_size, block_symbols, 0, payload_symbols_length);
        // Copy parity symbols
        qrtone_arraycopy_to32bits(symbols, block_id * block_symbols_size + payload_symbols_length, block_symbols, payload_symbols_size, block_ecc_symbols);
        // Use Reed-Solomon in order to fix correctable errors
        // Fix symbols thanks to ECC parity symbols
        int32_t ret = ecc_reed_solomon_decoder_decode(&(self->encoder.field), block_symbols, block_symbols_size, block_ecc_symbols, &(self->fixed_errors));
        if(ret == ECC_REED_SOLOMON_ERROR) {
            free(payload);
            payload = NULL;
            break;
        }
        // copy result to payload
        int32_t payload_block_byte_size = min(payload_byte_size, payload_length + offset - block_id * payload_byte_size);
        int32_t i;
        for(i=0; i < payload_block_byte_size; i++) {
            payload[i + block_id * payload_byte_size] = (int8_t)((block_symbols[i * 2] << 4) | (block_symbols[i * 2 + 1] & 0x0f));
        }
        if(has_crc) {
            int32_t maxi = min(payload_byte_size, payload_length - block_id * payload_byte_size);
            for(i = max(0, payload_block_byte_size); i < maxi; i++) {
                crc_value[crc_index++] = ((block_symbols[i * 2] << 4) | (block_symbols[i * 2 + 1] & 0x0F));
            }
        }
    }
    free(block_symbols);
    if(payload != NULL && has_crc) {
        int32_t stored_crc = 0;
        stored_crc = stored_crc | crc_value[0] << 8;
        stored_crc = stored_crc | crc_value[1];
        // Check if fixed payload+CRC give a correct result
        qrtone_crc16_t crc16;
        qrtone_crc16_init(&crc16);
        qrtone_crc16_add_array(&crc16, payload, payload_length + offset);
        if(crc16.crc16 != stored_crc) {
            free(payload);
            payload = NULL;
        }
    }
    return payload;
}


void qrtone_feed_trigger_analyzer(qrtone_t* self, float* samples, int32_t samples_length) {
    qrtone_trigger_analyzer_process_samples(&(self->trigger_analyzer), samples, samples_length);
    if(self->trigger_analyzer.first_tone_location != -1) {
        self->qr_tone_state = QRTONE_PARSING_SYMBOLS;
        if(self->payload != NULL) {
            free(self->payload);
        }
        self->payload = NULL;
        self->payload_length = 0;
        self->first_tone_sample_index = self->pushed_samples - (self->trigger_analyzer.total_processed - self->trigger_analyzer.first_tone_location);
        int32_t idfreq;
        for (idfreq = 0; idfreq < QRTONE_NUM_FREQUENCIES; idfreq++) {
            qrtone_goertzel_reset(&(self->frequency_analyzers[idfreq]));
        }
        if(self->symbols_cache != NULL) {
            free(self->symbols_cache);
        }
        self->symbols_cache = malloc(HEADER_SYMBOLS);
        memset(self->symbols_cache, 0, HEADER_SYMBOLS);
        self->symbols_cache_length = HEADER_SYMBOLS;
        qrtone_trigger_analyzer_reset(&(self->trigger_analyzer));
        self->fixed_errors = 0;
    }
}

int64_t qrtone_get_tone_location(qrtone_t* self) {
    return self->first_tone_sample_index + (int64_t)self->symbol_index * ((int64_t)self->word_length + self->word_silence_length) + self->word_silence_length;
}

int32_t qrtone_get_tone_index(qrtone_t* self, int32_t samples_length) {
    return (int32_t)(samples_length - (self->pushed_samples - qrtone_get_tone_location(self)));
}

void qrtone_cached_symbols_to_payload(qrtone_t* self) {
    if(self->payload != NULL) {
        free(self->payload);
    }
    self->payload = qrtone_symbols_to_payload(self, self->symbols_cache, self->symbols_cache_length, ECC_SYMBOLS[self->header_cache->ecc_level][0], ECC_SYMBOLS[self->header_cache->ecc_level][1], self->header_cache->crc);
    self->payload_length = self->header_cache->length;
}

void qrtone_cached_symbols_to_header(qrtone_t* self) {
    int8_t* header_bytes = qrtone_symbols_to_payload(self, self->symbols_cache, self->symbols_cache_length, HEADER_SYMBOLS, HEADER_ECC_SYMBOLS, 0);
    if(header_bytes != NULL) {
        if(self->header_cache != NULL) {
            free(self->header_cache);
        }
        self->header_cache = malloc(sizeof(qrtone_header_t));
        if(!qrtone_header_init_from_data(self->header_cache, header_bytes)) {
            self->header_cache = NULL;
        }
        free(header_bytes);
    }
}


int8_t qrtone_analyze_tones(qrtone_t* self, float* samples, int32_t samples_length) {
    int32_t cursor = max(0, qrtone_get_tone_index(self, samples_length));
    while(cursor < samples_length) {
        int32_t window_length = min(samples_length - cursor, self->word_length - self->frequency_analyzers[0].processed_samples);
        if(window_length == 0) {
            break;
        }
        float* window = malloc(sizeof(float) * window_length);
        memcpy(window, samples + cursor, window_length * sizeof(float));

        qrtone_hann_window(window, window_length, self->word_length, self->frequency_analyzers[0].processed_samples);
        int32_t idfreq;
        for (idfreq = 0; idfreq < QRTONE_NUM_FREQUENCIES; idfreq++) {
            qrtone_goertzel_process_samples((&self->frequency_analyzers[idfreq]), window, window_length);
        }
        free(window);
        if (self->frequency_analyzers[0].processed_samples == self->word_length) {
            double spl[QRTONE_NUM_FREQUENCIES];
            for (idfreq = 0; idfreq < QRTONE_NUM_FREQUENCIES; idfreq++) {
                double rmsValue = qrtone_goertzel_compute_rms(&(self->frequency_analyzers[idfreq]));
                spl[idfreq] = 20 * log10(rmsValue);
            }
            int32_t symbol_offset;
            for (symbol_offset = 0; symbol_offset < 2; symbol_offset++) {
                int32_t max_symbol_id = -1;
                double max_symbol_gain = -99999999999999.9;
                for (idfreq = symbol_offset * FREQUENCY_ROOT; idfreq < (symbol_offset + 1) * FREQUENCY_ROOT; idfreq++) {
                    double gain = spl[idfreq];
                    if (gain > max_symbol_gain) {
                        max_symbol_gain = gain;
                        max_symbol_id = idfreq;
                    }
                }
                self->symbols_cache[self->symbol_index * 2 + symbol_offset] = (int8_t)(max_symbol_id - symbol_offset * FREQUENCY_ROOT);
            }
            self->symbol_index += 1;
            if (self->symbol_index * 2 == self->symbols_cache_length) {
                if (self->header_cache == NULL) {
                    // Decoding of HEADER complete
                    qrtone_cached_symbols_to_header(self);
                    // CRC error
                    if (self->header_cache == NULL) {
                        qrtone_reset(self);
                        break;
                    }
                    free(self->symbols_cache);
                    self->symbols_cache = malloc(self->header_cache->number_of_symbols);
                    self->symbols_cache_length = self->header_cache->number_of_symbols;
                    self->symbol_index = 0;
                    self->first_tone_sample_index += ((int64_t)(HEADER_SYMBOLS) / 2) * ((int64_t)self->word_length + self->word_silence_length);
                } else {
                    // Decoding complete
                    qrtone_cached_symbols_to_payload(self);
                    qrtone_reset(self);
                    return 1;
                }
            }
        }
        cursor += window_length;
    }
    return 0;
}

int8_t qrtone_push_samples(qrtone_t* self,float* samples, int32_t samples_length) {
    self->pushed_samples += samples_length;
    if(self->qr_tone_state == QRTONE_WAITING_TRIGGER) {
        qrtone_feed_trigger_analyzer(self, samples, samples_length);
    }
    if(self->qr_tone_state == QRTONE_PARSING_SYMBOLS && self->first_tone_sample_index + self->word_silence_length < self->pushed_samples) {
        return qrtone_analyze_tones(self, samples, samples_length);
    }
    return 0;
}

int8_t* qrtone_get_payload(qrtone_t* self) {
    return self->payload;
}

int32_t qrtone_get_payload_length(qrtone_t* self) {
    return self->payload_length;
}


int32_t qrtone_get_fixed_errors(qrtone_t* self) {
    return self->fixed_errors;
}




