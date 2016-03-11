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

static bool check_parse_entry_resource_only;

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

    if (xcb_xrm_entry_parse(str, &entry, check_parse_entry_resource_only) < 0) {
        fprintf(stderr, "xcb_xrm_entry_parse() < 0\n");
        return true;
    }

    bool err = false;

    if (!check_parse_entry_resource_only) {
        /* Assert the entry's value. */
        err |= check_strings(value, entry->value, "Wrong entry value: <%s> / <%s>\n", value, entry->value);
    } else {
        err |= check_strings(NULL, entry->value, "Expected no value, but found <%s>\n", entry->value);
    }

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

    actual = xcb_xrm_entry_parse(str, &entry, check_parse_entry_resource_only);
    xcb_xrm_entry_free(entry);
    return check_ints(result, actual, "Wrong result code: <%d> / <%d>\n", result, actual);
}

static int check_get_resource(xcb_xrm_context_t *ctx, const char *database,
        const char *res_name, const char *res_class, const char *value) {
    bool err = false;
    const char *type;
    xcb_xrm_resource_t *resource;

    fprintf(stderr, "== Assert that getting resource <%s> / <%s> returns <%s>\n",
            res_name, res_class, value);

    xcb_xrm_database_load_from_string(ctx, database);
    if (xcb_xrm_resource_get(ctx, res_name, res_class, &type, &resource) < 0) {
        if (value != NULL) {
            fprintf(stderr, "xcb_xrm_resource_get() < 0\n");
            err = true;
        }

        goto done_get_resource;
    }

    err |= check_strings("String", type, "Expected <String>, but got <%s>\n", type);
    err |= check_strings(value, resource->value, "Expected <%s>, but got <%s>\n", value, resource->value);

done_get_resource:
    if (resource != NULL) {
        xcb_xrm_resource_free(resource);
    }

    return err;
}

static int test_entry_parser(void) {
    bool err = false;

    check_parse_entry_resource_only = false;

    /* Basic parsing */
    err |= check_parse_entry("Xft.dpi: 96", "96", 2, "Xft", "dpi");
    err |= check_parse_entry("*color0: #abcdef", "#abcdef", 2, "*", "color0");
    err |= check_parse_entry("*Menu?colors.blue: 0", "0", 5, "*", "Menu", "?", "colors", "blue");

    /* Whitespace */
    err |= check_parse_entry("*Menu.color10:\t#000000", "#000000", 3, "*", "Menu", "color10");
    err |= check_parse_entry("?Foo.Bar?Baz?la:\t\t \tA\tB C:D ", "A\tB C:D ", 7,
            "?", "Foo", "Bar", "?", "Baz", "?", "la");

    /* Subsequent '*' */
    err |= check_parse_entry("Foo**baz: x", "x", 3, "Foo", "*", "baz");

    /* Wildcards within the value. */
    err |= check_parse_entry("Foo: x.y", "x.y", 1, "Foo");
    err |= check_parse_entry("Foo: x?y", "x?y", 1, "Foo");
    err |= check_parse_entry("Foo: x*y", "x*y", 1, "Foo");

    /* Wildcards as last component */
    err |= check_parse_entry_error("Foo?: x", -1);
    err |= check_parse_entry_error("Foo*: x", -1);

    /* No / invalid resource */
    err |= check_parse_entry_error(": x", -1);
    err |= check_parse_entry_error("Foo", -1);
    err |= check_parse_entry_error("Foo? Bar", -1);

    /* Test for parsing a resource used for queries. */
    check_parse_entry_resource_only = true;

    err |= check_parse_entry("Foo.baz", NULL, 2, "Foo", "baz");
    err |= check_parse_entry_error("Foo.baz: on", -1);
    err |= check_parse_entry_error("Foo*baz", -1);
    err |= check_parse_entry_error("Foo?baz", -1);
    err |= check_parse_entry_error("*baz", -1);
    err |= check_parse_entry_error("?baz", -1);

    return err;
}

static int test_get_resource(void) {
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

    /* Some basic incomplete input tests. */
    err |= check_get_resource(ctx, "", "", "", NULL);
    err |= check_get_resource(ctx, "", NULL, "", NULL);
    err |= check_get_resource(ctx, "", "", NULL, NULL);
    err |= check_get_resource(ctx, "", NULL, NULL, NULL);

    /* Verify that all entries being filtered out works. */
    err |= check_get_resource(ctx, "", "Xft.dpi", "", NULL);

    err |= check_get_resource(ctx, "Xft.dpi: 96", "Xft.display", "", NULL);
    err |= check_get_resource(ctx, "Xft.dpi: 96", "Xft.dpi", "", "96");
    err |= check_get_resource(ctx, "Foo.baz: on\nXft.dpi: 96\nNothing?to.see: off", "Xft.dpi", "", "96");
    err |= check_get_resource(ctx, "Xft.dpi.trailingnonsense: 96", "Xft.dpi", "", NULL);
    err |= check_get_resource(ctx, "Xft.dpi: 96", "Xft.dpi.trailingnonsense", "", NULL);

    /* Basic '?' tests */
    err |= check_get_resource(ctx, "?dpi: 96", "Xft.dpi", "", "96");
    err |= check_get_resource(ctx, "A?C.d: 96", "A.B.C.d", "", "96");
    err |= check_get_resource(ctx, "A??d: 96", "A.B.C.d", "", "96");
    err |= check_get_resource(ctx, "A?d: 96", "A.B.C.d", "", NULL);

    /* Basic '*' tests */
    err |= check_get_resource(ctx, "*dpi: 96", "Xft.dpi", "", "96");
    err |= check_get_resource(ctx, "Xft*dpi: 96", "Xft.dpi", "", "96");
    err |= check_get_resource(ctx, "Xft**dpi: 96", "Xft.dpi", "", "96");
    err |= check_get_resource(ctx, "Xft**dpi: 96", "Xft.bla.dpi", "", "96");
    err |= check_get_resource(ctx, "Xft*?dpi: 96", "Xft.dpi", "", NULL);
//    err |= check_get_resource(ctx, "Xft*?dpi: 96", "Xft.foo.dpi", "", "96");

    /* Basic precedence tests*/
    //   - Individual tests for precedence rules
    err |= check_get_resource(ctx, "Xft*dpi: 96\nXft.foo.dpi: 97", "Xft.foo.dpi", "", "97");
    err |= check_get_resource(ctx, "Xft.foo.dpi: 96\nXft*dpi: 97", "Xft.foo.dpi", "", "96");
    err |= check_get_resource(ctx, "Xft?dpi: 96\nXft*dpi: 97", "Xft.foo.dpi", "", "96");
    err |= check_get_resource(ctx, "Xft*dpi: 96\nXft?dpi: 97", "Xft.foo.dpi", "", "97");
    err |= check_get_resource(ctx, "Xft.foo.dpi: 96\nXft?dpi: 97", "Xft.foo.dpi", "", "96");


    // TODO XXX Tests that need to be written and implemented:
    //   - The example from the docs
    //   - Individual tests for precedence rules
    //   - Different length for res_name / res_class in all combinations.

    // TODO XXX Tests
//    err |= check_get_resource("*theme: fun", "Cursor.theme", "", "fun");

    xcb_xrm_context_free(ctx);
    xcb_disconnect(conn);
    return err;
}

int main(void) {
    bool err = false;

    err |= test_entry_parser();
    err |= test_get_resource();

    return err;
}
