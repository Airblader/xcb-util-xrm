/* Copyright © 2016 Ingo Bürk
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
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>

#include "xrm.h"

#if defined(__GNUC__) && ((__GNUC__ * 100 + __GNUC_MINOR__) >= 203)
#define ATTRIBUTE_PRINTF(x,y) __attribute__((__format__(__printf__,x,y)))
#else
#define ATTRIBUTE_PRINTF(x,y)
#endif

#define SKIP 77

static bool check_strings(const char *expected, const char *actual,
        const char *format, ...) ATTRIBUTE_PRINTF(3, 4);
static bool check_ints(const int expected, const int actual,
        const char *format, ...) ATTRIBUTE_PRINTF(3, 4);

static bool check_strings(const char *expected, const char *actual,
        const char *format, ...) {
    va_list ap;

    if (expected == NULL && actual == NULL)
        return false;

    if (expected != NULL && actual != NULL && strcmp(expected, actual) == 0)
        return false;

    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    return true;
}

static bool check_ints(const int expected, const int actual,
        const char *format, ...) {
    va_list ap;

    if (expected == actual)
        return false;

    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    return true;
}

static int check_parse_entry(const char *str, const char *value, const int count, ...) {
    xcb_xrm_entry_t *entry;
    xcb_xrm_component_t *component;
    int actual_length = 0;
    va_list ap;

    fprintf(stderr, "== Assert that parsing \"%s\" is successful\n", str);

    if (xcb_xrm_parse_entry(str, &entry, false) < 0) {
        fprintf(stderr, "xcb_xrm_parse_entry() < 0\n");
        return true;
    }

    bool err = false;

    /* Assert the entry's value. */
    err |= check_strings(value, entry->value, "Wrong entry value: <%s> / <%s>\n", value, entry->value);

    /* Assert the number of components. */
    TAILQ_FOREACH(component, &(entry->components), components) {
        actual_length++;
    }
    err |= check_ints(count, actual_length, "Wrong number of components: <%d> / <%d>\n", count, actual_length);

    /* Assert the individual components. */
    va_start(ap, count);
    TAILQ_FOREACH(component, &(entry->components), components) {
        const char *curr = va_arg(ap, const char *);

        switch(component->type) {
            case CT_WILDCARD_SINGLE:
                err |= check_strings("?", curr, "Expected '?', but got <%s>\n", curr);
                break;
            case CT_WILDCARD_MULTI:
                err |= check_strings("*", curr, "Expected '*', but got <%s>\n", curr);
                break;
            default:
                err |= check_strings(component->name, curr, "Expected <%s>, but got <%s>\n", component->name, curr);
                break;
        }
    }
    va_end(ap);

    xcb_xrm_entry_free(entry);
    return err;
}

static int check_parse_entry_error(const char *str, const int result) {
    xcb_xrm_entry_t *entry;
    int actual;

    fprintf(stderr, "== Assert that parsing \"%s\" returns <%d>\n", str, result);

    actual = xcb_xrm_parse_entry(str, &entry, false);
    return check_ints(result, actual, "Wrong result code: <%d> / <%d>\n", result, actual);
}

static int test_entry_parser(void) {
    bool err = false;
#if 0
    int screennr;
    xcb_connection_t *conn;
    xcb_screen_t *screen;
    xcb_xrm_context_t *ctx;
    bool err = false;

    conn = xcb_connect(NULL, &screennr);
    if (conn == NULL || xcb_connection_has_error(conn)) {
        fprintf(stderr, "Failed to connect to X11 server.\n");
        return true;
    }

    screen = xcb_aux_get_screen(conn, screennr);
    if (screen == NULL) {
        fprintf(stderr, "Failed to query root screen.\n");
        return true;
    }

    if (xcb_xrm_context_new(conn, screen, &ctx) < 0) {
        fprintf(stderr, "Failed to initialize context.\n");
        return true;
    }
#endif

    err |= check_parse_entry("Xft.dpi: 96", "96", 2, "Xft", "dpi");
    err |= check_parse_entry("*color0: #abcdef", "#abcdef", 2, "*", "color0");
    err |= check_parse_entry("*Menu.color10:\t#000000", "#000000", 3, "*", "Menu", "color10");
    err |= check_parse_entry("?Foo.Bar?Baz?la:\t\t \tA\tB C:D ", "A\tB C:D ", 7,
            "?", "Foo", "Bar", "?", "Baz", "?", "la");
    err |= check_parse_entry("Foo**baz: x", "x", 3, "Foo", "*", "baz");

    err |= check_parse_entry_error("Foo?: x", -1);
    err |= check_parse_entry_error("Foo*: x", -1);
    err |= check_parse_entry_error(": x", -1);
    err |= check_parse_entry_error("Foo", -1);
    err |= check_parse_entry_error("Foo? Bar", -1);

    // TODO XXX Tests for no_wildcards

    return err;
}

int main(void) {
    bool err = false;

    err |= test_entry_parser();

    return err;
}
