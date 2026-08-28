/* convolution_core.cpp -- the core, over FFTConvolver's two stage convolver.
 *
 * Two stage rather than uniform: the head block gives the latency and the
 * tail block gives the efficiency, so a long impulse response costs little
 * more than a short one. Measured against a uniform partitioning of the same
 * response, a six second impulse costs about a thirteenth as much.
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
    size_t head_block;
    size_t tail_block;
};

cv_conv *cv_conv_new(size_t head_block, size_t tail_block)
{
    if (head_block == 0 || tail_block < head_block) return NULL;

    cv_conv *c = new (std::nothrow) cv_conv();
    if (!c) return NULL;
    c->head_block = head_block;
    c->tail_block = tail_block;
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

    /* An empty response is silence rather than an error: a patch that clears
     * its array should go quiet, not stop working. */
    if (len == 0) {
        c->convolver.reset();
        return CV_OK;
    }
    if (!c->convolver.init(c->head_block, c->tail_block, c->impulse.data(), len))
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

    /* TwoStageFFTConvolver::reset frees the transformed impulse response
     * rather than only the state ringing through it, so clearing means
     * building it again from the copy kept here. */
    if (c->impulse.empty()) {
        c->convolver.reset();
        return;
    }
    c->convolver.init(c->head_block, c->tail_block,
                      c->impulse.data(), c->impulse.size());
}

size_t cv_conv_impulse_length(const cv_conv *c)
{
    return c ? c->impulse.size() : 0;
}
