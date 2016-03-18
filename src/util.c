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

#include "util.h"

char *sstrdup(const char *str) {
    char *result = strdup(str);
    if (result == NULL) {
        err(-ENOMEM, "strdup() failed!");
    }

    return result;
}

void *scalloc(size_t num, size_t size) {
    void *result = calloc(num, size);
    if (result == NULL) {
        err(-ENOMEM, "calloc(%zd, %zd) failed!", num, size);
    }

    return result;
}

int str2int(int *out, char *input, int base) {
    char *end;
    long result;

    if (input[0] == '\0' || isspace(input[0]))
        return -FAILURE;

    errno = 0;
    result = strtol(input, &end, base);
    if (result > INT_MAX || (errno == ERANGE && result == LONG_MAX))
        return -FAILURE;
    if (result < INT_MIN || (errno == ERANGE && result == LONG_MIN))
        return -FAILURE;
    if (*end != '\0')
        return -FAILURE;

    *out = result;
    return SUCCESS;
}

char *xcb_util_get_property(xcb_connection_t *conn, xcb_window_t window, xcb_atom_t atom,
        xcb_atom_t type, size_t size) {
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;
    xcb_generic_error_t *err;
    int reply_length;
    char *content;

    cookie = xcb_get_property(conn, 0, window, atom, type, 0, size);
    reply = xcb_get_property_reply(conn, cookie, &err);
    if (err != NULL) {
        FREE(err);
        return NULL;
    }

    if (reply == NULL || (reply_length = xcb_get_property_value_length(reply)) == 0) {
        return NULL;
    }

    if (reply->bytes_after > 0) {
        size_t adjusted_size = size + ceil(reply->bytes_after / 4.0);
        FREE(reply);
        return xcb_util_get_property(conn, window, atom, type, adjusted_size);
    }

    if (asprintf(&content, "%.*s", reply_length, (char *)xcb_get_property_value(reply)) < 0) {
        FREE(reply);
        return NULL;
    }

    FREE(reply);
    return content;
}
