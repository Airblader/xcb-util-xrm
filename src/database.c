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

/* Forward declarations */
void xcb_xrm_database_put(xcb_xrm_database_t *database, xcb_xrm_entry_t *entry, bool override);

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
        xcb_xrm_database_put_resource_line(&database, line);
    }

    FREE(str);
    FREE(str_continued);
    return database;
}

/*
 * Returns a string representation of a database.
 *
 * @param database The database to return in string format.
 * @returns A string representation of the specified database.
 */
char *xcb_xrm_database_to_string(xcb_xrm_database_t *database) {
    char *result = NULL;

    xcb_xrm_entry_t *entry;
    TAILQ_FOREACH(entry, database, entries) {
        char *entry_str = xcb_xrm_entry_to_string(entry);
        char *tmp;
        sasprintf(&tmp, "%s%s\n", result == NULL ? "" : result, entry_str);
        FREE(entry_str);
        FREE(result);
        result = tmp;
    }

    return result;
}

/*
 * Combines two databases.
 * The entries from the source database are stored in the target database. If
 * the same specifier already exists in the target database, the value will be
 * overridden if override is set; otherwise, the value is discarded.
 * The source database will implicitly be free'd and must not be used
 * afterwards.
 * If NULL is passed for target_db, a new and empty database will be created
 * and returned in the pointer.
 *
 * @param source_db Source database.
 * @param target_db Target database.
 * @param override If true, entries from the source database override entries
 * in the target database using the same resource specifier.
 */
void xcb_xrm_database_combine(xcb_xrm_database_t *source_db, xcb_xrm_database_t **target_db, bool override) {
    assert(source_db != NULL);

    if (*target_db == NULL)
        *target_db = xcb_xrm_database_from_string("");

    while (!TAILQ_EMPTY(source_db)) {
        xcb_xrm_entry_t *entry = TAILQ_FIRST(source_db);
        TAILQ_REMOVE(source_db, entry, entries);
        xcb_xrm_database_put(*target_db, entry, override);
    }

    xcb_xrm_database_free(source_db);
}

/*
 * Inserts a new resource into the database.
 * If the resource already exists, the current value will be replaced.
 * If NULL is passed for database, a new and empty database will be created and
 * returned in the pointer.
 *
 * Note that this is not the equivalent of @ref
 * xcb_xrm_database_put_resource_line when concatenating the resource name and
 * value with a colon. For example, if the value starts with a leading space,
 * this must (and will) be replaced with the special '\ ' sequence.
 *
 * @param database The database to modify.
 * @param resource The fully qualified or partial resource specifier.
 * @param value The value of the resource.
 */
void xcb_xrm_database_put_resource(xcb_xrm_database_t **database, const char *resource, const char *value) {
    char *escaped;
    char *line;

    assert(resource != NULL);
    assert(value != NULL);

    if (*database == NULL)
        *database = xcb_xrm_database_from_string("");

    escaped = xcb_xrm_entry_escape_value(value);
    sasprintf(&line, "%s: %s", resource, escaped);
    FREE(escaped);
    xcb_xrm_database_put_resource_line(database, line);
    FREE(line);
}

/*
 * Inserts a new resource into the database.
 * If the resource already exists, the current value will be replaced.
 * If NULL is passed for database, a new and empty database will be created and
 * returned in the pointer.
 *
 * @param database The database to modify.
 * @param line The complete resource specification to insert.
 */
void xcb_xrm_database_put_resource_line(xcb_xrm_database_t **database, const char *line) {
    xcb_xrm_entry_t *entry;

    assert(line != NULL);

    if (*database == NULL)
        *database = xcb_xrm_database_from_string("");

    /* Ignore comments and directives. The specification guarantees that no
     * whitespace is allowed before these characters. */
    if (line[0] == '!' || line[0] == '#')
        return;

    if (xcb_xrm_entry_parse(line, &entry, false) == 0) {
        xcb_xrm_database_put(*database, entry, true);
    } else {
        xcb_xrm_entry_free(entry);
    }
}

/**
 * Destroys the given database.
 *
 * @param database The database to destroy.
 *
 * @ingroup xcb_xrm_database_t
 */
void xcb_xrm_database_free(xcb_xrm_database_t *database) {
    if (database == NULL)
        return;

    while (!TAILQ_EMPTY(database)) {
        xcb_xrm_entry_t *entry = TAILQ_FIRST(database);
        TAILQ_REMOVE(database, entry, entries);
        xcb_xrm_entry_free(entry);
    }

    FREE(database);
}

void xcb_xrm_database_put(xcb_xrm_database_t *database, xcb_xrm_entry_t *entry, bool override) {
    xcb_xrm_entry_t *current;

    if (entry == NULL)
        return;

    /* Let's see whether this is a duplicate entry. */
    current = TAILQ_FIRST(database);
    while (current != NULL) {
        xcb_xrm_entry_t *previous = TAILQ_PREV(current, xcb_xrm_database_t, entries);

        if (xcb_xrm_entry_compare(entry, current) == 0) {
            if (!override) {
                xcb_xrm_entry_free(entry);
                return;
            }

            TAILQ_REMOVE(database, current, entries);
            xcb_xrm_entry_free(current);

            current = previous;
            if (current == NULL)
                current = TAILQ_FIRST(database);
        }

        if (current == NULL)
            break;
        current = TAILQ_NEXT(current, entries);
    }

    TAILQ_INSERT_TAIL(database, entry, entries);
}
