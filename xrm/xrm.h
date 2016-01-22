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

#define FREE(p)          \
    do {                 \
        if (p != NULL) { \
            free(p);     \
            p = NULL;    \
        }                \
    } while (0)

#undef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#undef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))

/** Specifies the type of a component. */
typedef enum {
    /* A "normal" component, i.e., a name/class is given. */
    CT_NORMAL,
    /* A single wildcard component ("?"). */
    CT_WILDCARD_SINGLE,
    /* A multi wildcard component ("*"). */
    CT_WILDCARD_MULTI
} xcb_xrm_component_type_t;

/** One component of a resource, either in the name or class. */
typedef struct xcb_xrm_component_t {
    /* The type of this component. */
    xcb_xrm_component_type_t type;
    /* This component's name. Only useful for CT_NORMAL. */
    char *name;

    TAILQ_ENTRY(xcb_xrm_component_t) components;
} xcb_xrm_component_t;

/**
 * Parsed structure for a single entry in the xrm database, e.g. representing
 * the parsted state of
 *     Application*class?subclass.resource.
 */
typedef struct xcb_xrm_entry_t {
    /* The value of this entry. */
    char *value;

    /* The individual components making up this entry. */
    TAILQ_HEAD(components_head, xcb_xrm_component_t) components;
} xcb_xrm_entry_t;

/** Defines where the parser is currently at. */
typedef enum {
    /* Reading initial workspace before anything else. */
    CS_INITIAL = 0,
    /* Reading the resource path. */
    CS_COMPONENTS = 1,
    /* Reading whitespace between ':' and the value. */
    CS_PRE_VALUE_WHITESPACE = 2,
    /* Reading the resource's value. */
    CS_VALUE = 3
} xcb_xrm_entry_parser_chunk_status_t;

/** Used in xcb_xrm_parse_entry. */
typedef struct xcb_xrm_entry_parser_state_t {
    xcb_xrm_entry_parser_chunk_status_t chunk;
    char buffer[4096];
    char *buffer_pos;
    xcb_xrm_component_type_t current_type;
} xcb_xrm_entry_parser_state_t;

struct xcb_xrm_context_t {
    xcb_connection_t *conn;
    xcb_screen_t *screen;

    /* The unprocessed resource manager string. */
    char *resources;
};

struct xcb_xrm_resource_t {
    unsigned int size;
    char *value;
};

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
 * Parses a specific resource string.
 *
 * @param str The resource string.
 * @param entry A return struct that will contain the parsed resource. The
 * memory will be allocated dynamically, so it must be freed.
 * @param no_wildcards If true, only components of type CT_NORMAL are allowed.
 *
 * @return 0 on success, a negative error code otherwise.
 *
 */
int xcb_xrm_parse_entry(const char *str, xcb_xrm_entry_t **entry, bool no_wildcards);

/**
 * Frees the given entry.
 *
 */
void xcb_xrm_entry_free(xcb_xrm_entry_t *entry);

#endif /* __XRM_H__ */
