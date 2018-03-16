/*
* BSD 3-Clause License
*
* Copyright (c) 2017-2018, plures
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright notice,
*    this list of conditions and the following disclaimer in the documentation
*    and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its
*    contributors may be used to endorse or promote products derived from
*    this software without specific prior written permission.
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
*/


#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include "ndtypes.h"
#include "xnd.h"
#include "gumath.h"


/* Loops and functions for NumPy strided kernels. */


#define ASSIGN_OVERFLOW(array, index, value, max, ctx) \
    do {                                                                        \
        if (index >= max) {                                                     \
            ndt_err_format(ctx, NDT_RuntimeError, "unexpected array overflow"); \
            return -1;                                                          \
        }                                                                       \
        array[index++] = value;                                                 \
    } while (0)


/*
 * Convert an xnd container into the {args, dimensions, strides} representation.
 */
int
gm_np_convert_xnd(char **args, const int nargs,
                  int64_t *dimensions, const int dims_size,
                  int64_t *steps, const int steps_size,
                  xnd_t stack[], const int outer_dims,
                  ndt_context_t *ctx)
{
    xnd_ndarray_t nd[nargs];
    intptr_t shape;
    int n = 0, m = 0;
    int i, k;

    if (nargs == 0) {
        return 0;
    }

    for (i = 0; i < nargs; i++) {
        if (xnd_as_ndarray(&nd[i], &stack[i], ctx) < 0) {
            return -1;
        }
        args[i] = nd[i].ptr;
    }

    for (i = 0; i < outer_dims; i++) {
        shape = nd[0].shape[i];
        ASSIGN_OVERFLOW(dimensions, n, shape, dims_size, ctx);

        for (k = 0; k < nargs; k++) {
            if (nd[k].shape[i] != shape) {
                ndt_err_format(ctx, NDT_RuntimeError,
                    "unexpected shape mismatch in outer dimensions");
                return -1;
            }

            ASSIGN_OVERFLOW(steps, m, nd[k].strides[i], steps_size, ctx);
        }
    }

    for (i = 0; i < nargs; i++) {
        for (k = outer_dims; k < nd[i].ndim; k++) {
            ASSIGN_OVERFLOW(dimensions, n, nd[i].shape[k], dims_size, ctx);
            ASSIGN_OVERFLOW(steps, m, nd[i].strides[k], steps_size, ctx);
        }
    }

    return 0;
}

/*
 * Flatten an xnd container into a 1D representation for direct elementwise
 * kernel application.  A scalar is expanded into a 1D array of size 1.
 */
int
gm_np_flatten(char **args, const int nargs,
              int64_t *dimensions,
              int64_t *steps,
              const xnd_t stack[],
              ndt_context_t *ctx)
{
    xnd_ndarray_t nd;
    int i;

    for (i = 0; i < nargs; i++) {
        if (xnd_as_ndarray(&nd, &stack[i], ctx) < 0) {
            return -1;
        }
        args[i] = nd.ptr;
        dimensions[i] = nd.nelem;
        steps[i] = nd.itemsize;
    }

    return 0;
}

int
gm_np_map(const gm_strided_kernel_t f,
          char **args, int nargs,
          intptr_t *dimensions,
          intptr_t *steps,
          void *data,
          int outer_dims)
{
    char *next[nargs];
    intptr_t shape, i;
    int k;

    if (outer_dims == 1) {
        return f(args, dimensions, steps, data);
    }
    if (outer_dims == 0) {
        assert(0);
    }

    shape = dimensions[0];

    for (i = 0; i < shape; i++) {
        for (k = 0; k < nargs; k++) {
            next[k] = args[k] + i * steps[k];
        }

        if (gm_np_map(f, next, nargs, dimensions+1, steps+nargs, data,
                      outer_dims-1) < 0) {
            return -1;
        }
    }

    return 0;
}
