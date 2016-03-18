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
#include "externals.h"

#include "database.h"
#include "match.h"
#include "util.h"

/*
 * Loads the RESOURCE_MANAGER property and creates a database with its
 * contents. If the database could not be created, thie function will return
 * NULL.
 *
 * @param conn A working XCB connection.
 * @param screen The xcb_screen_t* screen to use.
 * @returns The database described by the RESOURCE_MANAGER property.
 *
 * @ingroup xcb_xrm_database_t
 */
xcb_xrm_database_t *xcb_xrm_database_from_resource_manager(xcb_connection_t *conn, xcb_screen_t *screen) {
    char *resources = xcb_util_get_property(conn, screen->root, XCB_ATOM_RESOURCE_MANAGER,
            XCB_ATOM_STRING, 16 * 1024);
    if (resources == NULL) {
        return NULL;
    }

    /* Parse the resource string. */
    return xcb_xrm_database_from_string(resources);
}

/*
 * Creates a database from the given string.
 * If the database could not be created, this function will return NULL.
 *
 * @param str The resource string.
 * @returns The database described by the resource string.
 *
 * @ingroup xcb_xrm_database_t
 */
xcb_xrm_database_t *xcb_xrm_database_from_string(const char *_str) {
    xcb_xrm_database_t *database;
    char *str = sstrdup(_str);

    int num_continuations = 0;
    char *str_continued;
    char *outwalk;

    /* Count the number of line continuations. */
    for (char *walk = str; *walk != '\0'; walk++) {
        if (*walk == '\\' && *(walk + 1) == '\n') {
            num_continuations++;
        }
    }

    /* Take care of line continuations. */
    str_continued = scalloc(1, strlen(str) + 1 - 2 * num_continuations);
    outwalk = str_continued;
    for (char *walk = str; *walk != '\0'; walk++) {
        if (*walk == '\\' && *(walk + 1) == '\n') {
            walk++;
            continue;
        }

        *(outwalk++) = *walk;
    }
    *outwalk = '\0';

    database = scalloc(1, sizeof(struct xcb_xrm_database_t));
    TAILQ_INIT(database);

    for (char *line = strtok(str_continued, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        xcb_xrm_entry_t *entry;

        /* Ignore comments and directives. The specification guarantees that no
         * whitespace is allowed before these characters. */
        if (line[0] == '!' || line[0] == '#')
            continue;

        if (xcb_xrm_entry_parse(line, &entry, false) == 0 && entry != NULL) {
            TAILQ_INSERT_TAIL(database, entry, entries);
        }
    }

    FREE(str);
    FREE(str_continued);
    return database;
}

/**
 * Destroys the given database.
 *
 * @param database The database to destroy.
 *
 * @ingroup xcb_xrm_database_t
 */
void xcb_xrm_database_free(xcb_xrm_database_t *database) {
    xcb_xrm_entry_t *entry;
    if (database == NULL)
        return;

    while (!TAILQ_EMPTY(database)) {
        entry = TAILQ_FIRST(database);
        TAILQ_REMOVE(database, entry, entries);
        xcb_xrm_entry_free(entry);
    }

    FREE(database);
}
