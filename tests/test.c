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

#include <X11/Xlib-xcb.h>
#include <X11/Xresource.h>
#include <xcb/xcb_aux.h>

#include "database.h"
#include "resource.h"

#if defined(__GNUC__) && ((__GNUC__ * 100 + __GNUC_MINOR__) >= 203)
#define ATTRIBUTE_PRINTF(x,y) __attribute__((__format__(__printf__,x,y)))
#else
#define ATTRIBUTE_PRINTF(x,y)
#endif

#define SKIP 77

static Display *display;
static xcb_connection_t *conn;
static xcb_screen_t *screen;
static bool check_parse_entry_resource_only;

/* Test setup */
static void setup(void);
static void cleanup(void);

/* Tests */
static int test_entry_parser(void);
static int test_get_resource(void);
static int test_put_resource(void);
static int test_combine_databases(void);

/* Assertion utilities */
static bool check_strings(const char *expected, const char *actual,
        const char *format, ...) ATTRIBUTE_PRINTF(3, 4);
static bool check_ints(const int expected, const int actual,
        const char *format, ...) ATTRIBUTE_PRINTF(3, 4);

/* Utilities */
static int check_parse_entry(const char *str, const char *value, const char *bindings, const int count, ...);
static int check_parse_entry_error(const char *str, const int result);
static char *check_get_resource_xlib(const char *str_database, const char *res_name, const char *res_class);
static int check_get_resource(const char *database, const char *res_name, const char *res_class, const char *value,
        bool expected_xlib_mismatch);
static int check_database(xcb_xrm_database_t *database, const char *expected);

