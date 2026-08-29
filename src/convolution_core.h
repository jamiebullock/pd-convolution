/* convolution_core.h -- convolving a signal with an impulse response.
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
    CV_ERR_ARGS,
    CV_ERR_MEMORY
} cv_error;

typedef struct cv_conv cv_conv;

/* head_block_size is the latency the caller is prepared to accept, in
 * samples, and is normally Pd's block size; tail_block_size is the size the
 * rest of the impulse response is convolved in, and trades memory and
 * per-call work against the cost of long responses. */
cv_conv *cv_conv_new(size_t head_block_size, size_t tail_block_size);
void cv_conv_free(cv_conv *c);

/* Replaces the impulse response. An empty response leaves the convolver
 * silent rather than failing. Allocates. */
cv_error cv_conv_set_impulse(cv_conv *c, const float *ir, size_t len);

/* Writes len samples of the convolution to out. in and out may point to the same
 * buffer. Lock and alloc free. */
void cv_conv_process(cv_conv *c, const float *in, float *out, size_t len);

/* Drops whatever is still ringing and starts the impulse response again.
 * Allocates because the convolver implementation has no way to clear its state
 * without rebuilding. */
void cv_conv_reset(cv_conv *c);

/* Gets the length of the impulse response in use, in samples. */
size_t cv_conv_impulse_length(const cv_conv *c);

#ifdef __cplusplus
}
#endif

#endif /* CONVOLUTION_CORE_H */
