#include <cJSON.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>

#include "payloads.h"

/* ------------------------------- dyn str ------------------------------ */

/**
 * Growable string buffer
 *
 * Created on 2020/06/05.
 * (c) Ondřej Hruška
 */

/** String capacity to use if cap=0 is given to the constructor */
#define STR_DEF_CAPACITY 64
/** Minimal re-alloc size increment - to avoid too frequent reallocs */
#define STR_MIN_ALLOC 16

/** Growable string buffer */
struct dynstr {
    /** Pointer to the backing buffer */
    char *buffer;
    /** Write pointer */
    char *wp;
    /** Buffer capacity */
    size_t capacity;
    /** Remaining capacity */
    size_t remains;
};

/** Dynamically allocated growable string buffer */
typedef struct dynstr *DynStr;

/** Create a new growable string buffer with the given initial capacity */
static DynStr dstr_new(size_t initial_capacity);

/** De-allocate a growable string buffer */
static void dstr_free(DynStr str);

/**
 * Append N bytes of a char* to a growable string buffer.
 *
 * @param[in,out] str - growable buffer, must not be NULL
 * @param[in] append - string to append, must not be NULL
 * @param[in] len - number of bytes to append
 * @return true on success, false on allocation error or bad arguments
 */
static bool dstr_pushn(DynStr str, const char *append, size_t len);

/**
 * Append a char* to a growable string buffer.
 *
 * @param[in,out] str - growable buffer, must not be NULL
 * @param[in] append - string to append, must not be NULL
 * @return true on success, false on allocation error or bad arguments
 */
static bool dstr_push(DynStr str, const char *append);

/** Turn a growable string buffer into a simple buffer, freeing the builder wrapper */
static char *dstr_into_raw(DynStr str);

/* -------------------------- dynstr impl ---------------------------- */

/**
 * Re-allocate a memory region, clearing the new part (like calloc).
 * Returns NULL if realloc fails.
 */
static char *realloc_clear(char *buf, size_t oldlen, size_t newlen)
{
    if (newlen <= oldlen) return buf;
    char *new = (char *) cJSON_malloc(newlen);
    if (!new) return NULL; // buf stays allocated

    // copy data over
    memcpy(new, buf, newlen > oldlen ? oldlen : newlen);

    // get rid of the old buffer
    cJSON_free(buf);

    bzero(new + oldlen, newlen - oldlen);
    return new;
}

static DynStr dstr_new(size_t initial_capacity)
{
    if (initial_capacity == 0) {
        initial_capacity = STR_DEF_CAPACITY;
    }

    if (initial_capacity < STR_MIN_ALLOC) {
        initial_capacity = STR_MIN_ALLOC;
    }

    DynStr s = cJSON_malloc(sizeof(struct dynstr));
    if (!s) return NULL;
    bzero(s, sizeof(struct dynstr));
    s->buffer = cJSON_malloc(initial_capacity);
    if (!s->buffer) {
        cJSON_free(s);
        return NULL;
    }
    bzero(s->buffer, sizeof(struct dynstr));
    s->wp = s->buffer;
    s->capacity = initial_capacity;
    s->remains = initial_capacity;
    return s;
}

static void dstr_free(DynStr str)
{
    if (!str) return;

    if (str->buffer) {
        cJSON_free(str->buffer);
        str->buffer = NULL;
        str->wp = NULL;
    }
    cJSON_free(str);
}

static bool dstr_pushn(DynStr str, const char *append, size_t len)
{
    if (!str->buffer || !str->wp) {
        return false;
    }

    if (append == NULL) {
        return false;
    }

    size_t needed = len + 1; // include the terminator
    if (str->remains >= needed) {
        enough_space:
        memcpy(str->wp, append, len);
        str->wp += len;
        *str->wp = 0; // terminator
        str->remains -= len;
        return true;
    } else {
        size_t to_add = needed - str->remains;
        if (to_add < STR_MIN_ALLOC) {
            to_add = STR_MIN_ALLOC;
        }
        size_t new_cap = str->capacity + to_add;
        char *newbuf = realloc_clear(str->buffer, str->capacity, new_cap);
        if (!newbuf) return false;
        str->buffer = newbuf;
        str->wp = str->buffer + (str->capacity - str->remains);
        str->capacity = new_cap;
        str->remains = str->remains + to_add;
        goto enough_space;
    }
}

static bool dstr_push(DynStr str, const char *append)
{
    if (!str->buffer || !str->wp) {
        return false;
    }

    if (append == NULL) {
        return false;
    }

    return dstr_pushn(str, append, strlen(append));
}

static char *dstr_into_raw(DynStr str)
{
    char *buf = str->buffer;
    str->buffer = NULL;
    str->wp = NULL;
    dstr_free(str);
    return buf;
}

/* ------------------------------------------------------------------- */



// 100 spaces
static const char *INDENTS = "                                                                                                    ";
#define INDENT_WIDTH 4

