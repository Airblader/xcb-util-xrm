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

static int check_parse_entry(const char *str, const char *value, const char *bindings, const int count, ...) {
    xcb_xrm_entry_t *entry;
    xcb_xrm_component_t *component;
    int actual_length = 0;
    int i = 0;
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
        char tmp[2] = "\0";

        switch (component->type) {
            case CT_WILDCARD:
                err |= check_strings("?", curr, "Expected '?', but got <%s>\n", curr);
                break;
            case CT_NORMAL:
                err |= check_strings(component->name, curr, "Expected <%s>, but got <%s>\n", component->name, curr);
                break;
            default:
                err = true;
                break;
        }

        tmp[0] = bindings[i++];
        switch (component->binding_type) {
            case BT_TIGHT:
                err |= check_strings(tmp, ".", "Expected <%s>, but got <.>\n", tmp);
                break;
            case BT_LOOSE:
                err |= check_strings(tmp, "*", "Expected <%s>, but got <*>\n", tmp);
                break;
            default:
                err = true;
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
    xcb_xrm_resource_t *resource;

    fprintf(stderr, "== Assert that getting resource <%s> / <%s> returns <%s>\n",
            res_name, res_class, value);

    xcb_xrm_database_from_string(ctx, database);
    if (xcb_xrm_resource_get(ctx, res_name, res_class, &resource) < 0) {
        if (value != NULL) {
            fprintf(stderr, "xcb_xrm_resource_get() < 0\n");
            err = true;
        }

        goto done_get_resource;
    }

    err |= check_strings(value, xcb_xrm_resource_value(resource), "Expected <%s>, but got <%s>\n",
            value, resource->value);

done_get_resource:
    if (resource != NULL) {
        xcb_xrm_resource_free(resource);
    }

    return err;
}

static int test_entry_parser(void) {
    bool err = false;

    check_parse_entry_resource_only = false;

    /* Basics */
    err |= check_parse_entry("First: 1", "1", ".", 1, "First");
    err |= check_parse_entry("First.second: 1", "1", "..", 2, "First", "second");
    err |= check_parse_entry("First..second: 1", "1", "..", 2, "First", "second");
    /* Wildcards */
    err |= check_parse_entry("?.second: 1", "1", "..", 2, "?", "second");
    err |= check_parse_entry("First.?.third: 1", "1", "...", 3, "First", "?", "third");
    /* Loose bindings */
    err |= check_parse_entry("*second: 1", "1", "*", 1, "second");
    err |= check_parse_entry("First*third: 1", "1", ".*", 2, "First", "third");
    err |= check_parse_entry("First**third: 1", "1", ".*", 2, "First", "third");
    /* Combinations */
    err |= check_parse_entry("First*?.fourth: 1", "1", ".*.", 3, "First", "?", "fourth");
    /* Values */
    err |= check_parse_entry("First: 1337", "1337", ".", 1, "First");
    err |= check_parse_entry("First: -1337", "-1337", ".", 1, "First");
    err |= check_parse_entry("First: 13.37", "13.37", ".", 1, "First");
    err |= check_parse_entry("First: value", "value", ".", 1, "First");
    err |= check_parse_entry("First: #abcdef", "#abcdef", ".", 1, "First");
    err |= check_parse_entry("First: { key: 'value' }", "{ key: 'value' }", ".", 1, "First");
    err |= check_parse_entry("First: x?y", "x?y", ".", 1, "First");
    err |= check_parse_entry("First: x*y", "x*y", ".", 1, "First");
    /* Whitespace */
    err |= check_parse_entry("First:    x", "x", ".", 1, "First");
    err |= check_parse_entry("First: x   ", "x   ", ".", 1, "First");
    err |= check_parse_entry("First:    x   ", "x   ", ".", 1, "First");
    err |= check_parse_entry("First:x", "x", ".", 1, "First");
    err |= check_parse_entry("First: \t x", "x", ".", 1, "First");
    err |= check_parse_entry("First: \t x \t", "x \t", ".", 1, "First");
    /* Special characters */
    err |= check_parse_entry("First: \\ x", " x", ".", 1, "First");
    err |= check_parse_entry("First: \\\tx", "\tx", ".", 1, "First");
    err |= check_parse_entry("First: \\011x", "\tx", ".", 1, "First");
    err |= check_parse_entry("First: x\\\\x", "x\\x", ".", 1, "First");
    err |= check_parse_entry("First: x\\nx", "x\nx", ".", 1, "First");
    err |= check_parse_entry("First: \\080", "\\080", ".", 1, "First");
    err |= check_parse_entry("First: \\00a", "\\00a", ".", 1, "First");

    /* Invalid entries */
    err |= check_parse_entry_error(": 1", -1);
    err |= check_parse_entry_error("?: 1", -1);
    err |= check_parse_entry_error("First", -1);
    err |= check_parse_entry_error("First second", -1);
    err |= check_parse_entry_error("First.?: 1", -1);
    err |= check_parse_entry_error("Först: 1", -1);
    err |= check_parse_entry_error("F~rst: 1", -1);

    /* Test for parsing a resource used for queries. */
    check_parse_entry_resource_only = true;
    err |= check_parse_entry("First.second", NULL, "..", 2, "First", "second");
    err |= check_parse_entry_error("First.second: on", -1);
    err |= check_parse_entry_error("First*second", -1);
    err |= check_parse_entry_error("First.?.second", -1);
    err |= check_parse_entry_error("*second", -1);
    err |= check_parse_entry_error("?.second", -1);

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

    /* Non-matches / Errors */
    err |= check_get_resource(ctx, "", "", "", NULL);
    err |= check_get_resource(ctx, "", NULL, "", NULL);
    err |= check_get_resource(ctx, "", "", NULL, NULL);
    err |= check_get_resource(ctx, "", NULL, NULL, NULL);
    err |= check_get_resource(ctx, "First.second: 1", "First.second", "First.second.third", NULL);
    err |= check_get_resource(ctx, "", "First.second", "", NULL);
    err |= check_get_resource(ctx, "First.second: 1", "First.third", "", NULL);
    err |= check_get_resource(ctx, "First.second: 1", "First", "", NULL);
    err |= check_get_resource(ctx, "First: 1", "First.second", "", NULL);
    err |= check_get_resource(ctx, "First.?.fourth: 1", "First.second.third.fourth", "", NULL);
    err |= check_get_resource(ctx, "First*?.third: 1", "First.third", "", NULL);
    err |= check_get_resource(ctx, "First: 1", "first", "", NULL);
    err |= check_get_resource(ctx, "First: 1", "", "first", NULL);

    /* Basic matching */
    err |= check_get_resource(ctx, "First: 1", "First", "", "1");
    err |= check_get_resource(ctx, "First.second: 1", "First.second", "", "1");
    err |= check_get_resource(ctx, "?.second: 1", "First.second", "", "1");
    err |= check_get_resource(ctx, "First.?.third: 1", "First.second.third", "", "1");
    err |= check_get_resource(ctx, "First.?.?.fourth: 1", "First.second.third.fourth", "", "1");
    err |= check_get_resource(ctx, "*second: 1", "First.second", "", "1");
    err |= check_get_resource(ctx, "*third: 1", "First.second.third", "", "1");
    err |= check_get_resource(ctx, "First*second: 1", "First.second", "", "1");
    err |= check_get_resource(ctx, "First*third: 1", "First.second.third", "", "1");
    err |= check_get_resource(ctx, "First*fourth: 1", "First.second.third.fourth", "", "1");
//  err |= check_get_resource(ctx, "First*?.third: 1", "First.second.third", "", "1");
    err |= check_get_resource(ctx, "First: 1", "Second", "First", "1");
    err |= check_get_resource(ctx, "First.second: 1", "First.third", "first.second", "1");
    err |= check_get_resource(ctx, "First.second.third: 1", "First.third.third", "first.second.fourth", "1");
    err |= check_get_resource(ctx, "First*third*fifth: 1", "First.second.third.fourth.third.fifth", "", "1");
    /* Matching among multiple entries */
    err |= check_get_resource(ctx,
            "First: 1\n"
            "Second: 2\n",
            "First", "", "1");
    err |= check_get_resource(ctx,
            "First: 1\n"
            "Second: 2\n",
            "Second", "", "2");

    /* Precedence rules */
    /* Rule 1 */
    err |= check_get_resource(ctx,
            "First.second.third: 1\n"
            "First*third: 2\n",
            "First.second.third", "", "1");
    err |= check_get_resource(ctx,
            "First*third: 2\n"
            "First.second.third: 1\n",
            "First.second.third", "", "1");
    err |= check_get_resource(ctx,
            "First.second.third: 1\n"
            "First*third: 2\n",
            "x.x.x", "First.second.third", "1");
    err |= check_get_resource(ctx,
            "First*third: 2\n"
            "First.second.third: 1\n",
            "x.x.x", "First.second.third", "1");

    /* Rule 2 */
    err |= check_get_resource(ctx,
            "First.second: 1\n"
            "First.third: 2\n",
            "First.second", "First.third", "1");
    err |= check_get_resource(ctx,
            "First.third: 2\n"
            "First.second: 1\n",
            "First.second", "First.third", "1");
    err |= check_get_resource(ctx,
            "First.second.third: 1\n"
            "First.?.third: 2\n",
            "First.second.third", "", "1");
    err |= check_get_resource(ctx,
            "First.?.third: 2\n"
            "First.second.third: 1\n",
            "First.second.third", "", "1");
    err |= check_get_resource(ctx,
            "First.second.third: 1\n"
            "First.?.third: 2\n",
            "x.x.x", "First.second.third", "1");
    err |= check_get_resource(ctx,
            "First.?.third: 2\n"
            "First.second.third: 1\n",
            "x.x.x", "First.second.third", "1");
    /* Rule 3 */
    err |= check_get_resource(ctx,
            "First.second: 1\n"
            "First*second: 2\n",
            "First.second", "", "1");
    err |= check_get_resource(ctx,
            "First*second: 2\n"
            "First.second: 1\n",
            "First.second", "", "1");

    /* Some real world examples. May contain duplicates to the above tests. */

    /* From the specification:
     * https://tronche.com/gui/x/xlib/resource-manager/matching-rules.html */
    err |= check_get_resource(ctx,
            "xmh*Paned*activeForeground: red\n"
            "*incorporate.Foreground: blue\n"
            "xmh.toc*Command*activeForeground: green\n"
            "xmh.toc*?.Foreground: white\n"
            "xmh.toc*Command.activeForeground: black",
            "xmh.toc.messagefunctions.incorporate.activeForeground",
            "Xmh.Paned.Box.Command.Foreground",
            "black");

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
