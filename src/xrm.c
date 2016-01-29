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
    xcb_xrm_entry_t *curr;
    xcb_xrm_entry_t *entry_name = NULL;
    xcb_xrm_entry_t *entry_class = NULL;
    xcb_xrm_hay_entry_t *cur_db_entry, *next_db_entry;
    xcb_xrm_component_t *cur_res_name_component;
    TAILQ_HEAD(entries_head, xcb_xrm_hay_entry_t) entries_head = TAILQ_HEAD_INITIALIZER(entries_head);
    int result = 0;

    if (ctx->resources == NULL || TAILQ_EMPTY(&(ctx->entries))) {
        *res_type = NULL;
        *_resource = NULL;
        return -1;
    }

    *res_type = "String";
    *_resource = scalloc(1, sizeof(struct xcb_xrm_resource_t));
    resource = *_resource;

    if (xcb_xrm_entry_parse(res_name, &entry_name, true) < 0) {
        result = -1;
        goto done;
    }

    /* For the resource class input, we allow NULL and empty string as
     * placeholders for not specifying this string. Technically this is
     * violating the spec, but it seems to be widely used. */
    if (res_class != NULL && strlen(res_class) > 0 &&
            xcb_xrm_entry_parse(res_class, &entry_class, true) < 0) {
        result = -1;
        goto done;
    }

    /* First, we set up a wrapper list of our database so we can store some
     * additional information for each entry and also filter entries out. */
    TAILQ_FOREACH(curr, &(ctx->entries), entries) {
        xcb_xrm_hay_entry_t *new = scalloc(1, sizeof(xcb_xrm_hay_entry_t));
        new->entry = curr;
        new->current_component = TAILQ_FIRST(&(curr->components));

        TAILQ_INSERT_TAIL(&entries_head, new, entries);
    }

    // TODO XXX This will currently eat our actual database and fail with the second request. Fix this.
    // TODO XXX We currently ignore res_class. Implement handling it.
    // TODO XXX The last component must be treated case insensitive.
    // TODO XXX This currently doesn't implement precedence at all.
    // https://tronche.com/gui/x/xlib/resource-manager/matching-rules.html
    cur_res_name_component = TAILQ_FIRST(&(entry_name->components));
    while (cur_res_name_component != NULL) {
        cur_db_entry = TAILQ_FIRST(&entries_head);

        while (cur_db_entry != NULL) {
            next_db_entry = NULL;

            /* If the queried resource is longer than this entry, eliminate it. */
            if (cur_db_entry->current_component == NULL) {
                goto eliminate_entry;
            }

            if (cur_db_entry->current_component->type == CT_NORMAL) {
                /* We are comparing against a non-wildcard component, so just compare names. */
                if (strcmp(cur_db_entry->current_component->name, cur_res_name_component->name) == 0) {
                    /* The name matches, so we don't filter this entry out. */
                    goto keep_entry;
                } else {
                    /* No match. Better luck next time! */
                    goto eliminate_entry;
                }
            } else if (cur_db_entry->current_component->type == CT_WILDCARD_SINGLE) {
                /* We'll happily accept the placeholder and move on. */
                goto keep_entry;
            } else {
                // TODO XXX Handle wildcards
            }

eliminate_entry:
            next_db_entry = TAILQ_NEXT(cur_db_entry, entries);
            TAILQ_REMOVE(&entries_head, cur_db_entry, entries);
            FREE(cur_db_entry);
keep_entry:
            if (next_db_entry == NULL && cur_db_entry != NULL) {
                next_db_entry = TAILQ_NEXT(cur_db_entry, entries);
            }

            if (cur_db_entry != NULL) {
                cur_db_entry->current_component = TAILQ_NEXT(cur_db_entry->current_component, components);
            }

            cur_db_entry = next_db_entry;
        }

        cur_res_name_component = TAILQ_NEXT(cur_res_name_component, components);
    }

    /* If we filtered everything out, there's no result. */
    if (TAILQ_EMPTY(&entries_head)) {
        result = -1;
        goto done;
    }

    // TODO XXX Implement precedence here
    resource->value = sstrdup(TAILQ_FIRST(&entries_head)->entry->value);
    resource->size = strlen(resource->value);

done:
    while (!TAILQ_EMPTY(&entries_head)) {
        cur_db_entry = TAILQ_FIRST(&entries_head);
        TAILQ_REMOVE(&entries_head, cur_db_entry, entries);
        FREE(cur_db_entry);
    }

    if (entry_name != NULL) {
        xcb_xrm_entry_free(entry_name);
    }
    if (entry_class != NULL) {
        xcb_xrm_entry_free(entry_class);
    }

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
    return 0;
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
    xcb_get_property_cookie_t rm_cookie;
    xcb_get_property_reply_t *rm_reply;
    xcb_generic_error_t *err;
    int rm_length;
    char *resources;

    // TODO XXX Be smarter here and really get the entire string, no matter how long it is.
    rm_cookie = xcb_get_property(ctx->conn, 0, ctx->screen->root, XCB_ATOM_RESOURCE_MANAGER,
            XCB_ATOM_STRING, 0, 16 * 1024);

    rm_reply = xcb_get_property_reply(ctx->conn, rm_cookie, &err);
    if (err != NULL) {
        FREE(err);
        return -1;
    }

    if (rm_reply == NULL)
        return -1;

    if ((rm_length = xcb_get_property_value_length(rm_reply)) == 0) {
        return 0;
    }

    if (asprintf(&resources, "%.*s", rm_length, (char *)xcb_get_property_value(rm_reply)) == -1) {
        return -ENOMEM;
    }

    /* We don't need this anymore. */
    FREE(rm_reply);

    /* Parse the resource string. */
    return xcb_xrm_database_load_from_string(ctx, resources);
}
