/* convolution_setup.c -- the library entry point, called by Pd on
 * -lib convolution.
 *
 * Part of pd-convolution
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include "m_pd.h"

#ifndef CONVOLUTION_VERSION
#error "define CONVOLUTION_VERSION: CMake passes the project version, other builds must too"
#endif

#if defined(_WIN32)
#define CV_LIBRARY_ENTRY __declspec(dllexport)
#else
#define CV_LIBRARY_ENTRY
#endif

void convolution_tilde_setup(void);

CV_LIBRARY_ENTRY void convolution_setup(void)
{
    convolution_tilde_setup();

    post("pd-convolution %s: [convolution~]", CONVOLUTION_VERSION);
}
