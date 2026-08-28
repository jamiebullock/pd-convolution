/* convolution_core.h -- convolving a signal with an impulse response. Includes
 * no Pd header, so the tests can have it without one.
 *
 * One thread owns a convolver. Pd runs messages and the perform routine on its
 * scheduler thread, and a libpd host has to serialise its calls the same way
 * libpd itself requires.
 *
 * cv_conv_set_impulse allocates and is not for the audio callback; process
 * allocates nothing. There is no latency: a sample reaches the output in the
 * call that took it in, provided len never exceeds the head block.
 *
 * Part of pd-convolution
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#ifndef CONVOLUTION_CORE_H
#define CONVOLUTION_CORE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CV_OK = 0,
    CV_ERR_ARGS,      /* a bad argument reached us */
    CV_ERR_MEMORY
} cv_error;

typedef struct cv_conv cv_conv;

/* head_block is the latency the caller is prepared to accept, in samples, and
 * is normally Pd's block size; tail_block is the size the rest of the impulse
 * response is convolved in, and trades memory and per-call work against the
 * cost of long responses. */
cv_conv *cv_conv_new(size_t head_block, size_t tail_block);
void cv_conv_free(cv_conv *c);

/* Replaces the impulse response. An empty response leaves the convolver
 * silent rather than failing. Allocates. */
cv_error cv_conv_set_impulse(cv_conv *c, const float *ir, size_t len);

/* Writes len samples of the convolution over out. in and out may be the same
 * buffer. Allocates nothing and takes no lock. */
void cv_conv_process(cv_conv *c, const float *in, float *out, size_t len);

/* Drops whatever is still ringing and starts the impulse response again.
 * Allocates, for the same reason cv_conv_set_impulse does: the convolver
 * underneath has no way to clear its state without rebuilding. */
void cv_conv_reset(cv_conv *c);

/* Length of the impulse response in use, in samples. */
size_t cv_conv_impulse_length(const cv_conv *c);

#ifdef __cplusplus
}
#endif

#endif /* CONVOLUTION_CORE_H */
