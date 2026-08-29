/* convolution_core.cpp -- the core, wrapping FFTConvolver's two stage convolver.
 *
 * Part of pd-convolution
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include "convolution_core.h"

#include "TwoStageFFTConvolver.h"

#include <new>
#include <vector>

struct cv_conv {
    fftconvolver::TwoStageFFTConvolver convolver;
    std::vector<float> impulse;
    size_t head_block_size;
    size_t tail_block_size;
};

cv_conv *cv_conv_new(size_t head_block_size, size_t tail_block_size)
{
    if (head_block_size == 0 || tail_block_size < head_block_size) return NULL;

    cv_conv *c = new (std::nothrow) cv_conv();
    if (!c) return NULL;
    c->head_block_size = head_block_size;
    c->tail_block_size = tail_block_size;
    return c;
}

void cv_conv_free(cv_conv *c)
{
    delete c;
}

cv_error cv_conv_set_impulse(cv_conv *c, const float *ir, size_t len)
{
    if (!c) return CV_ERR_ARGS;
    if (len && !ir) return CV_ERR_ARGS;

    try {
        c->impulse.assign(ir, ir + len);
    } catch (const std::bad_alloc &) {
        return CV_ERR_MEMORY;
    }

    /* An empty response produces silence */
    if (len == 0) {
        c->convolver.reset();
        return CV_OK;
    }
    if (!c->convolver.init(c->head_block_size, c->tail_block_size,
                           c->impulse.data(), len))
        return CV_ERR_MEMORY;
    return CV_OK;
}

void cv_conv_process(cv_conv *c, const float *in, float *out, size_t len)
{
    if (!c || !in || !out) return;
    if (c->impulse.empty()) {
        for (size_t i = 0; i < len; i++) out[i] = 0.0f;
        return;
    }
    c->convolver.process(in, out, len);
}

void cv_conv_reset(cv_conv *c)
{
    if (!c) return;

    if (c->impulse.empty()) {
        c->convolver.reset();
        return;
    }
    c->convolver.init(c->head_block_size, c->tail_block_size,
                      c->impulse.data(), c->impulse.size());
}

size_t cv_conv_impulse_length(const cv_conv *c)
{
    return c ? c->impulse.size() : 0;
}
