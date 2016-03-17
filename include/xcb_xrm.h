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
 * @defgroup xcb_xrm_context_t XCB XRM Functions
 *
 * These functions are the xcb equivalent of the Xrm* function family in Xlib.
 * They allow the parsing and matching of X resources as well as some utility
 * functions.
 *
 * Here is an example of how this library can be used to retrieve a
 * user-configured resource:
 * @code
 * char *value;
 *
 * int screennr;
 * xcb_connection_t *conn = xcb_connect(NULL, &screennr);
 * if (conn == NULL || xcb_connection_has_error(conn))
 *     err(EXIT_FAILURE, "Could not connect to the X server.");
 *
 * xcb_screen_t *screen = xcb_aux_get_screen(conn, screennr);
 *
 * xcb_xrm_context_t *ctx;
 * if (xcb_xrm_context_new(conn, screen, &ctx) < 0)
 *     err(EXIT_FAILURE, "Could not initialize xcb-xrm.");
 *
 * if (xcb_xrm_database_from_resource_manager(ctx) < 0)
 *     err(EXIT_FAILURE, "Could not load the X resource database.");
 *
 * xcb_xrm_resource_t *resource;
 * if (xcb_xrm_resource_get(ctx, "Xft.dpi", "Xft.dpi", &resource) < 0) {
 *     // Resource not found in database
 *     value = NULL;
 * } else {
 *     value = xcb_xrm_resource_value(resource);
 *     xcb_xrm_resource_free(resource);
 * }
 *
 * xcb_xrm_context_free(ctx);
 * xcb_disconnect(conn);
 * @endcode
 *
 * @{
 */

/**
 * @struct xcb_xrm_context_t
 * Describes a context for using this library.
 *
 * A context can be created using @ref xcb_xrm_context_new (). Afterwards, the
 * resource database must be loaded, e.g., with @ref
 * xcb_xrm_database_from_resource_manager (). After fetching resources, the
 * context must be destroyed by calling @ref xcb_xrm_context_free ().
 */
typedef struct xcb_xrm_context_t xcb_xrm_context_t;

/**
 * @struct xcb_xrm_resource_t
 * Describes a resource.
 *
 * This struct holds a resource after loading it from the database, e.g., by
 * calling @ref xcb_xrm_resource_get (). Its value can be retrieved using @ref
 * xcb_xrm_resource_value () or the utility functions for conversion. A
 * resource must always be free'd by calling @ref xcb_xrm_resource_free () on
 * it.
 */
typedef struct xcb_xrm_resource_t xcb_xrm_resource_t;

/**
 * Create a new @ref xcb_xrm_context_t.
 *
 * @param conn A working XCB connection. The connection must be kept open until
 * after the context has been destroyed again.
 * @param screen The xcb_screen_t to use.
 * @param ctx A pointer to a xcb_xrm_context_t* which will be modified to
 * refer to the newly created context.
 * @return 0 on success, a negative error code otherwise.
 *
 * @ingroup xcb_xrm_context_t
 */
int xcb_xrm_context_new(xcb_connection_t *conn, xcb_screen_t *screen, xcb_xrm_context_t **ctx);

/**
 * Destroys the @ref xcb_xrm_context_t.
 *
 * @param ctx The context to destroy.
 */
void xcb_xrm_context_free(xcb_xrm_context_t *ctx);

/**
 * Loads the RESOURCE_MANAGER property and uses it as the database for this
 * context.
 *
 * @param ctx The context to use.
 * @return 0 on success, a negative error code otherwise.
 */
int xcb_xrm_database_from_resource_manager(xcb_xrm_context_t *ctx);

/**
 * Uses the given string as the database for this context.
 *
 * @param ctx The context to use.
 * @param str The resource string.
 * @return 0 on success, a negative error code otherwise.
 */
int xcb_xrm_database_from_string(xcb_xrm_context_t *ctx, const char *str);

/**
 * Fetches a resource from the database.
 *
 * @param ctx The context to use.
 * @param res_name The fully qualified resource name.
 * @param res_class The fully qualified resource class. Note that this argument
 * may be left empty / NULL, but if given, it must contain the same number of
 * components as the resource name.
 * @param resource A pointer to a xcb_xrm_resource_t* which will be modified to
 * contain the matched resource. Note that this resource must be free'd by the
 * caller.
 * @return 0 on success, a negative error code otherwise.
 */
int xcb_xrm_resource_get(xcb_xrm_context_t *ctx, const char *res_name, const char *res_class,
                         xcb_xrm_resource_t **resource);

/**
 * Returns the string value of the resource.
 *
 * @param resource The resource to use.
 * @returns The string value of the given resource.
 */
char *xcb_xrm_resource_value(xcb_xrm_resource_t *resource);

/**
 * Converts the resource's value into an integer and returns it.
 *
 * @param resource The resource to use.
 * @returns The value as an integer if it was merely a number. Otherwise, this
 * returns 0 for 'off', 'no' or 'false' and 1 for 'on', 'yes' and 'true',
 * respectively, with all comparisons being case-insensitive. For all other
 * values, INT_MIN is returned.
 */
int xcb_xrm_resource_value_int(xcb_xrm_resource_t *resource);

/**
 * Destroy the given resource.
 *
 * @param resource The resource to destroy.
 */
void xcb_xrm_resource_free(xcb_xrm_resource_t *resource);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __XCB_XRM_H__ */
