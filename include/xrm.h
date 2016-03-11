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
#ifndef __XRM_H__
#define __XRM_H__

#include <sys/queue.h>
#include <stdbool.h>

#include "xcb_xrm.h"
#include "util.h"
#include "entry.h"

struct xcb_xrm_context_t {
    xcb_connection_t *conn;
    xcb_screen_t *screen;

    /* The unprocessed resource manager string. */
    char *resources;

    TAILQ_HEAD(database_head, xcb_xrm_entry_t) entries;
};

struct xcb_xrm_resource_t {
    unsigned int size;
    char *value;
};

/**
 * Interprets the string as a resource list, parses it and stores it in the database of the context.
 *
 * @param ctx Context.
 * @param str Resource string.
 * @return 0 on success, a negative error code otherwise.
 *
 */
int xcb_xrm_database_load_from_string(xcb_xrm_context_t *ctx, const char *str);

#endif /* __XRM_H__ */
