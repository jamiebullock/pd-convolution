/* test_core.c -- the core under greatest, with no Pd.
 *
 * Part of pd-convolution
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include "convolution_core.h"

#include "greatest.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK 64

/* A decaying pseudo-random impulse response, so every partition carries
 * energy and a partitioning mistake shows up as a wrong tail rather than a
 * wrong first block. */
static void make_ir(float *ir, int n)
{
    unsigned s = 12345u;
    for (int i = 0; i < n; i++) {
        s = s * 1664525u + 1013904223u;
        ir[i] = (((float)((s >> 9) & 0xFFFF) / 32768.0f) - 1.0f)
              * (float)exp(-3.0 * i / (n ? n : 1));
    }
    if (n) ir[0] = 1.0f;
}

/* Drives an impulse through the convolver, so the output is the impulse
 * response itself. */
static void impulse_response(cv_conv *c, float *out, int n)
{
    float in[BLOCK];
    memset(in, 0, sizeof in);
    in[0] = 1.0f;
    for (int i = 0; i + BLOCK <= n; i += BLOCK) {
        cv_conv_process(c, in, out + i, BLOCK);
        memset(in, 0, sizeof in);
    }
}

static double worst_error(const float *got, const float *want, int n)
{
    double peak = 0.0, err = 0.0;
    for (int i = 0; i < n; i++) {
        const double a = fabs(want[i]);
        if (a > peak) peak = a;
        const double d = fabs(got[i] - want[i]);
        if (d > err) err = d;
    }
    return peak > 0.0 ? err / peak : err;
}

TEST an_impulse_returns_the_impulse_response(void)
{
    const int len = 8000;
    const int probe = len + BLOCK * 4;
    float *ir = malloc(sizeof(float) * len);
    float *out = calloc(probe, sizeof(float));
    make_ir(ir, len);

    cv_conv *c = cv_conv_new(BLOCK, 4096);
    ASSERT(c != NULL);
    ASSERT_EQ(CV_OK, cv_conv_set_impulse(c, ir, len));
    impulse_response(c, out, probe);

    /* The first sample out is the first sample of the response: no latency. */
    ASSERT_IN_RANGE(ir[0], out[0], 1e-5);
    ASSERT(worst_error(out, ir, len) < 1e-5);

    cv_conv_free(c);
    free(ir); free(out);
    PASS();
}

/* Lengths that are not a multiple of the block or the tail block, and one
 * shorter than a single block. */
TEST every_impulse_length_convolves(void)
{
    const int lengths[] = { 1, 2, 63, 64, 65, 1000, 4095, 4096, 4097, 10000 };
    for (size_t k = 0; k < sizeof lengths / sizeof lengths[0]; k++) {
        const int len = lengths[k];
        const int probe = len + BLOCK * 4;
        float *ir = malloc(sizeof(float) * len);
        float *out = calloc(probe, sizeof(float));
        make_ir(ir, len);

        cv_conv *c = cv_conv_new(BLOCK, 4096);
        ASSERT(c != NULL);
        ASSERT_EQ(CV_OK, cv_conv_set_impulse(c, ir, len));
        impulse_response(c, out, probe);
        ASSERTm("a length that is not a whole number of blocks convolves wrongly",
                worst_error(out, ir, len) < 1e-5);

        cv_conv_free(c);
        free(ir); free(out);
    }
    PASS();
}

TEST an_empty_impulse_response_is_silence(void)
{
    cv_conv *c = cv_conv_new(BLOCK, 4096);
    ASSERT(c != NULL);
    ASSERT_EQ(CV_OK, cv_conv_set_impulse(c, NULL, 0));
    ASSERT_EQ(0u, cv_conv_impulse_length(c));

    float in[BLOCK], out[BLOCK];
    for (int i = 0; i < BLOCK; i++) in[i] = 1.0f;
    cv_conv_process(c, in, out, BLOCK);
    for (int i = 0; i < BLOCK; i++) ASSERT_EQ(0.0f, out[i]);

    cv_conv_free(c);
    PASS();
}

TEST a_replaced_impulse_response_takes_effect(void)
{
    const int len = 5000;
    const int probe = len + BLOCK * 4;
    float *first = malloc(sizeof(float) * len);
    float *second = malloc(sizeof(float) * len);
    float *out = calloc(probe, sizeof(float));
    make_ir(first, len);
    make_ir(second, len);
    for (int i = 0; i < len; i++) second[i] = -second[i];

    cv_conv *c = cv_conv_new(BLOCK, 4096);
    ASSERT_EQ(CV_OK, cv_conv_set_impulse(c, first, len));
    impulse_response(c, out, probe);
    ASSERT(worst_error(out, first, len) < 1e-5);

    ASSERT_EQ(CV_OK, cv_conv_set_impulse(c, second, len));
    cv_conv_reset(c);
    memset(out, 0, sizeof(float) * probe);
    impulse_response(c, out, probe);
    ASSERTm("the convolver kept convolving with the response it was given first",
            worst_error(out, second, len) < 1e-5);

    cv_conv_free(c);
    free(first); free(second); free(out);
    PASS();
}

TEST bad_arguments_are_refused(void)
{
    ASSERT_EQ(NULL, cv_conv_new(0, 4096));
    ASSERTm("a tail block smaller than the head block is not a two stage split",
            cv_conv_new(4096, 64) == NULL);

    cv_conv *c = cv_conv_new(BLOCK, 4096);
    ASSERT_EQ(CV_ERR_ARGS, cv_conv_set_impulse(c, NULL, 10));
    ASSERT_EQ(CV_ERR_ARGS, cv_conv_set_impulse(NULL, NULL, 0));
    cv_conv_free(c);
    PASS();
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv)
{
    GREATEST_MAIN_BEGIN();
    RUN_TEST(an_impulse_returns_the_impulse_response);
    RUN_TEST(every_impulse_length_convolves);
    RUN_TEST(an_empty_impulse_response_is_silence);
    RUN_TEST(a_replaced_impulse_response_takes_effect);
    RUN_TEST(bad_arguments_are_refused);
    GREATEST_MAIN_END();
}
