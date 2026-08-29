/* convolution_tilde.c -- the [convolution~] class: inlets, outlet, DSP.
 *
 * Part of pd-convolution
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include "convolution_core.h"

#include "m_pd.h"

#include <string.h>

#define CV_DEFAULT_TAIL_BLOCK 4096

#if defined(_WIN32)
#define CV_SETUP_ENTRY __declspec(dllexport)
#else
#define CV_SETUP_ENTRY
#endif

static t_class *convolution_tilde_class;

typedef struct _convolution_tilde {
    t_object obj;
    t_float x_f;                /* the value a float on the signal inlet sets */
    cv_conv *core;
    t_symbol *array;
    int tail_block;
    int block;                  /* the block size the core was built for */
} t_convolution_tilde;

/* Reads the named array into the convolver. Allocates, so it belongs on the
 * message thread, which is where Pd delivers set and where dsp runs. */
static void convolution_tilde_load(t_convolution_tilde *x)
{
    if (!x->core || !x->array || x->array == &s_) return;

    t_garray *array = (t_garray *)pd_findbyclass(x->array, garray_class);
    if (!array) {
        pd_error(x, "convolution~: %s: no such array", x->array->s_name);
        cv_conv_set_impulse(x->core, NULL, 0);
        return;
    }

    int npoints = 0;
    t_word *vec = NULL;
    if (!garray_getfloatwords(array, &npoints, &vec)) {
        pd_error(x, "convolution~: %s: bad template", x->array->s_name);
        cv_conv_set_impulse(x->core, NULL, 0);
        return;
    }

    float *ir = (float *)getbytes((size_t)npoints * sizeof(float));
    if (!ir) {
        pd_error(x, "convolution~: out of memory");
        return;
    }
    for (int i = 0; i < npoints; i++) ir[i] = (float)vec[i].w_float;

    const cv_error err = cv_conv_set_impulse(x->core, ir, (size_t)npoints);
    freebytes(ir, (size_t)npoints * sizeof(float));

    if (err != CV_OK) {
        pd_error(x, "convolution~: %s: could not be loaded", x->array->s_name);
        return;
    }
    post("convolution~: %s, %d samples", x->array->s_name, npoints);
}

static void convolution_tilde_set(t_convolution_tilde *x, t_symbol *s)
{
    if (s && s != &s_) x->array = s;
    convolution_tilde_load(x);
}

static void convolution_tilde_clear(t_convolution_tilde *x)
{
    cv_conv_reset(x->core);
}

static t_int *convolution_tilde_perform(t_int *w)
{
    t_convolution_tilde *x = (t_convolution_tilde *)(w[1]);
    const t_sample *in = (const t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    const int n = (int)(w[4]);

    cv_conv_process(x->core, in, out, (size_t)n);
    return (w + 5);
}

/* The head block is the DSP block size, which is what makes the convolution
 * latency free. A block size change means a new convolver, so the impulse
 * response is read again. */
static void convolution_tilde_dsp(t_convolution_tilde *x, t_signal **sp)
{
    const int n = sp[0]->s_n;

    if (n != x->block || !x->core) {
        int tail = x->tail_block;
        if (tail < n) tail = n;

        cv_conv *fresh = cv_conv_new((size_t)n, (size_t)tail);
        if (!fresh) {
            pd_error(x, "convolution~: could not build for a block of %d", n);
            return;
        }
        cv_conv_free(x->core);
        x->core = fresh;
        x->block = n;
        convolution_tilde_load(x);
    }

    dsp_add(convolution_tilde_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, (t_int)n);
}

static void *convolution_tilde_new(t_symbol *s, int argc, t_atom *argv)
{
    (void)s;
    t_convolution_tilde *x =
        (t_convolution_tilde *)pd_new(convolution_tilde_class);

    x->core = NULL;
    x->array = &s_;
    x->tail_block = CV_DEFAULT_TAIL_BLOCK;
    x->block = 0;
    x->x_f = 0;

    if (argc > 0 && argv[0].a_type == A_SYMBOL)
        x->array = atom_getsymbol(&argv[0]);
    if (argc > 1) {
        const int tail = (int)atom_getfloat(&argv[1]);
        if (tail > 0 && (tail & (tail - 1)) == 0) x->tail_block = tail;
        else pd_error(x, "convolution~: tail block must be a power of two");
    }

    outlet_new(&x->obj, &s_signal);
    return x;
}

static void convolution_tilde_free(t_convolution_tilde *x)
{
    cv_conv_free(x->core);
}

CV_SETUP_ENTRY void convolution_tilde_setup(void)
{
    convolution_tilde_class = class_new(gensym("convolution~"),
        (t_newmethod)convolution_tilde_new, (t_method)convolution_tilde_free,
        sizeof(t_convolution_tilde), 0, A_GIMME, 0);

    CLASS_MAINSIGNALIN(convolution_tilde_class, t_convolution_tilde, x_f);
    class_addmethod(convolution_tilde_class, (t_method)convolution_tilde_dsp,
        gensym("dsp"), A_CANT, 0);
    class_addmethod(convolution_tilde_class, (t_method)convolution_tilde_set,
        gensym("set"), A_DEFSYMBOL, 0);
    class_addmethod(convolution_tilde_class, (t_method)convolution_tilde_clear,
        gensym("clear"), 0);
}
