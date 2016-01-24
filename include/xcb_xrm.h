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
#ifndef __XCB_XRM_H__
#define __XCB_XRM_H__

#include <xcb/xcb.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A context for using this library.
 *
 * TODO Documentation
 *
 */
typedef struct xcb_xrm_context_t xcb_xrm_context_t;

/**
 * TODO Documentation
 *
 */
typedef struct xcb_xrm_resource_t xcb_xrm_resource_t;

/**
 * TODO Documentation
 *
 */
int xcb_xrm_context_new(xcb_connection_t *conn, xcb_screen_t *screen, xcb_xrm_context_t **ctx);

/**
 * TODO Documentation
 *
 */
void xcb_xrm_context_free(xcb_xrm_context_t *ctx);

/**
 * Initializes the database for the context by parsing the resource manager
 * property.
 *
 * @param ctx Context.
 *
 * @return 0 on success, a negative error code otherwise.
 *
 */
int xcb_xrm_initialize_database(xcb_xrm_context_t *ctx);

/**
 * TODO Documentation
 *
 */
int xcb_xrm_get_resource(xcb_xrm_context_t *ctx, const char *res_name, const char *res_class,
                         const char **res_type, xcb_xrm_resource_t **resource);

/**
 * TODO Documentation
 *
 */
void xcb_xrm_resource_free(xcb_xrm_resource_t *resource);

#ifdef __cplusplus
}
#endif

#endif /* __XCB_XRM_H__ */
