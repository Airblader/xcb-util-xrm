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

#include <xcb/xcb.h>

#include "xrm.h"
#include "entry.h"
#include "match.h"
#include "util.h"

/* Forward declarations */
static void xcb_xrm_database_free(xcb_xrm_context_t *ctx);

int xcb_xrm_context_new(xcb_connection_t *conn, xcb_screen_t *screen, xcb_xrm_context_t **c) {
    xcb_xrm_context_t *ctx = NULL;

    *c = scalloc(1, sizeof(struct xcb_xrm_context_t));

    ctx = *c;
    ctx->conn = conn;
    ctx->screen = screen;

    TAILQ_INIT(&(ctx->entries));

    return 0;
}

static void xcb_xrm_database_free(xcb_xrm_context_t *ctx) {
    xcb_xrm_entry_t *entry;

    FREE(ctx->resources);

    while (!TAILQ_EMPTY(&(ctx->entries))) {
        entry = TAILQ_FIRST(&(ctx->entries));
        TAILQ_REMOVE(&(ctx->entries), entry, entries);
        xcb_xrm_entry_free(entry);
    }
}

void xcb_xrm_context_free(xcb_xrm_context_t *ctx) {
    xcb_xrm_database_free(ctx);
    FREE(ctx);
}

int xcb_xrm_resource_get(xcb_xrm_context_t *ctx, const char *res_name, const char *res_class,
                         const char **res_type, xcb_xrm_resource_t **_resource) {
    xcb_xrm_resource_t *resource;
    xcb_xrm_entry_t *query_name = NULL;
    xcb_xrm_entry_t *query_class = NULL;
    int result = 0;

    if (ctx->resources == NULL || TAILQ_EMPTY(&(ctx->entries))) {
        *res_type = NULL;
        *_resource = NULL;
        return -1;
    }

    *res_type = "String";
    *_resource = scalloc(1, sizeof(struct xcb_xrm_resource_t));
    resource = *_resource;

    if (xcb_xrm_entry_parse(res_name, &query_name, true) < 0) {
        result = -1;
        goto done;
    }

    /* For the resource class input, we allow NULL and empty string as
     * placeholders for not specifying this string. Technically this is
     * violating the spec, but it seems to be widely used. */
    if (res_class != NULL && strlen(res_class) > 0 &&
            xcb_xrm_entry_parse(res_class, &query_class, true) < 0) {
        result = -1;
        goto done;
    }

    /* We rely on name and class query strings to have the same number of
     * components, so let's check that this is the case. The specification
     * backs us up here. */
    if (query_class != NULL &&
            xcb_xrm_entry_num_components(query_name) != xcb_xrm_entry_num_components(query_class)) {
        result = -1;
        goto done;
    }

    result = xcb_xrm_match(ctx, query_name, query_class, resource);
done:
    xcb_xrm_entry_free(query_name);
    xcb_xrm_entry_free(query_class);
    return result;
}

void xcb_xrm_resource_free(xcb_xrm_resource_t *resource) {
    FREE(resource->value);
    FREE(resource);
}

/*
 * Interprets the string as a resource list, parses it and stores it in the database of the context.
 *
 * @param ctx Context.
 * @param str Resource string.
 * @return 0 on success, a negative error code otherwise.
 *
 */
int xcb_xrm_database_load_from_string(xcb_xrm_context_t *ctx, const char *str) {
    char *copy = sstrdup(str);

    xcb_xrm_database_free(ctx);
    ctx->resources = sstrdup(str);

    // TODO XXX Don't split on "\\n", which is an escaped newline.
    for (char *line = strtok(copy, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        xcb_xrm_entry_t *entry;
        if (xcb_xrm_entry_parse(line, &entry, false) == 0 && entry != NULL) {
            TAILQ_INSERT_TAIL(&(ctx->entries), entry, entries);
        }
    }

    FREE(copy);
    return SUCCESS;
}

/*
 * Initializes the database for the context by parsing the resource manager
 * property.
 *
 * @param ctx Context.
 *
 * @return 0 on success, a negative error code otherwise.
 *
 */
int xcb_xrm_database_load(xcb_xrm_context_t *ctx) {
    char *resources = xcb_util_get_property(ctx->conn, ctx->screen->root, XCB_ATOM_RESOURCE_MANAGER,
            XCB_ATOM_STRING, 16 * 1024);

    /* Parse the resource string. */
    return xcb_xrm_database_load_from_string(ctx, resources);
}