struct export_opts {
    bool colors;
    bool annotated;
};

/** Print the data part of a payload */
static bool print_data(cJSON *data, DynStr str, int indent, struct export_opts *opts);

/** Push char* to Str (up to \0), goto fail on error. */
#define TRY_PUSH(__str, __what) \
    do {  \
        if(!dstr_push(__str, __what)) goto fail; \
    } while(0)

/** Push N bytes of char* to Str, goto fail on error. */
#define TRY_PUSHN(__str, __what, __len) \
    do {  \
        if(!dstr_pushn(__str, __what, __len)) goto fail;  \
    } while(0)

/* ANSI color escape sequences */
#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define YELLOW "\x1b[33m"
#define BLUE "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN "\x1b[36m"
#define WHITE "\x1b[37m"
#define GRAY "\x1b[90m"
#define RST "\x1b[m"
#define BOLD "\x1b[1m"
#define DEFAULT "\x1b[39m"

/* color setup */
#define COLOR_HEADER GREEN
#define COLOR_UNIT   DEFAULT
#define COLOR_SYMBOL DEFAULT
#define COLOR_NAME   GREEN BOLD
#define COLOR_FIELD  CYAN
#define COLOR_DESCR  GRAY

/* value colors */
#define COLOR_VALUE       YELLOW
#define COLOR_VALUE_TRUE  GREEN
#define COLOR_VALUE_FALSE RED

/* string templates */
#define S_DESCR_JOINER " ... "
#define S_VALUE_JOINER " = "
#define S_STRUCT_JOINER ":"

/** Set color, if colors are enabled */
#define CLR(__str, __c) \
    do {                                        \
        if (opts->colors) TRY_PUSH(__str, __c); \
    } while(0)

/** Push string in the given color, if colors are enabled */
#define TRY_PUSH_CLR(__str, __c, __val) \
    do {                                \
        CLR(__str, __c);                \
        TRY_PUSH(__str, __val);         \
        CLR(__str, RST);                \
    } while (0)


char *pld_pretty_print(cJSON *pld, bool colors)
{
    DynStr str = NULL;

    if (!cJSON_IsObject(pld)) {
        // invalid, try to retain information
        goto fail;
    }

    str = dstr_new(256);

    cJSON *name = cJSON_GetObjectItem(pld, "name");
    cJSON *descr = cJSON_GetObjectItem(pld, "descr");
    cJSON *data = cJSON_GetObjectItem(pld, "data");

    struct export_opts opts_s = {
        .colors = colors,
        .annotated = false,
    };
    struct export_opts *opts = &opts_s;

    if (data && !cJSON_IsObject(data)) {
        goto fail;
    }

    if (name && data) {
        // this looks like a BASIC or ANNOTATED payload
        TRY_PUSH_CLR(str, COLOR_HEADER, "Payload ");
        TRY_PUSH_CLR(str, COLOR_NAME, cJSON_GetStringValue(name));

        if (descr) {
            TRY_PUSH(str, "\n");
            TRY_PUSH_CLR(str, COLOR_DESCR, cJSON_GetStringValue(descr));
        }
        TRY_PUSH(str, "\n");

        cJSON* first = data->child;
        TRY_PUSHN(str, INDENTS, 1 * INDENT_WIDTH);

        if (!first) {
            TRY_PUSH_CLR(str, COLOR_FIELD, "Empty body.\n");
        } else {
            if (cJSON_IsObject(first) && cJSON_GetObjectItem(first, "type")) {
                opts->annotated = true;
                if (!print_data(data, str, 1, opts)) {
                    goto data_fail;
                }
            }
            else {
                if (!print_data(data, str, 1, opts)) {
                    goto data_fail;
                }
            }
        }

    } else {
        // this looks like a dataonly payload
        TRY_PUSH_CLR(str, COLOR_HEADER, "Payload data:\n");
        TRY_PUSHN(str, INDENTS, 1 * INDENT_WIDTH);

        if (!print_data(pld, str, 1, opts)) {
            goto data_fail;
        }
    }

    return dstr_into_raw(str);

data_fail:;
    // Failed to render the data part of the payload, export it as raw JSON
    char *stringified = cJSON_Print(pld);
    TRY_PUSH(str, "??? ");
    TRY_PUSH(str, stringified);
    cJSON_free(stringified);

    return dstr_into_raw(str);

fail:
    // Fatal error, export pld as raw JSON
    if (str) dstr_free(str);
    return cJSON_Print(pld);
}

/** Get color for a cJSON value */
static const char *value_color(const cJSON *field) {
    if (cJSON_IsBool(field)) {
        // red/green color for bool
        if (field->type == cJSON_True) {
            return COLOR_VALUE_TRUE;
        } else {
            return COLOR_VALUE_FALSE;
        }
    }
    // default value color
    return COLOR_VALUE;
}

