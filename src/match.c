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
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <sys/queue.h>

#include "match.h"
#include "util.h"

/* Forward declarations */
static int __match_matches(xcb_xrm_entry_t *db_entry, xcb_xrm_entry_t *query_name, xcb_xrm_entry_t *query_class,
        xcb_xrm_match_t *match);
static int __match_compare(int length, xcb_xrm_match_t *best, xcb_xrm_match_t *candidate);
static xcb_xrm_match_t *__match_new(int length);
static void __match_free(xcb_xrm_match_t *match);

/*
 * Finds the matching entry in the database given a full name / class query string.
 *
 */
int xcb_xrm_match(xcb_xrm_context_t *ctx, xcb_xrm_entry_t *query_name, xcb_xrm_entry_t *query_class,
        xcb_xrm_resource_t *resource) {
    xcb_xrm_match_t *best_match = NULL;
    xcb_xrm_entry_t *cur_entry = TAILQ_FIRST(&(ctx->entries));

    /* Let's figure out how many elements we need to store. */
    int num = 0;
    do {
        xcb_xrm_component_t *component;
        TAILQ_FOREACH(component, &(query_name->components), components) {
            num++;
        }
    } while (0);

    while (cur_entry != NULL) {
        xcb_xrm_match_t *cur_match = __match_new(num);

        /* First we check whether the current database entry even matches. */
        if (__match_matches(cur_entry, query_name, query_class, cur_match) == 0) {
            /* The first matching entry is the first one we pick as the best matching entry. */
            if (best_match == NULL) {
                best_match = cur_match;
            } else {
                /* Otherwise, check whether this match is better than the current best. */
                if (__match_compare(num, best_match, cur_match) == 0) {
                    __match_free(best_match);
                    best_match = cur_match;
                } else {
                    __match_free(cur_match);
                }
            }
        } else {
            __match_free(cur_match);
        }

        /* Advance to the next database entry. */
        cur_entry = TAILQ_NEXT(cur_entry, entries);
    }

    if (best_match != NULL) {
        resource->value = sstrdup(best_match->entry->value);
        resource->size = strlen(resource->value);

        __match_free(best_match);
        return SUCCESS;
    }

    return -FAILURE;
}

static int __match_matches(xcb_xrm_entry_t *db_entry, xcb_xrm_entry_t *query_name, xcb_xrm_entry_t *query_class,
        xcb_xrm_match_t *match) {
    /* We need to deal with an absent query_class since many applications don't
     * pass one, even though that violates the specification. */
    bool use_class = (query_class != NULL);

    xcb_xrm_component_t *cur_comp_name = TAILQ_FIRST(&(query_name->components));
    xcb_xrm_component_t *cur_comp_class = use_class ? TAILQ_FIRST(&(query_class->components)) : NULL;
    xcb_xrm_component_t *cur_comp_db = TAILQ_FIRST(&(db_entry->components));
    xcb_xrm_component_t *next_comp_db = NULL;

#define ADVANCE(entry) do {                      \
    if (entry != NULL)                           \
        entry = TAILQ_NEXT((entry), components); \
} while (0)

    int i = 0;
    while (cur_comp_name != NULL && (!use_class || cur_comp_class) && cur_comp_db != NULL) {
        switch (cur_comp_db->type) {
            case CT_NORMAL:
                if (strcmp(cur_comp_db->name, cur_comp_name->name) == 0) {
                    match->matches[i++] = MT_NAME | MT_EXACT;
                    ADVANCE(cur_comp_db);
                    ADVANCE(cur_comp_name);
                    ADVANCE(cur_comp_class);
                } else if (use_class && strcmp(cur_comp_db->name, cur_comp_class->name) == 0) {
                    match->matches[i++] = MT_CLASS | MT_EXACT;
                    ADVANCE(cur_comp_db);
                    ADVANCE(cur_comp_name);
                    ADVANCE(cur_comp_class);
                } else {
                    return -FAILURE;
                }

                break;
            case CT_WILDCARD_SINGLE:
                match->matches[i++] = MT_SINGLE;
                ADVANCE(cur_comp_db);
                ADVANCE(cur_comp_name);
                ADVANCE(cur_comp_class);

                break;
            case CT_WILDCARD_MULTI:
                next_comp_db = TAILQ_NEXT(cur_comp_db, components);
                /* The '**' case should never happen as the parser hopefully collapsed those. */
                if (next_comp_db->type == CT_WILDCARD_MULTI)
                    return -FAILURE;

                // TODO XXX This needs to be handled properly.
                if (next_comp_db->type == CT_WILDCARD_SINGLE) {
                    return -FAILURE;
                }

                if (strcmp(next_comp_db->name, cur_comp_name->name) == 0 ||
                        (use_class && strcmp(next_comp_db->name, cur_comp_class->name) == 0)) {
                    ADVANCE(cur_comp_db);
                } else {
                    match->matches[i++] = MT_MULTI;
                    ADVANCE(cur_comp_name);
                    ADVANCE(cur_comp_class);
                }

                break;
            default:
                /* Never reached. */
                return -FAILURE;
        }
    }

#undef ADVANCE

    if (cur_comp_db == NULL && cur_comp_name == NULL && (!use_class || cur_comp_class == NULL)) {
        match->entry = db_entry;
        return SUCCESS;
    }

    return -FAILURE;
}

static int __match_compare(int length, xcb_xrm_match_t *best, xcb_xrm_match_t *candidate) {
    for (int i = 0; i < length; i++) {
        xcb_xrm_match_type_t mt_best = best->matches[i];
        xcb_xrm_match_type_t mt_candidate = candidate->matches[i];

        /* Precedence rule #1: Matching components, including '?', outweigh '*'. */
        if (mt_best & MT_MULTI && (mt_candidate & MT_EXACT || mt_candidate & MT_SINGLE))
            return SUCCESS;

        /* Precedence rule #2: Matching name outweighs both matching class and '?'.
         *                     Matching class outweighs '?'. */
        if ((mt_best & MT_CLASS || mt_best & MT_SINGLE) &&
                (mt_candidate & MT_NAME && mt_candidate & MT_EXACT))
            return SUCCESS;

        if (mt_best & MT_SINGLE &&
                (mt_candidate & MT_CLASS && mt_candidate & MT_EXACT))
            return SUCCESS;

        /* Precedence rule #3: A preceding exact match outweighs a preceding '*'. */
        if (i != 0) {
            if (best->matches[i-1] & MT_MULTI && candidate->matches[i-1] & MT_EXACT)
                return SUCCESS;
        }
    }

    return -FAILURE;
}

static xcb_xrm_match_t *__match_new(int length) {
    xcb_xrm_match_t *match = scalloc(1, sizeof(struct xcb_xrm_match_t));
    match->entry = NULL;
    match->matches = scalloc(1, length * sizeof(xcb_xrm_match_type_t));
    return match;
}

static void __match_free(xcb_xrm_match_t *match) {
    FREE(match->matches);
    FREE(match);
}
