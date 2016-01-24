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

#include "entry.h"
#include "util.h"

static void xcb_xrm_append_char(xcb_xrm_entry_t *entry, xcb_xrm_entry_parser_state_t *state,
        const char str) {
    if (state->buffer_pos == NULL) {
        memset(&(state->buffer), 0, sizeof(state->buffer));
        state->buffer_pos = state->buffer;
    }

    *(state->buffer_pos++) = str;
}

// TODO propgate this result code
static int xcb_xrm_insert_component(xcb_xrm_entry_t *entry, xcb_xrm_component_type_t type,
        const char *str) {
    xcb_xrm_component_t *new;
    if ((new = calloc(1, sizeof(struct xcb_xrm_component_t))) == NULL) {
        return -ENOMEM;
    }

    if (str != NULL) {
        new->name = sstrdup(str);
    }

    new->type = type;
    TAILQ_INSERT_TAIL(&(entry->components), new, components);
    return 0;
}

static void xcb_xrm_finalize_component(xcb_xrm_entry_t *entry, xcb_xrm_entry_parser_state_t *state) {
    if (state->buffer_pos != NULL && state->buffer_pos != state->buffer) {
        *(state->buffer_pos) = '\0';
        xcb_xrm_insert_component(entry, state->current_type, state->buffer);
    }

    memset(&(state->buffer), 0, sizeof(state->buffer));
    state->buffer_pos = state->buffer;
    state->current_type = CT_NORMAL;
}

static void xcb_xrm_append_component(xcb_xrm_entry_t *entry, xcb_xrm_component_type_t type,
        xcb_xrm_entry_parser_state_t *state, const char *str) {
    xcb_xrm_finalize_component(entry, state);
    xcb_xrm_insert_component(entry, type, str);
}

int xcb_xrm_parse_entry(const char *_str, xcb_xrm_entry_t **_entry, bool no_wildcards) {
    char *str;
    char *walk;
    xcb_xrm_entry_t *entry = NULL;
    xcb_xrm_component_t *last;
    char value_buf[4096];
    char *value_pos = value_buf;

    xcb_xrm_entry_parser_state_t state = {
        .chunk = CS_INITIAL,
        .current_type = CT_NORMAL,
    };

    /* Copy the input string since it's const. */
    str = sstrdup(_str);

    /* Allocate memory for the return parameter. */
    if ((*_entry = calloc(1, sizeof(struct xcb_xrm_entry_t))) == NULL) {
        *_entry = NULL;
        return -ENOMEM;
    }
    entry = *_entry;
    TAILQ_INIT(&(entry->components));

    // TODO Respect no_wildcards argument
    for (walk = str; *walk != '\0'; walk++) {
        switch (*walk) {
            case '.':
                state.chunk = MAX(state.chunk, CS_COMPONENTS);
                if (state.chunk == CS_VALUE) {
                    goto process_normally;
                }

                xcb_xrm_finalize_component(entry, &state);
                state.current_type = CT_NORMAL;
                break;
            case '?':
                state.chunk = MAX(state.chunk, CS_COMPONENTS);
                if (state.chunk == CS_VALUE) {
                    goto process_normally;
                }

                xcb_xrm_append_component(entry, CT_WILDCARD_SINGLE, &state, NULL);
                break;
            case '*':
                state.chunk = MAX(state.chunk, CS_COMPONENTS);
                if (state.chunk == CS_VALUE) {
                    goto process_normally;
                }

                /* We can ignore a '*' if the previous component was also one. */
                last = TAILQ_LAST(&(entry->components), components_head);
                if (last != NULL && last->type == CT_WILDCARD_MULTI) {
                    break;
                }

                xcb_xrm_append_component(entry, CT_WILDCARD_MULTI, &state, NULL);
                break;
            case ' ':
            case '\t':
                /* Spaces are only allowed in the value, but spaces between the
                 * ':' and the value are omitted. */
                if (state.chunk <= CS_PRE_VALUE_WHITESPACE) {
                    break;
                }

                goto process_normally;
            case ':':
                // TODO XXX We should also handle state.chunk == CS_INITIAL
                // here, even though it's an exotic case.
                if (state.chunk == CS_COMPONENTS) {
                    xcb_xrm_finalize_component(entry, &state);
                    state.chunk = CS_PRE_VALUE_WHITESPACE;
                    break;
                } else if (state.chunk >= CS_PRE_VALUE_WHITESPACE) {
                    state.chunk = CS_VALUE;
                    goto process_normally;
                }
                break;
            default:
process_normally:
                state.chunk = MAX(state.chunk, CS_COMPONENTS);

                if (state.chunk == CS_PRE_VALUE_WHITESPACE) {
                    state.chunk = CS_VALUE;
                }

                if (state.chunk < CS_VALUE) {
                    xcb_xrm_append_char(entry, &state, *walk);
                } else {
                    *(value_pos++) = *walk;
                }
                break;
        }
    }
    FREE(str);

    if (state.chunk == CS_VALUE) {
        *value_pos = '\0';
        entry->value = sstrdup(value_buf);
    } else {
        /* Return error if there was no value for this entry. */
        goto done_error;
    }

    /* Assert that this entry actually had a resource component. */
    if ((last = TAILQ_LAST(&(entry->components), components_head)) == NULL) {
        goto done_error;
    }

    /* Assert that the last component is not a wildcard. */
    if (last->type != CT_NORMAL) {
        goto done_error;
    }

    return 0;

done_error:
    xcb_xrm_entry_free(entry);
    *_entry = NULL;
    return -1;
}

void xcb_xrm_entry_free(xcb_xrm_entry_t *entry) {
    xcb_xrm_component_t *component;
    if (entry == NULL)
        return;

    FREE(entry->value);
    while (!TAILQ_EMPTY(&(entry->components))) {
        component = TAILQ_FIRST(&(entry->components));
        FREE(component->name);
        TAILQ_REMOVE(&(entry->components), component, components);
        FREE(component);
    }

    FREE(entry);
    return;
}
