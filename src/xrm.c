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


typedef enum {
	MT_EXACT = 0,
	MT_SINGLE = 1,
	MT_MULTI = 2
} xcb_xrm_matchtype_t;

typedef struct {
	xcb_xrm_entry_t *db_match;
	xcb_xrm_matchtype_t* matchtype;
} xcb_xrm_match_result_t;

/* Forward declarations */
static void xcb_xrm_database_free(xcb_xrm_context_t *ctx);
static void xcb_xrm_match_resources(xcb_xrm_entry_t *queried_entry, xcb_xrm_entry_t *db_entry, xcb_xrm_match_result_t *match_result);
static xcb_xrm_entry_t* xcb_xrm_compare_matches(xcb_xrm_entry_t *first, xcb_xrm_entry_t *second);


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
    xcb_xrm_entry_t *next_entry = NULL;
    xcb_xrm_entry_t *best_matching_entry = NULL;
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


    next_entry = TAILQ_FIRST(&(ctx->entries));
    while (next_entry != NULL) {
    	// TODO allocate memory for the matchtypes
    	xcb_xrm_match_result_t *match_result = scalloc(1, sizeof(xcb_xrm_match_result_t));
    	xcb_xrm_match_resources(next_entry, query_name, match_result);
    	if (match_result->db_match != NULL) {
    		if (best_matching_entry == NULL) {
    			best_matching_entry = next_entry;
    		}
    		best_matching_entry = xcb_xrm_compare_matches(best_matching_entry, next_entry);
    	}
    	free(match_result);
    	next_entry = TAILQ_NEXT(next_entry, entries);
    }
	if (best_matching_entry != NULL) {
		resource->value = sstrdup(best_matching_entry->value);
		resource->size = strlen(resource->value);
	}

done:
    return result;
}

static xcb_xrm_entry_t* xcb_xrm_compare_matches(xcb_xrm_entry_t *first, xcb_xrm_entry_t *second) {

	xcb_xrm_component_t *first_component = TAILQ_FIRST(&(first->components));
	xcb_xrm_component_t *second_component = TAILQ_FIRST(&(second->components));

	while (true) {
		if (second_component == NULL) {
			return second;
		} else if (first_component == NULL) {
			return first;
		}
		if (first_component->type < second_component->type) { return first;}
		if (first_component->type > second_component->type) { return second;}

		first_component = TAILQ_NEXT(first_component, components);
		second_component = TAILQ_NEXT(second_component, components);
	}
	return first;
}

static void xcb_xrm_match_resources(xcb_xrm_entry_t *db_entry, xcb_xrm_entry_t *queried_entry, xcb_xrm_match_result_t *match_result) {

	xcb_xrm_component_t *current_component_query = TAILQ_FIRST(&(queried_entry->components));
	xcb_xrm_component_t *current_component_db = TAILQ_FIRST(&(db_entry->components));
	xcb_xrm_component_t *next_comp_db = current_component_db;

	while(true) {
		if(current_component_query == NULL && current_component_db == NULL) {
			match_result->db_match = db_entry;
			return;
		} else  if (current_component_query == NULL || current_component_db == NULL){
			return;
		}
		switch (current_component_db->type) {
			case CT_NORMAL:
				if (strcmp(current_component_query->name, current_component_db->name) == 0) {
					current_component_query = TAILQ_NEXT(current_component_query, components);
					current_component_db = TAILQ_NEXT(current_component_db, components);
				} else {
					return;
				}
				break;
			case CT_WILDCARD_SINGLE:
				current_component_query = TAILQ_NEXT(current_component_query, components);
				current_component_db = TAILQ_NEXT(current_component_db, components);
				break;
			case CT_WILDCARD_MULTI:
				next_comp_db = TAILQ_NEXT(current_component_db, components);
				// TODO: implement *?
				if (next_comp_db->type != CT_NORMAL) {
					return;
				}
				if (strcmp(current_component_query->name, next_comp_db->name) == 0) {
					current_component_db = TAILQ_NEXT(current_component_db, components);
				} else {
					current_component_query = TAILQ_NEXT(current_component_query, components);
				}
				break;
		}
	}

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
