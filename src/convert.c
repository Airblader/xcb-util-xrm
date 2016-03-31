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

#include "convert.h"
#include "util.h"

/*
 * Converts a string value to a long.
 * If value is NULL or cannot be converted to a long, LONG_MIN is returned.
 *
 * @param value The string value to convert.
 * @returns The long to which the value converts or LONG_MIN if it cannot be
 * converted.
 */
long xcb_xrm_convert_to_long(const char *value) {
    long converted;
    if (value == NULL)
        return LONG_MIN;

    if (str2long(&converted, value, 10) == 0)
        return converted;

    return LONG_MIN;
}

/*
 * Converts a string value to a bool.
 *
 * The conversion is done by applying the following steps in order:
 *   - If value is NULL, return false.
 *   - If value can be converted to a long, return the truthiness of the
 *     converted number.
 *   - If value is one of "true", "on" or "yes" (case-insensitive), return
 *     true.
 *   - If value is one of "false", "off" or "no" (case-insensitive), return
 *     false.
 *   - Return false.
 *
 * @param value The string value to convert.
 * @returns The bool to which the value converts or false if it cannot be
 * converted.
 */
bool xcb_xrm_convert_to_bool(const char *value) {
    long converted;
    if (value == NULL)
        return false;

    /* Let's first see if the value can be parsed into an integer directly. */
    if (str2long(&converted, value, 10) == 0)
        return converted;

    /* Next up, we take care of signal words. */
    if (strcasecmp(value, "true") == 0 ||
            strcasecmp(value, "on") == 0 ||
            strcasecmp(value, "yes") == 0) {
        return true;
    }

    if (strcasecmp(value, "false") == 0 ||
            strcasecmp(value, "off") == 0 ||
            strcasecmp(value, "no") == 0) {
        return false;
    }

    /* Time to give up. */
    return false;
}