int main(void) {
    bool err = false;

    setup();
    err |= test_entry_parser();
    err |= test_get_resource();
    err |= test_put_resource();
    err |= test_combine_databases();
    cleanup();

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
    err |= check_parse_entry("First: x\\ x", "x x", ".", 1, "First");
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
    bool err = false;

    /* Non-matches / Errors */
    err |= check_get_resource("", "", "", NULL, false);
    err |= check_get_resource("", NULL, "", NULL, false);
    err |= check_get_resource("", "", NULL, NULL, false);
    err |= check_get_resource("", NULL, NULL, NULL, false);
    /* Xlib returns the match here, despite the query violating the specs. */
    err |= check_get_resource("First.second: 1", "First.second", "First.second.third", NULL, true);
    err |= check_get_resource("", "First.second", "", NULL, false);
    err |= check_get_resource("First.second: 1", "First.third", "", NULL, false);
    err |= check_get_resource("First.second: 1", "First", "", NULL, false);
    err |= check_get_resource("First: 1", "First.second", "", NULL, false);
    err |= check_get_resource("First.?.fourth: 1", "First.second.third.fourth", "", NULL, false);
    err |= check_get_resource("First*?.third: 1", "First.third", "", NULL, false);
    err |= check_get_resource("First: 1", "first", "", NULL, false);
    err |= check_get_resource("First: 1", "", "first", NULL, false);
    /* Duplicate entries */
    err |= check_get_resource(
            "First: 1\n"
            "First: 2\n"
            "First: 3\n",
            "First", "", "3", false);
    err |= check_get_resource(
            "First: 1\n"
            "Second: 2\n"
            "Second: 3\n"
            "Third: 4\n",
            "Second", "", "3", false);

    /* Basic matching */
    err |= check_get_resource("First: 1", "First", "", "1", false);
    err |= check_get_resource("First.second: 1", "First.second", "", "1", false);
    err |= check_get_resource("?.second: 1", "First.second", "", "1", false);
    err |= check_get_resource("First.?.third: 1", "First.second.third", "", "1", false);
    err |= check_get_resource("First.?.?.fourth: 1", "First.second.third.fourth", "", "1", false);
    err |= check_get_resource("*second: 1", "First.second", "", "1", false);
    err |= check_get_resource("*third: 1", "First.second.third", "", "1", false);
    err |= check_get_resource("First*second: 1", "First.second", "", "1", false);
    err |= check_get_resource("First*third: 1", "First.second.third", "", "1", false);
    err |= check_get_resource("First*fourth: 1", "First.second.third.fourth", "", "1", false);
    err |= check_get_resource("First*?.third: 1", "First.second.third", "", "1", false);
    err |= check_get_resource("First: 1", "Second", "First", "1", false);
    err |= check_get_resource("First.second: 1", "First.third", "first.second", "1", false);
    err |= check_get_resource("First.second.third: 1", "First.third.third", "first.second.fourth", "1", false);
    err |= check_get_resource("First*third*fifth: 1", "First.second.third.fourth.third.fifth", "", "1", false);
    err |= check_get_resource("First: x\\\ny", "First", "", "xy", false);
    err |= check_get_resource("! First: x", "First", "", NULL, false);
    err |= check_get_resource("# First: x", "First", "", NULL, false);
    /* Matching among multiple entries */
    err |= check_get_resource(
            "First: 1\n"
            "Second: 2\n",
            "First", "", "1", false);
    err |= check_get_resource(
            "First: 1\n"
            "Second: 2\n",
            "Second", "", "2", false);

    /* Precedence rules */
    /* Rule 1 */
    err |= check_get_resource(
            "First.second.third: 1\n"
            "First*third: 2\n",
            "First.second.third", "", "1", false);
    err |= check_get_resource(
            "First*third: 2\n"
            "First.second.third: 1\n",
            "First.second.third", "", "1", false);
    err |= check_get_resource(
            "First.second.third: 1\n"
            "First*third: 2\n",
            "x.x.x", "First.second.third", "1", false);
    err |= check_get_resource(
            "First*third: 2\n"
            "First.second.third: 1\n",
            "x.x.x", "First.second.third", "1", false);

    /* Rule 2 */
    err |= check_get_resource(
            "First.second: 1\n"
            "First.third: 2\n",
            "First.second", "First.third", "1", false);
    err |= check_get_resource(
            "First.third: 2\n"
            "First.second: 1\n",
            "First.second", "First.third", "1", false);
    err |= check_get_resource(
            "First.second.third: 1\n"
            "First.?.third: 2\n",
            "First.second.third", "", "1", false);
    err |= check_get_resource(
            "First.?.third: 2\n"
            "First.second.third: 1\n",
            "First.second.third", "", "1", false);
    err |= check_get_resource(
            "First.second.third: 1\n"
            "First.?.third: 2\n",
            "x.x.x", "First.second.third", "1", false);
    err |= check_get_resource(
            "First.?.third: 2\n"
            "First.second.third: 1\n",
            "x.x.x", "First.second.third", "1", false);
    /* Rule 3 */
    err |= check_get_resource(
            "First.second: 1\n"
            "First*second: 2\n",
            "First.second", "", "1", false);
    err |= check_get_resource(
            "First*second: 2\n"
            "First.second: 1\n",
            "First.second", "", "1", false);

    /* Some real world examples. May contain duplicates to the above tests. */

    /* From the specification:
     * https://tronche.com/gui/x/xlib/resource-manager/matching-rules.html */
    err |= check_get_resource(
            "xmh*Paned*activeForeground: red\n"
            "*incorporate.Foreground: blue\n"
            "xmh.toc*Command*activeForeground: green\n"
            "xmh.toc*?.Foreground: white\n"
            "xmh.toc*Command.activeForeground: black",
            "xmh.toc.messagefunctions.incorporate.activeForeground",
            "Xmh.Paned.Box.Command.Foreground",
            "black", false);
    err |= check_get_resource("urxvt*background: [95]#000", "urxvt.background", "", "[95]#000", false);
    err |= check_get_resource("urxvt*scrollBar_right:true", "urxvt.scrollBar_right", "", "true", false);
    err |= check_get_resource("urxvt*cutchars:    '\"'()*<>[]{|}", "urxvt.cutchars", "", "'\"'()*<>[]{|}", false);
    err |= check_get_resource("urxvt.keysym.Control-Shift-Up: perl:font:increment", "urxvt.keysym.Control-Shift-Up",
            "", "perl:font:increment", false);
    err |= check_get_resource("rofi.normal: #000000, #000000, #000000, #000000", "rofi.normal", "",
            "#000000, #000000, #000000, #000000", false);

    return err;
}

static int test_put_resource(void) {
    bool err = false;

    xcb_xrm_database_t *database = NULL;
    xcb_xrm_database_put_resource(&database, "First", "1");
    xcb_xrm_database_put_resource(&database, "First*second", "2");
    xcb_xrm_database_put_resource(&database, "Third", "  a\\ b\nc d\te ");
    xcb_xrm_database_put_resource(&database, "Fourth", "\t\ta\\ b\nc d\te ");
    err |= check_database(database,
            "First: 1\n"
            "First*second: 2\n"
            "Third: \\  a\\\\ b\\nc d\te \n"
            "Fourth: \\\t\ta\\\\ b\\nc d\te \n");

    xcb_xrm_database_put_resource(&database, "First", "3");
    xcb_xrm_database_put_resource(&database, "First*second", "4");
    xcb_xrm_database_put_resource(&database, "Third", "x");
    xcb_xrm_database_put_resource(&database, "Fourth", "x");
    err |= check_database(database,
            "First: 3\n"
            "First*second: 4\n"
            "Third: x\n"
            "Fourth: x\n");

    xcb_xrm_database_put_resource_line(&database, "Second:xyz");
    xcb_xrm_database_put_resource_line(&database, "Third:  xyz");
    xcb_xrm_database_put_resource_line(&database, "*Fifth.sixth*seventh.?.eigth*?*last: xyz");
    err |= check_database(database,
            "First: 3\n"
            "First*second: 4\n"
            "Fourth: x\n"
            "Second: xyz\n"
            "Third: xyz\n"
            "*Fifth.sixth*seventh.?.eigth*?*last: xyz\n");

    xcb_xrm_database_free(database);
    return err;
}