static bool print_data(cJSON *data, DynStr str, int indent, struct export_opts *opts)
{
    cJSON *field_iter = NULL;
    bool obj_first_iter = true;
    /* iterate key-value pairs as array */
    cJSON_ArrayForEach(field_iter, data) {
        if (!obj_first_iter) TRY_PUSHN(str, INDENTS, indent * INDENT_WIDTH);
        obj_first_iter = false;

        cJSON *field = field_iter;

        const char *name = field->string;
        const char *unit = NULL;
        const char *descr = NULL;

        if (opts->annotated) {
            cJSON* cjDescr = cJSON_GetObjectItem(field, "descr");
            if (cjDescr && cJSON_IsString(cjDescr)) {
                descr = cjDescr->valuestring;
            }

            cJSON* cjUnit = cJSON_GetObjectItem(field, "unit");
            if (cjUnit && cJSON_IsString(cjUnit)) {
                unit = cjUnit->valuestring;
            }

            cJSON* cjValue = cJSON_GetObjectItem(field, "value");
            if (cjValue) {
                field = cjValue;
            } else {
                // special case for struct fields
                cjValue = cJSON_GetObjectItem(field, "items");
                if (cjValue) {
                    field = cjValue;
                } else {
                    // malformed
                    TRY_PUSH_CLR(str, COLOR_FIELD, name);
                    TRY_PUSH_CLR(str, COLOR_SYMBOL, S_VALUE_JOINER);
                    TRY_PUSH(str, "??? ");
                    char *stringified = cJSON_PrintUnformatted(field);
                    TRY_PUSH(str, stringified);
                    cJSON_free(stringified);
                    continue;
                }
            }
        }

        TRY_PUSH_CLR(str, COLOR_FIELD, name);

        if (cJSON_IsObject(field)) {
            /* nested struct or bitfield */
            if (descr) {
                TRY_PUSH_CLR(str, COLOR_SYMBOL, S_STRUCT_JOINER); // the struct will continue on the following lines
                TRY_PUSH_CLR(str, COLOR_DESCR, S_DESCR_JOINER);
                TRY_PUSH_CLR(str, COLOR_DESCR, descr);
                TRY_PUSH(str, "\n");
            } else {
                TRY_PUSH_CLR(str, COLOR_SYMBOL, S_STRUCT_JOINER "\n");
            }
            TRY_PUSHN(str, INDENTS, (indent + 1) * INDENT_WIDTH);
            print_data(field, str, indent + 1, opts);
        } else if (cJSON_IsArray(field)) {
            cJSON* first = field->child;
            cJSON *member = NULL;
            if (cJSON_IsObject(first)) {
                /* struct array */
                TRY_PUSH_CLR(str, COLOR_SYMBOL, S_STRUCT_JOINER "\n");

                cJSON_ArrayForEach(member, field) {
                    TRY_PUSHN(str, INDENTS, (indent + 1) * INDENT_WIDTH - 2);
                    TRY_PUSH_CLR(str, COLOR_SYMBOL, "- ");
                    print_data(member, str, indent + 1, opts);
                }
                TRY_PUSHN(str, INDENTS, indent * INDENT_WIDTH);
            } else {
                /* scalar array */
                TRY_PUSH_CLR(str, COLOR_SYMBOL, S_VALUE_JOINER);

                bool arr_first_iter = true;
                cJSON_ArrayForEach(member, field) {
                    if (!arr_first_iter) {
                        TRY_PUSH_CLR(str, COLOR_SYMBOL, ", ");
                    }
                    arr_first_iter = false;

                    char *repr = cJSON_PrintUnformatted(member);

                    CLR(str, value_color(member));
                    TRY_PUSH(str, repr);
                    cJSON_free(repr);

                    CLR(str, RST);
                }

                if (unit) {
                    TRY_PUSH(str, " ");
                    TRY_PUSH_CLR(str, COLOR_UNIT, unit);
                }

                if (descr) {
                    TRY_PUSH_CLR(str, COLOR_DESCR, S_DESCR_JOINER);
                    TRY_PUSH_CLR(str, COLOR_DESCR, descr);
                }

                TRY_PUSH(str, "\n");
            }
        } else {
            /* scalar value */
            TRY_PUSH_CLR(str, COLOR_SYMBOL, S_VALUE_JOINER);
            char *repr = cJSON_PrintUnformatted(field);

            CLR(str, value_color(field));
            TRY_PUSH(str, repr);
            cJSON_free(repr);
            CLR(str, RST);

            if (unit) {
                TRY_PUSH(str, " ");
                TRY_PUSH_CLR(str, COLOR_UNIT, unit);
            }

            if (descr) {
                TRY_PUSH_CLR(str, COLOR_DESCR, S_DESCR_JOINER);
                TRY_PUSH_CLR(str, COLOR_DESCR, descr);
            }

            TRY_PUSH(str, "\n");
        }
    }

    return true;
fail:
    return false;
}
