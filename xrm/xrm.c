/*
 * vim:ts=4:sw=4:expandtab
 *
 * Copyright © 2016 Ingo Bürk
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the names of the authors or their
 * institutions shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization from the authors.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>

#include <xcb/xcb.h>

#include "xrm.h"

int xcb_xrm_context_new(xcb_connection_t *conn, xcb_xrm_context_t **c) {
    xcb_xrm_context_t *ctx = NULL;

    if ((*c = calloc(1, sizeof(*c))) == NULL) {
        *c = NULL;
        return -ENOMEM;
    }

    ctx = *c;
    ctx->conn = conn;
    return 0;
}

void xcb_xrm_context_free(xcb_xrm_context_t *ctx) {
    free(ctx);
}

int xcb_xrm_get_resource(xcb_xrm_context_t *ctx, const char *res_name, const char *res_class,
                         char **res_type, xcb_xrm_resource_t *resource) {
    // TODO XXX
    return 0;
}