static int test_combine_databases(void) {
    bool err = false;

    xcb_xrm_database_t *source_db;
    xcb_xrm_database_t *target_db;

    source_db = xcb_xrm_database_from_string(
            "a1.b1*c1: 1\n"
            "a2.b2: 2\n"
            "a3: 3\n");
    target_db = xcb_xrm_database_from_string(
            "a3: 0\n"
            "a1.b1*c1: 0\n"
            "a4.?.b4: 0\n");
    xcb_xrm_database_combine(source_db, &target_db, false);
    err |= check_database(target_db,
            "a3: 0\n"
            "a1.b1*c1: 0\n"
            "a4.?.b4: 0\n"
            "a2.b2: 2\n");
    xcb_xrm_database_free(target_db);

    source_db = xcb_xrm_database_from_string(
            "a1.b1*c1: 1\n"
            "a2.b2: 2\n"
            "a3: 3\n");
    target_db = xcb_xrm_database_from_string(
            "a3: 0\n"
            "a1.b1*c1: 0\n"
            "a4.?.b4: 0\n");
    xcb_xrm_database_combine(source_db, &target_db, true);
    err |= check_database(target_db,
            "a4.?.b4: 0\n"
            "a1.b1*c1: 1\n"
            "a2.b2: 2\n"
            "a3: 3\n");
    xcb_xrm_database_free(target_db);

    return err;
}

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

static void setup(void) {
    display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "Failed to connect to X11 server.\n");
        exit(EXIT_FAILURE);
    }

    conn = XGetXCBConnection(display);

    screen = xcb_aux_get_screen(conn, DefaultScreen(display));
    if (screen == NULL) {
        fprintf(stderr, "Failed to get screen.\n");
        exit(EXIT_FAILURE);
    }

    XrmInitialize();
}

static void cleanup(void) {
    XCloseDisplay(display);
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

static char *check_get_resource_xlib(const char *str_database, const char *res_name, const char *res_class) {
    int res_code;
    char *res_type;
    XrmValue res_value;
    char *result;

    XrmDatabase database = XrmGetStringDatabase(str_database);
    res_code = XrmGetResource(database, res_name, res_class, &res_type, &res_value);

    if (res_code) {
        result = strdup((char *)res_value.addr);
    } else {
        result = NULL;
    }

    XrmDestroyDatabase(database);
    return result;
}

static int check_get_resource(const char *str_database, const char *res_name, const char *res_class, const char *value,
        bool expected_xlib_mismatch) {
    xcb_xrm_database_t *database;

    bool err = false;
    char *xcb_value;
    char *xlib_value;

    fprintf(stderr, "== Assert that getting resource <%s> / <%s> returns <%s>\n",
            res_name, res_class, value);

    database = xcb_xrm_database_from_string(str_database);
    xcb_value = xcb_xrm_resource_get_string(database, res_name, res_class);
    if (xcb_value == NULL) {
        if (value != NULL) {
            fprintf(stderr, "xcb_xrm_resource_get_string() returned NULL\n");
            err = true;
        }

        if (!expected_xlib_mismatch) {
            xlib_value = check_get_resource_xlib(str_database, res_name, res_class);
            err |= check_strings(NULL, xlib_value, "Returned NULL, but Xlib returned <%s>\n", xlib_value);
            if (xlib_value != NULL)
                free(xlib_value);
        }

        goto done_get_resource;
    }

    err |= check_strings(value, xcb_value, "Expected <%s>, but got <%s>\n", value, xcb_value);
    free(xcb_value);

    if (!expected_xlib_mismatch) {
        /* And for good measure, also compare it against Xlib. */
        xlib_value = check_get_resource_xlib(str_database, res_name, res_class);
        err |= check_strings(value, xlib_value, "Xlib returns <%s>, but expected <%s>\n",
                xlib_value, value);
        if (xlib_value != NULL)
            free(xlib_value);
    }

done_get_resource:
    xcb_xrm_database_free(database);
    return err;
}

static int check_database(xcb_xrm_database_t *database, const char *expected) {
    bool err = false;
    char *actual = xcb_xrm_database_to_string(database);

    err |= check_strings(expected, actual, "Expected database <%s>, but found <%s>\n", expected, actual);

    FREE(actual);
    return err;
}
