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

#include "resource.h"
#include "database.h"
#include "match.h"
#include "util.h"

/*
 * Fetches a resource from the database.
 *
 * @param database The database to use.
 * @param res_name The fully qualified resource name.
 * @param res_class The fully qualified resource class. Note that this argument
 * may be left empty / NULL, but if given, it must contain the same number of
 * components as the resource name.
 * @param resource A pointer to a xcb_xrm_resource_t* which will be modified to
 * contain the matched resource. Note that this resource must be free'd by the
 * caller.
 * @return 0 on success, a negative error code otherwise.
 */
int xcb_xrm_resource_get(xcb_xrm_database_t *database, const char *res_name, const char *res_class,
                         xcb_xrm_resource_t **_resource) {
    xcb_xrm_resource_t *resource;
    xcb_xrm_entry_t *query_name = NULL;
    xcb_xrm_entry_t *query_class = NULL;
    int result = 0;

    if (database == NULL || TAILQ_EMPTY(database)) {
        *_resource = NULL;
        return -1;
    }

    *_resource = scalloc(1, sizeof(struct xcb_xrm_resource_t));
    resource = *_resource;

    if (xcb_xrm_entry_parse(res_name, &query_name, true) < 0) {
        result = -1;
        goto done;
    }

    /* For the resource class input, we allow NULL and empty string as
     * placeholders for not specifying this string. Technically this is
     * violating the spec, but it seems to be widely used. */
    if (res_class != NULL && strlen(res_class) > 0 &&
            xcb_xrm_entry_parse(res_class, &query_class, true) < 0) {
        result = -1;
        goto done;
    }

    /* We rely on name and class query strings to have the same number of
     * components, so let's check that this is the case. The specification
     * backs us up here. */
    if (query_class != NULL &&
            xcb_xrm_entry_num_components(query_name) != xcb_xrm_entry_num_components(query_class)) {
        result = -1;
        goto done;
    }

    result = xcb_xrm_match(database, query_name, query_class, resource);
done:
    xcb_xrm_entry_free(query_name);
    xcb_xrm_entry_free(query_class);
    return result;
}

/*
 * Returns the string value of the resource.
 *
 * @param resource The resource to use.
 * @returns The string value of the given resource.
 */
char *xcb_xrm_resource_value(xcb_xrm_resource_t *resource) {
    if (resource == NULL)
        return NULL;

    return resource->value;
}

/*
 * Converts the resource's value into an integer and returns it.
 *
 * @param resource The resource to use.
 * @returns The value as an integer if it was merely a number. Otherwise, this
 * returns 0 for 'off', 'no' or 'false' and 1 for 'on', 'yes' and 'true',
 * respectively, with all comparisons being case-insensitive. For all other
 * values, INT_MIN is returned.
 */
int xcb_xrm_resource_value_int(xcb_xrm_resource_t *resource) {
    int converted;
    if (resource == NULL)
        return INT_MIN;

    /* Let's first see if the value can be parsed into an integer directly. */
    if (str2int(&converted, resource->value, 10) == 0)
        return converted;

    /* Next up, we take care of signal words. */
    if (strcasecmp(resource->value, "true") == 0 ||
            strcasecmp(resource->value, "on") == 0 ||
            strcasecmp(resource->value, "yes") == 0) {
        return 1;
    }

    if (strcasecmp(resource->value, "false") == 0 ||
            strcasecmp(resource->value, "off") == 0 ||
            strcasecmp(resource->value, "no") == 0) {
        return 0;
    }

    /* Time to give up. */
    return INT_MIN;
}

/*
 * Destroy the given resource.
 *
 * @param resource The resource to destroy.
 */
void xcb_xrm_resource_free(xcb_xrm_resource_t *resource) {
    if (resource == NULL)
        return;

    FREE(resource->value);
    FREE(resource);
}
