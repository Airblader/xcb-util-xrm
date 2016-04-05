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

#include "tests_utils.h"

/* Forward declarations */
static int test_put_resource(void);
static int test_combine_databases(void);
static int test_from_file(void);
static void setup(void);
static void cleanup(void);

xcb_connection_t *conn;

int main(void) {
    bool err = false;

    setup();
    err |= test_put_resource();
    err |= test_combine_databases();
    err |= test_from_file();
    cleanup();

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
    xcb_xrm_database_free(source_db);
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
    xcb_xrm_database_free(source_db);
    xcb_xrm_database_free(target_db);

    return err;
}

static int test_from_file(void) {
    bool err = false;
    xcb_xrm_database_t *database;

    /* Test xcb_xrm_database_from_file with relative #include directives */
    database = xcb_xrm_database_from_file("tests/resources/1/xresources1");
    err |= check_database(database,
            "First: 1\n"
            "Third: 3\n"
            "Second: 2\n");
    xcb_xrm_database_free(database);

    /* Test xcb_xrm_database_from_default for resolution of $HOME. */
    setenv("HOME", "tests/resources/2", true);
    setenv("XENVIRONMENT", "tests/resources/2/xenvironment", true);
    database = xcb_xrm_database_from_default(conn);
    err |= check_database(database,
            "First: 1\n"
            "Second: 2\n");
    xcb_xrm_database_free(database);

    return err;
}

static void setup(void) {
    int screennr;
    conn = xcb_connect(NULL, &screennr);
}

static void cleanup(void) {
    xcb_disconnect(conn);
}
